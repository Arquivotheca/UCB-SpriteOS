// openfile.cc -- routines to manage open files
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "filehdr.h"
#include "openfile.h"
#include "disk.h"
#include "synchdisk.h"
#include "system.h"

// Open a file
OpenFile::OpenFile(int sector)
{ 
    hdr = new FileHeader;
    hdr->FetchFrom(sector);
    seekPosition = 0;
}

// Close a file
OpenFile::~OpenFile()
{
    delete hdr;
}

// Read/write bytes from a file, starting from seekPosition
int
OpenFile::Read(char *into, int numBytes)
{
   int result = ReadAt(into, numBytes, seekPosition);
   seekPosition += result;
   return result;
}

int
OpenFile::Write(char *into, int numBytes)
{
   int result = WriteAt(into, numBytes, seekPosition);
   seekPosition += result;
   return result;
}

// Read/write "numBytes" starting at "position" in a file.
//   Egregiously simple -- implemented by reading the entire file 
//   into memory, then (for read) returning only the part we're interested 
//   in and (for write) modifying the part we're interested in,
//   and writing the whole file back out to disk.
int
OpenFile::ReadAt(char *into, int numBytes, int position)
{
    int length = hdr->FileLength();
    char *buf = new char[length + SectorSize];

    if (numBytes <= 0) return 0; 	// check request
    if ((position + numBytes) > length)		
	numBytes = length - position;
    DEBUG('f', "Reading %d bytes at %d, from file of length %d.\n", 	
			numBytes, position, length);

    // read whole file in
    for (int i = 0; i < length; i += SectorSize)
        synchDisk->ReadSector(hdr->ByteToSector(i), &buf[i]);

    // copy the part we want
    bcopy(&buf[position], into, numBytes);
    delete buf;
    return numBytes;
}

int
OpenFile::WriteAt(char *from, int numBytes, int position)
{
    int length = hdr->FileLength();
    char *buf = new char[length + SectorSize];

    if (numBytes <= 0) return 0;	// check request
    if ((position + numBytes) > length)
	numBytes = length - position;
    DEBUG('f', "Writing %d bytes at %d, from file of length %d.\n", 	
			numBytes, position, length);

    ReadAt(buf, length, 0);			// read whole file in
    bcopy(from, &buf[position], numBytes);	// change part
    for (int i = 0; i < length; i += SectorSize) // write whole file out
        synchDisk->WriteSector(hdr->ByteToSector(i), &buf[i]);
    delete buf;
    return numBytes;
}

int
OpenFile::Length() 
{ 
    return hdr->FileLength(); 
}
