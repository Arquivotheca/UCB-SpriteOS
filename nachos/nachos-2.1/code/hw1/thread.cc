// thread.cc -- thread management routines to fork/finish/switch threads
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "utility.h"
#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "switch.h"
#include "synch.h"
#include "system.h"

#define STACK_FENCEPOST 0xdeadbeef	 // for detecting stack overflows

// the constructor
Thread::Thread(char* threadName)
{
    name = threadName;
    stackTop = NULL;
    stack = NULL;
    status = JUST_CREATED;
#ifdef HW3
    space = NULL;
#endif
}

// the destructor
Thread::~Thread()
{
    DEBUG('t', "Deleting thread \"%s\"\n", name);
    if (stack != NULL)
	delete stack;
}

// Invoke (*func)(arg), allowing caller and callee to execute concurrently.
//
// Initialize the thread stack to run the procedure, and then put
// the thread on the ready queue.
void 
Thread::Fork(VoidFunctionPtr func, int arg)
{
    DEBUG('t', "Forking thread \"%s\" with func = 0x%x, arg = %d\n",
	  name, (int) func, arg);
    
    StackAllocate(func, arg);

    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    scheduler->ReadyToRun(this);	/* assumes interrupts off */
    (void) interrupt->SetLevel(oldLevel);
}    

// Check stack for overflow.  Because the compiler won't do it for us.
//
// NOTE! -- your program may crash if the stack overflows, before this
// catches the problem (since this is only called when the thread sleeps).
// If you get bizarre results (such as seg faults where there is no code)
// then you *may* need to increase the stack size.  You can avoid stack
// overflows by not putting large data structures on the stack.
// This is a bad idea: void foo() { int bigArray[10000]; ... }
void
Thread::CheckOverflow()
{
    if (stack != NULL)
	ASSERT(*stack == STACK_FENCEPOST);
}

// Called by a thread when it's done executing. 
//
// NOTE: we don't de-allocate the thread data structure or the execution
//  stack, because we're running in the thread and on the stack right now!
//  Instead, we set threadToBeDestroyed, so that Scheduler::Run() will
//  call the destructor.
void
Thread::Finish ()
{
    (void) interrupt->SetLevel(IntOff);
    ASSERT(this == currentThread);
    
    DEBUG('t', "Finishing thread \"%s\"\n", getName());
    
    threadToBeDestroyed = currentThread;
    Sleep(); 	
    // not reached
}

// Relinquish the processor if any other thread is ready to run.
// Similar to Thread::Sleep(), but a little different.
void
Thread::Yield ()
{
    Thread *nextThread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    
    ASSERT(this == currentThread);
    
    DEBUG('t', "Yielding thread \"%s\"\n", getName());
    
    nextThread = scheduler->FindNextToRun();
    if (nextThread != NULL) {
	scheduler->ReadyToRun(this);
	scheduler->Run(nextThread);
    }
    (void) interrupt->SetLevel(oldLevel);
}

// Put the current thread to sleep, relinquishing processor to a ready
// thread.  Idle the machine if there are no ready threads.
//
// Assumes interrupts are already disabled.
void
Thread::Sleep ()
{
    Thread *nextThread;
    
    ASSERT(this == currentThread);
    
    DEBUG('t', "Sleeping thread \"%s\"\n", getName());

    status = BLOCKED;
    while ((nextThread = scheduler->FindNextToRun()) == NULL)
	interrupt->Idle();	// no one to run, wait for an interrupt
        
    scheduler->Run(nextThread); // returns when we've been signalled
}

// We have to do this because pointers to member functions are strange.
static void ThreadFinish()    { currentThread->Finish(); }
static void InterruptEnable() { interrupt->Enable(); }
void ThreadPrint(int arg){ Thread *t = (Thread *)arg; t->Print(); }

// Allocate and return an execution stack, building an initial
//   call frame that will enable interrupts, invoke "func" with arg 
//   as its one parameter, and call Thread::Finish() when "func" returns.
void
Thread::StackAllocate (VoidFunctionPtr func, int arg)
{
    stack = new int[StackSize];
    stackTop = stack + StackSize;
    
    *stack = STACK_FENCEPOST;
    
    machineState[PCState] = (int) ThreadRoot;
    machineState[StartupPCState] = (int) InterruptEnable;
    machineState[InitialPCState] = (int) func;
    machineState[InitialArgState] = arg;
    machineState[WhenDonePCState] = (int) ThreadFinish;
}

