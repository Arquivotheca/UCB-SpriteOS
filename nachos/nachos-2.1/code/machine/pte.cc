// pte.cc -- page table control code
//
// DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "addrspace.h"
#include "system.h"
#include "machine.h"
#include "system.h"
#include "stats.h"

// Read/write 1, 2, or 4 bytes of virtual memory  
// First translate the virtual address to a physical address,
//   and if no errors, fetch/store the location in mainMemory.
bool
PageTable::ReadMem(int addr, int size, int* value)
{
    DEBUG('a', "Reading VA 0x%x, size %d\n", addr, size);
    
    int physicalAddress;
    
    if (!Translate(addr, &physicalAddress, size, FALSE))
	return FALSE;
    switch (size) {
      case 1:
	*value = machine->mainMemory[physicalAddress];
	break;
	
      case 2:
	*value = *(unsigned short *) &machine->mainMemory[physicalAddress];
	break;
	
      case 4:
	*value = *(unsigned int *) &machine->mainMemory[physicalAddress];
	break;

      default: ASSERT(FALSE);
    }
    
    DEBUG('a', "\tvalue read = %8.8x\n", *value);
    return (TRUE);
}

bool
PageTable::WriteMem(int addr, int size, int value)
{
    DEBUG('a', "Writing VA 0x%x, size %d, value 0x%x\n", addr, size, value);
    
    int physicalAddress;
    
    if (!Translate(addr, &physicalAddress, size, TRUE))
	return (FALSE);
    switch (size) {
      case 1:
	machine->mainMemory[physicalAddress] = (unsigned char) (value & 0xff);
	break;

      case 2:
	*(unsigned short *) &machine->mainMemory[physicalAddress]
		= (unsigned short) (value & 0xffff);
	break;
      
      case 4:
	*(unsigned int *) &machine->mainMemory[physicalAddress]
		= (unsigned int) value;
	break;
	
      default: abort();
    }
    
    return (TRUE);
}

// Translate an address using the pageTable.  Check for alignment and 
// all sorts of other errors, and set the use/dirty bits in the PTE.
bool
PageTable::Translate(int virtAddr, int* physAddr, int size, bool writing)
{
    DEBUG('a', "\tTranslate 0x%x, %s: ", virtAddr, writing ? "write" : "read");
    
    int vpn = virtAddr / PageSize;
    int offset = virtAddr % PageSize;

    if (((size == 4) && (virtAddr & 0x3)) || ((size == 2) && (offset & 0x1))) {
	DEBUG('a', "alignment problem at %d, size %d!\n", virtAddr, size);
	machine->RaiseException(AddressErrorException, virtAddr);
	return (FALSE);
    }
    if (vpn >= tableSize) {	// virtAddr out of the address space
	DEBUG('a', "*** vpn = %d, size = %d!\n", vpn, tableSize);
	machine->RaiseException(AddressErrorException, virtAddr);
	return (FALSE);
    }
    
    if (!table[vpn].valid) {
	DEBUG('a', "*** pte not valid!\n");
	stats->numPageFaults++;
	machine->RaiseException(PageFaultException, virtAddr);
	return (FALSE);
    }

    int frame = table[vpn].physicalFrame;

    if (frame >= NumPhysicalPages) {  // something really wrong!
	DEBUG('a', "*** frame %d > %d!\n", frame, NumPhysicalPages);
	machine->RaiseException(BusErrorException, virtAddr);
	return (FALSE);
    }
    
    table[vpn].use = TRUE;
    if (writing)
	table[vpn].dirty = TRUE;

    *physAddr = frame * PageSize + offset;
    
    ASSERT((*physAddr >= 0) && ((*physAddr + size) < MemorySize));

    DEBUG('a', "phys addr = 0x%x\n", *physAddr);
    
    return (TRUE);
}

