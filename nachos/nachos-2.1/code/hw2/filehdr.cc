// filehdr.cc -- routines for managing the disk file header block.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "bitmap.h"
#include "filehdr.h"
#include "synchdisk.h"
#include "system.h"

// Initialize a fresh file header by allocating blocks out of the bitmap.
bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
    numBytes = fileSize;
    if (freeMap->NumClear() < FileSectors())
	return FALSE;		// not enough space

    for (int i = 0; i < FileSectors(); i++)
	dataSectors[i] = freeMap->Find();
    return TRUE;
}

// de-allocate all of our data blocks
void 
FileHeader::Deallocate(BitMap *freeMap)
{
    for (int i = 0; i < FileSectors(); i++) {
	ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	freeMap->Clear((int) dataSectors[i]);
    }
}

// Fetch contents of header from disk 
void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

// Write back contents of header to disk
void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

// for debugging
void
FileHeader::Print()
{
    int i, j, k;
    char data[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < FileSectors(); i++)
	printf("%d ", dataSectors[i]);
    printf("\nFile contents:\n");
    for (i = k = 0; i < FileSectors(); i++) {
	synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if (isprint(data[j]))
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
}
