// filehdr.h -- class for managing disk file header block.
//
// This data structure lives on disk, and is assumed to be the same
// size as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "bitmap.h"

#define NumDirect 	((SectorSize - sizeof(int)) / sizeof(int))
#define MaxFileSize 	(NumDirect * SectorSize)

class FileHeader {
  public:
    bool Allocate(BitMap *bitMap, int fileSize);  // allocate data blocks
    void Deallocate(BitMap *bitMap);  		// de-allocate data blocks

    void FetchFrom(int sectorNumber); 	// Initialize from disk
    void WriteBack(int sectorNumber); 	// Write back to disk

    int ByteToSector(int offset) { return(dataSectors[offset / SectorSize]); }

    int FileLength() { return numBytes; }

    void Print();

  private:
    int numBytes;		// size of the file
    int dataSectors[NumDirect];	// pointers to the data blocks

    int FileSectors() {	    // FileLength, in sectors
       return (numBytes / SectorSize) + (((numBytes % SectorSize) > 0) ? 1 : 0);
    }
};

#endif
