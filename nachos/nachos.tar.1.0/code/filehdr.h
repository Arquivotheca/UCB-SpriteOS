/* filehdr.h -- class for managing disk file header block.
 *
 * This data structure lives on disk, and is assumed to be the same
 * size as one disk sector.  Without indirect addressing, this
 * limits the maximum file length to just under 4K bytes.
 */

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "bitmap.h"

const int NumDirect = (SectorSize - sizeof(int)) / sizeof(int);
const int MaxFileSize = NumDirect * SectorSize;

class FileHeader {
  public:
    bool Allocate(BitMap *bitMap, int fileSize);  // allocate data blocks
    void Deallocate(BitMap *bitMap);  		// de-allocate data blocks

    void FetchFrom(int sectorNumber); 	// Initialize from disk
    void WriteBack(int sectorNumber); 	// Write back to disk

    int ByteToSector(int offset) { return(dataSectors[offset / SectorSize]); }

    int FileLength() { return numBytes; }

    int NumSectors() {	    // compute # sectors needed, from the file size
       return (numBytes / SectorSize) + (((numBytes % SectorSize) > 0) ? 1 : 0);
    }

    void Print();

  private:
    int numBytes;		/* size of the file */
    int dataSectors[NumDirect];	/* pointers to the data blocks */
};

#endif
