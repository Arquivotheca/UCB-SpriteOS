// synchdisk.cc -- routines to synchronously access the disk
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "synchdisk.h"
#include "disk.h"
#include "scheduler.h"

static void
DiskRequestDone (int arg)
{
    SynchDisk* disk = (SynchDisk *)arg;

    DEBUG('d', "Disk request done: %x\n", arg);
    disk->RequestDone();
}

// initialize the synchronous disk to use the disk emulator
SynchDisk::SynchDisk(char* name)
{
    semaphore = new Semaphore("synch disk", 0);
    lock = new Lock("synch disk lock");
    disk = new Disk(name, DiskRequestDone, (int) this);
}

SynchDisk::~SynchDisk()
{
    delete disk;
    delete lock;
    delete semaphore;
}

// read a sector, wait for it to finish
void
SynchDisk::ReadSector(int sectorNumber, char* data)
{
    lock->Acquire();
    disk->ReadRequest(sectorNumber, data);
    semaphore->P();
    lock->Release();
}

// write a sector, wait for it to finish
void
SynchDisk::WriteSector(int sectorNumber, char* data)
{
    lock->Acquire();
    disk->WriteRequest(sectorNumber, data);
    semaphore->P();
    lock->Release();
}
