// test2.cc -- simple test routines for homework 2
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "utility.h"
#include "filesys.h"
#include "system.h"
#include "thread.h"
#include "disk.h"
#include "stats.h"

#define TransferSize 	10 	// make it small, just to be difficult

// Copy a file from UNIX to Nachos
void
Copy(char *from, char *to)
{
    FILE *fp;
    OpenFile* openFile;
    int amountRead, fileLength;
    char data[TransferSize];

    if ((fp = fopen(from, "r")) == NULL) {	 // Open UNIX file
	printf("Copy: couldn't open input file %s\n", from);
	return;
    }

    fseek(fp, 0, 2);		// figure out length of UNIX file
    fileLength = ftell(fp);
    fseek(fp, 0, 0);

    DEBUG('f', "Copying file %s, size %d, to file %s\n", from, fileLength, to);
    if (!fileSystem->Create(to, fileLength)) {	 // Create Nachos file
	printf("Copy: couldn't create output file %s\n", to);
	return;
    }
    
    openFile = fileSystem->Open(to);
    ASSERT(openFile != NULL);
    
    while ((amountRead = fread(data, sizeof(char), TransferSize, fp)) > 0)
	openFile->Write(data, amountRead);	// Copy

    delete openFile;
    fclose(fp);
    return;
}

// Print the contents of a Nachos file
void
Print(char *n)
{
    OpenFile *openFile;    
    int amountRead;
    char data[TransferSize];

    if ((openFile = fileSystem->Open(n)) == NULL) {
	printf("Print: unable to open file %s\n", n);
	return;
    }
    
    while ((amountRead = openFile->Read(data, TransferSize)) > 0)
	for (int i = 0; i < amountRead; i++)
	    printf("%c", data[i]);

    delete openFile;
    return;
}

#define FileName 	"TestFile"
#define Contents 	"1234567890"
#define ContentSize 	strlen(Contents)
#define FileSize 	(ContentSize * 5000)

// Write a large file, a bit at a time
void 
FileWrite()
{
    OpenFile *openFile;    
    int i, numBytes;

    printf("Sequential write of %d byte file, in %d byte chunks", 
	FileSize, ContentSize);
    if (!fileSystem->Create(FileName, 0)) {
      printf("Perf test: can't create %s\n", FileName);
      return;
    }
    openFile = fileSystem->Open(FileName);
    if (openFile == NULL) {
	printf("Perf test: unable to open %s\n", FileName);
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Write(Contents, ContentSize);
	if (numBytes < 10) {
	    printf("Perf test: unable to write %s\n", FileName);
	    delete openFile;
	    return;
	}
    }
    delete openFile;	// close file
}

// Read a large file, a bit at a time
void 
FileRead()
{
    OpenFile *openFile;    
    char data[ContentSize];
    int i, numBytes;

    printf("Sequential read of %d byte file, in %d byte chunks", 
	FileSize, ContentSize);

    if ((openFile = fileSystem->Open(FileName)) == NULL) {
	printf("Perf test: unable to open file %s\n", FileName);
	return;
    }
    for (i = 0; i < FileSize; i += ContentSize) {
        numBytes = openFile->Read(data, ContentSize);
	if ((numBytes < 10) || strncmp(data, Contents, ContentSize)) {
	    printf("Perf test: unable to read %s\n", FileName);
	    delete openFile;
	    return;
	}
    }
    delete openFile;	// close file
}

// Measure Nachos file system performance
//    Assumes: large, extensible files have been implemented
//             and file system is empty enough for the big file to fit.
void
PerformanceTest()
{
    printf("Starting file system performance test\n");
    stats->Print();
    FileWrite();
    FileRead();
    if (!fileSystem->Remove(FileName)) {
      printf("Perf test: unable to remove %s\n", FileName);
      return;
    }
    stats->Print();
}

