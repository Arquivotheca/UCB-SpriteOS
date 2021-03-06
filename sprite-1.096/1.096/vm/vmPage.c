/* 
 * vmPage.c --
 *
 *      This file contains routines that manage the core map, allocate
 *      list, free list, dirty list and reserve page list.  The core map
 *	contains one entry for each page frame in physical memory.  The four
 *	lists run through the core map.  The dirty list contains all pages
 *	that are being written to disk.  Dirty pages are written out by
 *	a set of pageout processes.  Pages are put onto the dirty list by the
 *	page allocation routine.  The allocate list contains all pages that
 *	are being used by user processes and are not on the dirty list.  It is
 *	kept in approximate LRU order by a version of the clock algorithm. 
 *	The free list contains pages that aren't being used by any user
 *	processes or the kernel.  The reserve list is a few pages
 *	that are set aside for emergencies when the kernel needs memory but 
 *	all of memory is dirty.
 *
 *	LOCKING PAGES
 *
 *	In general all pages that are on the allocate page list are eligible 
 *	to be given to any process.  However, if a page needs to be locked down
 *	so that it cannot be taken away from its owner, there is a lock count
 *	field in the core map entry for a page frame to allow this.  As long
 *	as the lock count is greater than zero, the page cannot be taken away
 *	from its owner.
 *	
 * Copyright (C) 1985 Regents of the University of California
 * All rights reserved.
 */

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/kernel/vm/RCS/vmPage.c,v 9.16 91/06/27 12:14:02 mgbaker Exp $ SPRITE (Berkeley)";
#endif not lint

#include <sprite.h>
#include <vmStat.h>
#include <vm.h>
#include <vmInt.h>
#include <vmTrace.h>
#include <vmSwapDir.h>
#include <user/vm.h>
#include <sync.h>
#include <dbg.h>
#include <list.h>
#include <timer.h>
#include <lock.h>
#include <sys.h>
#include <fscache.h>
#include <fsio.h>
#include <fsrmt.h>
#include <stdio.h>
#ifdef SOSP91
#include <fsStat.h>
#endif SOSP91

Boolean	vmDebug	= FALSE;

static	VmCore          *coreMap;	/* Pointer to core map that is 
					   allocated in VmCoreMapAlloc. */

/*
 * Minimum fraction of pages that VM wants for itself.  It keeps
 * 1 / MIN_VM_PAGE_FRACTION of the available pages at boot time for itself.
 */
#define	MIN_VM_PAGE_FRACTION	16

/*
 * Variables to define the number of page procs working at a time and the
 * maximum possible number that can be working at a time.
 */
static	int	numPageOutProcs = 0;
int		vmMaxPageOutProcs = VM_MAX_PAGE_OUT_PROCS;

/*
 * Page lists.  There are four different lists and a page can be on at most
 * one list.  The allocate list is a list of in use pages that is kept in
 * LRU order. The dirty list is a list of in use pages that are being
 * written to swap space.  The free list is a list of pages that are not
 * being used by any process.  The reserve list is a list with
 * NUM_RESERVE_PAGES on it that is kept for the case when the kernel needs
 * new memory but all of memory is dirty.
 */
#define	NUM_RESERVE_PAGES	3
static	List_Links      allocPageListHdr;
static	List_Links      dirtyPageListHdr;
static	List_Links	freePageListHdr;
static	List_Links	reservePageListHdr;
#define allocPageList	(&allocPageListHdr)
#define dirtyPageList	(&dirtyPageListHdr)
#define	freePageList	(&freePageListHdr)
#define	reservePageList	(&reservePageListHdr)

/*
 * Condition to wait for a clean page to be put onto the allocate list.
 */
Sync_Condition	cleanCondition;	

/*
 * Variables to allow recovery.
 */
static	Boolean		swapDown = FALSE;
Sync_Condition	swapDownCondition;

/*
 * Maximum amount of pages that can be on the dirty list before waiting for
 * a page to be cleaned.  It is a function of the amount of free memory at
 * boot time.
 */
#define	MAX_DIRTY_PAGE_FRACTION	4
int	vmMaxDirtyPages;

Boolean	vmFreeWhenClean = TRUE;	
Boolean	vmAlwaysRefuse = FALSE;	
Boolean	vmAlwaysSayYes = FALSE;	
Boolean	vmWriteablePageout = FALSE;
Boolean vmWriteableRefPageout = FALSE;

int	vmFSPenalty = 0;
int	vmNumPageGroups = 10;
int	vmPagesPerGroup;
int	vmCurPenalty;
int	vmBoundary;
Boolean	vmCORReadOnly = FALSE;

/*
 * Limit to put on the number of pages the machine can have.  Used for
 * benchmarking purposes only.
 */
int	vmPhysPageLimit = -1;

static void PageOut _ARGS_((ClientData data, Proc_CallInfo *callInfoPtr));
static void PutOnReserveList _ARGS_((register VmCore *corePtr));
static void PutOnFreeList _ARGS_((register VmCore *corePtr));


/*
 * ----------------------------------------------------------------------------
 *
 * VmCoreMapAlloc --
 *
 *     	Allocate space for the core map.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Core map allocated.
 * ----------------------------------------------------------------------------
 */
