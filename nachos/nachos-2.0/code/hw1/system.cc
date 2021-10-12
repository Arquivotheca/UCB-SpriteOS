// system.cc -- Nachos initialize and stop routines
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include <signal.h>
#include "system.h"
#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "stats.h"
#include "timer.h"

extern "C" {
void srandom(int seed);
// int atoi(char *str);
void exit();
}

Thread *currentThread;	// the thread we are running now
Thread *threadToBeDestroyed;  // the thread that just finished
Scheduler *scheduler;	// the ready list
Interrupt *interrupt;	// interrupt status
Statistics *stats;	// performance metrics
Timer *timer;

#ifdef HW2
#include "synchdisk.h"
#include "filesys.h"

SynchDisk   *synchDisk;
FileSystem  *fileSystem;
#endif

#ifdef HW3
#include "machine.h"

Machine *machine;	// user program memory and registers
#endif

#ifdef HW5
#include "post.h"

PostOffice *postOffice;
#endif

// Very primitive implementation of time-slicing 
static void
TimeOut(int dummy)
{
    if (interrupt->getStatus() != IdleMode)
	interrupt->YieldOnReturn();
}

// Parse the command line arguments, and initialize all global data structures
void
Initialize(int argc, char **argv)
{
    char* debugArgs = "";
    bool randomYield = FALSE;
    bool format = FALSE;	// hw2 -- format disk
    bool debugUserProg = FALSE;	// hw3 -- single step user program
    double rely = 1;		// hw5 -- network reliability
    int netname = 0;		// hw5 -- UNIX socket name
    
    argc--; argv++;
    while (argc > 0) {
	if (!strcmp(*argv, "-d")) {
	    ASSERT(argc > 1);
	    debugArgs = *++argv;
	    argc--;
	} else if (!strcmp(*argv, "-rs")) {
	    ASSERT(argc > 1);
	    srandom(atoi(*++argv));
	    randomYield = TRUE;
	    argc--;
#ifdef HW2
	} else if (!strcmp(*argv, "-f")) {
	    format = TRUE;
#endif
#ifdef HW3
	} else if (!strcmp(*argv, "-s")) {
	    debugUserProg = TRUE;
#endif
#ifdef HW5
	} else if (!strcmp(*argv, "-l")) {
	    ASSERT(argc > 1);
	    rely = atof(*++argv);
	    argc--;
	} else if (!strcmp(*argv, "-m")) {
	    ASSERT(argc > 1);
	    netname = atoi(*++argv);
	    argc--;
#endif
	}
	argc--; argv++;
    }

    DebugInit(debugArgs);
    stats = new Statistics();
    interrupt = new Interrupt();
    scheduler = new Scheduler();
    if (randomYield)	// only start the timer if we need it
	timer = new Timer(TimeOut, 0, randomYield);

    threadToBeDestroyed = NULL;
    // Bootstrap by creating a thread to represent the thread we're
    // running in as the UNIX process.
    currentThread = new Thread("main");
    currentThread->setStatus(RUNNING);

    interrupt->Enable();
    signal(SIGINT, Cleanup);
    
#ifdef HW3
    machine = new Machine(debugUserProg);
#endif
    
#ifdef HW2
    synchDisk = new SynchDisk("DISK");
    fileSystem = new FileSystem(format);
#endif

#if HW5
    postOffice = new PostOffice(netname, rely, 10);
#endif
}

// Nachos is halting; stop everything
void
Cleanup()
{
    printf("\nCleaning up...\n");
#ifdef HW5
    delete postOffice;
#endif
    
#ifdef HW3
    delete machine;
#endif

#ifdef HW2
    delete fileSystem;
    delete synchDisk;
#endif
    
    delete timer;
    delete scheduler;
    delete interrupt;
    
    exit(0);
}
