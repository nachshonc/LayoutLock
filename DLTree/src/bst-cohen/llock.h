#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <string.h>
#include <atomic>
#define CACHE_LINE_SIZE 64
#define PADDING(type) (CACHE_LINE_SIZE/sizeof(type))

extern __thread bool tidSet_;
extern __thread pid_t tid_;
extern __thread int stateOff_;
extern __thread int dirtyOff_;
extern pid_t nextThread;
typedef std::atomic<char> dptrtype; 

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#define CFENCE asm volatile ("":::"memory")

class ScalarRPRWLockImpl_ {
public:
	ScalarRPRWLockImpl_() : readerCount_(0) { }

	static void setup() {}

	void read_lock() {
		int64_t tmp=readerCount_;
		if (tmp < 0) {
			tmp = 0;
		}
		while (!__sync_bool_compare_and_swap(&readerCount_, tmp, tmp+1)) {
			tmp = readerCount_;
			if (tmp < 0)
				tmp = 0;
		}
	}
	void read_unlock() {
		__sync_sub_and_fetch(&readerCount_, 1);
	}
	void write_lock() {
		while (!__sync_bool_compare_and_swap(&readerCount_, 0, -1));
	}
	void write_unlock() {
		readerCount_ = 0;
	}

	inline bool isWriterActive() { return readerCount_ < 0; }
private:
	int32_t readerCount_;
};

class ScalarWPRWLockImpl_ : public ScalarRPRWLockImpl_ {
	volatile int readerCount_;
	volatile int writerPendingCount_;
	typedef ScalarRPRWLockImpl_ super;
public:
	ScalarWPRWLockImpl_() : writerPendingCount_(0) {}

	void read_lock() { 
		while (writerPendingCount_);
		super::read_lock();
	}
	void write_lock() { 
		__atomic_fetch_add(&writerPendingCount_, 1, __ATOMIC_SEQ_CST);
		super::write_lock();
	}

	void write_unlock() { 
		__atomic_fetch_sub(&writerPendingCount_, 1, __ATOMIC_SEQ_CST);
		super::write_unlock();
	}
};

template <int maxNumberOfThreads, typename baseRWLock_=ScalarRPRWLockImpl_>
class RWLock : public baseRWLock_ {
protected:
	enum {
		maxNumberOfThreads_ = maxNumberOfThreads
	};
public:
	typedef baseRWLock_ baseRWLock;

	static void setup() {}
	static void reset() {}
	int getMaxNumberOfThreads() const { return 1; }
};

template <int maxNumberOfThreads>
class ScalableRWLock {
	enum State {
		kInactive,
		kReaderActive,
		kWriterActive,
	};
	volatile int state_[maxNumberOfThreads * PADDING(int)];
protected:
	enum {
		maxNumberOfThreads_ = maxNumberOfThreads
	};
public:
	static const bool ACTIVE=true;
	ScalableRWLock() {
		for (int i=0; i < maxNumberOfThreads_; i++) {
			state_[i * PADDING(int)] = kInactive;
		}
	}

	bool isActive() { return true; }

	static void setup() {
		if (!tidSet_) {
			int tid = __atomic_fetch_add(&nextThread, 1, __ATOMIC_SEQ_CST);
      tid_=tid;
			stateOff_ = tid * PADDING(int);
			tidSet_ = true;
		}
	}

	static void reset() {
		nextThread=0;
	}

	void read_lock() {
		register int off = stateOff_;
		while (!__sync_bool_compare_and_swap(&state_[off], kInactive, kReaderActive)) ;
	}

	void read_unlock() {
		register int off = stateOff_;
		__sync_lock_release(&state_[off]);// = kInactive;
		//state_[off] = kInactive;
	}


	void write_lock() {
		for (int i=0; i < maxNumberOfThreads_; i++) {
			while (!__sync_bool_compare_and_swap(&state_[i * PADDING(int)], kInactive, kWriterActive)) ;
		}
		__sync_synchronize();
	}

