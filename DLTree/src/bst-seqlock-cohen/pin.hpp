/*
 * pin.hpp
 *
 *  Created on: Jun 2, 2015
 *      Author: ncohen
 */

#ifndef PIN_HPP_
#define PIN_HPP_
#ifdef __linux__
#include <sched.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <sys/types.h>
#include <unistd.h>

pid_t gettid(){
	return (pid_t) syscall(SYS_gettid);
}
void __pin(pid_t t, int cpu){
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	if(sched_setaffinity(t, sizeof(cpu_set_t), &cpuset))
		exit(1);
}
void pin(int tid){
	__pin(gettid(), tid);
}


#else
void pin(int tid){}
#endif

#endif /* PIN_HPP_ */
