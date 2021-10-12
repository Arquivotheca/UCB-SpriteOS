

/* $Revision: 1.2 $ on $Date: 1991/12/20 07:50:34 $
 * $Source: /home/ygdrasil/a/faustus/cs162/hw1+/RCS/machine.cc,v $
 *
 * Address space control code.  In order to run a binary, you must have compiled
 * it with the -N -T 0 option to make sure it's not shared text.
 */

#include "addrspace.h"
#include "memorymgr.h"
#include "scheduler.h"

extern "C" {
#include <filehdr.h>
#include <aouthdr.h>
#include <scnhdr.h>
}    

AddrSpace::AddrSpace(char* filename, FileSystem* fs)
{
    space = this;  // We need to set the global addrspace pointer now.
    int i;
    
    // ------------------------------------------------------------
    // This code reads in data from a COFF format file.  Don't worry about
    // this too much, except for replacing the UNIX file ops with Nachos ones.
    int filedes = open(filename, 0, 0);
    if (filedes == -1) {
	perror(filename);
	exit(1);
    }
    
    // Read in the file header and check the magic number.
    filehdr fileh;
    if (read(filedes, (char *) &fileh, sizeof (fileh)) != sizeof (fileh)) {
	fprintf(stderr, "File is too short\n");
	exit(1);
    }
    if (fileh.f_magic != MIPSELMAGIC) {
	fprintf(stderr, "File is not a MIPSEL COFF file\n");
	exit(1);
    }
    
    // Read in the system header and check the magic number.
    aouthdr systemh;
    if (read(filedes, (char *) &systemh, sizeof (systemh)) != sizeof (systemh)){
	fprintf(stderr, "File is too short\n");
	exit(1);
    }
    if (systemh.magic != OMAGIC) {
	fprintf(stderr, "File is not a OMAGIC file\n");
	exit(1);
    }
    
    // Read in the section headers.
    int numsections = fileh.f_nscns;
    scnhdr sections[numsections];
    if (read(filedes, (char *) sections, sizeof (scnhdr) * numsections)
	!= sizeof (scnhdr) * numsections) {
	fprintf(stderr, "File is too short\n");
	exit(1);
    }
    
    // Now we have to extract all the useful data from these section headers.
    int top = 0;
    for (i = 0; i < numsections; i++) {
	int ssize = sections[i].s_paddr + sections[i].s_size;
	if (ssize > top)
	    top = ssize;
    
    }
    // -----------------------------------------------
    // Now you should start paying attention again.
    
    // Calculate the total size of the program.
    totalSize = top + StackSize;
    if (totalSize & (PageSize - 1))
	totalSize = (totalSize & ~(PageSize - 1)) + PageSize;

    // Allocate a bunch of space in main memory.
    pageTableSize = totalSize / PageSize;
    pageTable = new PTE[pageTableSize];
    memoryManager->AllocateSpace(pageTable, pageTableSize);
    
    // Copy the segments in.
    DEBUG('a', "Loading %d sections:\n", numsections);
    for (i = 0; i < numsections; i++) {
	DEBUG('a', "\t\"%s\", filepos 0x%x, mempos 0x%x, size 0x%x\n",
	      sections[i].s_name, sections[i].s_scnptr,
	      sections[i].s_paddr, sections[i].s_size);
	if (!strcmp(sections[i].s_name, ".bss") ||
	    !strcmp(sections[i].s_name, ".sbss")) {
	    for (int j = 0; j < sections[i].s_size; j++)
		space->WriteMem(sections[i].s_paddr + j, 1, 0);
	} else {
	    int size = sections[i].s_size;
	    char buffer[size];
	    lseek(filedes, sections[i].s_scnptr, 0);
	    int n = read(filedes, buffer, size);
	    ASSERT(n == size);
	    space->WriteBuffer(sections[i].s_paddr, buffer, size);
	}
    }

    close(filedes);

#if 0
    // For debugging purposes.
    for (i = 0; i < totalSize; i += 4) {
	int val;
	ReadMem(i, 4, &val);
	printf("%8.8x: %8.8x\n", i, val);
    }
#endif
    
    // Set the initial values for the registers appropriately.
    // Setting the return address to -4 is a trick -- it's intercepted
    // by the machine simulator, and it means to take the thing at
    // the top of the stack and return it as the exit status of
    // the program.
    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);
    machine->WriteRegister(PCReg, 0); //text);
    machine->WriteRegister(NextPCReg, 4); // text + 4);
    machine->WriteRegister(StackReg, totalSize - 4);
    machine->WriteRegister(RetAddrReg, -4);
}

AddrSpace::~AddrSpace()
{
    memoryManager->DeallocateSpace(pageTable, pageTableSize);
}

void
AddrSpace::PrintInfo()
{
    printf("Address Space for thread \"%s\"\n", currentThread->getName());
    printf("     size: 0x%x\n", totalSize);
    printf("\n");
}

