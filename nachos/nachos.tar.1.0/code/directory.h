/* directory.h -- class for managing a directory of file names.
 *
 *  A directory is a table of directory entries.  Each directory 
 *  entry contains the name of the file and where to find it on disk.
 *
 *  We assume mutual exclusion is provided by the caller.
 */

#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "openfile.h"

const int FileNameMaxLen = 9;

class DirectoryEntry {
  public:			 // public, for convenience
    bool inUse;
    int sector;			// where to find the FileHeader for name
    char name[FileNameMaxLen + 1];	// +1 for the trailing '\0'
};

class Directory {
  public:
    Directory(int size);
    ~Directory() { delete table; } 

    void FetchFrom(OpenFile *file);  	// initialize from disk
    void WriteBack(OpenFile *file);	// write back to disk

    int Find(char *name);		// find the file header for "name"

    bool Add(char *name, int newSector);  // add name into the directory.

    bool Remove(char *name);		 // remove name from directory

    void List();			// print the names of the files
    void Print();			// verbose print of the directory

  private:
    int tableSize;
    DirectoryEntry *table;

    int FindIndex(char *name);		// find index into directory table
};

#endif