	void write_unlock() {
		for (int i=0; i < maxNumberOfThreads_; i++) {
			state_[i * PADDING(int)] = kInactive;
		}
	}

	int getMaxNumberOfThreads() const {
		return maxNumberOfThreads_;
	}

	inline bool isWriterActive() {
		return state_[stateOff_] == kWriterActive;
	}
};

template <typename BaseRWLock>
class LayoutLock_DefaultImpl_ : BaseRWLock {
	volatile int dirty_[BaseRWLock::maxNumberOfThreads_ * PADDING(int)];
public:
	typedef BaseRWLock baseRWLock;
	static const bool ACTIVE=true;
	LayoutLock_DefaultImpl_() {
		for (int i=0; i < BaseRWLock::maxNumberOfThreads_; i++) {
			dirty_[i * PADDING(int)] = 0;
		}
	}

	bool isActive() { return true; }

	static void setup() {
		if (!tidSet_) {
			BaseRWLock::setup();
			dirtyOff_ = tid_ * PADDING(int);
		}
	}

	static void reset() {
		BaseRWLock::reset();
	}

	void startRead() { 
		//while (BaseRWLock::isWriterActive()) ;
	}
	
	bool finishRead(std::atomic<char> *dirtyPtr){
		std::atomic<int> *pp = (std::atomic<int>*)dirtyPtr; 
		if(unlikely(std::atomic_load_explicit<int>(pp,std::memory_order_seq_cst)!=0)){
			resetDirty();
			return false;
		}
		return true;
	}

	bool finishRead() {
		if (unlikely(isDirty())) {
			resetDirty();
			return false;
		}
		/*register int off = dirtyOff_;
		register volatile int* dirty = dirty_+off;
		if (likely(*dirty == 0)) return true;
		*dirty = 0;*/
		return true;
	}

	std::atomic<char> *getDirtyP(){
		return (std::atomic<char> *)(char*)(&dirty_[dirtyOff_]);
	}

	inline bool isDirty() {
		return std::atomic_load_explicit(getDirtyP(), std::memory_order_seq_cst) != 0;
		//return dirty_[dirtyOff_] != 0;
	}

	void __attribute__((noinline)) resetDirty() {//slow path
		//while (BaseRWLock::isWriterActive());
	  register int off = dirtyOff_;
	  register volatile int* dirty = dirty_+off;
	  if (!BaseRWLock::isWriterActive()){//optimization. Effectiveness was NOT checked on real machine.
		 *dirty = 0;
		 __sync_synchronize();
		 if (BaseRWLock::isWriterActive())
			*dirty = 1;//no need for sync_syncrhonize. Other threads may only write 1 to dirty, so it cannot override anything.
	  }
	}


	void startWrite() { BaseRWLock::read_lock(); }

	void finishWrite() { BaseRWLock::read_unlock(); }

	void startLayoutChange() {
		BaseRWLock::write_lock();

		for (int i=0; i < BaseRWLock::maxNumberOfThreads_; i++) {
			dirty_[i * PADDING(int)] = 1;
		}

		__sync_synchronize();
	}

	void finishLayoutChange() {
		BaseRWLock::write_unlock();
	}

};


class NoLock {
	LayoutLock_DefaultImpl_<ScalableRWLock<1> > lock_;
public:
	typedef ScalableRWLock<1> baseRWLock;
	static const bool ACTIVE=true;
	
	NoLock() {}

	void setMaxThreads(int maxThreads) { } // { lock_.setMaxThreads(maxThreads); }

	bool isActive() { return false; } // { return lock_.isActive(); }

	static void setup() { LayoutLock_DefaultImpl_<ScalableRWLock<1> >::setup(); }

	static void reset() { LayoutLock_DefaultImpl_<ScalableRWLock<1> >::reset(); }

	void startRead() {} // { lock_.startRead(); }

	bool finishRead() { return true; } // { return lock_.finishRead(); } // { int d; lock_.finishRead(d); return d != 0; } //{ return lock_.finishRead(); }
	bool finishRead(std::atomic<char> *dirtyPtr){return true; }
	std::atomic<char> *getDirtyP(){return NULL;}

