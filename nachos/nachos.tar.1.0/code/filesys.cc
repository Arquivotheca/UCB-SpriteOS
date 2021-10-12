/* filesys.cc -- Routines to manage naming in the file system.
 *
 * The bitmap of free disk sectors and the filename directory
 *  are represented as normal files.  In order to find them,
 *  we put their FileHeaders in a known place (sector 0 and sector 1). 
 */

#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "openfile.h"
#include "filesys.h"

// Put these in a known place, so we can find them on boot up.
const int FreeMapSector = 0;
const int DirectorySector = 1;

// Constants for the directory and bitmap files.
const int FreeMapFileSize = NumSectors / bitsInByte;
const int NumDirEntries = 10;
const int DirectoryFileSize = sizeof(DirectoryEntry) * NumDirEntries;

// Initialize the file system.
//   Open the files representing the directory and the bitmap of free disk 
//   blocks.  If we are "formatting" the disk (i.e., if the disk 
//   has garbage on it), we have to set up the disk first.

FileSystem::FileSystem(bool format)
{ 
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        BitMap freeMap(NumSectors);
        Directory directory(NumDirEntries);
	FileHeader mapHdr, dirHdr;

        DEBUG('f', "Formatting the file system.\n");
	freeMap.Mark(FreeMapSector);	    // allocate space for FileHeaders
	freeMap.Mark(DirectorySector);

    // allocate space for directory and bitmap -- better be enough space!
	ASSERT(mapHdr.Allocate(&freeMap, FreeMapFileSize));
	ASSERT(dirHdr.Allocate(&freeMap, DirectoryFileSize));

        DEBUG('f', "Writing headers back to disk.\n");
	mapHdr.WriteBack(FreeMapSector);    // write FileHeader's back to disk
	dirHdr.WriteBack(DirectorySector);

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
     
        DEBUG('f', "Writing bitmap and directory back to disk.\n");
	freeMap.WriteBack(freeMapFile);	 // flush changes to disk
	directory.WriteBack(directoryFile);

	if (DebugIsEnabled('f')) {
	    freeMap.Print();
	    directory.Print();
	}
    } else {
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
}

// Create a file.  Allocate a sector for the file header, add the name to the 
//   directory, allocate a new file header, allocate space for the data
//   blocks for the file, and flush everything back to disk.
// On an error (eg, if name already exists), return FALSE.
bool
FileSystem::Create(char *name, int initialSize)
{
    int sector;
    FileHeader hdr;
    Directory directory(NumDirEntries);
    BitMap freeMap(NumSectors);

    freeMap.FetchFrom(freeMapFile);
    directory.FetchFrom(directoryFile);

    if (directory.Find(name) != -1)
	return FALSE;			// file already in directory
    sector = freeMap.Find();	 	// find a sector for file header
    if (sector == -1) 
        return FALSE;			// no free blocks
    if (!directory.Add(name, sector))  
	return FALSE;		  	// no space in directory
    if (!hdr.Allocate(&freeMap, initialSize))
        return FALSE; 			// no space on disk for data

    hdr.WriteBack(sector); 		// flush to disk
    directory.WriteBack(directoryFile);
    freeMap.WriteBack(freeMapFile);
    return TRUE;
}

// Open a file by looking up the name in the directory. 
OpenFile *
FileSystem::Open(char *name)
{ 
    Directory directory(NumDirEntries);
    int sector;

    directory.FetchFrom(directoryFile);
    sector = directory.Find(name); 
    if (sector >= 0) 		
	return new OpenFile(sector);	 // name was found in directory 
    else
	return NULL;			// name not found
}

// Delete a file.  First, remove it from the directory, then delete its space.
bool
FileSystem::Remove(char *name)
{ 
    FileHeader fileHdr;
    BitMap freeMap(NumSectors);
    Directory directory(NumDirEntries);
    int sector;
    
    freeMap.FetchFrom(freeMapFile);
    directory.FetchFrom(directoryFile);
    sector = directory.Find(name);
    if (sector == -1)
       return FALSE;			 // file not in directory
    fileHdr.FetchFrom(sector);

    fileHdr.Deallocate(&freeMap);  		// remove data blocks
    freeMap.Clear(sector);			// remove header block
    directory.Remove(name);

    freeMap.WriteBack(freeMapFile);		// flush to disk
    directory.WriteBack(directoryFile);    // flush to disk
    return TRUE;
} 

void
FileSystem::List()
{
    Directory directory(NumDirEntries);

    directory.FetchFrom(directoryFile);
    directory.List();
}

void
FileSystem::Print()
{
    FileHeader bitHdr, dirHdr;
    BitMap freeMap(NumSectors);
    Directory directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr.FetchFrom(FreeMapSector);
    bitHdr.Print();

    printf("Directory file header:\n");
    dirHdr.FetchFrom(DirectorySector);
    dirHdr.Print();

    freeMap.FetchFrom(freeMapFile);
    freeMap.Print();

    directory.FetchFrom(directoryFile);
    directory.Print();
} 
