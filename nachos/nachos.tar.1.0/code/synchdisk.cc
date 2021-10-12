/* synchdisk.cc -- routines to access the disk synchronously.  */

#include "synchdisk.h"
#include "disk.h"
#include "scheduler.h"

static void
DiskRequestDone (int arg)
{
    SynchDisk* disk = (SynchDisk *)arg;

    DEBUG('d', "Disk request done: %x %x\n", arg, disk);
    disk->RequestDone();
}

/* initialize the synchronous disk to use the disk emulator */
SynchDisk::SynchDisk(char* name)
{
    semaphore = new Semaphore("synch disk", 0);
    disk = new Disk(name, DiskRequestDone, (int) this);
}

SynchDisk::~SynchDisk()
{
    delete disk;
}

void
SynchDisk::ReadSector(int sectorNumber, char* data)
{
    disk->ReadRequest(sectorNumber, data);
    semaphore->P();
}

void
SynchDisk::WriteSector(int sectorNumber, char* data)
{
    disk->WriteRequest(sectorNumber, data);
    semaphore->P();
}

void
SynchDisk::RequestDone()
{
    semaphore->V();
}
