/* openfile.cc -- Routines to manage open files.  */

#include "filehdr.h"
#include "openfile.h"
#include "system.h"
#include "disk.h"
#include "synchdisk.h"

/* Open a file */
OpenFile::OpenFile(int sector)
{ 
    hdr = new FileHeader;
    hdr->FetchFrom(sector);
    seekPosition = 0;
}

/* Close a file */
OpenFile::~OpenFile()
{
    delete hdr;
}

/* Read/write bytes from a file, starting from seekPosition.  */
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

/* Read bytes from a specific point in a file.
 *
 * Implemented by reading the entire file into memory, then returning 
 *  the part we're interested in.
 */
int
OpenFile::ReadAt(char *into, int numBytes, int position)
{
    int length = hdr->FileLength();
    char *buf = new char[length];

    if (numBytes <= 0) return 0;
    if ((position + numBytes) > length)
	numBytes = length - position;
    DEBUG('f', "Reading %d bytes at %d, from file of length %d.\n", 	
			numBytes, position, length);

    // read in all of the blocks of the file
    for (int i = 0; i < length; i += SectorSize)
        synchDisk->ReadSector(hdr->ByteToSector(i), &buf[i]);

    bcopy(&buf[position], into, numBytes);
    delete buf;
    return numBytes;
}

/* Write bytes to a specific point in a file.
 *
 * Implemented by reading the entire file into memory, then modifying
 *  the part we're interested in, then writing the whole file back out.
 */
int
OpenFile::WriteAt(char *from, int numBytes, int position)
{
    int length = hdr->FileLength();
    char *buf = new char[length];

    if (numBytes <= 0) return 0;
    if ((position + numBytes) > length)
	numBytes = length - position;
    DEBUG('f', "Writing %d bytes at %d, from file of length %d.\n", 	
			numBytes, position, length);

    ReadAt(buf, length, 0);
    bcopy(from, &buf[position], numBytes);
    for (int i = 0; i < length; i += SectorSize)
        synchDisk->WriteSector(hdr->ByteToSector(i), &buf[i]);
    delete buf;
    return numBytes;
}