	bool isDirty() { return false; }

	void resetDirty() { }

	void startWrite() {}// { lock_.startWrite(); }

	void finishWrite() {}// { lock_.finishWrite(); }

	void startLayoutChange() {}// { lock_.startLayoutChange(); }

	void finishLayoutChange() {}// { lock_.finishLayoutChange(); }

};
class NoLockM {
	volatile int state_[64 * PADDING(int)];
public:
	static const bool ACTIVE=true;
	NoLockM() {}
	void setMaxThreads(int maxThreads) { } // { lock_.setMaxThreads(maxThreads); }
	bool isActive() { return false; } // { return lock_.isActive(); }
	static void setup() { LayoutLock_DefaultImpl_<ScalableRWLock<1> >::setup(); }
	static void reset() { LayoutLock_DefaultImpl_<ScalableRWLock<1> >::reset(); }
	void startRead() {}
	bool finishRead() { return true; }
	bool finishRead(std::atomic<char> *dirtyPtr){return true; }
	std::atomic<char> *getDirtyP(){return NULL;}
	bool isDirty() { return false; }
	void resetDirty() { }
	void startWrite() {}
	void finishWrite() {__sync_lock_release(&state_[stateOff_]);}//it improves performance!
	void startLayoutChange() {}
	void finishLayoutChange() {}

};

class DisabledLock {
public:
   static const bool ACTIVE=false;

   DisabledLock() {}
   bool isActive() { return false; } // { return lock_.isActive(); }
   static void setup() { LayoutLock_DefaultImpl_<ScalableRWLock<1> >::setup(); }
   static void reset() { LayoutLock_DefaultImpl_<ScalableRWLock<1> >::reset(); }
   void startRead() {} // { lock_.startRead(); }
   bool finishRead() { return true; } // { return lock_.finishRead(); } // { int d; lock_.finishRead(d); return d != 0; } //{ return lock_.finishRead(); }
   bool finishRead(std::atomic<char> *dirtyPtr){return true; }
   std::atomic<char> *getDirtyP(){return NULL;}
   void startWrite() {}// { lock_.startWrite(); }
   void finishWrite() {}// { lock_.finishWrite(); }
   void startLayoutChange() {}// { lock_.startLayoutChange(); }
   void finishLayoutChange() {}// { lock_.finishLayoutChange(); }

};
//ScalableRWLock
class LayoutRWLOCK {
	ScalableRWLock<64> lock_;
public:
	static const bool ACTIVE=true;
	LayoutRWLOCK():lock_() {}

	void setMaxThreads(int maxThreads) { } // { lock_.setMaxThreads(maxThreads); }

	bool isActive() { return lock_.isActive(); }

	static void setup() { ScalableRWLock<64>::setup(); }

	static void reset() { ScalableRWLock<64>::reset(); }

	void startRead()  { lock_.read_lock(); }

	bool finishRead() { lock_.read_unlock(); return true; } // { return lock_.finishRead(); } // { int d; lock_.finishRead(d); return d != 0; } //{ return lock_.finishRead(); }
	bool finishRead(std::atomic<char> *dirtyPtr){lock_.read_unlock(); return true; }
	std::atomic<char> *getDirtyP(){return NULL;}

	bool isDirty() { return false; }

	void resetDirty() { }

	void startWrite() {lock_.read_lock();}// { lock_.startWrite(); }

	void finishWrite() {lock_.read_unlock();}// { lock_.finishWrite(); }

	void startLayoutChange() {lock_.write_lock();}// { lock_.startLayoutChange(); }

	void finishLayoutChange() {lock_.write_unlock();}// { lock_.finishLayoutChange(); }

};


template <typename LockType>
class LayoutLock : public LockType {
public:
	typedef typename LockType::baseRWLock baseRWLock;

	LayoutLock(int maxThreads) : LockType(maxThreads) {}

	LayoutLock() {}
};


