// addrspace.h -- manage address spaces.
//
//   The main work here is taking a program from a disk file and 
//   getting it all set to be swapped in.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "pte.h"
#include "machine.h"
#include "filesys.h"

class AddrSpace {
  public:
    AddrSpace(OpenFile *binary);
    ~AddrSpace();

    void SaveState();		// Save user-level processor state
    void RestoreState();	// Restore user-level processor state

  private:
    PageTable *pageTable;
    int savedRegisters[NumTotalRegs];	// user-level processor state
};

#endif
