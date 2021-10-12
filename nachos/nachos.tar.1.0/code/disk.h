/* disk.h -- emulate a physical disk. */

#ifndef DISK_H
#define DISK_H

#include "thread.h"

const int SectorSize =		128;	/* bytes */
const int SectorsPerTrack =	32;
const int NumTracks =		32;
const int NumSectors =		(SectorsPerTrack * NumTracks);

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
    int ComputeLatency(int newSector);	
    
    void HandleInterrupt();

  private:
    int fileno;			// UNIX file number for emulated disk 
    VoidFunctionPtr handler;	// who to call when request finishes
    int handlerArg;
    bool active;     		// Is a disk operation in progress?
    int lastSector;		// last request to be made
};

#endif

