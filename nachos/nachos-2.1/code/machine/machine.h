// machine.h -- Low-level machine emulation data structures,  
//   reflecting the state of the machine for executing user programs.
// Source is in both machine.cc and mipssim.cc.
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"


#ifndef MACHINE_H
#define MACHINE_H

#include "utility.h"
#include "pte.h"

#define NumPhysicalPages 	32
#define MemorySize 		(NumPhysicalPages * PageSize)

// Program-generated exceptions.
enum ExceptionType { SyscallException,      // A program executed a system call.
		     PageFaultException,    // Valid bit in PTE not set.
		     BusErrorException,     // Translation gave invalid address.
		     AddressErrorException, // Unaligned or OOB addr ref.
		     OverflowException,     // Integer overflow in add or sub.
		     IllegalInstrException, // Unimplemented or reserved instr.
		     
		     NumExceptionTypes
};

#define StackReg	29
#define RetAddrReg	31
#define NumGPRegs	32
#define HiReg		32
#define LoReg		33
#define PCReg		34
#define NextPCReg	35
#define PrevPCReg	36
#define LoadReg		37	// The register target of a delayed load.
#define LoadValueReg 	38	// The value to be loaded by a delayed load.
#define BadVAddrReg	39
#define NumTotalRegs 	40 

class Machine {
  public:
    Machine(bool debug);

    void Run();	 	// Run a user program

    void OneInstruction(); // Do one instruction.
    void DelayedLoad(int nextReg, int nextVal);  // do pending load
    
    void RaiseException(ExceptionType which, int badVAddr);

    void Debugger();
    void DumpState();

    int ReadRegister(int num) {
	ASSERT((num >= 0) && (num < NumTotalRegs));
	return (registers[num]);
    }
    void WriteRegister(int num, int value) {
	ASSERT((num >= 0) && (num < NumTotalRegs));
	// DEBUG('m', "WriteRegister %d, value %d\n", num, value);
	registers[num] = value;
    }

    char mainMemory[MemorySize];	// public, for convenience
    int registers[NumTotalRegs];
    PageTable *pageTable;
    
  private:
    bool singleStep;
    int runUntilTime;
};

extern void ExceptionHandler(ExceptionType which);

#endif
