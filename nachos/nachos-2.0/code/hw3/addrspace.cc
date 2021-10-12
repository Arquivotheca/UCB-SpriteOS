// addrspace.cc -- address space control code. 
//
// In order to run a binary, you must have:
//	1. compiled it with the -N -T 0 option to make sure it's not shared text
//	2. run coff2flat to convert it to a flat file
//	3. loaded the flat file into the Nachos file system
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "addrspace.h"
#include "system.h"
#include "utility.h"

// Create an address space, loading it in from "binary"
// Also, set up the registers and page table.
AddrSpace::AddrSpace(OpenFile *binary)
{
    int size = binary->Length();

    if (size & (PageSize - 1))		// round up
	size = (size & ~(PageSize - 1)) + PageSize;

// first, set up page table
    pageTable = new PageTable(size / PageSize);
    for (int i = 0; i < pageTable->getTableSize(); i++)
	pageTable->SetPTE(i, i); // for now, virt page = phys page
    machine->pageTable = pageTable;
    
// then, copy binary into memory
    binary->ReadAt(machine->mainMemory, size, 0);  

// Then set the initial values for the registers appropriately.
    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);
    machine->WriteRegister(PCReg, 0);
    machine->WriteRegister(NextPCReg, 4);
    machine->WriteRegister(StackReg, size - 4);
}

AddrSpace::~AddrSpace()
{
    delete pageTable;
}

// Save user-level processor state
void
AddrSpace::SaveState()
{
    for (int i = 0; i < NumTotalRegs; i++)
	savedRegisters[i] = machine->ReadRegister(i);
}

// Restore user-level processor state
void
AddrSpace::RestoreState()
{
    for (int i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, savedRegisters[i]);
    machine->pageTable = pageTable;
}
