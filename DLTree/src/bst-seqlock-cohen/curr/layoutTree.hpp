/*
 * tree2.hpp
 *
 *  Created on: Jan 20, 2015
 *      Author: nachshonc
 */

#ifndef TREE2_HPP_
#define TREE2_HPP_

//TODO: fix according to macro
#define SYNC(S) S
//#define NOSYNC
#include "tree.hpp"
#include "globalLockTree.hpp"
#include <assert.h>
#define RWLOCK
#define NO_ASM_SEARCH
//============================================================================
// Author      : Nachshon Cohen
// Description : Cache-Conscious binary search tree
//============================================================================

#include <iostream>
#include <cstring>
#include <vector>
#include <stdio.h>
#include "llock.h"
/*#ifndef RWLOCK
typedef LayoutLock_DefaultImpl_<ScalableRWLock<64>> Layout_Lock;
#else
typedef LayoutRWLOCK Layout_Lock;
#endif*/
typedef SeqLock Layout_Lock;
using namespace std;
enum NODE_TYPES {NORMAL_NODE=0XDE, DATA_NODE_T=1};
#define NOINLINE __attribute__((noinline))
extern __thread bool tidSet_;
extern __thread pid_t tid_;
extern __thread int stateOff_;
extern __thread int dirtyOff_;
extern pid_t nextThread;
extern globalLockTree NULL_TREE;
int Rand();
#define CHCK(S)
class layoutTree{
   CHCK(unsigned int buffer[2500000];)
   CHCK(int next;)
public:
	struct cacheKeys{
		union{
			unsigned type;
			unsigned dummyKeys[];//keys starts at 1.
		};
		unsigned keys[15];
	} __attribute__ ((aligned (64)));
	struct node{
		cacheKeys keys;
		struct node *next[16];
		node(){memset(this, 0, sizeof(struct node)); keys.type=NORMAL_NODE;}
	}__attribute__ ((aligned(64)));
	struct datanode{
		unsigned type;
		unsigned key;
		void *data;
		datanode(unsigned key, void *data){
			this->key=key; this->data=data; this->type=DATA_NODE_T;
		}
	};
	node *head;
	SYNC(Layout_Lock llock_;)
	int fuzzySize=0;
	int enlargeWhen=16, shrinkWhen=-1, backboneSize=0;
	int heuristic[PADDING(int)*64]={0};
	node *buildSing(unsigned val){
		datanode *n=new datanode(val, NULL);
		return (node *)n;
	}
	bool DATA_NODE(node *n){
		return (n->keys.type==LOCKED || n->keys.type==UNLOCKED);
	}
	bool isNormalNode(node *n){return n->keys.type==NORMAL_NODE;}
	bool isLeaf(node *n){return n->next[0]->keys.type!=NORMAL_NODE;}
	node *build(int level, long first){
		/*if(level==0)
			return buildSing(first);*/
		if(level==0){
			std::vector<unsigned> v(1);
			v[0]=first;
			return (node*)new globalLockTree(v.begin(),v.end());
		}
		/*if(level<4){
			assert(level==4);
			int delta = 1<<(level-4);
			std::vector<unsigned> v(16);
			for(int i=0; i<16; ++i)
				v[i]=first+2*(i+1)*delta;
			node *ret= (node*)new globalLockTree(v.begin(),v.end());
			return ret;
		}*/
		int delta = 1<<(level-0);
		node *n = new node();
		for(int l=0; l<4; ++l, delta/=2){
			for(int i=0; i<(1<<l); ++i){
				n->keys.keys[(1<<l)-1+i] = first+(i*2+1)*delta;
			}
		}
		assert((level%4) == 0 && level>0);
		if(level>4)
		{
			for(int i=0; i<16; ++i)
				n->next[i]=build(level-4, first+(2*i)*delta);
		}
		else{
			for(int i=0; i<16; ++i){
				n->next[i]=build(0, first+2*(i+1)*delta);
			}
		}
		return n;
	}
	layoutTree(int i) CHCK(: next(0)) {
		if(i==0){
			head = (node*)new globalLockTree();
		}
		assert(!(i%4));
		head = build(i, 0);
	}
	layoutTree(){head = build(16, 0); }
	//code in assembly to generate the search code (hopefully) FAST
   virtual ~layoutTree() {
      CHCK(FILE *out = fopen("inserts.log", "w");\
      for (int i=0; i < next; i++) \
        fprintf(out, "%d\n", buffer[i]);\
      fclose(out);)
   }
	long asmsearch(unsigned key__, unsigned *keys__){
	    long dst;
	    int key=key__;
	    unsigned *keys=keys__;
#ifdef NO_ASM_SEARCH
		long idx=1;
		idx+=idx+(key>keys[1]);
		idx+=idx+(key>keys[idx]);
		idx+=idx+(key>keys[idx]);
		idx+=idx+(key>keys[idx]);
		dst=idx;
#else	    
		 asm ("mov $1, %0\n\t"       		//idx=1
	         "cmpl %2, 0x4(%1)\n\t" 		//key > keys[1]
	         "adc %0, %0\n\t"			//idx+=idx+carry (carry=key>key[1])
	         "cmpl %2, (%1, %0, 4)\n\t" 	//key > keys[2/3]
	         "adc %0, %0\n\t"			//idx+=idx+carry
	         "cmpl %2, (%1, %0, 4)\n\t" 	//key > keys[4-7]
	         "adc %0, %0\n\t"			//idx+=idx+carry
	         "cmpl %2, (%1, %0, 4)\n\t"  //key > keys[8-15]
	         "adc %0, %0\n\t"			//idx+=idx+carry
	         : "=&r" (dst)
	         : "r" (keys), "r"(key));
#endif
	    return dst;
	}
	globalLockTree *backboneGetTreelet(unsigned key){
		node *cur = head;
		while(cur->keys.type==NORMAL_NODE){
			unsigned idx = asmsearch(key, (unsigned *)cur);
			cur=cur->next[idx-16];
		}
		return (globalLockTree*) cur;
	}
	globalLockTree *getTreelet(unsigned key){
		long Start; 
		SYNC(start: Start=llock_.startRead();)
		node *cur = head;
		while(cur->keys.type==NORMAL_NODE){
			unsigned idx = asmsearch(key, (unsigned *)cur);
			cur = cur->next[idx-16];
			//assert(cur!=NULL);
		}
		globalLockTree *res = (globalLockTree*)cur;
		res->acquire();
#ifndef NOSYNC
		if(llock_.finishRead(Start)==false){
			res->release();
			goto start;
		}
#endif
		return res;
	}
	void free_treelet(globalLockTree *t){
		t->destroy();
		t->release();
		//TODO: recycle t itself...
	}
	void NOINLINE enlarge_tree(unsigned sz){
		SYNC(llock_.startLayoutChange();)
		if(sz<enlargeWhen){
			SYNC(llock_.finishLayoutChange();) 
			return;
		}
		//if(backboneSize==0) backboneSize=1;
		backboneSize*=16; 
		if(backboneSize==0) backboneSize=15;
		enlargeWhen = backboneSize*16;
		shrinkWhen = backboneSize/4;
		consolidateAllOptimal();
		//consolidateAll(head);
		static int ctr=0;
		ctr++;
		if(ctr>=3)
			printf("ConsolidateAll(#%d) by thread %d. "
					"EnlargeWhen=%d, shrinkWhen=%d, backboneSize=%d\n", ctr, tid_,
					enlargeWhen, shrinkWhen, backboneSize);
		SYNC(llock_.finishLayoutChange();)
	}
	bool NOINLINE search(unsigned key){
		return getTreelet(key)->search(key);
	}
	bool insert(unsigned key){
      CHCK(int n = __atomic_fetch_add(&next, 1, __ATOMIC_SEQ_CST);\
      buffer[n] = key;)
		bool res;
		res = getTreelet(key)->insert(key);
		if(unlikely((heuristic[stateOff_]+=res)>=200))
		{
			int res = __sync_add_and_fetch(&fuzzySize, heuristic[stateOff_]);
			//printf("fuzzySize=%d, th=%d\n", tid_, res);
			int lenlargeWhen;
			SYNC(long start; lb: start=llock_.startRead();) lenlargeWhen=enlargeWhen; SYNC(if(llock_.finishRead(start)==false) goto lb;)
			if(res >= lenlargeWhen) enlarge_tree(res);
			heuristic[stateOff_]=0;
		}
		return res;
	}
	void NOINLINE shrink_tree(unsigned sz){
		SYNC(llock_.startLayoutChange();)
		printf("shrinkWhen=%d. FuzzySize=%d, heuristici=%d [off=%d]\n", shrinkWhen, fuzzySize, heuristic[stateOff_], stateOff_); 
		if(sz>shrinkWhen){
			SYNC(llock_.finishLayoutChange();)
			return;
		}
		SYNC(llock_.finishLayoutChange();)
		assert(false);
		//now, all tl[i] are treelets. let us check if all are empty
		//<Are all treelets empty?>
		//Check again for another random BigNode?
		//assert(This should never really happen! yes?)	
//		__sync_synchronize();	
	}
	bool remove(unsigned key){
		bool res;//, shrink=false;
		res = getTreelet(key)->remove(key);
		if(unlikely((heuristic[stateOff_]-=res)<-200))
      {
         int res = __sync_add_and_fetch(&fuzzySize, heuristic[stateOff_]);
			int lshrinkWhen;
         SYNC(long start; lb: start=llock_.startRead();) lshrinkWhen=shrinkWhen; SYNC(if(llock_.finishRead(start)==false) goto lb;)
         if(res <= lshrinkWhen) shrink_tree(res);

			//printf("REMOVE: fuzzySize=%d, th=%d\n", res, tid_);
         heuristic[stateOff_]=0;
      }
		return res;
	}

