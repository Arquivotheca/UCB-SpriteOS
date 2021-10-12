// scheduler.h -- the thread scheduler -- routines to manage the ready queue.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "list.h"
#include "thread.h"

class Scheduler {
  public:
    Scheduler() { readyList = new List; } 

    void ReadyToRun(Thread* thread);	 // Put thread on ready list
    
    		// Dequeue thread from ready list, if any, and return thread.
    Thread* FindNextToRun();
    
    void Run(Thread* nextThread);	// Cause nextThread to start running

    void Print();			// print contents of ready list
    
  private:
    List *readyList;  		// queue of threads that are available to run
};

#endif