void
AddrSpace::ShowStackTop(int num)
{
    printf("Stack top:");
    for (int i = 0; i < num; i++) {
	int p = totalSize - (i + 1) * 4;
	int val;
	ReadMem(p, 4, &val);
	printf("  0x%x", val);
    }
    printf("\n");
}

// Stuff to read and write in an address space.  Note that everything here
// goes through Translate, which is inefficient if we are reading or
// writing a block at a time.  But that's okay.
//
// This needs to be modified to deal with virtual memory.

bool
AddrSpace::ReadMem(int addr, int size, int* value)
{
    DEBUG('a', "Reading VA 0x%x, size %d\n", addr, size);
    
    int physicalAddress;
    
    if (((size == 4) && (addr & 0x3)) || ((size == 2) && (addr & 0x1))) {
	machine->badVirtualAddress = addr;
	machine->RaiseException(AddressErrorException);
	return (FALSE);
    }

    if (!Translate(addr & ~0x3, &physicalAddress, FALSE))
	return (FALSE);
    physicalAddress += (addr & 0x3);

    switch (size) {
      case 1:
	*value = memory->ReadByte(physicalAddress);
	break;
	
      case 2:
	*value = memory->ReadShort(physicalAddress);
	break;
	
      case 4:
	*value = memory->ReadWord(physicalAddress);
	break;

      default: abort();
    }
    
    DEBUG('a', "\tvalue read = %8.8x\n", *value);
    return (TRUE);
}

bool
AddrSpace::WriteMem(int addr, int size, int value)
{
    DEBUG('a', "Writing VA 0x%x, size %d, value 0x%x\n", addr, size, value);
    
    int physicalAddress;
    
    if (((size == 4) && (addr & 0x3)) || ((size == 2) && (addr & 0x1))) {
	machine->badVirtualAddress = addr;
	machine->RaiseException(AddressErrorException);
	return (FALSE);
    }
    
    if (!Translate(addr & ~0x3, &physicalAddress, TRUE))
	return (FALSE);
    physicalAddress += (addr & 0x3);
    
    switch (size) {
      case 1:
	memory->WriteByte(physicalAddress, value & 0xff);
	break;

      case 2:
	memory->WriteShort(physicalAddress, value & 0xffff);
	break;
      
      case 4:
	memory->WriteWord(physicalAddress, value);
	break;
	
      default: abort();
    }
    
    return (TRUE);
}

bool
AddrSpace::Translate(int virtAddr, int* physAddr, bool writing)
{
    DEBUG('a', "\tTranslate 0x%x, %s: ", virtAddr, writing ? "write" : "read");
    
    int vpn, offset, frame;
    
    machine->badVirtualAddress = virtAddr;
    vpn = virtAddr / PageSize;
    offset = virtAddr % PageSize;
    
    // These three strings are walking down the street, and they come up
    // to a bar.  One goes in and asks for a beer.  The bartender says,
    // "Are you a string?"  The string says yes and the bartender says,
    // "We don't serve strings here!" and throws him out.  The second one
    // sees this, and tries to look tough as he goes in and asks for a beer.
    // The bartender asks him, "Another string??"  The string says, "Yeah,
    // what of it", and the bartender throws him out too.  The third one
    // tries to be devious, so he ties himself up and pulls his strands
    // apart a bit.  Then he goes into the bar and the bartender asks him,
    // "Are you a string too??".  The string says, "No, I'm afraid not".
    // (Get it?  A frayed knot....)
    if (virtAddr & 3) {
	DEBUG('a', "*** unaligned address!\n");
	machine->RaiseException(AddressErrorException);
	return (FALSE);
    }
    if (!pageTable) {
	DEBUG('a', "*** no page table!\n");
	machine->RaiseException(AddressErrorException);
	return (FALSE);
    }
    if (vpn >= pageTableSize) {
	DEBUG('a', "*** vpn = %d, size = %d!\n", vpn, pageTableSize);
	machine->RaiseException(AddressErrorException);
	return (FALSE);
    }
    
    PTE* pte = &pageTable[vpn];
    
    if (!pte->valid) {
	DEBUG('a', "*** pte not valid!\n");
	machine->RaiseException(PageFaultException);
	return (FALSE);
    }
    frame = pte->physicalFrame;
    if (frame >= NumPhysicalPages) {
	DEBUG('a', "*** frame %d > %d!\n", frame, NumPhysicalPages);
	machine->RaiseException(BusErrorException);
	return (FALSE);
    }
    
    pte->use = TRUE;
    if (writing)
	pte->dirty = TRUE;

    *physAddr = frame * PageSize + offset;
    
    DEBUG('a', "phys addr = 0x%x\n", *physAddr);
    
    return (TRUE);
}
