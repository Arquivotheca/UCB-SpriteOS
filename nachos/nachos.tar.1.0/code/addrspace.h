
/* $Revision: 1.2 $ on $Date: 1991/12/20 07:50:34 $
 * $Source: /home/ygdrasil/a/faustus/cs162/hw1+/RCS/machine.h,v $
 *
 * Stuff to handle address spaces.  The main work here is taking a program from
 * a disk file and getting it all set to be swapped in.
 */

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "utility.h"
#include "machine.h"
#include "filesys.h"

// The memory page size is also the default disk blocksize.
#define PageSize		128

// Page table entries.  These are used to map from the addresses in the
// address space to real, physical addresses.  We keep PTE's in a big
// array for each address space.
struct PTE {
    PTE() {
	physicalFrame = -1;
	valid = dirty = use = FALSE;
    }
    
    int physicalFrame;  // The page number in real memory.
    bool valid;         // Is this page valid?  (I.e., is it in memory now?)
    bool dirty;         // Has this page been written to?
    bool use;           // Has this page been accessed at all?
};

class AddrSpace {
  public:
    // Create an address space from the file on the disk.  This loads it into
    // the memoryBuffer but doesn't run it yet.
    // We also have to set up the registers and page table.
    AddrSpace(char* filename, FileSystem* fs);
    ~AddrSpace();

    void PrintInfo();
    void ShowStackTop(int num);
    
    // These routines read from and write to virtual memory, with the currently
    // defined page table entries.
    void ReadBuffer(int addr, char* buf, int len) {
	for (int i = 0; i < len; i++) {
	    int val;
	    ReadMem(addr + i, 1, &val);
	    buf[i] = val;
	}
    }
    void WriteBuffer(int addr, char* buf, int len) {
	for (int i = 0; i < len; i++)
	    WriteMem(addr + i, 1, buf[i]);
    }

    // These operate on NULL-terminated strings, for convenience.
    int ReadString(int addr, char* buf, int buflen) {
	for (int i = 0; i < buflen; i++) {
	    int val;
	    ReadMem(addr + i, 1, &val);
	    buf[i]= val;
	    if (!buf[i])
		break;
	}
	return (i);
    }
    void WriteString(int addr, char* buf) {
	for (;;) {
	    WriteMem(addr, 1, *buf);
	    if (!*buf)
		break;
	    addr++;
	    buf++;
	}
    }
    
    // Do address translation, based on the pageTable.
    // Set the dirty bit in the PTE if the write flag was given.
    bool Translate(int virtAddr, int* physAddr, bool writing);
    
    // Read and write the 1, 2, or 4 byte value at addr.  Return FALSE if
    // there was a translation error.  Addr is the virtual address.
    bool ReadMem(int addr, int size, int* value);
    bool WriteMem(int addr, int size, int value);
    
  private:
    int totalSize;  // The total size in bytes of everything.
    
    PTE* pageTable;
    int pageTableSize;
} ;

class Yack {} ;  // This has to be here to catch a bug.

#endif
