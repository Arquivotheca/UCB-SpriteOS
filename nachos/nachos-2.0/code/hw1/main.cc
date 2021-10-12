// main.cc -- entry point into the operating system
//
// Most of this file is not needed until later assignments.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#define MAIN
#include "copyright.h"
#undef MAIN

#include "utility.h"
#include "system.h"
#include "thread.h"
#ifdef HW2
#include "filesys.h"
#endif

extern void ThreadTest(void), Copy(char *unixFile, char *nachosFile);
extern void Print(char *file), PerformanceTest(void);
extern void StartProcess(char *file), ConsoleTest(char *in, char *out);
extern void MailTest(int networkID);

extern "C" {
// int atoi(char *str);
}

// Usage: nachos -d <debugflags> -rs <random seed #>
//		-f -cp <unix file> <nachos file>
//		-p <nachos file> -r <nachos file>
//		-l -D -t
//		-s -x <nachos file> -c <consoleIn> <consoleOut>
//              -n <network reliability> -m <machine id>
//              -o <other machine id>
//
// HW1
//    -d causes certain debugging messages to be printed (cf. utility.h)
//    -rs causes Yield to occur at random (but repeatable) spots
//
//  HW2
//    -f causes the physical disk to be formatted
//    -cp copies a file from UNIX to Nachos
//    -p prints a Nachos file to stdout
//    -r removes a Nachos file from the file system
//    -l lists the contents of the Nachos directory
//    -D prints the contents of the entire file system 
//    -t tests the performance of the Nachos file system
//
//  HW3 and HW4
//    -s causes user programs to be executed in single-step mode
//    -x runs a user program
//    -c tests the console
//
//  HW5
//    -n sets the network reliability
//    -m sets this machine's host id (needed for the network)
//    -o runs a simple test of the Nachos network software
//
//  NOTE -- flags are ignored until the relevant assignment
int
main(int argc, char **argv)
{
    DEBUG('t', "Entering main");
    (void) Initialize(argc, argv);
    
#ifndef HW2
    ThreadTest();
#else
    argc--; argv++;
    while (argc > 0) {
	if (!strcmp(*argv, "-cp")) { 		// copy from UNIX to Nachos
	    ASSERT(argc > 2);
	    Copy(*(argv + 1), *(argv + 2));
	    argc -= 2; argv += 2;
	} else if (!strcmp(*argv, "-p")) {	// print a Nachos file
	    ASSERT(argc > 1);
	    Print(*++argv);
	    argc--;
	} else if (!strcmp(*argv, "-r")) {	// remove Nachos file
	    ASSERT(argc > 1);
	    fileSystem->Remove(*++argv);
	    argc--;
	} else if (!strcmp(*argv, "-l")) {	// list Nachos directory
            fileSystem->List();
	} else if (!strcmp(*argv, "-D")) {	// print entire filesystem
            fileSystem->Print();
	} else if (!strcmp(*argv, "-t")) {	// performance test
            PerformanceTest();
	}
#ifdef HW3
        else if (!strcmp(*argv, "-x")) {        // run a user program
	    ASSERT(argc > 1);
            StartProcess(*++argv);
            argc--;
        } else if (!strcmp(*argv, "-c")) {      // test the console
	    ASSERT(argc > 2);
            ConsoleTest(*(argv + 1), *(argv + 2));
            argc -= 2; argv += 2;
	}
#ifdef HW5
        else if (!strcmp(*argv, "-o")) {
	    ASSERT(argc > 1);
            sleep(2); 	// give the user time to start up another nachos
            MailTest(atoi(*++argv));
            argc--;
        }
#endif
#endif
	argc--; argv++;
    }
#endif
    
    currentThread->Finish();
    
    return(0);		 // Not reached...
}
