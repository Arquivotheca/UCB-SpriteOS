// exception.cc -- stub to handle user mode exceptions, including system calls
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "machine.h"
#include "interrupt.h"
#include "syscall.h"

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(4);	

    // for system calls
    // type is in r4, arg1 is in r5, arg2 is in r6, and arg3 is in r7
    // put result in r2
    // and don't forget to increment the pc before returning!

    if ((which == SyscallException) && (type == SC_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    } else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}