	/**gets a node that all its children are treelets.
	 * Modify it so that all children are nodes.
	 * New grandchildren are created as treelets.
	 * Happens when the amount of items in the subtree is
	 * approximately larger than 16*16
	 */
	//Promise: Forall c in [0..16) range of child c is from first+(c*len)/16 to first+((c+1)*len/16).
	void constructBigNode(node *bn, std::vector<unsigned> &v, int first, int len){
		for(int l=0, delta=16/2; l<4; ++l, delta/=2){
			for(int i=0; i<(1<<l); ++i){
				int idxx = ((i*2+1)*delta)*len/16; //the index of the first element that should go RIGHT
				bn->keys.keys[(1<<l)-1+i] = (first+idxx<=0)?INVALID_KEY_SMALL:v[first+idxx-1];//the -1 is because a key goes right only if it is larger than the node key.
			}
		}
	}
	void consolidateAllOptimal(){
		node *root = head;
		if(!isNormalNode(root)){
			consolidateRoot((globalLockTree*)root);
			return ;
		}
		std::vector<unsigned> v(0);
		v.reserve(backboneSize+200*64); //
		int total_len = gatherAll(root, v);
		buildFromV(root, v, 0, total_len);
	}
	//Task: gather all nodes from all treelets into v. Destroy all treelets.
	//TODO: disconnect treelets.
	//assume: v is large enough.
	int gatherAll(node *root, std::vector<unsigned> &v){
		assert(root->keys.type==NORMAL_NODE);
		int sum=0;
		if(root->next[0]->keys.type!=NORMAL_NODE){
			for(int c=0; c<16; ++c){
				globalLockTree *ch = (globalLockTree*)root->next[c];
				ch->acquire();
				sum += ch->addItems(v);
				root->next[c]=(node*)&NULL_TREE;
				free_treelet(ch);
			}
			return sum;
		}
		else{
			for(int c=0; c<16; ++c)
				sum+=gatherAll(root->next[c], v);
			return sum;
		}
	}
	void buildFromV(node *root, std::vector<unsigned> &v, int first, int len){
		assert(root->keys.type==NORMAL_NODE); //HANLDE either in consolidateRoot or catch if child is like this.
		constructBigNode(root, v, first, len);
		if(root->next[0]->keys.type!=NORMAL_NODE){
			expandLeafBigNode(root, v, first, len);
			//leaf node. Need to allocate a lot of big nodes and a lot of treelets.
			//use a version of consolidate.
		}
		else{
			for(int c=0; c<16; ++c){
				int firstc=first+(c*len)/16, lastc=first+((c+1)*len/16), lenc=lastc-firstc;
				if(c==15) assert(firstc+lenc==first+len);
				buildFromV(root->next[c], v, firstc, lenc);
			}
		}
	}
	void expandLeafBigNode(node *cur, std::vector<unsigned> &v, int first, int len){
		constructBigNode(cur, v, first, len);
		assert(cur->keys.type==NORMAL_NODE);
		assert(cur->next[0]->keys.type!=NORMAL_NODE);//and all other children..
		for(int c=0; c<16; ++c){
			node *curc = new node();
			int firstc=first+(c*len)/16, lastc=first+((c+1)*len/16), lenc=lastc-firstc;
			constructBigNode(curc, v, firstc, lenc);
			for(int grc=0; grc<16; ++grc){//grand child
				int start = firstc+(grc*lenc)/16;
				int end   = firstc+((grc+1)*lenc)/16;
				if(grc==15) assert(end==lastc);
				std::vector<unsigned>::iterator stit=v.begin()+start;
				std::vector<unsigned>::iterator enit=v.begin()+end;
				if(c==15 && grc==15)	assert(end==first+len);
				curc->next[grc]=(node*)new globalLockTree(stit, enit);
			}
			cur->next[c] = curc;//install after it is prepared.
		}
	}
	void consolidate(node *cur){
		//unsigned keys[2000];
		for(int i=0; i<16; ++i)
			assert(cur->next[i]->keys.type!=NORMAL_NODE);
		int elems=0;
		for(int i=0; i<16; ++i){
			((globalLockTree*)cur->next[i])->acquire();
			elems+=((globalLockTree*)cur->next[i])->size();
		}
		assert(elems<2000);//THIS IS NOT INCORRECT, JUST STRANGE. Is the workload really unbalanced?
		std::vector<unsigned> v(0);
		v.reserve(elems);
		for(int i=0; i<16; ++i){
			globalLockTree *t = ((globalLockTree*)cur->next[i]);
			t->addItems(v);
			cur->next[i]=(node*)&NULL_TREE;
			free_treelet(t);
		}
		constructBigNode(cur, v, 0, elems);
		for(int c=0; c<16; ++c){
			node *curc = new node();
			//assert(curc->keys.type==NORMAL_NODE);
			int firstc=(c*elems)/16, lastc=((c+1)*elems/16), lenc=lastc-firstc; //elems/16;
			constructBigNode(curc, v, firstc, lenc);
			for(int grc=0; grc<16; ++grc){//grand child
				int start = firstc+(grc*lenc)/16;
				int end   = firstc+((grc+1)*lenc)/16;
				if(grc==15) assert(end==lastc);
				std::vector<unsigned>::iterator stit=v.begin()+start;
				std::vector<unsigned>::iterator enit=v.begin()+end;
				if(c==15 && grc==15)
					assert(end==elems);
				curc->next[grc]=(node*)new globalLockTree(stit, enit);
			}
			cur->next[c] = curc;//install after it is prepared.
		}
	}
	void consolidateRoot(globalLockTree *root){
		std::vector<unsigned> v(0);
		root->acquire();
		int elems=root->size();
		v.reserve(elems);
		root->addItems(v);
		node *newroot = new node();
		for(int l=0, delta=16/2; l<4; ++l, delta/=2){
			for(int i=0; i<(1<<l); ++i){
				int idxx = ((i*2+1)*delta)*elems/16; //the index of the first element that should go RIGHT
				newroot->keys.keys[(1<<l)-1+i] = (idxx<=0)?INVALID_KEY_SMALL:v[idxx-1];//recall a key goes right only if it is larger.
			}
		}
		for(int c=0; c<16; ++c){
			int start = c*elems/16;
			int end   = (c+1)*elems/16;
			std::vector<unsigned>::iterator stit=v.begin()+start;
			std::vector<unsigned>::iterator enit=v.begin()+end;
			newroot->next[c]=(node*)new globalLockTree(stit, enit);
		}
		this->head=newroot;
		free_treelet(root);
	}
	void consolidateAll(node *root){
		static int ctr=0;
		//static node *stack[10]={0};

		if(root->keys.type!=NORMAL_NODE){
			consolidateRoot((globalLockTree*)root);
			return;
		}
		//stack[ctr++]=root;
		bool isLeaf;
		node *c0 = root->next[0];
		assert(c0!=NULL);
		isLeaf = c0->keys.type!=NORMAL_NODE;
		{//debug
			for(int i=0; i<16; ++i){
				assert(root->next[i]!=NULL &&
						isLeaf==(root->next[i]->keys.type!=NORMAL_NODE));
				if(isLeaf){
					assert(DATA_NODE(root->next[i]));
				}
			}
		}
		if(isLeaf){
			consolidate(root);
		}
		else{
			for(int i=0; i<16; ++i){
				consolidateAll(root->next[i]);
			}
		}
		ctr--;
	}
	int size(node *root){
		if(root->keys.type!=NORMAL_NODE)
			return ((globalLockTree*)root)->size();
		int sum=0;
		for(int c=0; c<16; ++c)
			sum+=size(root->next[c]);
		return sum;
	}

