// thread.h -- Interface for users of threads.
// 
//  To fork a thread, we must first allocate a data structure for it
//  "t = new Thread" and then do the fork "t->fork(f, arg)"
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef THREAD_H
#define THREAD_H

#include "utility.h"
#ifdef HW3		// ignore until homework 3
#include "addrspace.h"
#endif

enum ThreadStatus { JUST_CREATED, RUNNING, READY, BLOCKED };

#define MachineStateSize 10   // "callee save" registers --  8 GP, FP, and RA

//  Every thread has:
//     an execution stack for activation records ("stackTop" and "stack")
//     space to save registers while blocked 
//     a "status" (running/ready/blocked)
class Thread {
  public:
    Thread(char* threadName);	// name for convenience
    ~Thread(); 

    void Fork(VoidFunctionPtr func, int arg); 	// make thread run (*func)(arg)
    void Yield();  // relinquish the processor if any other thread is runnable
    void Sleep();  // put the thread to sleep and relinquish the processor
    void Finish();  // the thread is done executing
    
    void CheckOverflow();   // make sure thread hasn't overflowed its stack
    void setStatus(ThreadStatus st) { status = st; }
    char* getName() { return (name); }
    void Print() { printf("%s, ", name); }

  private:
    // These first two members MUST be in these positions for SWITCH.
    int* stackTop;			 // the stack pointer
    int machineState[MachineStateSize];  // all registers except for stackTop
    
    int* stack; 	 // bottom of the stack (NULL if the main thread)
    ThreadStatus status;
    char* name;

    // allocate a stack for this thread -- used internally by Fork()
    void StackAllocate(VoidFunctionPtr func, int arg);

#ifdef HW3		// ignore until homework 3
  public:
    AddrSpace *space;
#endif
};

extern void ThreadPrint(int arg);	 // dummy to call Thread::Print

// Magical machine-dependent routines, defined in switch.s

extern "C" {
// First frame on thread execution stack; 
//   enable interrupts, call "func", and when that returns, call ThreadFinish()
void ThreadRoot();

// Stop running oldThread and start running newThread
void SWITCH(Thread *oldThread, Thread *newThread);
}

#endif
