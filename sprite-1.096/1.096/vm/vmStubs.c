/* 
 * vmStubs.c --
 *
 *	Stubs for Unix compatible system calls.
 *
 * Copyright 1990 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/kernel/vm/RCS/vmStubs.c,v 1.2 91/04/08 13:02:48 shirriff Exp $";
#endif /* not lint */

#define MACH_UNIX_COMPAT

#include <sprite.h>
#include <stdio.h>
#include <status.h>
#include <errno.h>
#include <user/sys/types.h>
#include <mach.h>
#include <proc.h>
#include <timer.h>
#include <vm.h>
#include <vmInt.h>

#ifndef Mach_SetErrno
#define Mach_SetErrno(err)
#endif

int debugVmStubs;


/*
 *----------------------------------------------------------------------
 *
 * Vm_SbrkStub --
 *
 *	The stub for the "sbrk" Unix system call.
 *
 * Results:
 *	Returns -1 on failure.
 *
 * Side effects:
 *	Side effects associated with the system call.
 *	 
 *
 *----------------------------------------------------------------------
 */
int
Vm_SbrkStub(addr)
    Address	addr;
{
    Vm_Segment	        *segPtr;
    Address	        lastAddr;
    Proc_ControlBlock	*procPtr;

    if (debugVmStubs) {
	printf("Vm_SbrkStub(0x%x)\n", addr);
    }

    /*
     * The UNIX brk and sbrk call stubs figure where the end of the
     * heap is and they always call us with the new end of data segment.
     */
    procPtr = Proc_GetCurrentProc();
    segPtr = procPtr->vmPtr->segPtrArray[VM_HEAP];
    if (segPtr != (Vm_Segment *)NIL) {
	lastAddr =
		(Address) ((segPtr->offset + segPtr->numPages) * vm_PageSize);
	if (Vm_CreateVA(lastAddr, addr - lastAddr) == SUCCESS) {

	    if (debugVmStubs) {
		printf("Vm_SbrkStub addr = %x, lastAddr = %x, newAddr = %x\n",
		    addr, lastAddr,
		    (segPtr->offset + segPtr->numPages) * vm_PageSize);
	    }

	    return 0;
	}
    }

    if (debugVmStubs) {
	printf("Vm_SbrkStub Failed\n", addr);
    }

    Mach_SetErrno(ENOMEM);
    return -1;
}


/*
 *----------------------------------------------------------------------
 *
 * Vm_GetpagesizeStub --
 *
 *	The stub for the "getpagesize" Unix system call.
 *
 * Results:
 *	Returns -1 on failure.
 *
 * Side effects:
 *	Side effects associated with the system call.
 *	 
 *
 *----------------------------------------------------------------------
 */
int
Vm_GetpagesizeStub()
{

    if (debugVmStubs) {
	printf("Vm_GetpagesizeStub\n");
    }
    return vm_PageSize;
}

#define _MAP_NEW 0x80000000 /* SunOS new mode mmap flag */


/*
 *----------------------------------------------------------------------
 *
 * Vm_MmapStub --
 *
 *	The stub for the "mmap" Unix system call.
 *
 * Results:
 *	Returns -1 on failure.
 *
 * Side effects:
 *	Side effects associated with the system call.
 *	 
 *
 *----------------------------------------------------------------------
 */
int
Vm_MmapStub(addr, len, prot, share, fd, pos)
    caddr_t	addr;
    int	len, prot, share, fd;
    off_t	pos;
{
    ReturnStatus	status;
    Address		mappedAddr;

    status = Vm_MmapInt(addr, len, prot, share, fd, pos, &mappedAddr);
    if (status == SUCCESS) {
#if defined(ds3100) || defined(ds5000)
        return mappedAddr;
#else
        if (share && _MAP_NEW) {
	    return (int)mappedAddr;
	} else {
	    return 0;
	}
#endif
    } else {
	Mach_SetErrno(Compat_MapCode(status));
	return -1;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Vm_MunmapStub --
 *
 *	The stub for the "munmap" Unix system call.
 *
 * Results:
 *	Returns -1 on failure.
 *
 * Side effects:
 *	Side effects associated with the system call.
 *	 
 *
 *----------------------------------------------------------------------
 */
int
Vm_MunmapStub(addr, len)
    caddr_t	addr;
    int	len;
{
    ReturnStatus	status;

    status = Vm_Munmap(addr, len, 0);
    if (status == SUCCESS) {
	return 0;
    } else {
	Mach_SetErrno(Compat_MapCode(status));
	return -1;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Vm_MincoreStub --
 *
 *	The stub for the "mincore" Unix system call.
 *
 * Results:
 *	Returns -1 on failure.
 *
 * Side effects:
 *	Side effects associated with the system call.
 *	 
 *
 *----------------------------------------------------------------------
 */
int
Vm_MincoreStub(addr, len, vec)
    caddr_t	addr;
    int len;
    char vec[];
{
    ReturnStatus	status;

    status = Vm_Mincore(addr, len, vec);
    if (status == SUCCESS) {
	return 0;
    } else {
	Mach_SetErrno(Compat_MapCode(status));
	return -1;
    }
}

