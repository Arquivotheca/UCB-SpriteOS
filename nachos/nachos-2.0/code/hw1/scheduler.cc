// scheduler.cc -- thread scheduling routines.
//
// Assumes interrupts are already disabled, to provide mutual exclusion.
// NOTE: We can't use Locks to provide mutual exclusion, since
// if we needed to wait for a Lock, we'd end up calling FindNextToRun(),
// and that would put us in an infinite loop.
//
// Very simple implementation -- no priorities, straight FIFO.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "scheduler.h"
#include "thread.h"
#include "system.h"
#include "list.h"

// Put thread on ready list
void
Scheduler::ReadyToRun (Thread *thread)
{
    DEBUG('t', "Putting thread %s on ready list.\n", thread->getName());

    thread->setStatus(READY);
    readyList->Append((void *)thread);
}

// Dequeue thread from ready list, and return it.  If none, return NULL.
Thread *
Scheduler::FindNextToRun ()
{
    return (Thread *)readyList->Remove();
}

// Cause nextThread to start running.
void
Scheduler::Run (Thread *nextThread)
{
    Thread *oldThread = currentThread;
    
#ifdef HW3	// ignore until running user programs in homework 3
    if (currentThread->space != NULL)
        currentThread->space->SaveState();
#endif
    
    oldThread->CheckOverflow();

    currentThread = nextThread;
    currentThread->setStatus(RUNNING);
    
    DEBUG('t', "Switching from thread \"%s\" to thread \"%s\"\n",
	  oldThread->getName(), nextThread->getName());
    
    /* This is the asm routine defined in switch.s.  You may have to think
     * a bit to figure out what happens after this, both from the point
     * of view of the thread and from the perspective of the "outside world".
     */
    SWITCH(oldThread, nextThread);
    
    DEBUG('t', "Now in thread \"%s\"\n", currentThread->getName());

    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }
    
#ifdef HW3
    if (currentThread->space != NULL)
        currentThread->space->RestoreState();
#endif
}

void 
Scheduler::Print()
{
    printf("Ready list contents:\n");
    readyList->Mapcar((VoidFunctionPtr) ThreadPrint);
}
