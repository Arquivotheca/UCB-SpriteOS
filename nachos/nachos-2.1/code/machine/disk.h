// disk.h -- emulate a physical disk.
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef DISK_H
#define DISK_H

#include "utility.h"

#define SectorSize 		128	// in bytes
#define SectorsPerTrack 	32
#define NumTracks 		32
#define NumSectors 		(SectorsPerTrack * NumTracks)

class Disk {
  public:
    // Create emulated disk; invoke (*callWhenDone)(callArg) every 
    // time a request completes.
    Disk(char* name, VoidFunctionPtr callWhenDone, int callArg);
    ~Disk();
    
    // These routines send a request to the disk and return immediately.
    // NOTE: Only one request allowed at a time!
    void ReadRequest(int sectorNumber, char* data);
    void WriteRequest(int sectorNumber, char* data);

    // how long will a request to newSector take?
    int ComputeLatency(int newSector, bool writing);	
    
    void HandleInterrupt();

  private:
    int fileno;			// UNIX file number for emulated disk 
    VoidFunctionPtr handler;	// who to call when request finishes
    int handlerArg;
    bool active;     		// Is a disk operation in progress?
    int lastSector;		// last request to be made
    int bufferInit;		// when the track buffer started being loaded

    int TimeToSeek(int newSector, int *rotate); // time to get to the new track
    int ModuloDiff(int to, int from);        // # sectors between to and from
    void UpdateLast(int newSector);
};

#endif