	bool searchV(unsigned key){
		printf("starting searchV for key %d\n", key);
		node *cur = head;
		do{
			unsigned idx=0;
			printNode(cur);
			idx = (cur->keys.dummyKeys[1]  >=key)?2:3;
			idx = (cur->keys.dummyKeys[idx]>=key)?2*idx:2*idx+1;
			idx = (cur->keys.dummyKeys[idx]>=key)?2*idx:2*idx+1;
			idx = (cur->keys.dummyKeys[idx]>=key)?2*idx:2*idx+1;
			cur = cur->next[idx-16];
		}while(cur!=NULL && cur->keys.type==NORMAL_NODE);
		//printf("DN[%p] = ", cur);
		//printf("idx=%d\n", idxx);
		printNode(cur);
		if(cur==NULL) return false; //redundant. Treelet should always exists.
		//return ((datanode*)cur)->key==key;
		return ((globalLockTree*)cur)->search(key);
	}
	void printNode(node *n){
		if(n==NULL) {
			printf("NULL\n");
			return;
		}
		if(n->keys.type!=NORMAL_NODE){
			//printf("DN[%p]: %d\n", n, ((datanode*)n)->key);
			printf("DN[%#x]: ", (int)(long)n);
			printf("lock=%X, set=", ((globalLockTree*)n)->lock);
			((globalLockTree*)n)->print();
			return;
		}
		printf("[%p]: ", n);
		for(int i=0; i<15; ++i)
			printf("%d ", n->keys.keys[i]);
		printf("\nNEXTS: ");
		for(int i=0; i<16; ++i){
			printf("%8lx ", ((long)n->next[i])&0xFFFFFFFF);
		}
		printf("\n");

	}
	void print(node *n){
		if(n==NULL) {
			cout<<"NULL"<<endl;
			return ;
		}
		printNode(n);
		if(n->keys.type==NORMAL_NODE)
			for(int i=0; i<16; ++i){
				cout<<"["<<i<<"] ";
				printNode(n->next[i]);
			}
	}
	virtual std::string name(){return "ArrBased"; }
};
/*int main(){
	tree2 t;
	int len=1<<17;
	t.build(len);
	for(int i=1; i<len; ++i){
		bool exp=!(i%2);
		bool found=t.search(i);
		if(exp!=found)
			cout<<"["<<i<<"] "<<t.search(i)<<endl;
	}
	cout<<"finished check"<<endl;
	cin.ignore();
	return 0;
}*/

#endif /* TREE2_HPP_ */





