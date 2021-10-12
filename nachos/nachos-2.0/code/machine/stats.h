// stats.h -- gather statistics for performance measurements
//
// DO NOT CHANGE -- these stats are maintained by the machine emulation
//
// fields are public for convenience
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef STATS_H
#define STATS_H

class Statistics {
  public:
    int totalTicks;      // How long we've been running.
    int idleTicks;       // ticks spent idle
    int systemTicks;	 // ticks spent executing system code
    int userTicks;       // ticks spent executing user code

    int numDiskReads;
    int numDiskWrites;
    int numConsoleCharsRead;
    int numConsoleCharsWritten;
    int numPageFaults;
    int numPacketsSent;
    int numPacketsRecvd;

    Statistics() {
    	totalTicks = idleTicks = systemTicks = userTicks = 0;
    	numDiskReads = numDiskWrites = 0;
	numConsoleCharsRead = numConsoleCharsWritten = 0;
	numPageFaults = numPacketsSent = numPacketsRecvd = 0;
	}
    void Print() {
    	printf("Ticks: total %d, idle %d, system %d, user %d\n", totalTicks, 
		idleTicks, systemTicks, userTicks);
    	printf("Disk I/O: reads %d, writes %d\n", numDiskReads, numDiskWrites);
    	printf("Console I/O: reads %d, writes %d\n", numConsoleCharsRead, 
		numConsoleCharsWritten);
    	printf("Paging: faults %d\n", numPageFaults);
    	printf("Network I/O: packets received %d, sent %d\n", numPacketsRecvd, 
		numPacketsSent);
	}
};

// Somewhat arbitrary constants used to advance emulated time.  
// A tick is a just a unit of time -- if you like, a microsecond

#define UserTick 	1	// advance for each user-level instruction 
#define SystemTick 	10 	// advance each time interrupts are enabled
#define RotationTime 	500 	// time disk takes to rotate one sector
#define SeekTime 	500    	// time disk takes to seek past one track
#define ConsoleTime 	100	// time to read or write one character
#define NetworkTime 	100   	// time to send or receive one packet
#define TimerTicks 	100    	// (average) time between timer interrupts


#endif
