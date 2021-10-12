// filesys.h -- Class representing the file system.
//
// Defines routines to manage naming in the file system --
//   creating, opening, and deleting named files.
//
// File read and file write are defined in the OpenFile class.
//
// These are similar to the UNIX commands creat, open, and unlink. 
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FS_H
#define FS_H

#include "utility.h"
#include "openfile.h"

class FileSystem {
  public:
    // Initialize the file system -- assumes "synchDisk" has been initialized.
    // If format is set, the disk is garbage, so initialize the directory
    // and the map of free blocks.
    FileSystem(bool format);

    bool Create(char *name, int initialSize);  	// create a file

    OpenFile* Open(char *name); 	// open a file

    bool Remove(char *name);  		// delete a file (UNIX unlink)

    void List();

    void Print();

  private:
   OpenFile* freeMapFile;		// bit map of free blocks
   OpenFile* directoryFile;		// table of file names
};

#endif
