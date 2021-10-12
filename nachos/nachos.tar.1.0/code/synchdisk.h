/* synchdisk.h -- synchronous interface to the raw disk device.  */

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
    
    void RequestDone();

  private:
    Disk* disk;
    Semaphore *semaphore;
};

#endif
