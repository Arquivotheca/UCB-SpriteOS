// system.h -- routines for starting up and halting Nachos
//     plus, all the global variables in Nachos 
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef SYSTEM_H
#define SYSTEM_H

  // Initialize is called before doing anything.
extern void Initialize(int argc, char **argv);
  // Cleanup is called by Interrupt::Halt to shut everything down
extern void Cleanup();	 

class Thread;
class Scheduler; 
class Interrupt;
class Statistics;
class Timer;

extern Thread* currentThread;	// the thread we are running now
extern Thread* threadToBeDestroyed;  // the thread that just finished
extern Scheduler* scheduler;	// the ready list
extern Interrupt *interrupt;	// interrupt status
extern Statistics* stats;	// performance metrics
extern Timer *timer;

#ifdef HW2
class SynchDisk;
class FileSystem;

extern SynchDisk   *synchDisk;
extern FileSystem  *fileSystem;
#endif

#ifdef HW3
class Machine;

extern Machine* machine;	// user program memory and registers
#endif

#ifdef HW5
class PostOffice;

extern PostOffice* postOffice;
#endif

#endif
