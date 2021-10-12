// synch.h -- synchronization primitives.  
//
// These objects take names as arguments when they are created; this
// is purely for debugging.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef SYNCH_H
#define SYNCH_H

#include "thread.h"
#include "list.h"

class Semaphore {
  public:
    Semaphore(char* semaphoreName, int val); 	// Constructor
    
    char* getName() { return name;}
    
    void P();	 // wait until semaphore value is > 0, then decrement

    void V();	 // increment semaphore value, waking up anyone who is waiting
    
  private:
    char* name;        // useful for debugging
    int value;         // value must be >= 0
    List *queue;       // threads who are waiting until value > 0
};

class Lock {
  public:
    Lock(char* lockName);  // Constructor

    char* getName() { return name; }

    void Acquire(); // wait until lock is free, then grab lock for ourselves

    void Release(); // relinquish lock, waking up any one who is waiting

  private:
    char* name;
    // plus some other stuff you'll need to define
};

class Condition {
  public:
    Condition(char* conditionName);
  
    char* getName() { return (name); }
    
    // Go to sleep.  Relinquish the processor, releasing conditionLock.
    // Re-acquire the lock on wakeup.
    void Wait(Lock *conditionLock);

    // Wake up a thread (if any) who is waiting on the condition. 
    // conditionLock must be held when called.
    void Signal(Lock *conditionLock);

    // Wake up all threads waiting on the condition. 
    // conditionLock must be held when called.
    void Broadcast(Lock *conditionLock);

  private:
    char* name;
    // plus some other stuff you'll need to define
};
#endif
