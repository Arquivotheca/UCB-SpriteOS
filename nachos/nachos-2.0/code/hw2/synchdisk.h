// synchdisk.h -- synchronous interface to the raw disk device
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef SYNCHDISK_H
#define SYNCHDISK_H

#include "disk.h"
#include "synch.h"

class SynchDisk {
  public:
    SynchDisk(char* name);    		// should invoke new Disk()
    ~SynchDisk();
    
    // These read and write a sector, but don't return until
    // the data is read or written.  These should call
    // Disk::ReadRequest, Disk::WriteRequest, then wait until done.
    void ReadSector(int sectorNumber, char* data);
    void WriteSector(int sectorNumber, char* data);
    
    void RequestDone() { semaphore->V(); }

  private:
    Disk *disk;		  // raw disk device
    Semaphore *semaphore; // synch requesting thread with interrupt handler
    Lock *lock;		  // only one read/write at a time
};

#endif
