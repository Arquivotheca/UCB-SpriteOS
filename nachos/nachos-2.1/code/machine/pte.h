// pte.h -- Handle page tables.
//
// DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef PTE_H
#define PTE_H

#include "disk.h"

#define PageSize 	SectorSize 	// the disk sector size

// Page table entries.  These are used to map from the addresses in the
// address space to real, physical addresses.  We keep PTE's in a big
// array for each address space.
class PTE {
  public:
    PTE() {
	physicalFrame = -1;
	valid = dirty = use = FALSE;
    }

    void Set(int frame) {
	physicalFrame = frame;
	valid = TRUE;
	dirty = use = FALSE;
    }
    
    int physicalFrame;  // The page number in real memory.
    bool valid;         // Is this page valid?  (I.e., is it in memory?)
    bool dirty;         // Has this page been written to?
    bool use;           // Has this page been accessed at all?
};

class PageTable {
  public:
    PageTable(int sz) { tableSize = sz; table = new PTE[sz]; }
    ~PageTable() { delete table; }

    // Read or write 1, 2, or 4 bytes of virtual memory (at addr).  
    // Return FALSE if there is a translation error.
    bool ReadMem(int addr, int size, int* value);
    bool WriteMem(int addr, int size, int value);
    
    // Translate an address using the pageTable, and check for alignment.
    // Set the use and dirty bits in the PTE appropriately,
    // and return FALSE on error.
    bool Translate(int virtAddr, int* physAddr, int size, bool writing);

    void SetPTE(int vFrame, int pFrame) { table[vFrame].Set(pFrame); }

    int getTableSize() { return tableSize; } 
    
  private:
    PTE *table;
    int tableSize;
};

#endif
