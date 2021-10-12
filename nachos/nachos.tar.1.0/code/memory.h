/* memory.h -- Stuff to emulate the physical memory of the machine.
 *
 * A Machine handles interrupts, and in later assignments, 
 * will contain the interface for the MIPS simulator.
 *
 * We will be changing this file, so don't modify it!
 */

#ifndef MEMORY_H
#define MEMORY_H

#include "utility.h"

class Memory {
  public:
    Memory(int sz) {
	size = sz;
	bytes = new unsigned char[size];
	for (int i = 0; i < size; i++)
	    bytes[i] = 0;
    }
    ~Memory() { delete bytes; }
    
    unsigned char ReadByte(int addr) {
	ASSERT((addr >= 0) && (addr < size));
	return (bytes[addr]);
    }
    void WriteByte(int addr, unsigned char val) {
	ASSERT((addr >= 0) && (addr < size));
	bytes[addr] = val;
    }

    unsigned short ReadShort(int addr) {
	ASSERT((addr >= 0) && (addr < size) && !(addr & 1));
	return (*(unsigned short *) &bytes[addr]);
    }
    void WriteShort(int addr, unsigned short val) {
	ASSERT((addr >= 0) && (addr < size) && !(addr & 1));
	*(unsigned short *) &bytes[addr] = val;
    }
    
    unsigned long ReadWord(int addr) {
	ASSERT((addr >= 0) && (addr < size) && !(addr & 3));
	return (*(unsigned long *) &bytes[addr]);
    }
    void WriteWord(int addr, unsigned long val) {
	ASSERT((addr >= 0) && (addr < size) && !(addr & 3));
	*(unsigned long *) &bytes[addr] = val;
    }
    
  private:
    int size;
    unsigned char* bytes;
};

#endif
