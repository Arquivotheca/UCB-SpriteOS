// disk.cc -- emulation of a physical disk
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "interrupt.h"
#include "utility.h"
#include "system.h"
#include "stats.h"

extern "C" {
#include <sys/types.h>
#include <sys/file.h>

// int open(char *name, int flags, int mode);
int abs(int i);
}

// We put this at the front of the UNIX file representing the
// disk, to make it less likely we are over-writing useful data.
#define MagicNumber 	0x456789ab
#define MagicSize 	sizeof(int)

#define DiskSize 	(MagicSize + (NumSectors * SectorSize))

// dummy procedure because we can't take a pointer of a member function
static void DiskDone(int arg) { ((Disk *)arg)->HandleInterrupt(); }

// Create the emulated disk -- open the UNIX file, and make sure it's 
// the right one.  Create the file if it doesn't exist.
Disk::Disk(char* name, VoidFunctionPtr callWhenDone, int callArg)
{
    int magicNum;
    int tmp = 0;

    DEBUG('d', "Initializing the disk, 0x%x 0x%x\n", callWhenDone, callArg);
    handler = callWhenDone;
    handlerArg = callArg;
    lastSector = 0;
    bufferInit = 0;
    
    fileno = open(name, O_RDWR, 0);
    if (fileno >= 0) {		 // file exists, check magic number 
	Read(fileno, (char *) &magicNum, MagicSize);
	ASSERT(magicNum == MagicNumber);
    } else {			// file doesn't exist, create it
        fileno = Open(name, O_RDWR|O_CREAT,  0644);
	magicNum = MagicNumber;  
	Write(fileno, (char *) &magicNum, MagicSize); 	// write magic number

	// need to write at end of file, so that reads will not return EOF
        Lseek(fileno, DiskSize - sizeof(int), 0);	
	Write(fileno, (char *)&tmp, sizeof(int));  
    }
    active = FALSE;
}

// Just close the UNIX file 
Disk::~Disk()
{
    Close(fileno);
}

// for debugging
static void
PrintSector (bool writing, int sector, char *data)
{
    int *p = (int *) data;

    if (writing)
        printf("Writing sector: %d\n", sector); 
    else
        printf("Reading sector: %d\n", sector); 
    for (unsigned int i = 0; i < (SectorSize/sizeof(int)); i++)
	printf("%x ", p[i]);
    printf("\n"); 
}

// Emulate a disk read (or write) request -- do the read (write) immediately,
// and notify the caller asynchronously some number of ticks later.
void
Disk::ReadRequest(int sectorNumber, char* data)
{
    int ticks = ComputeLatency(sectorNumber, FALSE);

    ASSERT(!active);
    ASSERT((sectorNumber >= 0) && (sectorNumber < NumSectors));
    
    DEBUG('d', "Reading from sector %d\n", sectorNumber);
    Lseek(fileno, SectorSize * sectorNumber + MagicSize, 0);
    Read(fileno, data, SectorSize);
    if (DebugIsEnabled('d'))
	PrintSector(FALSE, sectorNumber, data);
    
    active = TRUE;
    UpdateLast(sectorNumber);
    stats->numDiskReads++;
    interrupt->Schedule(DiskDone, (int) this, ticks, DiskInt);
}

void
Disk::WriteRequest(int sectorNumber, char* data)
{
    int ticks = ComputeLatency(sectorNumber, TRUE);

    ASSERT(!active);
    ASSERT((sectorNumber >= 0) && (sectorNumber < NumSectors));
    
    DEBUG('d', "Writing to sector %d\n", sectorNumber);
    Lseek(fileno, SectorSize * sectorNumber + MagicSize, 0);
    Write(fileno, data, SectorSize);
    if (DebugIsEnabled('d'))
	PrintSector(TRUE, sectorNumber, data);
    
    active = TRUE;
    UpdateLast(sectorNumber);
    stats->numDiskWrites++;
    interrupt->Schedule(DiskDone, (int) this, ticks, DiskInt);
}

// Handle the interrupt, vectoring to whoever made the disk request
void
Disk::HandleInterrupt ()
{ 
    active = FALSE;
    (*handler)(handlerArg);
}

// Latency = seek time + rotational latency + transfer time
//   Disk seeks at one track per SeekTime ticks (cf. stats.h)
//   and rotates at one sector per RotationTime ticks and 
//
//   To find the rotational latency, we first must figure out where the 
//   disk head will be after the seek (if any).  We then figure out
//   how long it will take to rotate completely past newSector after that point.
//
//   The disk also has a "track buffer"; the disk continuously reads
//   the contents of the current disk track into the buffer.  This allows 
//   read requests to the current track to be satisfied more quickly.
//   The contents of the track buffer are discarded after every seek to 
//   a new track.

int
Disk::TimeToSeek(int newSector, int *rotation) 
{
    int newTrack = newSector / SectorsPerTrack;
    int oldTrack = lastSector / SectorsPerTrack;
    int seek = abs(newTrack - oldTrack) * SeekTime;
    int over = (stats->totalTicks + seek) % RotationTime; 

    *rotation = 0;
    if (over > 0)	 // time to get to beginning of next sector
   	*rotation = RotationTime - over;
    return seek;
}

// number of sectors between to and from
int 
Disk::ModuloDiff(int to, int from)
{
    int toOffset = to % SectorsPerTrack;
    int fromOffset = from % SectorsPerTrack;

    return ((toOffset - fromOffset) + SectorsPerTrack) % SectorsPerTrack;
}

int
Disk::ComputeLatency(int newSector, bool writing)
{
    int rotation;
    int seek = TimeToSeek(newSector, &rotation);
    int timeAfter = stats->totalTicks + seek + rotation;

    // check if track buffer applies
    if ((writing == FALSE) && (seek == 0) 
		&& (((timeAfter - bufferInit) / RotationTime) 
	     		> ModuloDiff(newSector, bufferInit / RotationTime))) {
        DEBUG('d', "Request latency = %d\n", RotationTime);
	return RotationTime; // time to transfer sector from the track buffer
    }

    rotation += ModuloDiff(newSector, timeAfter / RotationTime) * RotationTime;

    DEBUG('d', "Request latency = %d\n", seek + rotation + RotationTime);
    return(seek + rotation + RotationTime);
}

void
Disk::UpdateLast(int newSector)
{
    int rotate;
    int seek = TimeToSeek(newSector, &rotate);
    
    if (seek != 0)
	bufferInit = stats->totalTicks + seek + rotate;
    lastSector = newSector;
    DEBUG('d', "Updating last sector = %d, %d\n", lastSector, bufferInit);
}
