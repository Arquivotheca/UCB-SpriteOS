// synch.cc -- Thread synchronization routines -- semaphores, locks 
//   and condition variables.
//
// Semaphores disable/enable interrupts for mutual exclusive 
//   access to the queue of threads waiting for the semaphore.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "synch.h"
#include "utility.h"
#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "system.h"

// Semaphores ---------------------------------------------------------- 

// Initialize a new Semaphore.
Semaphore::Semaphore(char* semaphoreName, int val)
{
    name = semaphoreName;
    value = val;
    queue = new List;
}

// Block until semaphore value > 0; then decrement it and return.
void
Semaphore::P ()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    
    // Checks to see if the semaphore is available.
    if (value == 0) { 	// semaphore not available
	queue->Append((void *)currentThread);
	currentThread->Sleep();
    } else 		 // semaphore available, consume its value
        value--;
    
    (void) interrupt->SetLevel(oldLevel);
}

// Increment semaphore value; wake up a waiting thread, if any.
void
Semaphore::V ()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if (thread != NULL)	   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
    else
        value++;
    (void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in HW5 won't work!
Lock::Lock(char* lockName) {}
void Lock::Acquire() {}
void Lock::Release() {}
Condition::Condition(char* conditionName) { }
void Condition::Wait(Lock* conditionLock) { }
void Condition::Signal(Lock* conditionLock) { }
void Condition::Broadcast(Lock* conditionLock) { }