void
VmCoreMapAlloc()
{
    if (vmPhysPageLimit > 0 && vmPhysPageLimit < vmStat.numPhysPages) {
	vmStat.numPhysPages = vmPhysPageLimit;
    }
    printf("Available memory %d\n", vmStat.numPhysPages * vm_PageSize);
    coreMap = (VmCore *) Vm_BootAlloc(sizeof(VmCore) * vmStat.numPhysPages);
    bzero((char *) coreMap, sizeof(VmCore) * vmStat.numPhysPages);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmCoreMapInit --
 *
 *     	Initialize the core map.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Core map initialized.
 * ----------------------------------------------------------------------------
 */
void
VmCoreMapInit()
{
    register	int	i;
    register	VmCore	*corePtr;
    int			firstKernPage;

    /*   
     * Initialize the allocate, dirty, free and reserve lists.
     */
    List_Init(allocPageList);
    List_Init(dirtyPageList);
    List_Init(freePageList);
    List_Init(reservePageList);

    firstKernPage = (unsigned int)mach_KernStart >> vmPageShift;
    /*
     * Initialize the core map.  All pages up to vmFirstFreePage are
     * owned by the kernel and the rest are free.
     */
    for (i = 0, corePtr = coreMap; i < vmFirstFreePage; i++, corePtr++) {
	corePtr->links.nextPtr = (List_Links *) NIL;
	corePtr->links.prevPtr = (List_Links *) NIL;
        corePtr->lockCount = 1;
	corePtr->wireCount = 0;
        corePtr->flags = 0;
        corePtr->virtPage.segPtr = vm_SysSegPtr;
        corePtr->virtPage.page = i + firstKernPage;
	corePtr->virtPage.sharedPtr = (Vm_SegProcList *) NIL;
    }
    /*
     * The first NUM_RESERVED_PAGES are put onto the reserve list.
     */
    for (i = vmFirstFreePage, vmStat.numReservePages = 0;
         vmStat.numReservePages < NUM_RESERVE_PAGES;
	 i++, corePtr++) {
	corePtr->links.nextPtr = (List_Links *) NIL;
	corePtr->links.prevPtr = (List_Links *) NIL;
	corePtr->virtPage.sharedPtr = (Vm_SegProcList *) NIL;
	PutOnReserveList(corePtr);
    }
    /*
     * The remaining pages are put onto the free list.
     */
    for (vmStat.numFreePages = 0; i < vmStat.numPhysPages; i++, corePtr++) {
	corePtr->links.nextPtr = (List_Links *) NIL;
	corePtr->links.prevPtr = (List_Links *) NIL;
	corePtr->virtPage.sharedPtr = (Vm_SegProcList *) NIL;
	PutOnFreeList(corePtr);
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 *	Routines to manage the four lists.
 *
 * ----------------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------------------------
 *
 * PutOnAllocListFront --
 *
 *     	Put this core map entry onto the front of the allocate list.  
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Alloc lists modified and core map entry modified.
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
PutOnAllocListFront(corePtr)
    register	VmCore	*corePtr;
{
    VmListInsert((List_Links *) corePtr, LIST_ATFRONT(allocPageList));
    vmStat.numUserPages++;
}


/*
 * ----------------------------------------------------------------------------
 *
 * PutOnAllocListRear --
 *
 *     	Put this core map entry onto the rear of the allocate list
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Alloc list modified and core map entry modified.
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
PutOnAllocListRear(corePtr)
    VmCore	*corePtr;
{
    VmListInsert((List_Links *) corePtr, LIST_ATREAR(allocPageList));
    vmStat.numUserPages++;
}


/*
 * ----------------------------------------------------------------------------
 *
 * PutOnAllocList --
 *
 *     	Put the given core map entry onto the end of the allocate list.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	Core map entry put onto end of allocate list.
 *
 * ----------------------------------------------------------------------------
 */

ENTRY static void
PutOnAllocList(virtAddrPtr, page)
    Vm_VirtAddr		*virtAddrPtr;	/* The translated virtual address that 
					 * indicates the segment and virtual
					 * page that this physical page is
					 * being allocated for */
    unsigned int	page;
{
    register	VmCore	*corePtr; 
    Time		curTime;

    LOCK_MONITOR;

    Timer_GetTimeOfDay(&curTime, (int *) NIL, (Boolean *) NIL);

    corePtr = &coreMap[page];

    /*
     * Move the page to the end of the allocate list and initialize the core 
     * map entry.  If page is for a kernel process then don't put it onto
     * the end of the allocate list.
     */
    if (virtAddrPtr->segPtr != vm_SysSegPtr) {
	PutOnAllocListRear(corePtr);
    }

    corePtr->virtPage = *virtAddrPtr;
    corePtr->flags = 0;
    corePtr->lockCount = 1;
    corePtr->lastRef = curTime.seconds;

    UNLOCK_MONITOR;
}


/*
 * ----------------------------------------------------------------------------
 *
 * TakeOffAllocList --
 *
 *     	Take this core map entry off of the allocate list
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Alloc list modified and core map entry modified.
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
TakeOffAllocList(corePtr)
    VmCore	*corePtr;
{
    VmListRemove((List_Links *) corePtr);
    vmStat.numUserPages--;
}


/*
 * ----------------------------------------------------------------------------
 *
 * PutOnReserveList --
 *
 *     	Put this core map entry onto the reserve page list.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Reserve list modified and core map entry modified.
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
PutOnReserveList(corePtr)
    register	VmCore	*corePtr;
{
    corePtr->flags = 0;
    corePtr->lockCount = 1;
    VmListInsert((List_Links *) corePtr, LIST_ATREAR(reservePageList));
    vmStat.numReservePages++;
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmGetReservePage --
 *
 *     	Take a core map entry off of the reserve list and return its 
 *	page frame number.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Reserve list modified and core map entry modified.
 * ----------------------------------------------------------------------------
 */
INTERNAL unsigned int
VmGetReservePage(virtAddrPtr)
    Vm_VirtAddr	*virtAddrPtr;
{
    VmCore	*corePtr;

    if (List_IsEmpty(reservePageList)) {
	return(VM_NO_MEM_VAL);
    }
    printf("Taking from reserve list\n");
    vmStat.reservePagesUsed++;
    corePtr = (VmCore *) List_First(reservePageList);
    List_Remove((List_Links *) corePtr);
    vmStat.numReservePages--;
    corePtr->virtPage = *virtAddrPtr;

    return(corePtr - coreMap);
}


/*
 * ----------------------------------------------------------------------------
 *
 * PutOnFreeList --
 *
 *     	Put this core map entry onto the free list.  The page will actually
 *	end up on the reserve list if the reserve list needs more pages.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Free list or reserve list modified and core map entry modified.
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
PutOnFreeList(corePtr)
    register	VmCore	*corePtr;
{
    if (vmStat.numReservePages < NUM_RESERVE_PAGES) {
	PutOnReserveList(corePtr);
    } else {
	corePtr->flags = VM_FREE_PAGE;
	corePtr->lockCount = 0;
	VmListInsert((List_Links *) corePtr, LIST_ATREAR(freePageList));
	vmStat.numFreePages++;
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * TakeOffFreeList --
 *
 *     	Take this core map entry off of the free list.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Free list modified and core map entry modified.
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
TakeOffFreeList(corePtr)
    VmCore	*corePtr;
{
    VmListRemove((List_Links *) corePtr);
    vmStat.numFreePages--;
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmPutOnFreePageList --
 *
 *      Put the given page frame onto the free list.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	None.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL void
VmPutOnFreePageList(pfNum)
    unsigned	int	pfNum;		/* The page frame to be freed. */
{
    if (pfNum == 0) {
	/*
	 * Page frame number 0 is special because a page frame of 0 on a
	 * user page fault has special meaning.  Thus if the kernel decides
	 * to free page frame 0 then we can't make this page eligible for user
	 * use.  Instead of throwing it away put it onto the reserve list
	 * because only the kernel uses pages on the reserve list.
	 */
	PutOnReserveList(&coreMap[pfNum]);
    } else {
	PutOnFreeList(&coreMap[pfNum]);
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * PutOnDirtyList --
 *
 *	Put the given core map entry onto the dirty list and wakeup the page
 *	out daemon.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	Page added to dirty list, number of dirty pages is incremented and 
 *	number of active page out processes may be incremented.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
PutOnDirtyList(corePtr)
    register	VmCore	*corePtr;
{
    vmStat.numDirtyPages++;
    VmListInsert((List_Links *) corePtr, LIST_ATREAR(dirtyPageList));
    corePtr->flags |= VM_DIRTY_PAGE;
    if (vmStat.numDirtyPages - numPageOutProcs > 0 &&
	numPageOutProcs < vmMaxPageOutProcs) { 
	Proc_CallFunc(PageOut, (ClientData) numPageOutProcs, 0);
	numPageOutProcs++;
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * TakeOffDirtyList --
 *
 *     	Take this core map entry off of the dirty list.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Dirty list modified and core map entry modified.
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
TakeOffDirtyList(corePtr)
    VmCore	*corePtr;
{
    VmListRemove((List_Links *) corePtr);
    vmStat.numDirtyPages--;
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmPutOnDirtyList --
 *
 *     	Put the given page onto the front of the dirty list.  It is assumed
 *	the page is currently on either the allocate list or the dirty list.
 *	In either case mark the page such that it will not get freed until
 *	it is written out.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	The dirty list is modified.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmPutOnDirtyList(pfNum)
    unsigned	int	pfNum;
{
    register	VmCore	*corePtr; 

    LOCK_MONITOR;

    corePtr = &(coreMap[pfNum]);
    if (!(corePtr->flags & VM_DIRTY_PAGE)) {
	TakeOffAllocList(corePtr);
	PutOnDirtyList(corePtr);
    }
    corePtr->flags |= VM_DONT_FREE_UNTIL_CLEAN;

    UNLOCK_MONITOR;
}


/*
 * ----------------------------------------------------------------------------
 *
 *	Routines to validate and invalidate pages.
 *
 * ----------------------------------------------------------------------------
 */


/*
 * ----------------------------------------------------------------------------
 *
 * VmPageValidate --
 *
 *     	Validate the page at the given virtual address.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	None.
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmPageValidate(virtAddrPtr)
    Vm_VirtAddr	*virtAddrPtr;
{
    LOCK_MONITOR;

    VmPageValidateInt(virtAddrPtr, 
		      VmGetAddrPTEPtr(virtAddrPtr, virtAddrPtr->page));

    UNLOCK_MONITOR;
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmPageValidateInt --
 *
 *     	Validate the page at the given virtual address.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Page table modified.
 * ----------------------------------------------------------------------------
 */
INTERNAL void
VmPageValidateInt(virtAddrPtr, ptePtr)
    Vm_VirtAddr		*virtAddrPtr;
    register	Vm_PTE	*ptePtr;
{
    if  (!(*ptePtr & VM_PHYS_RES_BIT)) {
	virtAddrPtr->segPtr->resPages++;
	*ptePtr |= VM_PHYS_RES_BIT;
    }
    VmMach_PageValidate(virtAddrPtr, *ptePtr);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmPageInvalidate --
 *
 *     	Invalidate the page at the given virtual address.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	None.
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmPageInvalidate(virtAddrPtr)
    register	Vm_VirtAddr	*virtAddrPtr;
{
    LOCK_MONITOR;

    VmPageInvalidateInt(virtAddrPtr, 
	VmGetAddrPTEPtr(virtAddrPtr, virtAddrPtr->page));

    UNLOCK_MONITOR;
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmPageInvalidateInt --
 *
 *     	Invalidate the page at the given virtual address.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	Page table modified.
 * ----------------------------------------------------------------------------
 */
INTERNAL void
VmPageInvalidateInt(virtAddrPtr, ptePtr)
    Vm_VirtAddr		*virtAddrPtr;
    register	Vm_PTE	*ptePtr;
{
    if (*ptePtr & VM_PHYS_RES_BIT) {
	virtAddrPtr->segPtr->resPages--;
	VmMach_PageInvalidate(virtAddrPtr, Vm_GetPageFrame(*ptePtr), FALSE);
	*ptePtr &= ~(VM_PHYS_RES_BIT | VM_PAGE_FRAME_FIELD);
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 *	Routines to lock and unlock pages.
 *
 * ----------------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------------------------
 *
 * VmLockPageInt --
 *
 *     	Increment the lock count on a page.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	The core map entry for the page has its lock count incremented.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL void
VmLockPageInt(pfNum)
    unsigned	int		pfNum;
{
    coreMap[pfNum].lockCount++;
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmUnlockPage --
 *
 *     	Decrement the lock count on a page.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	The core map entry for the page has its lock count decremented.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmUnlockPage(pfNum)
    unsigned	int	pfNum;
{
    LOCK_MONITOR;
    coreMap[pfNum].lockCount--;
    if (coreMap[pfNum].lockCount < 0) {
	panic("VmUnlockPage: Coremap lock count < 0\n");
    }
    UNLOCK_MONITOR;
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmUnlockPageInt --
 *
 *     	Decrement the lock count on a page.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	The core map entry for the page has its lock count decremented.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL void
VmUnlockPageInt(pfNum)
    unsigned	int	pfNum;
{
    coreMap[pfNum].lockCount--;
    if (coreMap[pfNum].lockCount < 0) {
	panic("VmUnlockPage: Coremap lock count < 0\n");
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmPageSwitch --
 *
 *     	Move the given page from the current owner to the new owner.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *	The segment pointer int the core map entry for the page is modified and
 *	the page is unlocked.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL void
VmPageSwitch(pageNum, newSegPtr)
    unsigned	int	pageNum;
    Vm_Segment		*newSegPtr;
{
    coreMap[pageNum].virtPage.segPtr = newSegPtr;
    coreMap[pageNum].lockCount--;
}


/*
 * ----------------------------------------------------------------------------
 *
 *	Routines to get reference times of VM pages.
 *
 * ----------------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------------------------
 *
 * Vm_GetRefTime --
 *
 *     	Return the age of the LRU page (0 if is a free page).
 *
 * Results:
 *     	Age of LRU page (0 if there is a free page).
 *
 * Side effects:
 *     	None.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY int
Vm_GetRefTime()
{
    register	VmCore	*corePtr; 
    int			refTime;

    LOCK_MONITOR;

    vmStat.fsAsked++;

    if (swapDown || (vmStat.numFreePages + vmStat.numUserPages + 
		     vmStat.numDirtyPages <= vmStat.minVMPages)) {
	/*
	 * We are already at or below the minimum amount of memory that
	 * we are guaranteed for our use so refuse to give any memory to
	 * the file system.
	 */
	UNLOCK_MONITOR;
	return((int) 0x7fffffff);
    }

    if (!List_IsEmpty(freePageList)) {
	vmStat.haveFreePage++;
	refTime = 0;
	if (vmDebug) {
	    printf("Vm_GetRefTime: VM has free page\n");
	}
    } else {
	refTime = (int) 0x7fffffff;
	if (!List_IsEmpty(dirtyPageList)) {
	    corePtr = (VmCore *) List_First(dirtyPageList);
	    refTime = corePtr->lastRef;
	}
	if (!List_IsEmpty(allocPageList)) {
	    corePtr = (VmCore *) List_First(allocPageList);
	    if (corePtr->lastRef < refTime) {
		refTime = corePtr->lastRef;
	    }
	}
	if (vmDebug) {
	    printf("Vm_GetRefTime: Reftime = %d\n", refTime);
	}
    }

    if (vmAlwaysRefuse) {
	refTime = INT_MAX;
    } else if (vmAlwaysSayYes) {
	refTime = 0;
    } else {
	refTime += vmCurPenalty;
    }

    UNLOCK_MONITOR;

    return(refTime);
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetRefTime --
 *
 *     	Return either the first free page on the allocate list or the
 *	last reference time of the first page on the list.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	First page removed from allocate list if one is free.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY static void
GetRefTime(refTimePtr, pagePtr)
    register	int	*refTimePtr;
    unsigned	int	*pagePtr;
{
    register	VmCore	*corePtr; 

    LOCK_MONITOR;

    if (!List_IsEmpty(freePageList)) {
	vmStat.gotFreePage++;
	corePtr = (VmCore *) List_First(freePageList);
	TakeOffFreeList(corePtr);
	*pagePtr = corePtr - coreMap;
    } else {
	*refTimePtr = (int) 0x7fffffff;
	if (!List_IsEmpty(dirtyPageList)) {
	    corePtr = (VmCore *) List_First(dirtyPageList);
	    *refTimePtr = corePtr->lastRef;
	}
	if (!List_IsEmpty(allocPageList)) {
	    corePtr = (VmCore *) List_First(allocPageList);
	    if (corePtr->lastRef < *refTimePtr) {
		*refTimePtr = corePtr->lastRef;
	    }
	}
	*pagePtr = VM_NO_MEM_VAL;
    }

    UNLOCK_MONITOR;
}

/*
 * ----------------------------------------------------------------------------
 *
 *	Routines to allocate pages.
 *
 * ----------------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------------------------
 *
 * DoPageAllocate --
 *
 *     	Grab the monitor lock and call VmPageAllocate.
 *
 * Results:
 *     	The virtual page number that is allocated.
 *
 * Side effects:
 *     	None.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY static unsigned	int
DoPageAllocate(virtAddrPtr, flags)
    Vm_VirtAddr	*virtAddrPtr;	/* The translated virtual address that 
				   indicates the segment and virtual page 
				   that this physical page is being allocated 
				   for */
    int		flags;		/* VM_CAN_BLOCK | VM_ABORT_WHEN_DIRTY */
{
    unsigned	int page;

    LOCK_MONITOR;

    while (swapDown) {
	(void)Sync_Wait(&swapDownCondition, FALSE);
    }
    page = VmPageAllocateInt(virtAddrPtr, flags);

    UNLOCK_MONITOR;
    return(page);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmPageAllocate --
 *
 *     	Return a page frame.  Will either get a page from VM or FS depending
 *	on the LRU comparison and if there is a free page or not.
 *
 * Results:
 *     	The page frame number that is allocated.
 *
 * Side effects:
 *     	None.
 *
 * ----------------------------------------------------------------------------
 */
unsigned int
VmPageAllocate(virtAddrPtr, flags)
    Vm_VirtAddr	*virtAddrPtr;	/* The translated virtual address that this
				 * page frame is being allocated for */
    int		flags;		/* VM_CAN_BLOCK | VM_ABORT_WHEN_DIRTY. */
{
    unsigned	int	page;
    int			refTime;
    int			tPage;

    vmStat.numAllocs++;

    GetRefTime(&refTime, &page);
    if (page == VM_NO_MEM_VAL) {
	Fscache_GetPageFromFS(refTime + vmCurPenalty, &tPage);
	if (tPage == -1) {
	    vmStat.pageAllocs++;
	    return(DoPageAllocate(virtAddrPtr, flags));
	} else {
	    page = tPage;
	    vmStat.gotPageFromFS++;
	    if (vmDebug) {
		printf("VmPageAllocate: Took page from FS (refTime = %d)\n",
			    refTime);
	    }
	}
    }

    /*
     * Move the page to the end of the allocate list and initialize the core 
     * map entry.
     */
    PutOnAllocList(virtAddrPtr, page);

    return(page);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmPageAllocateInt --
 *
 *     	This routine will return the page frame number of the first free or
 *     	unreferenced, unmodified, unlocked page that it can find on the 
 *	allocate list.  The core map entry for this page will be initialized to 
 *	contain the virtual page number and the lock count will be set to 
 *	1 to indicate that this page is locked down.
 *
 *	This routine will sleep if the entire allocate list is dirty in order
 *	to give the page-out daemon some time to clean pages.
 *
 * Results:
 *     	The physical page number that is allocated.
 *
 * Side effects:
 *     	The allocate list is modified and the  dirty list may be modified.
 *	In addition the appropriate core map entry is initialized.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL unsigned int
VmPageAllocateInt(virtAddrPtr, flags)
    Vm_VirtAddr	*virtAddrPtr;	/* The translated virtual address that 
				   this page frame is being allocated for */
    int		flags;		/* VM_CAN_BLOCK if can block if non memory is
				 * available. VM_ABORT_WHEN_DIRTY if should
				 * abort even if VM_CAN_BLOCK is set if have
				 * exceeded the maximum number of dirty pages
				 * on the dirty list. */
{
    register	VmCore	*corePtr; 
    register	Vm_PTE	*ptePtr;
    Time		curTime;
    List_Links		endMarker;
    Boolean		referenced;
    Boolean		modified;

    Timer_GetTimeOfDay(&curTime, (int *) NIL, (Boolean *) NIL);

    vmStat.numListSearches++;

again:
    if (!List_IsEmpty(freePageList)) {
	corePtr = (VmCore *) List_First(freePageList);
	TakeOffFreeList(corePtr);
	vmStat.usedFreePage++;
    } else {
	/*
	 * Put a marker at the end of the core map so that we can detect loops.
	 */
	endMarker.nextPtr = (List_Links *) NIL;
	endMarker.prevPtr = (List_Links *) NIL;
	VmListInsert(&endMarker, LIST_ATREAR(allocPageList));

	/*
	 * Loop examining the page on the front of the allocate list until 
	 * a free or unreferenced, unmodified, unlocked page frame is found.
	 * If the whole list is examined and nothing found, then return 
	 * VM_NO_MEM_VAL.
	 */
	while (TRUE) {
	    corePtr = (VmCore *) List_First(allocPageList);

	    /*
	     * See if have gone all of the way through the list without finding
	     * anything.
	     */
	    if (((flags & (VM_CAN_BLOCK | VM_ABORT_WHEN_DIRTY)) && 
	         vmStat.numDirtyPages > vmMaxDirtyPages) ||
	        corePtr == (VmCore *) &endMarker) {	
		VmListRemove((List_Links *) &endMarker);
		if (!(flags & VM_CAN_BLOCK)) {
		    return(VM_NO_MEM_VAL);
		} else {
		    /*
		     * There were no pages available.  Wait for a clean
		     * page to appear on the allocate list.
		     */
		    (void)Sync_Wait(&cleanCondition, FALSE);
		    goto again;
		}
	    }
    
	    /*
	     * Make sure that the page is not locked down.
	     */
	    if (corePtr->lockCount > 0) {
		vmStat.lockSearched++;
		VmListMove((List_Links *) corePtr, LIST_ATREAR(allocPageList));
		continue;
	    }

	    ptePtr = VmGetAddrPTEPtr(&corePtr->virtPage,
				 corePtr->virtPage.page);
	    referenced = *ptePtr & VM_REFERENCED_BIT;
	    modified = *ptePtr & VM_MODIFIED_BIT;
	    VmMach_AllocCheck(&corePtr->virtPage, Vm_GetPageFrame(*ptePtr),
			      &referenced, &modified);
	    /*
	     * Now make sure that the page has not been referenced.  It it has
	     * then clear the reference bit and put it onto the end of the
	     * allocate list.
	     */
	    if (referenced) {
		vmStat.refSearched++;
		corePtr->lastRef = curTime.seconds;
		*ptePtr &= ~VM_REFERENCED_BIT;
		VmMach_ClearRefBit(&corePtr->virtPage,
				   Vm_GetPageFrame(*ptePtr));
		if (vmWriteableRefPageout &&
		    corePtr->virtPage.segPtr->type != VM_CODE) {
		    *ptePtr |= VM_MODIFIED_BIT;
		}

		VmListMove((List_Links *) corePtr, LIST_ATREAR(allocPageList));
		/*
		 * Set the last page marker so that we will try to examine this
		 * page again if we go all the way around without finding 
		 * anything.  
		 *
		 * NOTE: This is only a uni-processor solution since
		 *       on a multi-processor a process could be continually 
		 *       touching pages while we are scanning the list.
		 */
		VmListMove(&endMarker, LIST_ATREAR(allocPageList));
		continue;
	    }

	    if (corePtr->virtPage.segPtr->type != VM_CODE) {
		vmStat.potModPages++;
	    }
	    /*
	     * The page is available and it has not been referenced.  Now
	     * it must be determined if it is dirty.
	     */
	    if (modified) {
		/*
		 * Dirty pages go onto the dirty list.
		 */
		vmStat.dirtySearched++;
		TakeOffAllocList(corePtr);
		PutOnDirtyList(corePtr);
		continue;
	    }

	    if (corePtr->virtPage.segPtr->type != VM_CODE) {
		vmStat.notModPages++;
	    }
	    /*
	     * We can take this page.  Invalidate the page for the old segment.
	     * VmMach_AllocCheck will have already invalidated the page for
	     * us in hardware.
	     */
	    corePtr->virtPage.segPtr->resPages--;
	    *ptePtr &= ~(VM_PHYS_RES_BIT | VM_PAGE_FRAME_FIELD);

	    TakeOffAllocList(corePtr);
	    VmListRemove(&endMarker);
	    break;
	}
    }

    /*
     * If this page is being allocated for the kernel segment then don't put
     * it back onto the allocate list because kernel pages don't exist on
     * the allocate list.  Otherwise move it to the rear of the allocate list.
     */
    if (virtAddrPtr->segPtr != vm_SysSegPtr) {
	PutOnAllocListRear(corePtr);
    }
    corePtr->virtPage = *virtAddrPtr;
    corePtr->flags = 0;
    corePtr->lockCount = 1;
    corePtr->wireCount = 0;
    corePtr->lastRef = curTime.seconds;

    return(corePtr - coreMap);
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmPageFreeInt --
 *
 *      This routine will put the given page frame onto the front of the
 *      free list if it is not on the dirty list.  If the page frame is on 
 *	the dirty list then this routine will sleep until the page has been
 *	cleaned.  The page-out daemon will put the page onto the front of the
 *	allocate list when it finishes cleaning the page.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	The free list is modified and the core map entry is set to free
 *	with a lockcount of 0.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL void
VmPageFreeInt(pfNum)
    unsigned	int	pfNum;		/* The page frame to be freed. */
{
    register	VmCore	*corePtr; 

    corePtr = &(coreMap[pfNum]);

    corePtr->flags |= VM_FREE_PAGE;
    corePtr->lockCount = 0;

    if (corePtr->virtPage.segPtr == vm_SysSegPtr) {
        /*
	 * Pages given to the kernel are removed from the allocate list when
	 * they are allocated.  Therefore just put it back onto the free list.
	 */
	if (corePtr->flags & (VM_DIRTY_PAGE | VM_PAGE_BEING_CLEANED)) {
	    panic("VmPageFreeInt: Kernel page on dirty list\n");
	}
	PutOnFreeList(corePtr);
    } else {
	/*
	 * If the page is being written then wait for it to finish.
	 * Once it has been cleaned it will automatically be put onto the free
	 * list.  We must wait for it to be cleaned because 
	 * the segment may die otherwise while the page is still waiting to be 
	 * cleaned.  This would be a disaster because the page-out daemon uses
	 * the segment table entry to determine where to write the page.
	 */
	if (corePtr->flags & 
			(VM_PAGE_BEING_CLEANED | VM_DONT_FREE_UNTIL_CLEAN)) {
	    do {
		corePtr->flags |= VM_SEG_PAGEOUT_WAIT;
		vmStat.cleanWait++;
		(void) Sync_Wait(&corePtr->virtPage.segPtr->condition, FALSE);
	    } while (corePtr->flags & 
			(VM_PAGE_BEING_CLEANED | VM_DONT_FREE_UNTIL_CLEAN));
	} else {
	    if (corePtr->flags & VM_DIRTY_PAGE) {
		TakeOffDirtyList(corePtr);
	    } else {
		TakeOffAllocList(corePtr);
	    }
	    PutOnFreeList(corePtr);
	}
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmPageFree --
 *
 *      Free the given page.  Call VmPageFreeInt to do the work.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	None.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmPageFree(pfNum)
    unsigned	int	pfNum;		/* The page frame to be freed. */
{
    LOCK_MONITOR;

    VmPageFreeInt(pfNum);

    UNLOCK_MONITOR;
}


/*
 * ----------------------------------------------------------------------------
 *
 * Vm_ReservePage --
 *
 *      Take a page out of the available pages because this page is
 *	being used by the hardware dependent module.  This routine is
 *	called at boot time.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	None.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
Vm_ReservePage(pfNum)
    unsigned	int	pfNum;		/* The page frame to be freed. */
{
    register	VmCore	*corePtr;

    LOCK_MONITOR;

    if (pfNum < vmStat.numPhysPages) {
	corePtr = &coreMap[pfNum];
	TakeOffFreeList(corePtr);
	corePtr->virtPage.segPtr = vm_SysSegPtr;
	corePtr->flags = 0;
	corePtr->lockCount = 1;
    }

    UNLOCK_MONITOR;
}
	    

/*-----------------------------------------------------------------------
 *
 * 		Routines to handle page faults.
 *
 * Page fault handling is divided into three routines.  The first
 * routine is Vm_PageIn.  It calls two monitored routines PreparePage and
 * FinishPage to do most of the monitor level work.
 */

typedef enum {
    IS_COR,	/* This page is copy-on-reference. */
    IS_COW, 	/* This page is copy-on-write. */
    IS_DONE, 	/* The page-in has already completed. */
    NOT_DONE,	/* The page-in is not yet done yet. */
} PrepareResult;

static PrepareResult PreparePage _ARGS_((register Vm_VirtAddr *virtAddrPtr, Boolean protFault, register Vm_PTE *curPTEPtr));
static void FinishPage _ARGS_((register Vm_VirtAddr *transVirtAddrPtr, register Vm_PTE *ptePtr));


/*
 * ----------------------------------------------------------------------------
 *
 * Vm_PageIn --
 *
 *     This routine is called to read in the page at the given virtual address.
 *
 * Results:
 *     SUCCESS if the page-in was successful and FAILURE otherwise.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
ReturnStatus
Vm_PageIn(virtAddr, protFault)
    Address 	virtAddr;	/* The virtual address of the desired page */
    Boolean	protFault;	/* TRUE if fault is because of a protection
				 * violation. */
{
    register	Vm_PTE 		*ptePtr;
    register	Vm_Segment	*segPtr;
    register	int		page;
    Vm_VirtAddr	 		transVirtAddr;	
    ReturnStatus 		status;
    Proc_ControlBlock		*procPtr;
    unsigned	int		virtFrameNum;
    PrepareResult		result;
#ifdef SOSP91
    Boolean			isForeign = FALSE;
#endif SOSP91

#ifdef SOSP91
    if (proc_RunningProcesses[0] != (Proc_ControlBlock *) NIL) {
	if ((proc_RunningProcesses[0]->state == PROC_MIGRATED) ||
		(proc_RunningProcesses[0]->genFlags &
		(PROC_FOREIGN | PROC_MIGRATING))) {
	    isForeign = TRUE;
	}
    }
#endif SOSP91

    vmStat.totalFaults++;
#ifdef SOSP91
	if (isForeign) {
	    fs_MoreStats.totalFaultsM++;
	}
#endif SOSP91

    procPtr = Proc_GetCurrentProc();
    /*
     * Determine which segment this virtual address falls into.
     */
    VmVirtAddrParse(procPtr, virtAddr, &transVirtAddr);
    segPtr = transVirtAddr.segPtr;
    page = transVirtAddr.page;
    if (segPtr == (Vm_Segment *) NIL) {
	return(FAILURE);
    }
    if (segPtr->flags & VM_SEG_IO_ERROR) {
	/*
	 * Bad segment - disk full..  Go to pageinDone to clean up ptUserCount.
	 * If a process is wildly growing its stack we'll have the heap locked
	 * while we try to grow the stack, and we have to unlock the heap.
	 */
	status = FAILURE;
	goto pageinDone;
    }
    if ((protFault && ( segPtr->type == VM_CODE) ||
	    transVirtAddr.flags & VM_READONLY_SEG)) {
	/*
	 * Access violation.  Go to pageinDone to clean up ptUserCount.
	 */
	if (segPtr->type == VM_SHARED) {
	    dprintf("Vm_PageIn: access violation\n");
	}
	status = FAILURE;
	goto pageinDone;
    }

    /*
     * Make sure that the virtual address is within the allocated part of the
     * segment.  If not, then either return error if heap or code segment,
     * or automatically expand the stack if stack segment.
     */
    if (!VmCheckBounds(&transVirtAddr)) {
	if (segPtr->type == VM_STACK) {
	    int	lastPage;
	    /*
	     * If this is a stack segment, then automatically grow it.
	     */
	    lastPage = mach_LastUserStackPage - segPtr->numPages;
	    status = VmAddToSeg(segPtr, page, lastPage);
	    if (status != SUCCESS) {
		goto pageinDone;
	    }
	} else {
	    if (segPtr->type == VM_SHARED) {
		dprintf("Vm_PageIn: VmCheckBounds failure\n");
	    }
	    status = FAILURE;
	    goto pageinDone;
	}
    }

    switch (segPtr->type) {
	case VM_CODE:
	    vmStat.codeFaults++;
	    break;
	case VM_HEAP:
	    vmStat.heapFaults++;
	    break;
	case VM_STACK:
	    vmStat.stackFaults++;
	    break;
    }

    ptePtr = VmGetAddrPTEPtr(&transVirtAddr, page);

    if (protFault && (*ptePtr & VM_READ_ONLY_PROT) &&
	    !(*ptePtr & VM_COR_CHECK_BIT)) {
	status = FAILURE;
	goto pageinDone;
    }

    /*
     * Fetch the next page.
     */
    if (vmPrefetch) {
	VmPrefetch(&transVirtAddr, ptePtr + 1);
    }

    while (TRUE) {
	/*
	 * Do the first part of the page-in.
	 */
	result = PreparePage(&transVirtAddr, protFault, ptePtr);
	if (!vm_CanCOW && (result == IS_COR || result == IS_COW)) {
	    panic("Vm_PageIn: Bogus COW or COR\n");
	}
	if (result == IS_COR) {
	    status = VmCOR(&transVirtAddr);
	    if (status != SUCCESS) {
		if (segPtr->type == VM_SHARED) {
		    dprintf("Vm_PageIn: VmCOR failure\n");
		}
		status = FAILURE;
		goto pageinError;
	    }
	} else if (result == IS_COW) {
	    VmCOW(&transVirtAddr);
	} else {
	    break;
	}
    }
    if (result == IS_DONE) {
	status = SUCCESS;
	goto pageinDone;
    }

    /*
     * Allocate a page.
     */
    virtFrameNum = VmPageAllocate(&transVirtAddr, TRUE);
    *ptePtr |= virtFrameNum;

    /*
     * Call the appropriate routine to fill the page.
     */
    if (*ptePtr & VM_ZERO_FILL_BIT) {
	vmStat.zeroFilled++;
#ifdef SOSP91
	if (isForeign) {
	    fs_MoreStats.zeroFilledM++;
	}
#endif SOSP91
	VmZeroPage(virtFrameNum);
	*ptePtr |= VM_MODIFIED_BIT;
	status = SUCCESS;
	if (vm_Tracing) {
	    Vm_TracePageFault	faultRec;

	    faultRec.segNum = transVirtAddr.segPtr->segNum;
	    faultRec.pageNum = transVirtAddr.page;
	    faultRec.faultType = VM_TRACE_ZERO_FILL;
	    VmStoreTraceRec(VM_TRACE_PAGE_FAULT_REC, sizeof(faultRec),
			    (Address)&faultRec, TRUE);
	}
    } else if (*ptePtr & VM_ON_SWAP_BIT ||
	    transVirtAddr.segPtr->type == VM_SHARED) {
	vmStat.psFilled++;
#ifdef SOSP91
	if (isForeign) {
	    fs_MoreStats.psFilledM++;
	}
#endif SOSP91
	if (transVirtAddr.segPtr->type == VM_SHARED) {
	    dprintf("Vm_PageIn: paging in shared page %d\n",transVirtAddr.page);
	}
	status = VmPageServerRead(&transVirtAddr, virtFrameNum);
	if (vm_Tracing) {
	    Vm_TracePageFault	faultRec;

	    faultRec.segNum = transVirtAddr.segPtr->segNum;
	    faultRec.pageNum = transVirtAddr.page;
	    faultRec.faultType = VM_TRACE_SWAP_FILE;
	    VmStoreTraceRec(VM_TRACE_PAGE_FAULT_REC, sizeof(faultRec),
			    (Address)&faultRec, TRUE);
	}
    } else {
	vmStat.fsFilled++;
#ifdef SOSP91
	if (isForeign) {
	    fs_MoreStats.fsFilledM++;
	}
#endif SOSP91
	status = VmFileServerRead(&transVirtAddr, virtFrameNum);
	if (vm_Tracing && transVirtAddr.segPtr->type == VM_HEAP) {
	    Vm_TracePageFault	faultRec;

	    faultRec.segNum = transVirtAddr.segPtr->segNum;
	    faultRec.pageNum = transVirtAddr.page;
	    faultRec.faultType = VM_TRACE_OBJ_FILE;
	    VmStoreTraceRec(VM_TRACE_PAGE_FAULT_REC, sizeof(faultRec),
			    (Address)&faultRec, TRUE);
	}
    }

    *ptePtr |= VM_REFERENCED_BIT;
    if (vmWriteablePageout && transVirtAddr.segPtr->type != VM_CODE) {
	*ptePtr |= VM_MODIFIED_BIT;
    }

    /*
     * Finish up the page-in process.
     */
    FinishPage(&transVirtAddr, ptePtr);

    /*
     * Now check to see if the read succeeded.  If not destroy all processes
     * that are sharing the code segment.
     */
pageinError:
    if (status != SUCCESS) {
	if (transVirtAddr.segPtr->type == VM_SHARED) {
	    dprintf("Vm_PageIn: Page read failed.  Invalidating pages.\n");
	    VmPageFree(Vm_GetPageFrame(*ptePtr));
	    VmPageInvalidateInt(&transVirtAddr, ptePtr);
	} else {
	    VmKillSharers(segPtr);
	}
    }

pageinDone:

    if (transVirtAddr.flags & VM_HEAP_PT_IN_USE) {
	/*
	 * The heap segment has been made not expandable by VmVirtAddrParse
	 * so that the address parse would remain valid.  Decrement the
	 * in use count now.
	 */
	VmDecPTUserCount(procPtr->vmPtr->segPtrArray[VM_HEAP]);
    }

    return(status);
}


/*
 * ----------------------------------------------------------------------------
 *
 * PreparePage --
 *
 *	This routine performs the first half of the page-in process.
 *	It will return a status to the caller telling them what the status
 *	of the page is.
 *
 * Results:
 *	IS_DONE if the page is already resident in memory and it is not a 
 *	COW faults.  IS_COR is it is for a copy-on-reference fault.  IS_COW
 *	if is for a copy-on-write fault.  Otherwise returns NOT_DONE.
 *
 * Side effects:
 *	*ptePtrPtr is set to point to the page table entry for this virtual
 *	page.  In progress bit set if the NOT_DONE status is returned.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY static PrepareResult
PreparePage(virtAddrPtr, protFault, curPTEPtr)
    register Vm_VirtAddr *virtAddrPtr; 	/* The translated virtual address */
    Boolean		protFault;	/* TRUE if faulted because of a
					 * protection fault. */
    register	Vm_PTE	*curPTEPtr;	/* Page table pointer for the page. */
{
    PrepareResult	retVal;

    LOCK_MONITOR;

again:
    if (*curPTEPtr & VM_IN_PROGRESS_BIT) {
	/*
	 * The page is being faulted on by someone else.  In this case wait
	 * for the page fault to complete.
	 */
	vmStat.collFaults++;
	(void) Sync_Wait(&virtAddrPtr->segPtr->condition, FALSE);
	goto again;
    } else if (*curPTEPtr & VM_COR_BIT) {
	/*
	 * Copy-on-reference fault.
	 */
	retVal = IS_COR;
    } else if (protFault && (*curPTEPtr & VM_COW_BIT) && 
	       (*curPTEPtr & VM_PHYS_RES_BIT)) {
	/*
	 * Copy-on-write fault.
	 */
	retVal = IS_COW;
    } else if (*curPTEPtr & VM_PHYS_RES_BIT) {
	/*
	 * The page is already in memory.  Validate it in hardware and set
	 * the reference bit since we are about to reference it.
	 */
	if (protFault && (*curPTEPtr & VM_COR_CHECK_BIT)) {
	    if (virtAddrPtr->segPtr->type == VM_HEAP) {
		vmStat.numCORCOWHeapFaults++;
	    } else {
		vmStat.numCORCOWStkFaults++;
	    }
	    *curPTEPtr &= ~(VM_COR_CHECK_BIT | VM_READ_ONLY_PROT);
	} else {
	    /* 
	     * Remove "quick" faults from the per-segment counts, so 
	     * that the per-segment counts are more meaningful.
	     */
	    vmStat.quickFaults++;
	    switch (virtAddrPtr->segPtr->type) {
	    case VM_CODE:
		vmStat.codeFaults--;
		break;
	    case VM_HEAP:
		vmStat.heapFaults--;
		break;
	    case VM_STACK:
		vmStat.stackFaults--;
		break;
	    }
	}
	if (*curPTEPtr & VM_PREFETCH_BIT) {
	    switch (virtAddrPtr->segPtr->type) {
		case VM_CODE:
		    vmStat.codePrefetchHits++;
		    break;
		case VM_HEAP:
		    if (*curPTEPtr & VM_ON_SWAP_BIT) {
			vmStat.heapSwapPrefetchHits++;
		    } else {
			vmStat.heapFSPrefetchHits++;
		    }
		    break;
		case VM_STACK:
		    vmStat.stackPrefetchHits++;
		    break;
	    }
	    *curPTEPtr &= ~VM_PREFETCH_BIT;
	}
	VmPageValidateInt(virtAddrPtr, curPTEPtr);
	*curPTEPtr |= VM_REFERENCED_BIT;
        retVal = IS_DONE;
    } else {
	*curPTEPtr |= VM_IN_PROGRESS_BIT;
	retVal = NOT_DONE;
    }

    UNLOCK_MONITOR;
    return(retVal);
}


/*
 * ----------------------------------------------------------------------------
 *
 * FinishPage --
 *	This routine finishes the page-in process.  This includes validating
 *	the page for the currently executing process and releasing the 
 *	lock on the page.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Page-in in progress cleared and lockcount decremented in the
 * 	core map entry.
 *	
 *
 * ----------------------------------------------------------------------------
 */
ENTRY static void
FinishPage(transVirtAddrPtr, ptePtr) 
    register	Vm_VirtAddr	*transVirtAddrPtr;
    register	Vm_PTE		*ptePtr;
{
    LOCK_MONITOR;

    /*
     * Make the page accessible to the user.
     */
    VmPageValidateInt(transVirtAddrPtr, ptePtr);
    coreMap[Vm_GetPageFrame(*ptePtr)].lockCount--;
    *ptePtr &= ~(VM_ZERO_FILL_BIT | VM_IN_PROGRESS_BIT);
    /*
     * Wakeup processes waiting for this pagein to complete.
     */
    Sync_Broadcast(&transVirtAddrPtr->segPtr->condition);

    UNLOCK_MONITOR;
}


/*
 *----------------------------------------------------------------------
 *
 * KillCallback --
 *
 *	Send a process the SIG_KILL signal when an I/O error
 *	occurs.  This routine is a callback procedure used by
 *	VmKillSharers to perform signals without the vm monitor lock
 *	held.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The specified process is killed.
 *
 *----------------------------------------------------------------------
 */

static void
KillCallback(data)
    ClientData data;
{
    (void) Sig_Send(SIG_KILL, PROC_VM_READ_ERROR, (Proc_PID) data, FALSE,
	    (Address)0);
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmKillSharers --
 *
 *	Go down the list of processes sharing this segment and set up
 *	a callback to send a kill signal to each one without the
 *	monitor lock held.  This is called when a page from a segment
 *	couldn't be written to or read from swap space.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     All processes sharing this segment are destroyed.
 *	Marks the segment as having an I/O error.
 *
 * ----------------------------------------------------------------------------
 */

ENTRY void
VmKillSharers(segPtr) 
    register	Vm_Segment	*segPtr;
{
    register	VmProcLink	*procLinkPtr;

    LOCK_MONITOR;

    if ((segPtr->flags & VM_SEG_IO_ERROR) == 0) {
	LIST_FORALL(segPtr->procList, (List_Links *) procLinkPtr) {
	    Proc_CallFunc(KillCallback,
			  (ClientData) procLinkPtr->procPtr->processID,
			  0);
	}
    }
    segPtr->flags |= VM_SEG_IO_ERROR;

    UNLOCK_MONITOR;
}

static void PinPages _ARGS_((register Vm_VirtAddr *virtAddrPtr, register int lastPage));


/*
 * ----------------------------------------------------------------------------
 *
 * Vm_PinUserMem --
 *
 *      Hardwire pages for all user addresses between firstAddr and
 *	lastAddr.
 *
 * Results:
 *     SUCCESS if the page-in was successful and SYS_ARG_NO_ACCESS otherwise.
 *
 * Side effects:
 *     Pages between firstAddr and lastAddr are wired down in memory.
 *
 * ----------------------------------------------------------------------------
 */
ReturnStatus
Vm_PinUserMem(mapType, numBytes, addr)
    int		     mapType;	/* VM_READONLY_ACCESS | VM_READWRITE_ACCESS */
    int		     numBytes;	/* Number of bytes to map. */
    register Address addr;	/* Where to start mapping at. */
{
    Vm_VirtAddr	 		virtAddr;
    ReturnStatus		status = SUCCESS;
    int				firstPage;
    int				lastPage;
    Proc_ControlBlock		*procPtr;

    procPtr = Proc_GetCurrentProc();
    VmVirtAddrParse(procPtr, addr, &virtAddr);
    if (virtAddr.segPtr == (Vm_Segment *)NIL ||
	(virtAddr.segPtr->type == VM_CODE && mapType == VM_READWRITE_ACCESS)) {
	return(SYS_ARG_NOACCESS);
    }

    firstPage = virtAddr.page;
    lastPage = ((unsigned)addr + numBytes - 1) >> vmPageShift;
    while (virtAddr.page <= lastPage) {
	/*
	 * Loop until we got all of the pages locked down.  We have to
	 * loop because a page could get freed after we touch it but before
	 * we get a chance to wire it down.
	 */
	status = Vm_TouchPages(virtAddr.page, lastPage - virtAddr.page + 1);
	if (status != SUCCESS) {
	    goto done;
	}
	PinPages(&virtAddr, lastPage);
    }

    virtAddr.page = firstPage;
    VmMach_PinUserPages(mapType,  &virtAddr, lastPage);

done:
    if (virtAddr.flags & VM_HEAP_PT_IN_USE) {
	VmDecPTUserCount(procPtr->vmPtr->segPtrArray[VM_HEAP]);
    }
    return(status);
}


/*
 * ----------------------------------------------------------------------------
 *
 * PinPages --
 *
 *      Hardwire pages for all user addresses between firstAddr and
 *	lastAddr.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	virtAddrPtr->page is updated as we successfully wire down pages.  When
 *	we return its value will be of the last page that we successfully
 *	wired down + 1.
 *
 * ----------------------------------------------------------------------------
 */
static void
PinPages(virtAddrPtr, lastPage)
    register	Vm_VirtAddr	*virtAddrPtr;
    register	int		lastPage;
{
    register	VmCore	*corePtr;
    register	Vm_PTE	*ptePtr;

    LOCK_MONITOR;

    for (ptePtr = VmGetAddrPTEPtr(virtAddrPtr, virtAddrPtr->page);
         virtAddrPtr->page <= lastPage;
	 VmIncPTEPtr(ptePtr, 1), virtAddrPtr->page++) {
	while (*ptePtr & VM_IN_PROGRESS_BIT) {
	    (void)Sync_Wait(&virtAddrPtr->segPtr->condition, FALSE);
	}
	if (*ptePtr & VM_PHYS_RES_BIT) {
	    corePtr = &coreMap[Vm_GetPageFrame(*ptePtr)];
	    corePtr->wireCount++;
	    corePtr->lockCount++;
	} else {
	    break;
	}
    }

    UNLOCK_MONITOR;
}

static void UnpinPages _ARGS_((Vm_VirtAddr *virtAddrPtr, int lastPage));


/*
 * ----------------------------------------------------------------------------
 *
 * Vm_UnpinUserMem --
 *
 *      Unlock all pages between firstAddr and lastAddr.
 *	lastAddr.
 *
 * Results:
 *     SUCCESS if the page-in was successful and FAILURE otherwise.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
void
Vm_UnpinUserMem(numBytes, addr)
    int		numBytes;	/* The number of bytes to map. */
    Address 	addr;		/* The address to start mapping at. */
{
    Vm_VirtAddr	 		virtAddr;
    Proc_ControlBlock		*procPtr;
    int				lastPage;

    procPtr = Proc_GetCurrentProc();
    VmVirtAddrParse(procPtr, addr, &virtAddr);
    lastPage = (unsigned int)(addr + numBytes - 1) >> vmPageShift;
    /*
     * Now unlock all of the pages.
     */
    VmMach_UnpinUserPages(&virtAddr, lastPage);
    UnpinPages(&virtAddr, lastPage);

    if (virtAddr.flags & VM_HEAP_PT_IN_USE) {
	/*
	 * The heap segment has been made not expandable by VmVirtAddrParse
	 * so that the address parse would remain valid.  Decrement the
	 * in use count now.
	 */
	VmDecPTUserCount(procPtr->vmPtr->segPtrArray[VM_HEAP]);
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * UnpinPages --
 *
 *      Unlock pages for all user addresses between firstAddr and
 *	lastAddr.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Core map entry lock count and flags field may be modified.
 *
 * ----------------------------------------------------------------------------
 */
static void
UnpinPages(virtAddrPtr, lastPage)
    Vm_VirtAddr	*virtAddrPtr;
    int		lastPage;
{
    register	VmCore	*corePtr;
    register	Vm_PTE	*ptePtr;
    register	int	i;

    LOCK_MONITOR;

    for (i = virtAddrPtr->page,
		ptePtr = VmGetAddrPTEPtr(virtAddrPtr, virtAddrPtr->page);
	 i <= lastPage;
	 VmIncPTEPtr(ptePtr, 1), i++) {

	while (*ptePtr & VM_IN_PROGRESS_BIT) {
	    (void)Sync_Wait(&virtAddrPtr->segPtr->condition, FALSE);
	}

	if (*ptePtr & VM_PHYS_RES_BIT) {
	    corePtr = &coreMap[Vm_GetPageFrame(*ptePtr)];
	    if (corePtr->wireCount > 0) {
		corePtr->wireCount--;
		corePtr->lockCount--;
	    }
	}
    }

    UNLOCK_MONITOR;
}



/*
 * ----------------------------------------------------------------------------
 *
 * VmPagePinned --
 *
 *      Return TRUE if the page is wired down in memory and FALSE otherwise.
 *
 * Results:
 *     TRUE if the page is wired down and FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL Boolean
VmPagePinned(ptePtr)
    Vm_PTE	*ptePtr;
{
    return(coreMap[Vm_GetPageFrame(*ptePtr)].wireCount > 0);
}


/*----------------------------------------------------------------------------
 *
 * 		Routines for writing out dirty pages		
 *
 * Dirty pages are written to the swap file by the function PageOut.
 * PageOut is called by using the Proc_CallFunc routine which invokes
 * a process on PageOut.  When a page is put onto the dirty list a new
 * incantation of PageOut will be created unless there are already
 * more than vmMaxPageOutProcs already writing out the dirty list.  Thus the
 * dirty list will be cleaned by at most vmMaxPageOutProcs working in parallel.
 *
 * The work done by PageOut is split into work done at non-monitor level and
 * monitor level.  It calls the monitored routine PageOutPutAndGet to get the 
 * next page off of the dirty list.  It then writes the page out to the 
 * file server at non-monitor level.  Next it calls the monitored routine 
 * PageOutPutAndGet to put the page onto the front of the allocate list and
 * get the next dirty page.  Finally when there are no more pages to clean it
 * returns (and dies).
 */

static void PageOutPutAndGet _ARGS_((VmCore **corePtrPtr, ReturnStatus status, Boolean *doRecoveryPtr, Fs_Stream **recStreamPtrPtr));
static void PutOnFront _ARGS_((register VmCore *corePtr));

/*
 * ----------------------------------------------------------------------------
 *
 * PageOut --
 *
 *	Function to write out pages on dirty list.  It will keep retrieving
 *	pages from the dirty list until there are no more left.  This function
 *	is designed to be called through Proc_CallFunc.
 *	
 * Results:
 *     	None.
 *
 * Side effects:
 *     	The dirty list is emptied.
 *
 * ----------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
PageOut(data, callInfoPtr)
    ClientData		data;		/* Ignored. */
    Proc_CallInfo	*callInfoPtr;	/* Ignored. */
{
    VmCore		*corePtr;
    ReturnStatus	status = SUCCESS;
    Fs_Stream		*recoveryStreamPtr;
    Boolean		doRecovery;
    Boolean		returnSwapStream;

    vmStat.pageoutWakeup++;

    corePtr = (VmCore *) NIL;
    while (TRUE) {
	doRecovery = FALSE;
	PageOutPutAndGet(&corePtr, status, &doRecovery, &recoveryStreamPtr);
	if (doRecovery) {
	    /*
	     * The following shenanigans are used to carefully
	     * synchronize access to the swap directory stream.
	     */
	    returnSwapStream = FALSE;
	    if (recoveryStreamPtr == (Fs_Stream  *)NIL) {
		recoveryStreamPtr = VmGetSwapStreamPtr();
		if (recoveryStreamPtr != (Fs_Stream *)NIL) {
		    returnSwapStream = TRUE;
		}
	    }
	    if (recoveryStreamPtr != (Fs_Stream  *)NIL) {
		(void)Fsutil_WaitForHost(recoveryStreamPtr, FS_NON_BLOCKING,
					 status);
		if (returnSwapStream) {
		    VmDoneWithSwapStreamPtr();
		}
	    }
	}

	if (corePtr == (VmCore *) NIL) {
	    break;
	}
	status = VmPageServerWrite(&corePtr->virtPage, 
				   (unsigned int) (corePtr - coreMap),
				   FALSE);
	if (status != SUCCESS) {
	    if ( ! VmSwapStreamOk() ||
	        (status != RPC_TIMEOUT && status != FS_STALE_HANDLE &&
		 status != RPC_SERVICE_DISABLED)) {
		/*
		 * Non-recoverable error on page write, so kill all users of 
		 * this segment.
		 */
		VmKillSharers(corePtr->virtPage.segPtr);
	    }
	}
    }

}


/*
 * ----------------------------------------------------------------------------
 *
 * PageOutPutAndGet --
 *
 *	This routine does two things.  First it puts the page pointed to by
 *	*corePtrPtr (if any) onto the front of the allocate list and wakes
 *	up any dying processes waiting for this page to be cleaned.
 *	It then takes the first page off of the dirty list and returns a 
 *	pointer to it.  Before returning the pointer it clears the 
 *      modified bit of the page frame.
 *
 * Results:
 *     A pointer to the first page on the dirty list.  If there are no pages
 *     then *corePtrPtr is set to NIL.
 *
 * Side effects:
 *	The dirty list and allocate lists may both be modified.  In addition
 *      the onSwap bit is set to indicate that the page is now on swap space.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY static void
PageOutPutAndGet(corePtrPtr, status, doRecoveryPtr, recStreamPtrPtr)
    VmCore	 **corePtrPtr;		/* On input points to page frame
					 * to be put back onto allocate list.
					 * On output points to page frame
					 * to be cleaned. */
    ReturnStatus status;		/* Status from the write. */
    Boolean	*doRecoveryPtr;		/* Return.  TRUE if recovery should
					 * be attempted.  In this case check
					 * *recStreamPtrPtr and wait for
					 * recovery on that, or wait for
					 * recovery on vmSwapStreamPtr. */
    Fs_Stream	 **recStreamPtrPtr;	/* Pointer to stream to do recovery
					 * on if *doRecoveryPtr is TRUE.  If
					 * this is still NIL, then do recovery
					 * on vmSwapStreamPtr instead */
{
    register	Vm_PTE	*ptePtr;
    register	VmCore	*corePtr;

    LOCK_MONITOR;

    *doRecoveryPtr = FALSE;
    *recStreamPtrPtr = (Fs_Stream *)NIL;
    corePtr = *corePtrPtr;
    if (corePtr == (VmCore *)NIL) {
	if (swapDown) {
	    numPageOutProcs--;
	    UNLOCK_MONITOR;
	    return;
	}
    } else {
	switch (status) {
	    case RPC_TIMEOUT:
	    case RPC_SERVICE_DISABLED:
	    case FS_STALE_HANDLE: {
		    if (!swapDown) {
			/*
			 * We have not realized that we have an error yet.
			 * Mark the swap server as down, and return a
			 * pointer to the swap stream.  If it isn't open
			 * yet we'll return NIL, and our caller should use
			 * vmSwapStreamPtr for recovery, which is guarded
			 * by a different monitor.
			 */
			*recStreamPtrPtr = corePtr->virtPage.segPtr->swapFilePtr;
			*doRecoveryPtr = TRUE;
			swapDown = TRUE;
		    }
		    corePtr->flags &= ~VM_PAGE_BEING_CLEANED;
		    VmListInsert((List_Links *)corePtr,
				 LIST_ATREAR(dirtyPageList));
		    *corePtrPtr = (VmCore *)NIL;
		    numPageOutProcs--;
		    UNLOCK_MONITOR;
		    return;
		}
		break;
	    default:
		break;
	}
	PutOnFront(corePtr);
	corePtr = (VmCore *) NIL;	
    }

    while (!List_IsEmpty(dirtyPageList)) {
        /*
	 * Get the first page off of the dirty list.
	 */
	corePtr = (VmCore *) List_First(dirtyPageList);
	VmListRemove((List_Links *) corePtr);
	/*
	 * If this segment is being deleted then invalidate the page and
	 * then free it.
	 */
        if (corePtr->virtPage.segPtr->flags & VM_SEG_DEAD) {
	    vmStat.numDirtyPages--;
	    VmPageInvalidateInt(&corePtr->virtPage,
		VmGetAddrPTEPtr(&corePtr->virtPage, corePtr->virtPage.page));
	    PutOnFreeList(corePtr);
	    corePtr = (VmCore *) NIL;
	} else {
	    break;
	}
    }

    if (corePtr != (VmCore *) NIL) {
	/*
	 * This page will now be on the page server so set the pte accordingly.
	 * In addition the modified bit must be cleared here since the page
	 * could get modified while it is being cleaned.
	 */
	ptePtr = VmGetAddrPTEPtr(&corePtr->virtPage, corePtr->virtPage.page);
	*ptePtr |= VM_ON_SWAP_BIT;
	/*
	 * If the page has become locked while it was on the dirty list, don't
	 * clear the modify bit.  The set modify bit after the page write 
	 * completes will cause this page to be put back on the alloc list.
	 */
	if (corePtr->lockCount == 0) {
	    *ptePtr &= ~VM_MODIFIED_BIT;
	    VmMach_ClearModBit(&corePtr->virtPage, Vm_GetPageFrame(*ptePtr));
	}
	corePtr->flags |= VM_PAGE_BEING_CLEANED;
    } else {
	/*
	 * No dirty pages.  Decrement the number of page out procs and
	 * return nil.  PageOut will kill itself when it receives NIL.
	 */
	numPageOutProcs--;

	if (numPageOutProcs == 0 && vmStat.numDirtyPages != 0) {
	    panic("PageOutPutAndGet: Dirty pages but no pageout procs\n");
	}
    }

    *corePtrPtr = corePtr;

    UNLOCK_MONITOR;
}


/*
 * ----------------------------------------------------------------------------
 *
 * PutOnFront --
 *
 *	Take one of two actions.  If page frame is already marked as free
 *	then put it onto the front of the free list.  Otherwise put it onto
 *	the front of the allocate list.  
 *
 * Results:
 *	None.	
 *
 * Side effects:
 *	Allocate list or free list modified.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
PutOnFront(corePtr)
    register	VmCore	*corePtr;
{
    register	Vm_PTE	*ptePtr;

    if (corePtr->flags & VM_SEG_PAGEOUT_WAIT) {
	Sync_Broadcast(&corePtr->virtPage.segPtr->condition);
    }
    corePtr->flags &= ~(VM_DIRTY_PAGE | VM_PAGE_BEING_CLEANED | 
		        VM_SEG_PAGEOUT_WAIT | VM_DONT_FREE_UNTIL_CLEAN);
    if (corePtr->flags & VM_FREE_PAGE) {
	PutOnFreeList(corePtr);
    } else {
	Boolean	referenced, modified;
	/*
	 * Update the software page table from the hardware.  This 
	 * catches the case when a page that we are writting out is
	 * modified.  
	 */
	ptePtr = VmGetAddrPTEPtr(&corePtr->virtPage,
			     corePtr->virtPage.page);
	referenced = *ptePtr & VM_REFERENCED_BIT;
	modified = *ptePtr & VM_MODIFIED_BIT;
	VmMach_GetRefModBits(&corePtr->virtPage, Vm_GetPageFrame(*ptePtr),
			  &referenced, &modified);
	if (referenced) {
	    *ptePtr |= (VM_REFERENCED_BIT);
	}
	if (modified) {
	    *ptePtr |= (VM_REFERENCED_BIT|VM_MODIFIED_BIT);
	}
	if (vmFreeWhenClean && corePtr->lockCount == 0 && 
					!(*ptePtr & VM_REFERENCED_BIT)) {
	    /*
	     * We are supposed to free pages after we clean them.  Before
	     * we put this page onto the dirty list, we already invalidated
	     * it in hardware, thus forcing it to be faulted on before being
	     * referenced.  If it was faulted on then PreparePage would have
	     * set the reference bit in the PTE.  Thus if the reference bit
	     * isn't set then the page isn't valid and thus it couldn't
	     * possibly have been modified or referenced.  So we free this
	     * page.
	     */
	     if (!(*ptePtr & VM_PHYS_RES_BIT)) {
	       panic("PutOnFront: Resident bit not set\n");
	     }
	     corePtr->virtPage.segPtr->resPages--;
	     *ptePtr &= ~(VM_PHYS_RES_BIT | VM_PAGE_FRAME_FIELD);
	    PutOnFreeList(corePtr);
	} else {
	    PutOnAllocListFront(corePtr);
	}
    }
    vmStat.numDirtyPages--; 
    Sync_Broadcast(&cleanCondition);
}


/*
 * Variables for the clock daemon.  vmPagesToCheck is the number of page 
 * frames to examine each time that the clock daemon wakes up.  vmClockSleep
 * is the amount of time for the clock daemon before it runs again.
 */
unsigned int	vmClockSleep;		
int		vmPagesToCheck = 100;
static	int	clockHand = 0;

/*
 * ----------------------------------------------------------------------------
 *
 * Vm_Clock --
 *
 *	Main loop for the clock daemon process.  It will wakeup every 
 *	few seconds, examine a few page frames, and then go back to sleep.
 *	It is used to keep the allocate list in approximate LRU order.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	The allocate list is modified.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
ENTRY void
Vm_Clock(data, callInfoPtr)
    ClientData	data;
    Proc_CallInfo	*callInfoPtr;
{
    static Boolean initialized = FALSE;

    register	VmCore	*corePtr;
    register	Vm_PTE	*ptePtr;
    int			i;
    Time		curTime;
    Boolean		referenced;
    Boolean		modified;

    LOCK_MONITOR;

    Timer_GetTimeOfDay(&curTime, (int *) NIL, (Boolean *) NIL);

    if (vmTraceNeedsInit) {
	short	initEnd;

	vmTraceTime = 0;
	VmTraceSegStart();
	VmMach_Trace();
	VmStoreTraceRec(VM_TRACE_END_INIT_REC, sizeof(short),
			(Address)&initEnd, TRUE);
	vmTracesToGo = vmTracesPerClock;
	vmClockSleep = timer_IntOneSecond / vmTracesPerClock;
	vm_Tracing = TRUE;
	vmTraceNeedsInit = FALSE;
    } else if (vm_Tracing) {
	vmTraceStats.numTraces++;
	VmMach_Trace();

	/*
	 * Decrement the number of traces per iteration of the clock.  If we
	 * are at 0 then run the clock for one iteration.
	 */
	vmTracesToGo--;
	if (vmTracesToGo > 0) {
	    callInfoPtr->interval = vmClockSleep;
	    UNLOCK_MONITOR;
	    return;
	}
	vmTracesToGo = vmTracesPerClock;
    }

    /*
     * Examine vmPagesToCheck pages.
     */

    for (i = 0; i < vmPagesToCheck; i++) {
	corePtr = &(coreMap[clockHand]);

	/*
	 * Move to the next page in the core map.  If have reached the
	 * end of the core map then go back to the first page that may not
	 * be used by the kernel.
	 */
	if (clockHand == vmStat.numPhysPages - 1) {
	    clockHand = vmFirstFreePage;
	} else {
	    clockHand++;
	}

	/*
	 * If the page is free, locked, in the middle of a page-in, 
	 * or in the middle of a pageout, then we aren't concerned 
	 * with this page.
	 */
	if ((corePtr->flags & (VM_DIRTY_PAGE | VM_FREE_PAGE)) ||
	    corePtr->lockCount > 0) {
	    continue;
	}

	ptePtr = VmGetAddrPTEPtr(&corePtr->virtPage, corePtr->virtPage.page);

	/*
	 * If the page has been referenced, then put it on the end of the
	 * allocate list.
	 */
	VmMach_GetRefModBits(&corePtr->virtPage, Vm_GetPageFrame(*ptePtr),
			     &referenced, &modified);
	if ((*ptePtr & VM_REFERENCED_BIT) || referenced) {
	    VmListMove((List_Links *) corePtr, LIST_ATREAR(allocPageList));
	    corePtr->lastRef = curTime.seconds;
	    *ptePtr &= ~VM_REFERENCED_BIT;
	    VmMach_ClearRefBit(&corePtr->virtPage, Vm_GetPageFrame(*ptePtr));
	    if (vmWriteableRefPageout &&
		corePtr->virtPage.segPtr->type != VM_CODE) {
		*ptePtr |= VM_MODIFIED_BIT;
	    }
	}
    }

    if (!initialized) {
        vmClockSleep = timer_IntOneSecond;
	initialized = TRUE;
    }

    callInfoPtr->interval = vmClockSleep;

    UNLOCK_MONITOR;
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmCountDirtyPages --
 *
 *	Return the number of dirty pages.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	The allocate list is modified.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY int
VmCountDirtyPages()
{
    register	Vm_PTE	*ptePtr;
    register	VmCore	*corePtr;
    register	int	i;
    register	int	numDirtyPages = 0;
    Boolean		referenced;
    Boolean		modified;

    LOCK_MONITOR;

    for (corePtr = &coreMap[vmFirstFreePage], i = vmFirstFreePage;
         i < vmStat.numPhysPages;
	 i++, corePtr++) {
	if ((corePtr->flags & VM_FREE_PAGE) || corePtr->lockCount > 0) {
	    continue;
	}
	if (corePtr->flags & VM_DIRTY_PAGE) {
	    numDirtyPages++;
	    continue;
	}
	ptePtr = VmGetAddrPTEPtr(&corePtr->virtPage, corePtr->virtPage.page);
	if (*ptePtr & VM_MODIFIED_BIT) {
	    numDirtyPages++;
	    continue;
	}
	VmMach_GetRefModBits(&corePtr->virtPage, Vm_GetPageFrame(*ptePtr),
			     &referenced, &modified);
	if (modified) {
	    numDirtyPages++;
	}
    }
    UNLOCK_MONITOR;
    return(numDirtyPages);
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmFlushSegment --
 *
 *	Flush the given range of pages in the segment to swap space and take
 *	them out of memory.  It is assumed that the processes that own this
 *	segment are frozen.
 *
 * Results:
 *     	None.
 *
 * Side effects:
 *     	All memory in the given range is forced out to swap and freed.
 *	*virtAddrPtr is modified.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmFlushSegment(virtAddrPtr, lastPage)
    Vm_VirtAddr	*virtAddrPtr;
    int		lastPage;
{
    register	Vm_PTE		*ptePtr;
    register	VmCore		*corePtr;
    unsigned int		pfNum;
    Boolean			referenced;
    Boolean			modified;

    LOCK_MONITOR;

    if (virtAddrPtr->segPtr->ptPtr == (Vm_PTE *)NIL) {
	UNLOCK_MONITOR;
	return;
    }
    for (ptePtr = VmGetAddrPTEPtr(virtAddrPtr, virtAddrPtr->page);
         virtAddrPtr->page <= lastPage;
	 virtAddrPtr->page++, VmIncPTEPtr(ptePtr, 1)) {
	if (!(*ptePtr & VM_PHYS_RES_BIT)) {
	    continue;
	}
	pfNum = Vm_GetPageFrame(*ptePtr);
	corePtr = &coreMap[pfNum];
	if (corePtr->lockCount > 0) {
	    continue;
	}
	if (corePtr->flags & VM_DIRTY_PAGE) {
	    corePtr->flags |= VM_DONT_FREE_UNTIL_CLEAN;
	} else {
	    VmMach_GetRefModBits(virtAddrPtr, Vm_GetPageFrame(*ptePtr),
				 &referenced, &modified);
	    if ((*ptePtr & VM_MODIFIED_BIT) || modified) {
		TakeOffAllocList(corePtr);
		PutOnDirtyList(corePtr);
		corePtr->flags |= VM_DONT_FREE_UNTIL_CLEAN;
	    }
	}
	VmPageFreeInt(pfNum);
	VmPageInvalidateInt(virtAddrPtr, ptePtr);
    }

    UNLOCK_MONITOR;
}


/*
 *----------------------------------------------------------------------
 *
 * Vm_GetPageSize --
 *
 *      Return the page size.
 *
 * Results:
 *      The page size.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
int
Vm_GetPageSize()
{
    return(vm_PageSize);
}


/*
 *----------------------------------------------------------------------
 *
 * Vm_MapBlock --
 *
 *      Allocate and validate enough pages at the given address to map
 *	one FS cache block.
 *
 * Results:
 *      The number of pages that were allocated.
 *
 * Side effects:
 *      Pages added to kernels address space.
 *
 *----------------------------------------------------------------------
 */
int
Vm_MapBlock(addr)
    Address	addr;	/* Address where to map in pages. */
{
    register	Vm_PTE	*ptePtr;
    Vm_VirtAddr		virtAddr;
    unsigned	int	page;
    int			curFSPages;

    vmStat.fsMap++;
    curFSPages = vmStat.fsMap - vmStat.fsUnmap;
    if (curFSPages >= vmBoundary) {
	vmBoundary += vmPagesPerGroup;
	vmCurPenalty += vmFSPenalty;
    }
    if (curFSPages > vmStat.maxFSPages) {
	vmStat.maxFSPages = curFSPages;
    }

    virtAddr.page = (unsigned int) addr >> vmPageShift;
    virtAddr.offset = 0;
    virtAddr.segPtr = vm_SysSegPtr;
    virtAddr.flags = 0;
    virtAddr.sharedPtr = (Vm_SegProcList *)NIL;
    ptePtr = VmGetPTEPtr(vm_SysSegPtr, virtAddr.page);

    /*
     * Allocate a block.  We know that the page size is not smaller than
     * the block size so that one page will suffice.
     */
    page = DoPageAllocate(&virtAddr, 0);
    if (page == VM_NO_MEM_VAL) {
	/*
	 * Couldn't get any memory.  
	 */
	return(0);
    }
    *ptePtr |= page;
    VmPageValidate(&virtAddr);

    return(1);
}


/*
 *----------------------------------------------------------------------
 *
 * Vm_UnmapBlock --
 *
 *      Free and invalidate enough pages at the given address to unmap
 *	one fs cache block.
 *
 * Results:
 *      The number of pages that were deallocated.
 *
 * Side effects:
 *      Pages removed from kernels address space.
 *
 *----------------------------------------------------------------------
 */
int
Vm_UnmapBlock(addr, retOnePage, pageNumPtr)
    Address	addr;		/* Address where to map in pages. */
    Boolean	retOnePage;	/* TRUE => don't put one of the pages on
				 * the free list and return its value in
				 * *pageNumPtr. */
    unsigned int *pageNumPtr;	/* One of the pages that was unmapped. */
{
    register	Vm_PTE	*ptePtr;
    Vm_VirtAddr		virtAddr;
    int			curFSPages;

    vmStat.fsUnmap++;
    curFSPages = vmStat.fsMap - vmStat.fsUnmap;

    if (curFSPages < vmBoundary) {
	vmBoundary -= vmPagesPerGroup;
	vmCurPenalty -= vmFSPenalty;
    }
    if (curFSPages < vmStat.minFSPages) {
	vmStat.minFSPages = curFSPages;
    }

    virtAddr.page = (unsigned int) addr >> vmPageShift;
    virtAddr.offset = 0;
    virtAddr.segPtr = vm_SysSegPtr;
    virtAddr.flags = 0;
    virtAddr.sharedPtr = (Vm_SegProcList *)NIL;
    ptePtr = VmGetPTEPtr(vm_SysSegPtr, virtAddr.page);

    if (retOnePage) {
	*pageNumPtr = Vm_GetPageFrame(*ptePtr);
    } else {
	/*
	 * If we aren't supposed to return the page, then free it.
	 */
	VmPageFree(Vm_GetPageFrame(*ptePtr));
    }
    VmPageInvalidate(&virtAddr);

    return(1);
}


/*
 *----------------------------------------------------------------------
 *
 * Vm_FsCacheSize --
 *
 *	Return the virtual addresses of the start and end of the file systems
 *	cache.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
Vm_FsCacheSize(startAddrPtr, endAddrPtr)
    Address	*startAddrPtr;	/* Lowest virtual address. */
    Address	*endAddrPtr;	/* Highest virtual address. */
{
    int	numPages;

    /*
     * Compute the minimum number of pages that are reserved for VM.  The number
     * of free pages is the maximum number of pages that will ever exist
     * for user processes.
     */
    vmStat.minVMPages = vmStat.numFreePages / MIN_VM_PAGE_FRACTION;
    vmMaxDirtyPages = vmStat.numFreePages / MAX_DIRTY_PAGE_FRACTION;

    *startAddrPtr = vmBlockCacheBaseAddr;
    /*
     * We aren't going to get any more free pages so limit the maximum number
     * of blocks in the cache to the number of free pages that we have minus
     * the minimum amount of free pages that we keep for user
     * processes to run.
     */
    numPages = ((unsigned int)vmBlockCacheEndAddr - 
		(unsigned int)vmBlockCacheBaseAddr) / vm_PageSize;
    if (numPages > vmStat.numFreePages - vmStat.minVMPages) {
	numPages = vmStat.numFreePages - vmStat.minVMPages;
    }
    *endAddrPtr = vmBlockCacheBaseAddr + numPages * vm_PageSize - 1;
    /*
     * Compute the penalties to put onto FS pages.
     */
    vmPagesPerGroup = vmStat.numFreePages / vmNumPageGroups;
    vmCurPenalty = 0;
    vmBoundary = vmPagesPerGroup;
}

/*---------------------------------------------------------------------------
 * 
 *	Routines for recovery
 *
 * VM needs to be able to recover when the server of swap crashes.  This is
 * done in the following manner:
 *
 *    1) At boot time the directory "/swap/host_number" is open by the
 *	 routine Vm_OpenSwapDirectory and the stream is stored in 
 *	 vmSwapStreamPtr.
 *    2) If an error occurs on a page write then the variable swapDown
 *	 is set to TRUE which prohibits all further actions that would dirty
 *	 physical memory pages (e.g. page faults) and prohibits dirty pages
 *	 from being written to swap.
 *    3) Next the routine Fsutil_WaitForHost is called to asynchronously wait
 *	 for the server to come up.  When it detects that the server is
 *	 in fact up and the file system is alive, it calls Vm_Recovery.
 *    4) Vm_Recovery when called will set swapDown to FALSE and start cleaning
 *	 dirty pages if necessary.
 */


/*
 *----------------------------------------------------------------------
 *
 * Vm_Recovery --
 *
 *	The swap area has just come back up.  Wake up anyone waiting for it to
 *	come back and start up page cleaners if there are dirty pages to be
 *	written out.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	swapDown flag set to FALSE.
 *
 *----------------------------------------------------------------------
 */
ENTRY void
Vm_Recovery()
{
    LOCK_MONITOR;

    swapDown = FALSE;
    Sync_Broadcast(&swapDownCondition);
    while (vmStat.numDirtyPages - numPageOutProcs > 0 &&
	   numPageOutProcs < vmMaxPageOutProcs) { 
	Proc_CallFunc(PageOut, (ClientData) numPageOutProcs, 0);
	numPageOutProcs++;
    }

    UNLOCK_MONITOR;
}

/*
 *----------------------------------------------------------------------
 *
 * VmPageFlush --
 *
 *	Flush (shared) pages to the server or disk.
 *
 * Results:
 *	SUCCESS if it worked.
 *
 * Side effects:
 *	Page is written to disk and removed from memory.
 *	If page is pinned down, it will be unpinned.
 *	The page is invalidated from the local cache.
 *	*virtAddrPtr is modified.
 *
 *----------------------------------------------------------------------
 */
ENTRY ReturnStatus
VmPageFlush(virtAddrPtr, length, toDisk, wantRes)
    Vm_VirtAddr		*virtAddrPtr;
    int			length;
    Boolean		toDisk;
    Boolean		wantRes;
{
    VmCore		*corePtr;
    Fscache_FileInfo	*cacheInfoPtr;
    ReturnStatus	status = SUCCESS;
    ReturnStatus	statusTmp;
    int			firstBlock;
    Vm_Segment		*segPtr;
    Fs_Stream		*streamPtr;
    Vm_PTE		*ptePtr;
    int			lastPage;
    int			pageFrame;
    int			referenced, modified;

    LOCK_MONITOR;
    dprintf("VmPageFlush(%x, %d, %d, %d)\n", virtAddrPtr, length, toDisk,
	    wantRes);
    segPtr = virtAddrPtr->segPtr;
    lastPage = virtAddrPtr->page + (length>>vmPageShift) - 1;
    streamPtr = segPtr->swapFilePtr;
    dprintf("segPtr = %x, firstPage = %d, lastPage = %d, streamPtr = %x\n",
	   segPtr, virtAddrPtr->page, lastPage, streamPtr);
    for (ptePtr = VmGetAddrPTEPtr(virtAddrPtr, virtAddrPtr->page);
	    virtAddrPtr->page <= lastPage;
	    VmIncPTEPtr(ptePtr,1), virtAddrPtr->page++) {
	if (!(*ptePtr & VM_PHYS_RES_BIT)) {
	    if (wantRes) {
		dprintf("Page is not physically resident\n");
		status = FAILURE;
	    }
	    continue;
	}
	pageFrame = Vm_GetPageFrame(*ptePtr);
	corePtr = &coreMap[pageFrame];
	referenced = *ptePtr & VM_REFERENCED_BIT;
	modified = *ptePtr & VM_MODIFIED_BIT;
	VmMach_AllocCheck(&corePtr->virtPage, pageFrame,
			  &referenced, &modified);
	if (!modified) {
	    dprintf("Page is clean, so skipping\n");
	    continue;
	}
	*ptePtr |= VM_ON_SWAP_BIT;
	corePtr->flags |= VM_PAGE_BEING_CLEANED;
	*ptePtr &= ~VM_MODIFIED_BIT;
	dprintf("VmPageFlush: paging out %d (%d)\n", virtAddrPtr->page, 
		corePtr-coreMap);
	VmMach_ClearModBit(&corePtr->virtPage, Vm_GetPageFrame(*ptePtr)); 
	UNLOCK_MONITOR;
	statusTmp = VmPageServerWrite(&corePtr->virtPage,
		(unsigned int)(corePtr-coreMap), toDisk);
	dprintf("VmPageFlush: status = %x, wrote %x, %x\n", statusTmp,
		&corePtr->virtPage, (unsigned int)(corePtr-coreMap));
	LOCK_MONITOR;
	corePtr->flags &= ~VM_PAGE_BEING_CLEANED;
	if (statusTmp != SUCCESS) {
	    status = statusTmp;
	    break;
	}
	/*
	 * This stuff should probably be in the fs module.
	 */
	if (streamPtr->ioHandlePtr->fileID.type == FSIO_RMT_FILE_STREAM) {
	    cacheInfoPtr = & ((Fsrmt_FileIOHandle *)streamPtr
		    ->ioHandlePtr)->cacheInfo;
	    if (segPtr->type == VM_STACK) {
		firstBlock = mach_LastUserStackPage - virtAddrPtr->page;
	    } else if (segPtr->type == VM_SHARED) {
		firstBlock= virtAddrPtr->page - segOffset(virtAddrPtr) +
			(virtAddrPtr->sharedPtr->fileAddr>>vmPageShift);
	    } else {
		firstBlock = virtAddrPtr->page - segPtr->offset;
	    }
	    dprintf("Invalidating block %d\n", firstBlock);
	    Fscache_FileInvalidate(cacheInfoPtr, firstBlock,
		firstBlock+ (length>>vmPageShift)-1);
	}
    }
    UNLOCK_MONITOR;
    if (status != SUCCESS) {
	dprintf("VmPageFlush: failure: %x\n", status);
    }
    return status;
}
