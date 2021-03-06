/* vmSunMach.c -
 *
 *     	This file contains all hardware dependent routines for Sun2's and
 *	Sun3's.  I will not attempt to explain the Sun mapping hardware in 
 *	here.  See the Sun2 and Sun3 architecture manuals for details on
 *	the mapping hardware.
 *
 * Copyright (C) 1985 Regents of the University of California
 * All rights reserved.
 */

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/kernel/vm/sun3.md/RCS/vmSun.c,v 9.17 90/10/09 12:03:14 jhh Exp $ SPRITE (Berkeley)";
#endif not lint

#include <sprite.h>
#include <vmSunConst.h>
#include <machMon.h>
#include <vm.h>
#include <vmInt.h>
#include <vmMach.h>
#include <vmMachInt.h>
#include <vmTrace.h>
#include <list.h>
#include <mach.h>
#include <proc.h>
#include <sched.h>
#include <stdlib.h>
#include <sync.h>
#include <sys.h>
#include <dbg.h>
#include <net.h>
#include <stdio.h>
#include <bstring.h>

#ifndef multiprocessor

#undef MASTER_LOCK
#define MASTER_LOCK(mutexPtr)
#undef MASTER_UNLOCK
#define MASTER_UNLOCK(mutexPtr)

#else

/*
 * The master lock to synchronize access to pmegs and context.
 */
static Sync_Semaphore vmMachMutex;
static Sync_Semaphore volatile *vmMachMutexPtr = &vmMachMutex;

#endif

/*----------------------------------------------------------------------
 * 
 * 			Hardware data structures
 *
 * Terminology: 
 *	1) Physical page frame: A frame that contains one hardware page.
 *	2) Virtual page frame:  A frame that contains VMMACH_CLUSTER_SIZE 
 *				hardware pages.
 *	3) Software segment: The segment structure used by the hardware
 *			     independent VM module.
 *	4) Hardware segment: A piece of a hardware context.
 *
 * A hardware context corresponds to a process's address space.  A context
 * is made up of many equal sized hardware segments.  The 
 * kernel is mapped into each hardware context so that the kernel has easy
 * access to user data.  One context (context 0) is reserved for use by
 * kernel processes.  The hardware contexts are defined by the array
 * contextArray which contains an entry for each context.  Each entry 
 * contains a pointer back to the process that is executing in the context 
 * and an array which is an exact duplicate of the hardware segment map for
 * the context.  The contexts are managed by keeping all contexts except for
 * the system context in a list that is kept in LRU order.  Whenever a context 
 * is needed the first context off of the list is used and the context is
 * stolen from the current process that owns it if necessary.
 *
 * PMEGs are allocated to software segments in order to allow pages to be mapped
 * into the segment's virtual address space. There are only a small number of 
 * PMEGs (256) which have to be shared by all segments.  PMEGs that have
 * been allocated to user segments can be stolen at any time.  PMEGs that have
 * been allocated to the system segment cannot be taken away unless the system
 * segment voluntarily gives it up.  In order to manage the PMEGs there are
 * two data structures.  One is an array of PMEG info structures that contains
 * one entry for each PMEG.  The other is an array stored with each software
 * segment struct that contains the PMEGs that have been allocated to the
 * software segment.  Each entry in the array of PMEG info structures
 * contains enough information to remove the PMEG from its software segment.
 * One of the fields in the PMEG info struct is the count of pages that have
 * been validated in the PMEG.  This is used to determine when a PMEG is no
 * longer being actively used.  
 *
 * There are two lists that are used to manage PMEGs.  The pmegFreeList 
 * contains all PMEGs that are either not being used or contain no valid 
 * page map entries; unused ones are inserted at the front of the list 
 * and empty ones at the rear.  The pmegInuseList contains all PMEGs
 * that are being actively used to map user segments and is managed as a FIFO.
 * PMEGs that are being used to map tbe kernel's VAS do not appear on the 
 * pmegInuseList. When a pmeg is needed to map a virtual address, first the
 * free list is checked.  If it is not empty then the first PMEG is pulled 
 * off of the list.  If it is empty then the first PMEG is pulled off of the
 * inUse list.  If the PMEG  that is selected is being used (either actively 
 * or inactively) then it is freed from the software segment that is using it.
 * Once the PMEG is freed up then if it is being allocated for a user segment
 * it is put onto the end of the pmegInuseList.
 *
 * Page frames are allocated to software segments even when there is
 * no PMEG to map it in.  Thus when a PMEG that was mapping a page needs to
 * be removed from the software segment that owns the page, the reference
 * and modify bits stored in the PMEG for the page must be saved.  The
 * array refModMap is used for this.  It contains one entry for each
 * virtual page frame.  Its value for a page frame or'd with the bits stored
 * in the PMEG (if any) comprise the referenced and modified bits for a 
 * virtual page frame.
 * 
 * IMPORTANT SYNCHRONIZATION NOTE:
 *
 * The internal data structures in this file have to be protected by a
 * master lock if this code is to be run on a multi-processor.  Since a
 * process cannot start executing unless VmMach_SetupContext can be
 * executed first, VmMach_SetupContext cannot context switch inside itself;
 * otherwise a deadlock will occur.  However, VmMach_SetupContext mucks with
 * contexts and PMEGS and therefore would have to be synchronized
 * on a multi-processor.  A monitor lock cannot be used because it may force
 * VmMach_SetupContext to be context switched.
 *
 * The routines in this file also muck with other per segment data structures.
 * Access to these data structures is synchronized by our caller (the
 * machine independent module).
 *
 *----------------------------------------------------------------------
 */

/*
 * Machine dependent flags for the flags field in the Vm_VirtAddr struct.
 * We are only allowed to use the second byte of the flags.
 *
 *	USING_MAPPED_SEG		The parsed virtual address falls into
 *					the mapping segment.
 */
#define	USING_MAPPED_SEG	0x100

/*
 * Macros to get to and from hardware segments and pages.
 */
#define PageToSeg(page) ((page) >> (VMMACH_SEG_SHIFT - VMMACH_PAGE_SHIFT))
#define SegToPage(seg) ((seg) << (VMMACH_SEG_SHIFT - VMMACH_PAGE_SHIFT))

/*
 * Convert from page to hardware segment, with correction for
 * any difference between virtAddrPtr offset and segment offset.
 * (This difference will only happen for shared segments.)
*/
#define PageToOffSeg(page,virtAddrPtr) (PageToSeg((page)- \
	segOffset(virtAddrPtr)+(virtAddrPtr->segPtr->offset)))

/*
 * Macro to set all page map entries for the given virtual address.
 */
#define	SET_ALL_PAGE_MAP(virtAddr, pte) { \
    int	__i; \
    for (__i = 0; __i < VMMACH_CLUSTER_SIZE; __i++) { \
	VmMachSetPageMap((Address)((virtAddr) + __i * VMMACH_PAGE_SIZE_INT),\
	(VmMachPTE)((pte) + __i)); \
    } \
}

/*
 * PMEG table entry structure.
 */
typedef struct {
    List_Links			links;		/* Links so that the pmeg */
						/* can be in a list */
    struct      Vm_Segment      *segPtr;        /* Back pointer to segment that
                                                   this cluster is in */
    int				hardSegNum;	/* The hardware segment number
						   for this pmeg. */
    int				pageCount;	/* Count of resident pages in
						 * this pmeg. */
    int				lockCount;	/* The number of times that
						 * this PMEG has been locked.*/
    int				flags;		/* Flags defined below. */
} PMEG;

/*
 * Flags to indicate the state of a pmeg.
 *
 *    PMEG_DONT_ALLOC	This pmeg should not be reallocated.  This is 
 *			when a pmeg cannot be reclaimed until it is
 *			voluntarily freed.
 *    PMEG_NEVER_FREE	Don't ever free this pmeg no matter what anybody says.
 */
#define	PMEG_DONT_ALLOC		0x1
#define	PMEG_NEVER_FREE		0x2

/*
 * Pmeg information.  pmegArray contains one entry for each pmeg.  pmegFreeList
 * is a list of all pmegs that aren't being actively used.  pmegInuseList
 * is a list of all pmegs that are being actively used.
 */
static	PMEG   		pmegArray[VMMACH_NUM_PMEGS];
static	List_Links   	pmegFreeListHeader;
static	List_Links   	*pmegFreeList = &pmegFreeListHeader;
static	List_Links   	pmegInuseListHeader;
static	List_Links   	*pmegInuseList = &pmegInuseListHeader;

/*
 * The context table structure.
 */
typedef struct VmMach_Context {
    List_Links		     links;	  /* Links so that the contexts can be
					     in a list. */
    struct Proc_ControlBlock *procPtr;	/* A pointer to the process table entry
					   for the process that is running in
					   this context. */
					/* A reflection of the hardware context
					 * map. */
    unsigned char 	     map[VMMACH_NUM_SEGS_PER_CONTEXT];
    int			     context;	/* Which context this is. */
    int			     flags;	/* Defined below. */
} VmMach_Context;

/*
 * Context flags:
 *
 *     	CONTEXT_IN_USE	This context is used by a process.
 */
#define	CONTEXT_IN_USE	0x1

/*
 * Context information.  contextArray contains one entry for each context. 
 * contextList is a list of contexts in LRU order.
 */
static	VmMach_Context	contextArray[VMMACH_NUM_CONTEXTS];
static	List_Links   	contextListHeader;
static	List_Links   	*contextList = &contextListHeader;

/*
 * Map containing one entry for each virtual page.
 */
static	VmMachPTE		*refModMap;

/*
 * Macros to translate from a virtual page to a physical page and back.
 */
#define	VirtToPhysPage(pfNum) ((pfNum) << VMMACH_CLUSTER_SHIFT)
#define	PhysToVirtPage(pfNum) ((pfNum) >> VMMACH_CLUSTER_SHIFT)

/*
 * Macro to get a pointer into a software segment's hardware segment table.
 */
#ifdef CLEAN
#define GetHardSegPtr(machPtr, segNum) \
    ((machPtr)->segTablePtr + (segNum) - (machPtr)->offset)
#else
#define GetHardSegPtr(machPtr, segNum) \
    ( ((unsigned)((segNum) - (machPtr)->offset) > (machPtr)->numSegs) ? \
    (panic("Invalid segNum\n"),(machPtr)->segTablePtr) : \
    ((machPtr)->segTablePtr + (segNum) - (machPtr)->offset) )
#endif

/*
 * The maximum amount of kernel code + data available. 
 */
int	vmMachKernMemSize = VMMACH_MAX_KERN_SIZE;

/*
 * The segment that is used to map a segment into a process's virtual address
 * space for cross-address-space copies.
 */
#define	MAP_SEG_NUM (VMMACH_MAP_SEG_ADDR >> VMMACH_SEG_SHIFT)

static void MMUInit _ARGS_((int firstFreeSegment));
static int GetNumPages _ARGS_((void));
static int PMEGGet _ARGS_((Vm_Segment *softSegPtr, int hardSegNum,
	Boolean flags));
static void PMEGFree _ARGS_((int pmegNum));
ENTRY static Boolean PMEGLock _ARGS_((register VmMach_SegData *machPtr,
	int segNum));
static void ByteFill _ARGS_((register unsigned int fillByte,
	register int numBytes, Address destPtr));
INTERNAL static void SetupContext _ARGS_((register Proc_ControlBlock *procPtr));
INTERNAL void VmMachTracePage _ARGS_((register VmMachPTE pte,
	unsigned int pageNum));
static void PageInvalidate _ARGS_((register Vm_VirtAddr *virtAddrPtr,
	unsigned int virtPage, Boolean segDeletion));
static void VmMach_Unalloc _ARGS_((VmMach_SharedData *sharedData,
	Address addr));

static	VmMach_SegData	*sysMachPtr;
Address			vmMachPTESegAddr;
Address			vmMachPMEGSegAddr;

static	Boolean		printedSegTrace;
static	PMEG		*tracePMEGPtr;


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_BootInit --
 *
 *      Do hardware dependent boot time initialization.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Hardware page map for the kernel is initialized.  Also the various size
 * 	fields are filled in.
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_BootInit(pageSizePtr, pageShiftPtr, pageTableIncPtr, kernMemSizePtr,
		numKernPagesPtr, maxSegsPtr, maxProcessesPtr)
    int	*pageSizePtr;
    int	*pageShiftPtr;
    int	*pageTableIncPtr;
    int	*kernMemSizePtr;
    int	*numKernPagesPtr;
    int	*maxSegsPtr;
    int *maxProcessesPtr;
{
    register Address	virtAddr;
    register int	i;
    int			kernPages;
    int			numPages;

#ifdef multiprocessor
    Sync_SemInitDynamic(&vmMachMutex, "Vm:vmMachMutex");
#endif

    kernPages = VMMACH_BOOT_MAP_PAGES;
    /*
     * Map all of the kernel memory that we might need one for one.  We know
     * that the monitor maps the first part of memory one for one but for some
     * reason it doesn't map enough.  We assume that the pmegs have been
     * mapped correctly.
     */
    for (i = 0, virtAddr = (Address)mach_KernStart; 
	 i < kernPages;
	 i++, virtAddr += VMMACH_PAGE_SIZE_INT) {
        VmMachSetPageMap(virtAddr, 
	    (VmMachPTE)(VMMACH_KRW_PROT | VMMACH_RESIDENT_BIT | i));
    }

    /*
     * Do boot time allocation.
     */
    sysMachPtr = (VmMach_SegData *)Vm_BootAlloc(sizeof(VmMach_SegData) + 
						VMMACH_NUM_SEGS_PER_CONTEXT);
    numPages = GetNumPages();
    refModMap = (VmMachPTE *)Vm_BootAlloc(sizeof(VmMachPTE) * numPages);

    /*
     * Return lots of sizes to the machine independent module who called us.
     */
    *pageSizePtr = VMMACH_PAGE_SIZE;
    *pageShiftPtr = VMMACH_PAGE_SHIFT;
    *pageTableIncPtr = VMMACH_PAGE_TABLE_INCREMENT;
    /*
     * Set max size for kernel code and data to vmMachKernMemSize or
     * the amount of physical memory whichever is less.
     */
    if (vmMachKernMemSize > (numPages*VMMACH_PAGE_SIZE)) {
	vmMachKernMemSize = numPages*VMMACH_PAGE_SIZE;
    }
    *kernMemSizePtr = vmMachKernMemSize;
    *maxProcessesPtr = VMMACH_MAX_KERN_STACKS;
    *numKernPagesPtr = GetNumPages();
    /* 
     * We don't care how many software segments there are so return -1 as
     * the max.
     */
    *maxSegsPtr = -1;
}


/*
 * ----------------------------------------------------------------------------
 *
 * GetNumPages --
 *
 *     Determine how many pages of physical memory there are.
 *
 * Results:
 *     The number of physical pages.
 *
 * Side effects:
 *     None.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetNumPages()
{
#ifdef sun3
    return(*romVectorPtr->memoryAvail / VMMACH_PAGE_SIZE);
#else
    return(*romVectorPtr->memorySize / VMMACH_PAGE_SIZE);
#endif
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_AllocKernSpace --
 *
 *     Allocate memory for machine dependent stuff in the kernels VAS.
 *	This allocates space used to map PMEG and PTE registers
 *	into kernel virtual space.  Two segments are reserved.
 *
 * Results:
 *     The kernel virtual address after the reserved area.
 *
 * Side effects:
 *     Sets vmMachPTESegAddr and vmMachPMEGSegAddr.
 *
 * ----------------------------------------------------------------------------
 */
Address
VmMach_AllocKernSpace(baseAddr)
    Address	baseAddr;
{
    baseAddr = (Address) (((unsigned int)baseAddr + VMMACH_SEG_SIZE - 1) / 
					VMMACH_SEG_SIZE * VMMACH_SEG_SIZE);
    vmMachPTESegAddr = baseAddr;
    vmMachPMEGSegAddr = baseAddr + VMMACH_SEG_SIZE;
    return(baseAddr + 2 * VMMACH_SEG_SIZE);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_Init --
 *
 *     Initialize all virtual memory data structures.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     All virtual memory linked lists and arrays are initialized.
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_Init(firstFreePage)
    int	firstFreePage;	/* Virtual page that is the first free for the 
			 * kernel. */
{
    register 	unsigned char	*segTablePtr;
    register 	VmMachPTE	pte;
    register	int 		i;
    int 			firstFreeSegment;
    Address			virtAddr;
    Address			lastCodeAddr;
    extern	int		etext;

    /*
     * Initialize the kernel's hardware segment table.
     */
    vm_SysSegPtr->machPtr = sysMachPtr;
    sysMachPtr->numSegs = VMMACH_NUM_SEGS_PER_CONTEXT;
    sysMachPtr->offset = PageToSeg(vm_SysSegPtr->offset);
    sysMachPtr->segTablePtr =
	    (unsigned char *) ((Address)sysMachPtr + sizeof(VmMach_SegData));
    ByteFill(VMMACH_INV_PMEG, VMMACH_NUM_SEGS_PER_CONTEXT,
	      (Address)sysMachPtr->segTablePtr);

    /*
     * Determine which hardware segment is the first that is not in use.
     */
    firstFreeSegment = ((firstFreePage - 1) << VMMACH_PAGE_SHIFT) / 
					VMMACH_SEG_SIZE + 1;
    firstFreeSegment += (unsigned int)mach_KernStart >> VMMACH_SEG_SHIFT;

    /* 
     * Initialize the PMEG and context tables and lists.
     */
    MMUInit(firstFreeSegment);

    /*
     * Initialize the page map.
     */
    bzero((Address)refModMap, sizeof(VmMachPTE) * GetNumPages());

    /*
     * The code segment is read only and all other in use kernel memory
     * is read/write.  Since the loader may put the data in the same page
     * as the last code page, the last code page is also read/write.
     */
    lastCodeAddr = (Address) ((unsigned)&etext - VMMACH_PAGE_SIZE);
    for (i = 0, virtAddr = (Address)mach_KernStart;
	 i < firstFreePage;
	 virtAddr += VMMACH_PAGE_SIZE, i++) {
	if (virtAddr >= (Address)MACH_CODE_START && 
	    virtAddr <= lastCodeAddr) {
	    pte = VMMACH_RESIDENT_BIT | VMMACH_KR_PROT | 
			  i * VMMACH_CLUSTER_SIZE;
	} else {
	    pte = VMMACH_RESIDENT_BIT | VMMACH_KRW_PROT | 
			  i * VMMACH_CLUSTER_SIZE;
        }
	SET_ALL_PAGE_MAP(virtAddr, pte);
    }

    /*
     * Protect the bottom of the kernel stack.
     */
    SET_ALL_PAGE_MAP((Address)mach_StackBottom, (VmMachPTE)0);

    /*
     * Invalid until the end of the last segment
     */
    for (;virtAddr < (Address) (firstFreeSegment << VMMACH_SEG_SHIFT);
	 virtAddr += VMMACH_PAGE_SIZE) {
	SET_ALL_PAGE_MAP(virtAddr, (VmMachPTE)0);
    }

    /* 
     * Zero out the invalid pmeg.
     */
    VmMachPMEGZero(VMMACH_INV_PMEG);

    /*
     * Finally copy the kernels context to each of the other contexts.
     */
    for (i = 0; i < VMMACH_NUM_CONTEXTS; i++) {
	if (i == VMMACH_KERN_CONTEXT) {
	    continue;
	}
	VmMachSetUserContext(i);
	for (virtAddr = (Address)mach_KernStart,
		 segTablePtr = vm_SysSegPtr->machPtr->segTablePtr;
	     virtAddr < (Address)(VMMACH_NUM_SEGS_PER_CONTEXT*VMMACH_SEG_SIZE);
	     virtAddr += VMMACH_SEG_SIZE, segTablePtr++) {
	    VmMachSetSegMap(virtAddr, *segTablePtr);
	}
    }
    VmMachSetUserContext(VMMACH_KERN_CONTEXT);
    if (Mach_GetMachineType() == SYS_SUN_3_50) {
	unsigned int vidPage;

#define	VIDEO_START	0x100000
#define	VIDEO_SIZE	0x20000
	vidPage = VIDEO_START / VMMACH_PAGE_SIZE;
	if (firstFreePage > vidPage) {
	    panic("VmMach_Init: We overran video memory.\n");
	}
	/*
	 * On 3/50's the display is kept in main memory beginning at 1 
	 * Mbyte and going for 128 kbytes.  Reserve this memory so VM
	 * doesn't try to use it.
	 */
	for (;vidPage < (VIDEO_START + VIDEO_SIZE) / VMMACH_PAGE_SIZE;
	     vidPage++) {
	    Vm_ReservePage(vidPage);
	}
    }
}


/*
 *----------------------------------------------------------------------
 *
 * MMUInit --
 *
 *	Initialize the context table and lists and the Pmeg table and 
 *	lists.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Context table and Pmeg table are initialized.  Also context list
 *	and pmeg list are initialized.
 *
 *----------------------------------------------------------------------
 */
static void
MMUInit(firstFreeSegment)
    int		firstFreeSegment;
{
    register	int		i;
    register	PMEG		*pmegPtr;
    register	unsigned char	*segTablePtr;
    int				pageCluster;
#ifdef sun3
    int				dontUse;
#endif

    /*
     * Initialize the context table.
     */
    contextArray[VMMACH_KERN_CONTEXT].flags = CONTEXT_IN_USE;
    List_Init(contextList);
    for (i = 0; i < VMMACH_NUM_CONTEXTS; i++) {
	if (i != VMMACH_KERN_CONTEXT) {
	    contextArray[i].flags = 0;
	    List_Insert((List_Links *) &contextArray[i], 
			LIST_ATREAR(contextList));
	}
	contextArray[i].context = i;
    }

    /*
     * Initialize the page cluster list.
     */
    List_Init(pmegFreeList);
    List_Init(pmegInuseList);

    /*
     * Initialize the pmeg structure.
     */
    bzero((Address)pmegArray, VMMACH_NUM_PMEGS * sizeof(PMEG));
    for (i = 0, pmegPtr = pmegArray; 
	 i < VMMACH_NUM_PMEGS; 
	 i++, pmegPtr++) {
	pmegPtr->segPtr = (Vm_Segment *) NIL;
	pmegPtr->flags = PMEG_DONT_ALLOC;
    }

#ifdef sun3
    i = 0;
#else
    /*
     * Segment 0 is left alone because it is required for the monitor.
     */
    pmegArray[0].segPtr = (Vm_Segment *)NIL;
    pmegArray[0].hardSegNum = 0;
    i = 1;
#endif

    /*
     * Invalidate all hardware segments from segment 1 up to the beginning
     * of the kernel.
     */
    for (; i < ((unsigned int)mach_KernStart >> VMMACH_SEG_SHIFT); i++) {
	VmMachSetSegMap((Address)(i << VMMACH_SEG_SHIFT), VMMACH_INV_PMEG);
    }

    /*
     * Reserve all pmegs that have kernel code or heap.
     */
    for (segTablePtr = vm_SysSegPtr->machPtr->segTablePtr;
         i < firstFreeSegment;
	 i++, segTablePtr++) {
	pageCluster = VmMachGetSegMap((Address) (i << VMMACH_SEG_SHIFT));
	pmegArray[pageCluster].pageCount = VMMACH_NUM_PAGES_PER_SEG;
	pmegArray[pageCluster].segPtr = vm_SysSegPtr;
	pmegArray[pageCluster].hardSegNum = i;
	*segTablePtr = pageCluster;
    }

    /*
     * Invalidate all hardware segments that aren't in code or heap and are 
     * before the specially mapped page clusters.
     */
    for (; i < VMMACH_FIRST_SPECIAL_SEG; i++, segTablePtr++) {
	VmMachSetSegMap((Address)(i << VMMACH_SEG_SHIFT), VMMACH_INV_PMEG);
    }

    /*
     * Mark the invalid pmeg so that it never gets used.
     */
    pmegArray[VMMACH_INV_PMEG].segPtr = vm_SysSegPtr;
    pmegArray[VMMACH_INV_PMEG].flags = PMEG_NEVER_FREE;

    /*
     * Now reserve the rest of the page clusters that have been set up by
     * the monitor.  Don't reserve any PMEGs that don't have any valid 
     * mappings in them.
     */
    for (; i < VMMACH_NUM_SEGS_PER_CONTEXT; i++, segTablePtr++) {
	Address		virtAddr;
	int		j;
	VmMachPTE	pte;
	Boolean		inusePMEG;

	virtAddr = (Address) (i << VMMACH_SEG_SHIFT);
	pageCluster = VmMachGetSegMap(virtAddr);
	if (pageCluster != VMMACH_INV_PMEG) {
	    inusePMEG = FALSE;
	    for (j = 0; 
	         j < VMMACH_NUM_PAGES_PER_SEG_INT; 
		 j++, virtAddr += VMMACH_PAGE_SIZE_INT) {
		pte = VmMachGetPageMap(virtAddr);
		/* printf("%x: %x\n", virtAddr, pte); */
		if ((pte & VMMACH_RESIDENT_BIT) &&
		    (pte & (VMMACH_TYPE_FIELD|VMMACH_PAGE_FRAME_FIELD)) != 0) {
		    /*
		     * A PMEG contains a valid mapping if the resident
		     * bit is set and the page frame and type field
		     * are non-zero.  On Sun 2/50's the PROM sets
		     * the resident bit but leaves the page frame equal
		     * to zero.
		     */
		    if (!inusePMEG) {
			pmegArray[pageCluster].segPtr = vm_SysSegPtr;
			pmegArray[pageCluster].hardSegNum = i;
			pmegArray[pageCluster].flags = PMEG_NEVER_FREE;
			inusePMEG = TRUE;
		    }
		} else {
		    VmMachSetPageMap(virtAddr, (VmMachPTE)0);
		}
	    }
	    virtAddr -= VMMACH_SEG_SIZE;
	    if (!inusePMEG ||
	        (virtAddr >= (Address)VMMACH_DMA_START_ADDR &&
		 virtAddr < (Address)(VMMACH_DMA_START_ADDR+VMMACH_DMA_SIZE))) {
#ifdef PRINT_ZAP
		int z;
		/* 
		 * We didn't find any valid mappings in the PMEG or the PMEG
		 * is in DMA space so delete it.
		 */
		printf("Zapping segment at virtAddr %x\n", virtAddr);
		for (z = 0; z < 100000; z++) {
		}
#endif

		VmMachSetSegMap(virtAddr, VMMACH_INV_PMEG);
		pageCluster = VMMACH_INV_PMEG;
	    }
	}
	*segTablePtr = pageCluster;
    }

#ifdef sun3
    /*
     * We can't use the hardware segment that corresponds to the
     * last segment of physical memory for some reason.  Zero it out
     * and can't reboot w/o powering the machine off.
     */
    dontUse = (*romVectorPtr->memoryAvail - 1) / VMMACH_SEG_SIZE;
#endif

    /*
     * Now finally, all page clusters that have a NIL segment pointer are
     * put onto the page cluster fifo.  On a Sun-3 one hardware segment is 
     * off limits for some reason.  Zero it out and can't reboot w/o 
     * powering the machine off.
     */
    for (i = 0, pmegPtr = pmegArray; 
	 i < VMMACH_NUM_PMEGS;
	 i++, pmegPtr++) {
	if (pmegPtr->segPtr == (Vm_Segment *) NIL 
#ifdef sun3
	    && i != dontUse
#endif
	) {
	    List_Insert((List_Links *) pmegPtr, LIST_ATREAR(pmegFreeList));
	    pmegPtr->flags = 0;
	    VmMachPMEGZero(i);
	}
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_SegInit --
 *
 *      Initialize hardware dependent data for a segment.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Machine dependent data struct and is allocated and initialized.
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_SegInit(segPtr)
    Vm_Segment	*segPtr;
{
    register	VmMach_SegData	*segDataPtr;
    int				segTableSize;

    if (segPtr->type == VM_CODE) {
	segTableSize =
	    (segPtr->ptSize + segPtr->offset + VMMACH_NUM_PAGES_PER_SEG - 1) / 
						    VMMACH_NUM_PAGES_PER_SEG;
    } else {
	segTableSize = segPtr->ptSize / VMMACH_NUM_PAGES_PER_SEG;
    }
    segDataPtr = 
	(VmMach_SegData *)malloc(sizeof(VmMach_SegData) + segTableSize);
    segDataPtr->numSegs = segTableSize;
    segDataPtr->offset = PageToSeg(segPtr->offset);
    segDataPtr->segTablePtr =
	    (unsigned char *) ((Address)segDataPtr + sizeof(VmMach_SegData));
    ByteFill(VMMACH_INV_PMEG, segTableSize, (Address)segDataPtr->segTablePtr);
    segPtr->machPtr = segDataPtr;
    /*
     * Set the minimum and maximum virtual addresses for this segment to
     * be as small and as big as possible respectively because things will
     * be prevented from growing automatically as soon as segments run into
     * each other.
     */
    segPtr->minAddr = (Address)0;
    segPtr->maxAddr = (Address)0x7fffffff;
}

static void	CopySegData();


/*
 *----------------------------------------------------------------------
 *
 * VmMach_SegExpand --
 *
 *	Allocate more space for the machine dependent structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory allocated for a new hardware segment table.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
void
VmMach_SegExpand(segPtr, firstPage, lastPage)
    register	Vm_Segment	*segPtr;	/* Segment to expand. */
    int				firstPage;	/* First page to add. */
    int				lastPage;	/* Last page to add. */
{
    int				newSegTableSize;
    register	VmMach_SegData	*oldSegDataPtr;
    register	VmMach_SegData	*newSegDataPtr;

    newSegTableSize = segPtr->ptSize / VMMACH_NUM_PAGES_PER_SEG;
    oldSegDataPtr = segPtr->machPtr;
    if (newSegTableSize <= oldSegDataPtr->numSegs) {
	return;
    }
    newSegDataPtr = 
	(VmMach_SegData *)malloc(sizeof(VmMach_SegData) + newSegTableSize);
    newSegDataPtr->numSegs = newSegTableSize;
    newSegDataPtr->offset = PageToSeg(segPtr->offset);
    newSegDataPtr->segTablePtr =
	    (unsigned char *) ((Address)newSegDataPtr + sizeof(VmMach_SegData));
    CopySegData(segPtr, oldSegDataPtr, newSegDataPtr);
    free((Address)oldSegDataPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * CopySegData --
 *
 *	Copy over the old hardware segment data into the new expanded
 *	structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The hardware segment table is copied.
 *
 *----------------------------------------------------------------------
 */
ENTRY static void
CopySegData(segPtr, oldSegDataPtr, newSegDataPtr)
    register	Vm_Segment	*segPtr;	/* The segment to add the
						   virtual pages to. */
    register	VmMach_SegData	*oldSegDataPtr;
    register	VmMach_SegData	*newSegDataPtr;
{
    MASTER_LOCK(vmMachMutexPtr);

    if (segPtr->type == VM_HEAP) {
	/*
	 * Copy over the hardware segment table into the lower part
	 * and set the rest to invalid.
	 */
	bcopy((Address)oldSegDataPtr->segTablePtr,
		(Address)newSegDataPtr->segTablePtr, oldSegDataPtr->numSegs);
	ByteFill(VMMACH_INV_PMEG,
	  newSegDataPtr->numSegs - oldSegDataPtr->numSegs,
	  (Address)(newSegDataPtr->segTablePtr + oldSegDataPtr->numSegs));
    } else {
	/*
	 * Copy the current segment table into the high part of the
	 * new segment table and set the lower part to invalid.
	 */
	bcopy((Address)oldSegDataPtr->segTablePtr,
	    (Address)(newSegDataPtr->segTablePtr + 
	    newSegDataPtr->numSegs - oldSegDataPtr->numSegs),
	    oldSegDataPtr->numSegs);
	ByteFill(VMMACH_INV_PMEG, 
		newSegDataPtr->numSegs - oldSegDataPtr->numSegs,
		(Address)newSegDataPtr->segTablePtr);
    }
    segPtr->machPtr = newSegDataPtr;

    MASTER_UNLOCK(vmMachMutexPtr);
}

static void	SegDelete();

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_SegDelete --
 *
 *      Free hardware dependent resources for this software segment.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Machine dependent struct freed and the pointer in the segment
 *	is set to NIL.
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_SegDelete(segPtr)
    register	Vm_Segment	*segPtr;    /* Pointer to segment to free. */
{
    if (vm_Tracing) {
	Vm_TraceSegDestroy	segDestroy;

	segDestroy.segNum = segPtr->segNum;
	VmStoreTraceRec(VM_TRACE_SEG_DESTROY_REC, sizeof(segDestroy),
			(Address)&segDestroy, TRUE);
    }

    SegDelete(segPtr);
    free((Address)segPtr->machPtr);
    segPtr->machPtr = (VmMach_SegData *)NIL;
}


/*
 * ----------------------------------------------------------------------------
 *
 * SegDelete --
 *
 *      Free up any pmegs used by this segment.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      All pmegs used by this segment are freed.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY static void
SegDelete(segPtr)
    Vm_Segment	*segPtr;    /* Pointer to segment to free. */
{
    register	int 		i;
    register	unsigned char 	*pmegPtr;
    register	VmMach_SegData	*machPtr;

    MASTER_LOCK(vmMachMutexPtr);

    machPtr = segPtr->machPtr;
    for (i = 0, pmegPtr = machPtr->segTablePtr;
         i < machPtr->numSegs;
	 i++, pmegPtr++) {
	if (*pmegPtr != VMMACH_INV_PMEG) {
	    PMEGFree((int) *pmegPtr);
	}
    }

    MASTER_UNLOCK(vmMachMutexPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_ProcInit --
 *
 *	Initalize the machine dependent part of the VM proc info.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Machine dependent proc info is initialized.
 *
 *----------------------------------------------------------------------
 */
void
VmMach_ProcInit(vmPtr)
    register	Vm_ProcInfo	*vmPtr;
{
    if (vmPtr->machPtr == (VmMach_ProcData *)NIL) {
	vmPtr->machPtr = (VmMach_ProcData *)malloc(sizeof(VmMach_ProcData));
    }
    vmPtr->machPtr->contextPtr = (VmMach_Context *)NIL;
    vmPtr->machPtr->mapSegPtr = (struct Vm_Segment *)NIL;
    vmPtr->machPtr->sharedData.allocVector = (int *)NIL;
}


/*
 * ----------------------------------------------------------------------------
 *
 * PMEGGet --
 *
 *      Return the next pmeg from the list of available pmegs.  If the 
 *      lock flag is set then the pmeg is removed from the pmeg list.
 *      Otherwise it is moved to the back.
 *
 * Results:
 *      The pmeg number that is allocated.
 *
 * Side effects:
 *      A pmeg is either removed from the pmeg list or moved to the back.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL static int
PMEGGet(softSegPtr, hardSegNum, flags)
    Vm_Segment 	*softSegPtr;	/* Which software segment this is. */
    int		hardSegNum;	/* Which hardware segment in the software 
				   segment that this is */
    Boolean	flags;		/* Flags that indicate the state of the pmeg. */
{
    register PMEG		*pmegPtr;
    register Vm_Segment		*segPtr;
    register VmMachPTE		*ptePtr;
    register VmMach_Context	*contextPtr;
    register int		i;
    register VmMachPTE		hardPTE;
    VmMachPTE			pteArray[VMMACH_NUM_PAGES_PER_SEG_INT];
    int	     			oldContext;
    int				pmegNum;
    Address			virtAddr;
    Boolean			found = FALSE;

    if (List_IsEmpty(pmegFreeList)) {
	LIST_FORALL(pmegInuseList, (List_Links *)pmegPtr) {
	    if (pmegPtr->lockCount == 0) {
		found = TRUE;
		break;
	    }
	}
	if (!found) {
	    panic("Pmeg lists empty\n");
	    return(VMMACH_INV_PMEG);
	}
    } else {
	pmegPtr = (PMEG *)List_First(pmegFreeList);
    }
    pmegNum = pmegPtr - pmegArray;

    if (pmegPtr->segPtr != (Vm_Segment *) NIL) {
	/*
	 * Need to steal the pmeg from its current owner.
	 */
	vmStat.machDepStat.stealPmeg++;
	segPtr = pmegPtr->segPtr;
	*GetHardSegPtr(segPtr->machPtr, pmegPtr->hardSegNum) = VMMACH_INV_PMEG;
	virtAddr = (Address) (pmegPtr->hardSegNum << VMMACH_SEG_SHIFT);
	/*
	 * Delete the pmeg from all appropriate contexts.
	 */
	oldContext = VmMachGetContextReg();
        if (segPtr->type == VM_SYSTEM) {
	    for (i = 0; i < VMMACH_NUM_CONTEXTS; i++) {
		VmMachSetContextReg(i);
		VmMachSetSegMap(virtAddr, VMMACH_INV_PMEG);
	    }
        } else {
	    for (i = 1, contextPtr = &contextArray[1];
		 i < VMMACH_NUM_CONTEXTS; 
		 i++, contextPtr++) {
		if (contextPtr->flags & CONTEXT_IN_USE) {
		    if (contextPtr->map[pmegPtr->hardSegNum] == pmegNum) {
			VmMachSetContextReg(i);
			contextPtr->map[pmegPtr->hardSegNum] = VMMACH_INV_PMEG;
			VmMachSetSegMap(virtAddr, VMMACH_INV_PMEG);
		    }
		    if (contextPtr->map[MAP_SEG_NUM] == pmegNum) {
			VmMachSetContextReg(i);
			contextPtr->map[MAP_SEG_NUM] = VMMACH_INV_PMEG;
			VmMachSetSegMap((Address)VMMACH_MAP_SEG_ADDR,
					VMMACH_INV_PMEG);
		    }
		}
	    }
        }
	VmMachSetContextReg(oldContext);
	/*
	 * Read out all reference and modify bits from the pmeg.
	 */
	if (pmegPtr->pageCount > 0) {
	    Boolean	printedStealPMEG = FALSE;

	    ptePtr = pteArray;
	    VmMachReadAndZeroPMEG(pmegNum, ptePtr);
	    for (i = 0;
		 i < VMMACH_NUM_PAGES_PER_SEG_INT;
		 i++, ptePtr++) {
		hardPTE = *ptePtr;
		if ((hardPTE & VMMACH_RESIDENT_BIT) &&
		    (hardPTE & (VMMACH_REFERENCED_BIT | VMMACH_MODIFIED_BIT))) {
		    if (vm_Tracing) {
			if (!printedStealPMEG) {
			    short	stealPMEGRec;

			    printedStealPMEG = TRUE;
			    printedSegTrace = FALSE;
			    tracePMEGPtr = pmegPtr;
			    VmStoreTraceRec(VM_TRACE_STEAL_PMEG_REC,
					    sizeof(short), 
					    (Address)&stealPMEGRec,TRUE);
			}
			VmMachTracePage(hardPTE,
			    (unsigned int) (VMMACH_NUM_PAGES_PER_SEG_INT - i));
		    } else {
		    refModMap[PhysToVirtPage(hardPTE & VMMACH_PAGE_FRAME_FIELD)]
		     |= hardPTE & (VMMACH_REFERENCED_BIT | VMMACH_MODIFIED_BIT);
		    }
		}
	    }
	 }
    }

    /* Initialize the pmeg and delete it from the fifo.  If we aren't 
     * supposed to lock this pmeg, then put it at the rear of the list.
     */
    pmegPtr->segPtr = softSegPtr;
    pmegPtr->hardSegNum = hardSegNum;
    pmegPtr->pageCount = 0;
    List_Remove((List_Links *) pmegPtr);
    if (!(flags & PMEG_DONT_ALLOC)) {
	List_Insert((List_Links *) pmegPtr, LIST_ATREAR(pmegInuseList));
    }
    pmegPtr->flags = flags;

    return(pmegNum);
}


/*
 * ----------------------------------------------------------------------------
 *
 * PMEGFree --
 *
 *      Return the given pmeg to the pmeg list.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The pmeg is returned to the pmeg list.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
PMEGFree(pmegNum)
    int 	pmegNum;	/* Which pmeg to free */
{
    register	PMEG	*pmegPtr;

    pmegPtr = &pmegArray[pmegNum];
    /*
     * If this pmeg can never be freed then don't free it.  This case can
     * occur when a device is mapped into a user's address space.
     */
    if (pmegPtr->flags & PMEG_NEVER_FREE) {
	return;
    }
    if (pmegPtr->pageCount > 0) {
	VmMachPMEGZero(pmegNum);
    }
    pmegPtr->segPtr = (Vm_Segment *) NIL;
    if (pmegPtr->pageCount == 0 || !(pmegPtr->flags & PMEG_DONT_ALLOC)) {
	List_Remove((List_Links *) pmegPtr);
    }
    pmegPtr->flags = 0;
    pmegPtr->lockCount = 0;
    /*
     * Put this pmeg at the front of the pmeg free list.
     */
    List_Insert((List_Links *) pmegPtr, LIST_ATFRONT(pmegFreeList));
}


/*
 * ----------------------------------------------------------------------------
 *
 * PMEGLock --
 *
 *      Increment the lock count on a pmeg.
 *
 * Results:
 *      TRUE if there was a valid PMEG behind the given hardware segment.
 *
 * Side effects:
 *      The lock count is incremented if there is a valid pmeg at the given
 *	hardware segment.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY static Boolean
PMEGLock(machPtr, segNum)
    register VmMach_SegData	*machPtr;
    int				segNum;
{
    int pmegNum;

    MASTER_LOCK(vmMachMutexPtr);

    pmegNum = *GetHardSegPtr(machPtr, segNum);
    if (pmegNum != VMMACH_INV_PMEG) {
	pmegArray[pmegNum].lockCount++;
	MASTER_UNLOCK(vmMachMutexPtr);
	return(TRUE);
    } else {
	MASTER_UNLOCK(vmMachMutexPtr);
	return(FALSE);
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_SetupContext --
 *
 *      Return the value of the context register for the given process.
 *	It is assumed that this routine is called on a uni-processor right
 *	before the process starts executing.
 *	
 * Results:
 *      None.
 *
 * Side effects:
 *      The context list is modified.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY ClientData
VmMach_SetupContext(procPtr)
    register	Proc_ControlBlock	*procPtr;
{
    register	VmMach_Context	*contextPtr;

    MASTER_LOCK(vmMachMutexPtr);

    while (TRUE) {
	contextPtr = procPtr->vmPtr->machPtr->contextPtr;
	if (contextPtr != (VmMach_Context *)NIL) {
	    if (contextPtr != &contextArray[VMMACH_KERN_CONTEXT]) {
		if (vm_Tracing) {
		    Vm_ProcInfo	*vmPtr;

		    vmPtr = procPtr->vmPtr;
		    vmPtr->segPtrArray[VM_CODE]->traceTime = vmTraceTime;
		    vmPtr->segPtrArray[VM_HEAP]->traceTime = vmTraceTime;
		    vmPtr->segPtrArray[VM_STACK]->traceTime = vmTraceTime;
		}
		List_Move((List_Links *)contextPtr, LIST_ATREAR(contextList));
	    }
	    MASTER_UNLOCK(vmMachMutexPtr);
	    return((ClientData)contextPtr->context);
	}
        SetupContext(procPtr);
    }
}


/*
 * ----------------------------------------------------------------------------
 *
 * SetupContext --
 *
 *      Initialize the context for the given process.  If the process does
 *	not have a context associated with it then one is allocated.
 *
 *	Note that this routine runs unsynchronized even though it is using
 *	internal structures.  See the note above while this is OK.  I
 * 	eliminated the monitor lock because it is unnecessary anyway and
 *	it slows down context-switching.
 *	
 * Results:
 *      None.
 *
 * Side effects:
 *      The context field in the process table entry and the context list are
 * 	both modified if a new context is allocated.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL static void
SetupContext(procPtr)
    register	Proc_ControlBlock	*procPtr;
{
    register	VmMach_Context	*contextPtr;
    register	VmMach_SegData	*segDataPtr;
    register	Vm_ProcInfo	*vmPtr;

    vmPtr = procPtr->vmPtr;
    contextPtr = vmPtr->machPtr->contextPtr;

    if (procPtr->genFlags & (PROC_KERNEL | PROC_NO_VM)) {
	/*
	 * This is a kernel process or a process that is exiting.
	 * Set the context to kernel and return.
	 */
	VmMachSetContextReg(VMMACH_KERN_CONTEXT);
	vmPtr->machPtr->contextPtr = &contextArray[VMMACH_KERN_CONTEXT];
	return;
    }

    if (contextPtr == (VmMach_Context *)NIL) {
	/*
	 * In this case there is no context setup for this process.  Therefore
	 * we have to find a context, initialize the context table entry and 
	 * initialize the context stuff in the proc table.
	 */
	if (List_IsEmpty((List_Links *) contextList)) {
	    panic("SetupContext: Context list empty\n");
	}
	/* 
	 * Take the first context off of the context list.
	 */
	contextPtr = (VmMach_Context *) List_First(contextList);
	if (contextPtr->flags & CONTEXT_IN_USE) {
	    contextPtr->procPtr->vmPtr->machPtr->contextPtr =
							(VmMach_Context *)NIL;
	    vmStat.machDepStat.stealContext++;
	}
	/*
	 * Initialize the context table entry.
	 */
	contextPtr->flags = CONTEXT_IN_USE;
	contextPtr->procPtr = procPtr;
	vmPtr->machPtr->contextPtr = contextPtr;
	VmMachSetContextReg(contextPtr->context);
	/*
	 * Set the context map.
	 */
	ByteFill(VMMACH_INV_PMEG,
		  (int)((unsigned int)mach_KernStart >> VMMACH_SEG_SHIFT),
		  (Address)contextPtr->map);
	segDataPtr = vmPtr->segPtrArray[VM_CODE]->machPtr;
	bcopy((Address)segDataPtr->segTablePtr, 
	    (Address) (contextPtr->map + segDataPtr->offset),
	    segDataPtr->numSegs);
	segDataPtr = vmPtr->segPtrArray[VM_HEAP]->machPtr;
	bcopy((Address)segDataPtr->segTablePtr, 
		(Address) (contextPtr->map + segDataPtr->offset),
		segDataPtr->numSegs);
	segDataPtr = vmPtr->segPtrArray[VM_STACK]->machPtr;
	bcopy((Address)segDataPtr->segTablePtr, 
		(Address) (contextPtr->map + segDataPtr->offset),
		segDataPtr->numSegs);
	if (vmPtr->sharedSegs != (List_Links *)NIL) {
	    Vm_SegProcList *segList;
	    LIST_FORALL(vmPtr->sharedSegs,(List_Links *)segList) {
		segDataPtr = segList->segTabPtr->segPtr->machPtr;
		bcopy((Address)segDataPtr->segTablePtr, 
			(Address) (contextPtr->map+PageToSeg(segList->offset)),
			segDataPtr->numSegs);
	    }
	}
	if (vmPtr->machPtr->mapSegPtr != (struct Vm_Segment *)NIL) {
	    contextPtr->map[MAP_SEG_NUM] = vmPtr->machPtr->mapHardSeg;
	} else {
	    contextPtr->map[MAP_SEG_NUM] = VMMACH_INV_PMEG;
	}
	/*
	 * Push map out to hardware.
	 */
	VmMachSegMapCopy((Address)contextPtr->map, 0, (int)mach_KernStart);
    } else {
	VmMachSetContextReg(contextPtr->context);
    }
    List_Move((List_Links *)contextPtr, LIST_ATREAR(contextList));
}


/*
 * ----------------------------------------------------------------------------
 *
 * Vm_FreeContext --
 *
 *      Free the given context.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The context table and context lists are modified.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmMach_FreeContext(procPtr)
    register	Proc_ControlBlock	*procPtr;
{
    register	VmMach_Context	*contextPtr;
    register	VmMach_ProcData	*machPtr;

    MASTER_LOCK(vmMachMutexPtr);

    machPtr = procPtr->vmPtr->machPtr;
    contextPtr = machPtr->contextPtr;
    if (contextPtr == (VmMach_Context *)NIL ||
        contextPtr->context == VMMACH_KERN_CONTEXT) {
	MASTER_UNLOCK(vmMachMutexPtr);
	return;
    }
    List_Move((List_Links *)contextPtr, LIST_ATFRONT(contextList));
    contextPtr->flags = 0;
    machPtr->contextPtr = (VmMach_Context *)NIL;

    MASTER_UNLOCK(vmMachMutexPtr);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_ReinitContext --
 *
 *      Free the current context and set up another one.  This is called
 *	by routines such as Proc_Exec that add things to the context and
 *	then have to abort or start a process running with a new image.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The context table and context lists are modified.
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_ReinitContext(procPtr)
    register	Proc_ControlBlock	*procPtr;
{
    VmMach_FreeContext(procPtr);
    MASTER_LOCK(vmMachMutexPtr);
    procPtr->vmPtr->machPtr->contextPtr = (VmMach_Context *)NIL;
    SetupContext(procPtr);
    MASTER_UNLOCK(vmMachMutexPtr);
}

#ifdef sun2

static int	 allocatedPMEG;
static VmMachPTE intelSavedPTE;		/* The page table entry that is stored
					 * at the address that the intel page
					 * has to overwrite. */
static unsigned int intelPage;		/* The page frame that was allocated.*/
#endif


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_MapIntelPage --
 *
 *      Allocate and validate a page for the Intel Ethernet chip.  This routine
 *	is required in order to initialize the chip.  The chip expects 
 *	certain stuff to be at a specific virtual address when it is 
 *	initialized.  This routine sets things up so that the expected
 *	virtual address is accessible.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The old pte stored at the virtual address and the page frame that is
 *	allocated are stored in static globals.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
void
VmMach_MapIntelPage(virtAddr) 
    Address	virtAddr; /* Virtual address where a page has to be validated
			     at. */
{
#ifdef sun2
    VmMachPTE		pte;
    int			pmeg;

    /*
     * See if there is a PMEG already.  If not allocate one.
     */

    pmeg = VmMachGetSegMap(virtAddr);
    if (pmeg == VMMACH_INV_PMEG) {
	MASTER_LOCK(vmMachMutexPtr);
	allocatedPMEG = PMEGGet(vm_SysSegPtr, 
				(unsigned)virtAddr >> VMMACH_SEG_SHIFT,
				PMEG_DONT_ALLOC);
	MASTER_UNLOCK(vmMachMutexPtr);
	VmMachSetSegMap(virtAddr, allocatedPMEG);
    } else {
	allocatedPMEG = VMMACH_INV_PMEG;
	intelSavedPTE = VmMachGetPageMap(virtAddr);
    }

    /*
     * Set up the page table entry.
     */
    intelPage = Vm_KernPageAllocate();
    pte = VMMACH_RESIDENT_BIT | VMMACH_KRW_PROT | VirtToPhysPage(intelPage);
    VmMachSetPageMap(virtAddr, pte);
#endif
}


/*
 * ----------------------------------------------------------------------------
 *
 * Vm_UnmapIntelPage --
 *
 *      Deallocate and invalidate a page for the intel chip.  This is a special
 *	case routine that is only for the intel ethernet chip.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The hardware segment table associated with the segment
 *      is modified to invalidate the page.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
void
VmMach_UnmapIntelPage(virtAddr) 
    Address	virtAddr;
{
#ifdef sun2
    if (allocatedPMEG != VMMACH_INV_PMEG) {
	/*
	 * Free up the PMEG.
	 */
	VmMachSetPageMap(virtAddr, (VmMachPTE)0);
	VmMachSetSegMap(virtAddr, VMMACH_INV_PMEG);

	MASTER_LOCK(vmMachMutexPtr);

	PMEGFree(allocatedPMEG);

	MASTER_UNLOCK(vmMachMutexPtr);
    } else {
	/*
	 * Restore the saved pte and free the allocated page.
	 */
	VmMachSetPageMap(virtAddr, intelSavedPTE);
    }
    Vm_KernPageFree(intelPage);
#endif
}

#ifdef sun3

static Address		netMemAddr;
static unsigned int	netLastPage;


/*
 * ----------------------------------------------------------------------------
 *
 * InitNetMem --
 *
 *      Initialize the memory mappings for the network.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      PMEGS are allocated and initialized.
 *
 * ----------------------------------------------------------------------------
 */
static void
InitNetMem()
{
    unsigned char		pmeg;
    register unsigned char	*segTablePtr;
    int				i;
    int				segNum;
    Address			virtAddr;

    /*
     * Allocate two pmegs, one for memory and one for mapping.
     */
    segNum = VMMACH_NET_MAP_START >> VMMACH_SEG_SHIFT;
    for (i = 0, virtAddr = (Address)VMMACH_NET_MAP_START,
	    segTablePtr = GetHardSegPtr(vm_SysSegPtr->machPtr, segNum);
	 i < 2;
         i++, virtAddr += VMMACH_SEG_SIZE, segNum++) {
	pmeg = VmMachGetSegMap(virtAddr);
	if (pmeg == VMMACH_INV_PMEG) {
	    *(segTablePtr + i) = PMEGGet(vm_SysSegPtr, segNum, PMEG_DONT_ALLOC);
	    VmMachSetSegMap(virtAddr, *(segTablePtr + i));
	}
    }
    /*
     * Propagate the new pmeg mappings to all contexts.
     */
    for (i = 0; i < VMMACH_NUM_CONTEXTS; i++) {
	if (i == VMMACH_KERN_CONTEXT) {
	    continue;
	}
	VmMachSetContextReg(i);
	VmMachSetSegMap((Address)VMMACH_NET_MAP_START, *segTablePtr);
	VmMachSetSegMap((Address)(VMMACH_NET_MAP_START + VMMACH_SEG_SIZE),
			*(segTablePtr + 1));
    }
    VmMachSetContextReg(VMMACH_KERN_CONTEXT);
    netMemAddr = (Address)VMMACH_NET_MEM_START;
    netLastPage = ((unsigned)(VMMACH_NET_MEM_START) >> VMMACH_PAGE_SHIFT) - 1;
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_NetMemAlloc --
 *
 *      Allocate physical memory for a network driver.
 *
 * Results:
 *      The address where the memory is allocated at.
 *
 * Side effects:
 *      Memory allocated.
 *
 * ----------------------------------------------------------------------------
 */
Address
VmMach_NetMemAlloc(numBytes)
    int	numBytes;	/* Number of bytes of memory to allocated. */
{
    VmMachPTE	pte;
    Address	retAddr;
    Address	maxAddr;
    Address	virtAddr;
    static Boolean initialized = FALSE;

    if (!initialized) {
	InitNetMem();
	initialized = TRUE;
    }

    retAddr = netMemAddr;
    netMemAddr += (numBytes + 3) & ~3;
    /*
     * Panic if we are out of memory.  We are out of memory if we have filled
     * up a whole PMEG minus one page.  We have to leave one page at the
     * end because this is used to initialize the INTEL chip.
     */
    if (netMemAddr > (Address) (VMMACH_NET_MEM_START + VMMACH_SEG_SIZE - 
				VMMACH_PAGE_SIZE)) {
	panic("VmMach_NetMemAlloc: Out of network memory\n");
    }

    maxAddr = (Address) ((netLastPage + 1) * VMMACH_PAGE_SIZE - 1);

    /*
     * Add new pages to the virtual address space until we have added enough
     * to handle this memory request.
     */
    while (netMemAddr - 1 > maxAddr) {
	maxAddr += VMMACH_PAGE_SIZE;
	netLastPage++;
	virtAddr = (Address) (netLastPage << VMMACH_PAGE_SHIFT);
	pte = VMMACH_RESIDENT_BIT | VMMACH_KRW_PROT |
	      VirtToPhysPage(Vm_KernPageAllocate());
	SET_ALL_PAGE_MAP(virtAddr, pte);
    }

    return(retAddr);
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_NetMapPacket --
 *
 *	Map the packet pointed to by the scatter-gather array.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The outScatGathArray is filled in with pointers to where the
 *	packet was mapped in.
 *
 *----------------------------------------------------------------------
 */
void
VmMach_NetMapPacket(inScatGathPtr, scatGathLength, outScatGathPtr)
    register Net_ScatterGather	*inScatGathPtr;
    register int		scatGathLength;
    register Net_ScatterGather	*outScatGathPtr;
{
    register Address	mapAddr;
    register Address	endAddr;

    for (mapAddr = (Address)VMMACH_NET_MAP_START;
         scatGathLength > 0;
	 scatGathLength--, inScatGathPtr++, outScatGathPtr++) {
	outScatGathPtr->length = inScatGathPtr->length;
	if (inScatGathPtr->length == 0) {
	    continue;
	}
	/*
	 * Map the piece of the packet in.  Note that we know that a packet
	 * piece is no longer than 1536 bytes so we know that we will need
	 * at most two page table entries to map a piece in.
	 */
	VmMachSetPageMap(mapAddr, VmMachGetPageMap(inScatGathPtr->bufAddr));
	outScatGathPtr->bufAddr = 
	    mapAddr + ((unsigned)inScatGathPtr->bufAddr & VMMACH_OFFSET_MASK);
	mapAddr += VMMACH_PAGE_SIZE_INT;
	endAddr = inScatGathPtr->bufAddr + inScatGathPtr->length - 1;
	if (((unsigned)inScatGathPtr->bufAddr & ~VMMACH_OFFSET_MASK_INT) !=
	    ((unsigned)endAddr & ~VMMACH_OFFSET_MASK_INT)) {
	    VmMachSetPageMap(mapAddr, VmMachGetPageMap(endAddr));
	    mapAddr += VMMACH_PAGE_SIZE_INT;
	}
    }
}

#endif


/*
 *----------------------------------------------------------------------
 *
 * VmMach_VirtAddrParse --
 *
 *	See if the given address falls into the special mapping segment.
 *	If so parse it for our caller.
 *
 * Results:
 *	TRUE if the address fell into the special mapping segment, FALSE
 *	otherwise.
 *
 * Side effects:
 *	*transVirtAddrPtr may be filled in.
 *
 *----------------------------------------------------------------------
 */
Boolean
VmMach_VirtAddrParse(procPtr, virtAddr, transVirtAddrPtr)
    Proc_ControlBlock		*procPtr;
    Address			virtAddr;
    register	Vm_VirtAddr	*transVirtAddrPtr;
{
    Address	origVirtAddr;
    Boolean	retVal;

    if (virtAddr >= (Address)VMMACH_MAP_SEG_ADDR && 
        virtAddr < (Address)mach_KernStart) {
	/*
	 * The address falls into the special mapping segment.  Translate
	 * the address back to the segment that it falls into.
	 */
	origVirtAddr = 
	    (Address)(procPtr->vmPtr->machPtr->mapHardSeg << VMMACH_SEG_SHIFT);
	origVirtAddr += (unsigned int)virtAddr & (VMMACH_SEG_SIZE - 1);
	transVirtAddrPtr->segPtr = procPtr->vmPtr->machPtr->mapSegPtr;
	transVirtAddrPtr->page = (unsigned) (origVirtAddr) >> VMMACH_PAGE_SHIFT;
	transVirtAddrPtr->offset = (unsigned)virtAddr & VMMACH_OFFSET_MASK;
	transVirtAddrPtr->flags = USING_MAPPED_SEG;
	retVal = TRUE;
    } else {
	retVal = FALSE;
    }
    return(retVal);
}

static void	WriteHardMapSeg();


/*
 *----------------------------------------------------------------------
 *
 * VmMach_CopyInProc --
 *
 *	Copy from another processes address space into the current address
 *	space.   This is done by mapping the other processes segment into
 *	the current VAS and then doing the copy.  It assumed that this 
 *	routine is called with the source process locked such that its
 *	VM will not go away while we are doing this copy.
 *
 * Results:
 *	SUCCESS if the copy succeeded, SYS_ARG_NOACCESS if fromAddr is invalid.
 *
 * Side effects:
 *	What toAddr points to is modified.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
ReturnStatus
VmMach_CopyInProc(numBytes, fromProcPtr, fromAddr, virtAddrPtr,
	      toAddr, toKernel)
    int 	numBytes;		/* The maximum number of bytes to 
					   copy in. */
    Proc_ControlBlock	*fromProcPtr;	/* Which process to copy from.*/
    Address		fromAddr;	/* The address to copy from */
    Vm_VirtAddr		*virtAddrPtr;
    Address		toAddr;		/* The address to copy to */
    Boolean		toKernel;	/* This copy is happening to the
					 * kernel's address space. */
{
    ReturnStatus		status = SUCCESS;
    register VmMach_ProcData	*machPtr;
    Proc_ControlBlock		*toProcPtr;
    int				segOffset;
    int				bytesToCopy;

    toProcPtr = Proc_GetCurrentProc();
    machPtr = toProcPtr->vmPtr->machPtr;
    machPtr->mapSegPtr = virtAddrPtr->segPtr;
    machPtr->mapHardSeg = (unsigned int) (fromAddr) >> VMMACH_SEG_SHIFT;
    /*
     * Do a hardware segments worth at a time until done.
     */
    while (numBytes > 0 && status == SUCCESS) {
	segOffset = (unsigned int)fromAddr & (VMMACH_SEG_SIZE - 1);
	bytesToCopy = VMMACH_SEG_SIZE - segOffset;
	if (bytesToCopy > numBytes) {
	    bytesToCopy = numBytes;
	}
	/*
	 * Push out the hardware segment.
	 */
	WriteHardMapSeg(machPtr);
	/*
	 * Do the copy.
	 */
	toProcPtr->vmPtr->vmFlags |= VM_COPY_IN_PROGRESS;
	status = VmMachDoCopy(bytesToCopy,
			      (Address)(VMMACH_MAP_SEG_ADDR + segOffset),
			      toAddr);
	toProcPtr->vmPtr->vmFlags &= ~VM_COPY_IN_PROGRESS;
	if (status == SUCCESS) {
	    numBytes -= bytesToCopy;
	    fromAddr += bytesToCopy;
	    toAddr += bytesToCopy;
	} else {
	    status = SYS_ARG_NOACCESS;
	}
	/*
	 * Zap the hardware segment.
	 */
	VmMachSetSegMap((Address)VMMACH_MAP_SEG_ADDR, VMMACH_INV_PMEG); 
	machPtr->mapHardSeg++;
    }
    machPtr->mapSegPtr = (struct Vm_Segment *)NIL;
    return(status);
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_CopyOutProc --
 *
 *	Copy from the current VAS to another processes VAS.  This is done by 
 *	mapping the other processes segment into the current VAS and then 
 *	doing the copy.  It assumed that this routine is called with the dest
 *	process locked such that its VM will not go away while we are doing
 *	the copy.
 *
 * Results:
 *	SUCCESS if the copy succeeded, SYS_ARG_NOACCESS if fromAddr is invalid.
 *
 * Side effects:
 *	What toAddr points to is modified.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
ReturnStatus
VmMach_CopyOutProc(numBytes, fromAddr, fromKernel, toProcPtr, toAddr,
		   virtAddrPtr)
    int 		numBytes;	/* The maximum number of bytes to 
					   copy in. */
    Address		fromAddr;	/* The address to copy from */
    Boolean		fromKernel;	/* This copy is happening to the
					 * kernel's address space. */
    Proc_ControlBlock	*toProcPtr;	/* Which process to copy from.*/
    Address		toAddr;		/* The address to copy to */
    Vm_VirtAddr		*virtAddrPtr;
{
    ReturnStatus		status = SUCCESS;
    register VmMach_ProcData	*machPtr;
    Proc_ControlBlock		*fromProcPtr;
    int				segOffset;
    int				bytesToCopy;


    fromProcPtr = Proc_GetCurrentProc();
    machPtr = fromProcPtr->vmPtr->machPtr;
    machPtr->mapSegPtr = virtAddrPtr->segPtr;
    machPtr->mapHardSeg = (unsigned int) (toAddr) >> VMMACH_SEG_SHIFT;
    /*
     * Do a hardware segments worth at a time until done.
     */
    while (numBytes > 0 && status == SUCCESS) {
	segOffset = (unsigned int)toAddr & (VMMACH_SEG_SIZE - 1);
	bytesToCopy = VMMACH_SEG_SIZE - segOffset;
	if (bytesToCopy > numBytes) {
	    bytesToCopy = numBytes;
	}
	/*
	 * Push out the hardware segment.
	 */
	WriteHardMapSeg(machPtr);
	/*
	 * Do the copy.
	 */
	fromProcPtr->vmPtr->vmFlags |= VM_COPY_IN_PROGRESS;
	status = VmMachDoCopy(bytesToCopy, fromAddr,
			  (Address) (VMMACH_MAP_SEG_ADDR + segOffset));
	fromProcPtr->vmPtr->vmFlags &= ~VM_COPY_IN_PROGRESS;
	if (status == SUCCESS) {
	    numBytes -= bytesToCopy;
	    fromAddr += bytesToCopy;
	    toAddr += bytesToCopy;
	} else {
	    status = SYS_ARG_NOACCESS;
	}
	/*
	 * Zap the hardware segment.
	 */
	VmMachSetSegMap((Address)VMMACH_MAP_SEG_ADDR, VMMACH_INV_PMEG); 

	machPtr->mapHardSeg++;
    }
    machPtr->mapSegPtr = (struct Vm_Segment *)NIL;
    return(status);
}


/*
 *----------------------------------------------------------------------
 *
 * WriteHardMapSeg --
 *
 *	Push the hardware segment map entry out to the hardware for the
 *	given mapped segment.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Hardware segment modified.
 *
 *----------------------------------------------------------------------
 */
ENTRY static void
WriteHardMapSeg(machPtr)
    VmMach_ProcData	*machPtr;
{
    MASTER_LOCK(vmMachMutexPtr);

    if (machPtr->contextPtr != (VmMach_Context *) NIL) {
        machPtr->contextPtr->map[MAP_SEG_NUM] =
            (int)*GetHardSegPtr(machPtr->mapSegPtr->machPtr,
            machPtr->mapHardSeg);
    }
    VmMachSetSegMap((Address)VMMACH_MAP_SEG_ADDR, 
	 *GetHardSegPtr(machPtr->mapSegPtr->machPtr, machPtr->mapHardSeg));

    MASTER_UNLOCK(vmMachMutexPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_SetSegProt --
 *
 *	Change the protection in the page table for the given range of bytes
 *	for the given segment.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Page table may be modified for the segment.
 *
 *----------------------------------------------------------------------
 */
ENTRY void
VmMach_SetSegProt(segPtr, firstPage, lastPage, makeWriteable)
    register Vm_Segment		*segPtr;    /* Segment to change protection
					       for. */
    register int		firstPage;  /* First page to set protection
					     * for. */
    int				lastPage;   /* First page to set protection
					     * for. */
    Boolean			makeWriteable;/* TRUE => make the pages 
					       *	 writable.
					       * FALSE => make readable only.*/
{
    register	VmMachPTE	pte;
    register	Address		virtAddr;
    register	unsigned char	*pmegNumPtr;
    register	PMEG		*pmegPtr;
    register	Boolean		skipSeg = FALSE;
    Boolean			nextSeg = TRUE;
    Address			tVirtAddr;
    Address			pageVirtAddr;
    int				i;

    MASTER_LOCK(vmMachMutexPtr);

    pmegNumPtr = GetHardSegPtr(segPtr->machPtr, PageToSeg(firstPage)) - 1;
    virtAddr = (Address)(firstPage << VMMACH_PAGE_SHIFT);
    while (firstPage <= lastPage) {
	if (nextSeg) {
	    pmegNumPtr++;
	    if (*pmegNumPtr != VMMACH_INV_PMEG) {
		pmegPtr = &pmegArray[*pmegNumPtr];
		if (pmegPtr->pageCount != 0) {
		    VmMachSetSegMap(vmMachPTESegAddr, *pmegNumPtr);
		    skipSeg = FALSE;
		} else {
		    skipSeg = TRUE;
		}
	    } else {
		skipSeg = TRUE;
	    }
	    nextSeg = FALSE;
	}
	if (!skipSeg) {
	    /*
	     * Change the hardware page table.
	     */
	    tVirtAddr =
		((unsigned int)virtAddr & VMMACH_PAGE_MASK) + vmMachPTESegAddr;
	    for (i = 0; i < VMMACH_CLUSTER_SIZE; i++) {
		pageVirtAddr = tVirtAddr + i * VMMACH_PAGE_SIZE_INT;
		pte = VmMachGetPageMap(pageVirtAddr);
		if (pte & VMMACH_RESIDENT_BIT) {
		    Vm_TracePTEChange	pteChange;
		    if (vm_Tracing) {
			pteChange.changeType = VM_TRACE_SET_SEG_PROT;
			pteChange.segNum = segPtr->segNum;
			pteChange.pageNum = firstPage;
			pteChange.softPTE = FALSE;
			pteChange.beforePTE = pte;
		    }
		    pte &= ~VMMACH_PROTECTION_FIELD;
		    pte |= makeWriteable ? VMMACH_URW_PROT : VMMACH_UR_PROT;
		    if (vm_Tracing) {
			pteChange.afterPTE = pte;
			VmStoreTraceRec(VM_TRACE_PTE_CHANGE_REC,
					 sizeof(Vm_TracePTEChange),
					 (Address)&pteChange, TRUE);
		    }
		    VmMachSetPageMap(pageVirtAddr, pte);
		}
	    }
	    virtAddr += VMMACH_PAGE_SIZE;
	    firstPage++;
	    if (((unsigned int)virtAddr & VMMACH_PAGE_MASK) == 0) {
		nextSeg = TRUE;
	    }
	} else {
	    int	segNum;

	    segNum = PageToSeg(firstPage) + 1;
	    firstPage = SegToPage(segNum);
	    virtAddr = (Address)(firstPage << VMMACH_PAGE_SHIFT);
	    nextSeg = TRUE;
	}
    }

    MASTER_UNLOCK(vmMachMutexPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_SetPageProt --
 *
 *	Set the protection in hardware and software for the given virtual
 *	page.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Page table may be modified for the segment.
 *
 *----------------------------------------------------------------------
 */
ENTRY void
VmMach_SetPageProt(virtAddrPtr, softPTE)
    register	Vm_VirtAddr	*virtAddrPtr;	/* The virtual page to set the
						 * protection for.*/
    Vm_PTE			softPTE;	/* Software pte. */
{
    register	VmMachPTE 	hardPTE;
    register	VmMach_SegData	*machPtr;
    Address   			virtAddr;
    int				pmegNum;
    int				i;
    Vm_TracePTEChange		pteChange;

    MASTER_LOCK(vmMachMutexPtr);

    machPtr = virtAddrPtr->segPtr->machPtr;
    pmegNum = *GetHardSegPtr(machPtr, PageToOffSeg(virtAddrPtr->page,
	    virtAddrPtr));
    if (pmegNum != VMMACH_INV_PMEG) {
	virtAddr = ((virtAddrPtr->page << VMMACH_PAGE_SHIFT) & 
			VMMACH_PAGE_MASK) + vmMachPTESegAddr;	
	for (i = 0; 
	     i < VMMACH_CLUSTER_SIZE; 
	     i++, virtAddr += VMMACH_PAGE_SIZE_INT) {
	    hardPTE = VmMachReadPTE(pmegNum, virtAddr);
	    if (vm_Tracing) {
		pteChange.changeType = VM_TRACE_SET_PAGE_PROT;
		pteChange.segNum = virtAddrPtr->segPtr->segNum;
		pteChange.pageNum = virtAddrPtr->page;
		pteChange.softPTE = FALSE;
		pteChange.beforePTE = hardPTE;
	    }
	    hardPTE &= ~VMMACH_PROTECTION_FIELD;
	    hardPTE |= (softPTE & (VM_COW_BIT | VM_READ_ONLY_PROT)) ? 
					VMMACH_UR_PROT : VMMACH_URW_PROT;
	    if (vm_Tracing) {
		pteChange.afterPTE = hardPTE;
		VmStoreTraceRec(VM_TRACE_PTE_CHANGE_REC,
				 sizeof(Vm_TracePTEChange), 
				 (Address)&pteChange, TRUE);
	    }
	    VmMachWritePTE(pmegNum, virtAddr, hardPTE);
	}
    }

    MASTER_UNLOCK(vmMachMutexPtr);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_AllocCheck --
 *
 *      Determine if this page can be reallocated.  A page can be reallocated
 *	if it has not been referenced or modified.
 *  
 * Results:
 *      None.
 *
 * Side effects:
 *      The given page will be invalidated in the hardware if it has not
 *	been referenced and *refPtr and *modPtr will have the hardware 
 *	reference and modify bits or'd in.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmMach_AllocCheck(virtAddrPtr, virtFrameNum, refPtr, modPtr)
    register	Vm_VirtAddr	*virtAddrPtr;
    unsigned	int		virtFrameNum;
    register	Boolean		*refPtr;
    register	Boolean		*modPtr;
{
    register VmMach_SegData	*machPtr;
    register VmMachPTE 		hardPTE;  
    int				pmegNum; 
    Address			virtAddr;
    int				i;
    int				origMod;

    MASTER_LOCK(vmMachMutexPtr);

    origMod = *modPtr;

    *refPtr |= refModMap[virtFrameNum] & VMMACH_REFERENCED_BIT;
    *modPtr = refModMap[virtFrameNum] & VMMACH_MODIFIED_BIT;
    if (!*refPtr || !*modPtr) {
	machPtr = virtAddrPtr->segPtr->machPtr;
	pmegNum = *GetHardSegPtr(machPtr, PageToOffSeg(virtAddrPtr->page,
		virtAddrPtr));
	if (pmegNum != VMMACH_INV_PMEG) {
	    hardPTE = 0;
	    virtAddr = 
		((virtAddrPtr->page << VMMACH_PAGE_SHIFT) & VMMACH_PAGE_MASK) + 
		    vmMachPTESegAddr;
	    for (i = 0; i < VMMACH_CLUSTER_SIZE; i++ ) {
		hardPTE |= VmMachReadPTE(pmegNum, 
					virtAddr + i * VMMACH_PAGE_SIZE_INT);
	    }
	    *refPtr |= hardPTE & VMMACH_REFERENCED_BIT;
	    *modPtr |= hardPTE & VMMACH_MODIFIED_BIT;
	}
    }
    if (!*refPtr) {
	/*
	 * Invalidate the page so that it will force a fault if it is
	 * referenced.  Since our caller has blocked all faults on this
	 * page, by invalidating it we can guarantee that the reference and
	 * modify information that we are returning will be valid until
	 * our caller reenables faults on this page.
	 */
	PageInvalidate(virtAddrPtr, virtFrameNum, FALSE);

	if (origMod && !*modPtr) {
	    /*
	     * This page had the modify bit set in software but not in
	     * hardware.
	     */
	    vmStat.notHardModPages++;
	}
    }
    *modPtr |= origMod;

    MASTER_UNLOCK(vmMachMutexPtr);

}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_GetRefModBits --
 *
 *      Pull the reference and modified bits out of hardware.
 *  
 * Results:
 *      None.
 *
 * Side effects:
 *      
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmMach_GetRefModBits(virtAddrPtr, virtFrameNum, refPtr, modPtr)
    register	Vm_VirtAddr	*virtAddrPtr;
    unsigned	int		virtFrameNum;
    register	Boolean		*refPtr;
    register	Boolean		*modPtr;
{
    register VmMach_SegData	*machPtr;
    register VmMachPTE 		hardPTE;  
    int				pmegNum; 
    Address			virtAddr;
    int				i;

    MASTER_LOCK(vmMachMutexPtr);

    *refPtr = refModMap[virtFrameNum] & VMMACH_REFERENCED_BIT;
    *modPtr = refModMap[virtFrameNum] & VMMACH_MODIFIED_BIT;
    if (!*refPtr || !*modPtr) {
	machPtr = virtAddrPtr->segPtr->machPtr;
	pmegNum = *GetHardSegPtr(machPtr, PageToOffSeg(virtAddrPtr->page,
		virtAddrPtr));
	if (pmegNum != VMMACH_INV_PMEG) {
	    hardPTE = 0;
	    virtAddr = 
		((virtAddrPtr->page << VMMACH_PAGE_SHIFT) & VMMACH_PAGE_MASK) + 
		    vmMachPTESegAddr;
	    for (i = 0; i < VMMACH_CLUSTER_SIZE; i++ ) {
		hardPTE |= VmMachReadPTE(pmegNum, 
					virtAddr + i * VMMACH_PAGE_SIZE_INT);
	    }
	    if (!*refPtr) {
		*refPtr = hardPTE & VMMACH_REFERENCED_BIT;
	    }
	    if (!*modPtr) {
		*modPtr = hardPTE & VMMACH_MODIFIED_BIT;
	    }
	}
    }

    MASTER_UNLOCK(vmMachMutexPtr);

}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_ClearRefBit --
 *
 *      Clear the reference bit at the given virtual address.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Hardware reference bit cleared.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmMach_ClearRefBit(virtAddrPtr, virtFrameNum)
    register	Vm_VirtAddr	*virtAddrPtr;
    unsigned 	int		virtFrameNum;
{
    register	VmMach_SegData	*machPtr;
    int				pmegNum;
    Address			virtAddr;
    int				i;
    VmMachPTE			pte;
    Vm_TracePTEChange		pteChange;

    MASTER_LOCK(vmMachMutexPtr);

    refModMap[virtFrameNum] &= ~VMMACH_REFERENCED_BIT;
    machPtr = virtAddrPtr->segPtr->machPtr;
    pmegNum = *GetHardSegPtr(machPtr, PageToOffSeg(virtAddrPtr->page,
	    virtAddrPtr));
    if (pmegNum != VMMACH_INV_PMEG) {
	virtAddr = ((virtAddrPtr->page << VMMACH_PAGE_SHIFT) & 
			VMMACH_PAGE_MASK) + vmMachPTESegAddr;
	for (i = 0; 
	     i < VMMACH_CLUSTER_SIZE;
	     i++, virtAddr += VMMACH_PAGE_SIZE_INT) {
	    pte = VmMachReadPTE(pmegNum, virtAddr);
	    if (vm_Tracing) {
		pteChange.changeType = VM_TRACE_CLEAR_REF_BIT;
		pteChange.segNum = virtAddrPtr->segPtr->segNum;
		pteChange.pageNum = virtAddrPtr->page;
		pteChange.softPTE = FALSE;
		pteChange.beforePTE = pte;
		pteChange.afterPTE = pte & ~VMMACH_REFERENCED_BIT;
		VmStoreTraceRec(VM_TRACE_PTE_CHANGE_REC,
				 sizeof(Vm_TracePTEChange), 
				 (Address)&pteChange, TRUE);
	    }
	    pte &= ~VMMACH_REFERENCED_BIT;
	    VmMachWritePTE(pmegNum, virtAddr, pte);
	}
    }

    MASTER_UNLOCK(vmMachMutexPtr);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_ClearModBit --
 *
 *      Clear the modified bit at the given virtual address.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Hardware modified bit cleared.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmMach_ClearModBit(virtAddrPtr, virtFrameNum)
    register	Vm_VirtAddr	*virtAddrPtr;
    unsigned	int		virtFrameNum;
{
    register	VmMach_SegData	*machPtr;
    int				pmegNum;
    Address			virtAddr;
    int				i;
    Vm_PTE			pte;
    Vm_TracePTEChange		pteChange;

    MASTER_LOCK(vmMachMutexPtr);

    refModMap[virtFrameNum] &= ~VMMACH_MODIFIED_BIT;
    machPtr = virtAddrPtr->segPtr->machPtr;
    pmegNum = *GetHardSegPtr(machPtr, PageToOffSeg(virtAddrPtr->page,
	    virtAddrPtr));
    if (pmegNum != VMMACH_INV_PMEG) {
	virtAddr = ((virtAddrPtr->page << VMMACH_PAGE_SHIFT) & 
			VMMACH_PAGE_MASK) + vmMachPTESegAddr;
	for (i = 0; 
	     i < VMMACH_CLUSTER_SIZE; 
	     i++, virtAddr += VMMACH_PAGE_SIZE_INT) {
	    pte = VmMachReadPTE(pmegNum, virtAddr);
	    if (vm_Tracing) {
		pteChange.changeType = VM_TRACE_CLEAR_MOD_BIT;
		pteChange.segNum = virtAddrPtr->segPtr->segNum;
		pteChange.pageNum = virtAddrPtr->page;
		pteChange.softPTE = FALSE;
		pteChange.beforePTE = pte;
		pteChange.afterPTE = pte & ~VMMACH_MODIFIED_BIT;
		VmStoreTraceRec(VM_TRACE_PTE_CHANGE_REC,
				sizeof(Vm_TracePTEChange), 
				(Address)&pteChange, TRUE);
	    }
	    pte &= ~VMMACH_MODIFIED_BIT;
	    VmMachWritePTE(pmegNum, virtAddr, pte);
	}
    }

    MASTER_UNLOCK(vmMachMutexPtr);
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_PageValidate --
 *
 *      Validate a page for the given virtual address.  It is assumed that when
 *      this routine is called that the user context register contains the
 *	context in which the page will be validated.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The page table and hardware segment tables associated with the segment
 *      are modified to validate the page.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmMach_PageValidate(virtAddrPtr, pte) 
    register	Vm_VirtAddr	*virtAddrPtr;
    Vm_PTE			pte;
{
    register  Vm_Segment	*segPtr;
    register  unsigned  char	*segTablePtr;
    register  PMEG		*pmegPtr;
    register  int		hardSeg;
    register  int		newPMEG;
    register  VmMachPTE		hardPTE;
    register  VmMachPTE		tHardPTE;
    Address			addr;
    int				i;

    MASTER_LOCK(vmMachMutexPtr);

    segPtr = virtAddrPtr->segPtr;
    addr = (Address) (virtAddrPtr->page << VMMACH_PAGE_SHIFT);

    /*
     * Find out the hardware segment that has to be mapped.
     */
    hardSeg = PageToOffSeg(virtAddrPtr->page,virtAddrPtr);
    segTablePtr = GetHardSegPtr(segPtr->machPtr, hardSeg);

    if (*segTablePtr == VMMACH_INV_PMEG) {
	/*
	 * If there is not already a pmeg for this hardware segment, then get
	 * one and initialize it.  If this is for the kernel then make
	 * sure that the pmeg cannot be taken away from the kernel.
	 */
	if (segPtr == vm_SysSegPtr) {
	    newPMEG = PMEGGet(segPtr, hardSeg, PMEG_DONT_ALLOC);
	} else {
	    newPMEG = PMEGGet(segPtr, hardSeg, 0);
	}
        *segTablePtr = newPMEG;
    } else {
	pmegPtr = &pmegArray[*segTablePtr];
	if (pmegPtr->pageCount == 0) {
	    /*
	     * We are using a PMEG that had a pagecount of 0.  In this case
	     * it was put onto the end of the free pmeg list in anticipation
	     * of someone stealing this empty pmeg.  Now we have to move
	     * it off of the free list.
	     */
	    if (pmegPtr->flags & PMEG_DONT_ALLOC) {
		List_Remove((List_Links *)pmegPtr);
	    } else {
		List_Move((List_Links *)pmegPtr, LIST_ATREAR(pmegInuseList));
	    }
	}
    }
    hardPTE = VMMACH_RESIDENT_BIT | VirtToPhysPage(Vm_GetPageFrame(pte));
    if (segPtr == vm_SysSegPtr) {
	int	oldContext;
	/*
	 * Have to propagate the PMEG to all contexts.
	 */
	oldContext = VmMachGetContextReg();
	for (i = 0; i < VMMACH_NUM_CONTEXTS; i++) {
	    VmMachSetContextReg(i);
	    VmMachSetSegMap(addr, *segTablePtr);
	}
	VmMachSetContextReg(oldContext);
	hardPTE |= VMMACH_KRW_PROT;
    } else {
	Proc_ControlBlock	*procPtr;
        VmProcLink              *procLinkPtr;
        VmMach_Context          *contextPtr;

	procPtr = Proc_GetCurrentProc();
	if (virtAddrPtr->flags & USING_MAPPED_SEG) {
	    addr = (Address) (VMMACH_MAP_SEG_ADDR + 
				((unsigned int)addr & (VMMACH_SEG_SIZE - 1)));
            /* PUT IT IN SOFTWARE OF MAP AREA FOR PROCESS */
            procPtr->vmPtr->machPtr->contextPtr->map[MAP_SEG_NUM] =
                    *segTablePtr;
        } else{
            /* update it for regular seg num */
            procPtr->vmPtr->machPtr->contextPtr->map[hardSeg] = *segTablePtr;
        }
	VmMachSetSegMap(addr, *segTablePtr);
        if (segPtr != (Vm_Segment *) NIL) {
            VmCheckListIntegrity((List_Links *)segPtr->procList);
            LIST_FORALL(segPtr->procList, (List_Links *)procLinkPtr) {
                if (procLinkPtr->procPtr->vmPtr != (Vm_ProcInfo *) NIL &&
                        procLinkPtr->procPtr->vmPtr->machPtr !=
                        (VmMach_ProcData *) NIL &&
                        (contextPtr =
                        procLinkPtr->procPtr->vmPtr->machPtr->contextPtr) !=
                        (VmMach_Context *) NIL) {
                    contextPtr->map[hardSeg] = *segTablePtr;
                }
            }
        }


	if ((pte & (VM_COW_BIT | VM_READ_ONLY_PROT)) ||
		(virtAddrPtr->flags & VM_READONLY_SEG)) {
	    hardPTE |= VMMACH_UR_PROT;
	} else {
	    hardPTE |= VMMACH_URW_PROT;
	}
    }
    tHardPTE = VmMachGetPageMap(addr);
    if (tHardPTE & VMMACH_RESIDENT_BIT) {
	hardPTE |= tHardPTE & (VMMACH_REFERENCED_BIT | VMMACH_MODIFIED_BIT);
	for (i = 1; i < VMMACH_CLUSTER_SIZE; i++ ) {
	    hardPTE |= VmMachGetPageMap(addr + i * VMMACH_PAGE_SIZE_INT) & 
			    (VMMACH_REFERENCED_BIT | VMMACH_MODIFIED_BIT);
	}
    } else {
	pmegArray[*segTablePtr].pageCount++;
    }
    if (vm_Tracing) {
	Vm_TracePTEChange	pteChange;

	pteChange.changeType = VM_TRACE_VALIDATE_PAGE;
	pteChange.segNum = segPtr->segNum;
	pteChange.pageNum = virtAddrPtr->page;
	pteChange.softPTE = FALSE;
	if (tHardPTE & VMMACH_RESIDENT_BIT) {
	    pteChange.beforePTE = tHardPTE;
	} else {
	    pteChange.beforePTE = 0;
	}
	pteChange.afterPTE = hardPTE;
	VmStoreTraceRec(VM_TRACE_PTE_CHANGE_REC,
			sizeof(Vm_TracePTEChange), 
			(Address)&pteChange, TRUE);
    }

    SET_ALL_PAGE_MAP(addr, hardPTE);

    MASTER_UNLOCK(vmMachMutexPtr);
}


/*
 * ----------------------------------------------------------------------------
 *
 * PageInvalidate --
 *
 *      Invalidate a page for the given segment.  
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The page table and hardware segment tables associated with the segment
 *      are modified to invalidate the page.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL void
PageInvalidate(virtAddrPtr, virtPage, segDeletion) 
    register	Vm_VirtAddr	*virtAddrPtr;
    unsigned 	int		virtPage;
    Boolean			segDeletion;
{
    register VmMach_SegData	*machPtr;
    register PMEG		*pmegPtr;
    VmMachPTE			hardPTE;
    int				pmegNum;
    Address			addr;
    int				i;
    Vm_TracePTEChange		pteChange;

    refModMap[virtPage] = 0;
    if (segDeletion) {
	return;
    }
    machPtr = virtAddrPtr->segPtr->machPtr;
    pmegNum = *GetHardSegPtr(machPtr, PageToOffSeg(virtAddrPtr->page,
	    virtAddrPtr));
    if (pmegNum == VMMACH_INV_PMEG) {
	return;
    }
    addr = ((virtAddrPtr->page << VMMACH_PAGE_SHIFT) &
				VMMACH_PAGE_MASK) + vmMachPTESegAddr;
    hardPTE = VmMachReadPTE(pmegNum, addr);
    if (vm_Tracing) {
	pteChange.changeType = VM_TRACE_INVALIDATE_PAGE;
	pteChange.segNum = virtAddrPtr->segPtr->segNum;
	pteChange.pageNum = virtAddrPtr->page;
	pteChange.softPTE = FALSE;
	pteChange.beforePTE = hardPTE;
	pteChange.afterPTE = 0;
	VmStoreTraceRec(VM_TRACE_PTE_CHANGE_REC, sizeof(Vm_TracePTEChange),
			(Address)&pteChange, TRUE);
    }
    /*
     * Invalidate the page table entry.
     */
    for (i = 0; i < VMMACH_CLUSTER_SIZE; i++, addr += VMMACH_PAGE_SIZE_INT) {
	VmMachWritePTE(pmegNum, addr, (VmMachPTE)0);
    }
    pmegPtr = &pmegArray[pmegNum];
    if (hardPTE & VMMACH_RESIDENT_BIT) {
	pmegPtr->pageCount--;
	if (pmegPtr->pageCount == 0) {
	    /*
	     * When the pageCount goes to zero, the pmeg is put onto the end
	     * of the free list so that it can get freed if someone else
	     * needs a pmeg.  It isn't freed here because there is a fair
	     * amount of overhead when freeing a pmeg so its best to keep
	     * it around in case it is needed again.
	     */
	    if (pmegPtr->flags & PMEG_DONT_ALLOC) {
		List_Insert((List_Links *)pmegPtr, 
			    LIST_ATREAR(pmegFreeList));
	    } else {
		List_Move((List_Links *)pmegPtr, 
			  LIST_ATREAR(pmegFreeList));
	    }
	}
    }
}



/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_PageInvalidate --
 *
 *      Invalidate a page for the given segment.  
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The page table and hardware segment tables associated with the segment
 *      are modified to invalidate the page.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmMach_PageInvalidate(virtAddrPtr, virtPage, segDeletion) 
    register	Vm_VirtAddr	*virtAddrPtr;
    unsigned 	int		virtPage;
    Boolean			segDeletion;
{
    MASTER_LOCK(vmMachMutexPtr);

    PageInvalidate(virtAddrPtr, virtPage, segDeletion);

    MASTER_UNLOCK(vmMachMutexPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_PinUserPages --
 *
 *	Force a user page to be resident in memory.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
void
VmMach_PinUserPages(mapType, virtAddrPtr, lastPage)
    int		mapType;
    Vm_VirtAddr	*virtAddrPtr;
    int		lastPage;
{
    int				*intPtr;
    int				dummy;
    register VmMach_SegData	*machPtr;
    register int		firstSeg;
    register int		lastSeg;

    machPtr = virtAddrPtr->segPtr->machPtr;

    firstSeg = PageToOffSeg(virtAddrPtr->page,virtAddrPtr);
    lastSeg = PageToOffSeg(lastPage,virtAddrPtr);
    /*
     * Lock down the PMEG behind the first segment.
     */
    intPtr = (int *) (virtAddrPtr->page << VMMACH_PAGE_SHIFT);
    while (!PMEGLock(machPtr, firstSeg)) {
	/*
	 * Touch the page to bring it into memory.  We know that we can
	 * safely touch it because we wouldn't have been called if these
	 * weren't good addresses.
	 */
	dummy = *intPtr;
    }
    /*
     * Lock down the rest of the segments.
     */
    for (firstSeg++; firstSeg <= lastSeg; firstSeg++) {
	intPtr = (int *)(firstSeg << VMMACH_SEG_SHIFT);
	while (!PMEGLock(machPtr, firstSeg)) {
	    dummy = *intPtr;
	}
    }
#ifdef lint
    dummy = dummy;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_UnpinUserPages --
 *
 *	Allow a page that was pinned to be unpinned.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
ENTRY void
VmMach_UnpinUserPages(virtAddrPtr, lastPage)
    Vm_VirtAddr	*virtAddrPtr;
    int		lastPage;
{
    register int	firstSeg;
    register int	lastSeg;
    int			pmegNum;
    register VmMach_SegData	*machPtr;

    MASTER_LOCK(vmMachMutexPtr);

    machPtr = virtAddrPtr->segPtr->machPtr;
    firstSeg = PageToOffSeg(virtAddrPtr->page, virtAddrPtr);
    lastSeg = PageToOffSeg(lastPage, virtAddrPtr);
    for (; firstSeg <= lastSeg; firstSeg++) {
	pmegNum = *GetHardSegPtr(machPtr, firstSeg);
	if (pmegNum == VMMACH_INV_PMEG) {
	    MASTER_UNLOCK(vmMachMutexPtr);
	    panic("Pinned PMEG invalid???\n");
	    return;
	}
	pmegArray[pmegNum].lockCount--;
    }

    MASTER_UNLOCK(vmMachMutexPtr);
}


/*
 ----------------------------------------------------------------------
 *
 * VmMach_MapInDevice --
 *
 *	Map a device at some physical address into kernel virtual address.
 *	This is for use by the controller initialization routines.
 *	This routine looks for a free page in the special range of
 *	kernel virtual that is reserved for this kind of thing and
 *	sets up the page table so that it references the device.
 *
 * Results:
 *	The kernel virtual address needed to reference the device is returned.
 *
 * Side effects:
 *	The hardware page table is modified.  This may steal another
 *	page from kernel virtual space, unless a page can be cleverly re-used.
 *
 *----------------------------------------------------------------------
 */
Address
VmMach_MapInDevice(devPhysAddr, type)
    Address	devPhysAddr;	/* Physical address of the device to map in */
    int		type;		/* Value for the page table entry type field.
				 * This depends on the address space that
				 * the devices live in, ie. VME D16 or D32 */
{
    Address 		virtAddr;
    Address		freeVirtAddr = (Address)0;
    Address		freePMEGAddr = (Address)0;
    int			page;
    int			pageFrame;
    VmMachPTE		pte;

    /*
     * Get the page frame for the physical device so we can
     * compare it against existing pte's.
     */
    pageFrame = (unsigned)devPhysAddr >> VMMACH_PAGE_SHIFT_INT;

    /*
     * Spin through the segments and their pages looking for a free
     * page or a virtual page that is already mapped to the physical page.
     */
    for (virtAddr = (Address)VMMACH_DEV_START_ADDR;
         virtAddr < (Address)VMMACH_DEV_END_ADDR; ) {
	if (VmMachGetSegMap(virtAddr) == VMMACH_INV_PMEG) {
	    /* 
	     * If we can't find any free mappings we can use this PMEG.
	     */
	    if (freePMEGAddr == 0) {
		freePMEGAddr = virtAddr;
	    }
	    virtAddr += VMMACH_SEG_SIZE;
	    continue;
	}
	/*
	 * Careful, use the correct page size when incrementing virtAddr.
	 * Use the real hardware size (ignore software klustering) because
	 * we are at a low level munging page table entries ourselves here.
	 */
	for (page = 0;
	     page < VMMACH_NUM_PAGES_PER_SEG_INT;
	     page++, virtAddr += VMMACH_PAGE_SIZE_INT) {
	    pte = VmMachGetPageMap(virtAddr);
	    if (!(pte & VMMACH_RESIDENT_BIT)) {
	        if (freeVirtAddr == 0) {
		    /*
		     * Note the unused page in this special area of the
		     * kernel virtual address space.
		     */
		    freeVirtAddr = virtAddr;
		}
	    } else if ((pte & VMMACH_PAGE_FRAME_FIELD) == pageFrame &&
		       VmMachGetPageType(pte) == type) {
		/*
		 * A page is already mapped for this physical address.
		 */
		return(virtAddr + ((int)devPhysAddr & VMMACH_OFFSET_MASK));
	    }
	}
    }

    pte = VMMACH_RESIDENT_BIT | VMMACH_KRW_PROT | pageFrame;
#ifdef sun3
    pte |= VMMACH_DONT_CACHE_BIT;
#endif
    VmMachSetPageType(pte, type);
    if (freeVirtAddr != 0) {
	VmMachSetPageMap(freeVirtAddr, pte);
	/*
	 * Return the kernel virtual address used to access it.
	 */
	return(freeVirtAddr + ((int)devPhysAddr & VMMACH_OFFSET_MASK));
    } else if (freePMEGAddr != 0) {
	int oldContext;
	int pmeg;
	int i;

	/*
	 * Map in a new PMEG so we can use it for mapping.
	 */
	pmeg = PMEGGet(vm_SysSegPtr, 
		       (int) ((unsigned)freePMEGAddr >> VMMACH_SEG_SHIFT),
		       PMEG_DONT_ALLOC);
	oldContext = VmMachGetContextReg();
	for (i = 0; i < VMMACH_NUM_CONTEXTS; i++) {
	    VmMachSetContextReg(i);
	    VmMachSetSegMap(freePMEGAddr, (unsigned char) pmeg);
	}
	VmMachSetContextReg(oldContext);
	VmMachSetPageMap(freePMEGAddr, pte);
	return(freePMEGAddr + ((int)devPhysAddr & VMMACH_OFFSET_MASK));
    } else {
	return((Address)NIL);
    }
}

/*----------------------------------------------------------------------
 *
 * DevBufferInit --
 *
 *	Initialize a range of virtual memory to allocate from out of the
 *	device memory space.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The buffer struct is initialized and the hardware page map is zeroed
 *	out in the range of addresses.
 *
 *----------------------------------------------------------------------
 */
ENTRY static void
DevBufferInit()
{
    Address		virtAddr;
    unsigned char	pmeg;
    int			oldContext;
    int			i;
    Address		baseAddr, endAddr;

    MASTER_LOCK(vmMachMutexPtr);

    /*
     * Round base up to next page boundary and end down to page boundary.
     */
    baseAddr = (Address)VMMACH_DMA_START_ADDR;
    endAddr = (Address)(VMMACH_DMA_START_ADDR + VMMACH_DMA_SIZE);

    /* 
     * Set up the hardware pages tables in the range of addresses given.
     */
    for (virtAddr = baseAddr; virtAddr < endAddr; ) {
	if (VmMachGetSegMap(virtAddr) != VMMACH_INV_PMEG) {
	    panic("DevBufferInit: DMA space already valid\n");
	}
	/* 
	 * Need to allocate a PMEG.
	 */
	pmeg = PMEGGet(vm_SysSegPtr, 
		       (int) ((unsigned)virtAddr >> VMMACH_SEG_SHIFT),
		       PMEG_DONT_ALLOC);
	oldContext = VmMachGetContextReg();
	for (i = 0; i < VMMACH_NUM_CONTEXTS; i++) {
	    VmMachSetContextReg(i);
	    VmMachSetSegMap(virtAddr, pmeg);
	}
	VmMachSetContextReg(oldContext);
	virtAddr += VMMACH_SEG_SIZE;
    }

    MASTER_UNLOCK(vmMachMutexPtr);
}

/*
 * 32Bit DMA stubs for the sun3.  The 32-bit user dvma stuff isn't on the
 * sun3's, just the sun4's.
 */
/*ARGSUSED*/
Address
VmMach_32BitDMAAlloc(numBytes, srcAddr)
    int		numBytes;		/* Number of bytes to map in. */
    Address	srcAddr;	/* Kernel virtual address to start mapping in.*/
{
    panic("VmMach_32BitDMAAlloc: should never be called on a sun3!\n");
    return (Address) 0;
}
/*ARGSUSED*/
void
VmMach_32BitDMAFree(numBytes, mapAddr)
    int		numBytes;		/* Number of bytes to map in. */
    Address	mapAddr;	/* Kernel virtual address to unmap.*/
{
    panic("VmMach_32BitDMAFree: should never be called on a sun3!\n");
}

static	Boolean	dmaPageBitMap[VMMACH_DMA_SIZE / VMMACH_PAGE_SIZE_INT];

static Boolean dmaInitialized = FALSE;

/*
 ----------------------------------------------------------------------
 *
 * VmMach_DMAAlloc --
 *
 *	Allocate a set of virtual pages to a routine for mapping purposes.
 *	
 * Results:
 *	Pointer into kernel virtual address space of where to access the
 *	memory, or NIL if the request couldn't be satisfied.
 *
 * Side effects:
 *	The hardware page table is modified.
 *
 *----------------------------------------------------------------------
 */
Address
VmMach_DMAAlloc(numBytes, srcAddr)
    int		numBytes;		/* Number of bytes to map in. */
    Address	srcAddr;	/* Kernel virtual address to start mapping in.*/
{
    Address	beginAddr;
    Address	endAddr;
    int		numPages;
    int		i, j;
    VmMachPTE	pte;
    Boolean	foundIt = FALSE;
    int		virtPage;

    if (!dmaInitialized) {
	dmaInitialized = TRUE;
	DevBufferInit();
    }
    /* calculate number of pages needed */
						/* beginning of first page */
    beginAddr = (Address) (((unsigned int)(srcAddr)) & ~VMMACH_OFFSET_MASK_INT);
						/* begging of last page */
    endAddr = (Address) ((((unsigned int) srcAddr) + numBytes) &
	    ~VMMACH_OFFSET_MASK_INT);
    numPages = (((unsigned int) endAddr) >> VMMACH_PAGE_SHIFT_INT) -
	    (((unsigned int) beginAddr) >> VMMACH_PAGE_SHIFT_INT) + 1;

    /* see if request can be satisfied */
    for (i = 0; i < (VMMACH_DMA_SIZE / VMMACH_PAGE_SIZE_INT); i++) {
	if (dmaPageBitMap[i] == 1) {
	    continue;
	}
	for (j = 1; j < numPages; j++) {
	    if (dmaPageBitMap[i + j] == 1) {
		break;
	    }
	}
	if (j == numPages) {
	    foundIt = TRUE;
	    break;
	}
    }
    if (!foundIt) {
	return (Address) NIL;
    }
    for (j = 0; j < numPages; j++) {
	dmaPageBitMap[i + j] = 1;	/* allocate page */
	virtPage = ((unsigned int) srcAddr) >> VMMACH_PAGE_SHIFT;
	pte = VMMACH_RESIDENT_BIT | VMMACH_KRW_PROT |
	      VirtToPhysPage(Vm_GetKernPageFrame(virtPage));
	SET_ALL_PAGE_MAP(((i + j) * VMMACH_PAGE_SIZE_INT) +
		VMMACH_DMA_START_ADDR, pte);
	srcAddr += VMMACH_PAGE_SIZE;
    }
    beginAddr = (Address) (VMMACH_DMA_START_ADDR + (i * VMMACH_PAGE_SIZE_INT) +
	    (((unsigned int) srcAddr) & VMMACH_OFFSET_MASK));
    return beginAddr;
}

/*
 ----------------------------------------------------------------------
 *
 * VmMach_DMAAllocContiguous --
 *
 *	Allocate a set of virtual pages to a routine for mapping purposes.
 *	
 * Results:
 *	Pointer into kernel virtual address space of where to access the
 *	memory, or NIL if the request couldn't be satisfied.
 *
 * Side effects:
 *	The hardware page table is modified.
 *
 *----------------------------------------------------------------------
 */
ReturnStatus
VmMach_DMAAllocContiguous(inScatGathPtr, scatGathLength, outScatGathPtr)
    register Net_ScatterGather	*inScatGathPtr;
    register int		scatGathLength;
    register Net_ScatterGather	*outScatGathPtr;
{
    Address	beginAddr;
    Address	endAddr;
    int		numPages;
    int		i, j;
    VmMachPTE	pte;
    Boolean	foundIt = FALSE;
    int		virtPage;
    Net_ScatterGather		*inPtr;
    Net_ScatterGather		*outPtr;
    int				pageOffset;
    Address			srcAddr;

    if (!dmaInitialized) {
	dmaInitialized = TRUE;
	DevBufferInit();
    }
    /* calculate number of pages needed */
    inPtr = inScatGathPtr;
    outPtr = outScatGathPtr;
    numPages = 0;
    for (i = 0; i < scatGathLength; i++) {
	if (inPtr->length > 0) {
	    /* beginning of first page */
	    beginAddr = (Address) (((unsigned int)(inPtr->bufAddr)) & 
		    ~VMMACH_OFFSET_MASK_INT);
	    /* beginning of last page */
	    endAddr = (Address) ((((unsigned int) inPtr->bufAddr) + 
		inPtr->length - 1) & ~VMMACH_OFFSET_MASK_INT);
	    /* 
	     * Temporarily store the number of pages in the out scatter/gather
	     * array.
	     */
	    outPtr->length =
		    (((unsigned int) endAddr) >> VMMACH_PAGE_SHIFT_INT) -
		    (((unsigned int) beginAddr) >> VMMACH_PAGE_SHIFT_INT) + 1;
	} else {
	    outPtr->length = 0;
	}
	if ((i == 0) && (outPtr->length != 1)) {
	    panic("Help! Help! I'm being repressed!\n");
	}
	numPages += outPtr->length;
	inPtr++;
	outPtr++;
    }

    /* see if request can be satisfied */
    for (i = 0; i < (VMMACH_DMA_SIZE / VMMACH_PAGE_SIZE_INT); i++) {
	if (dmaPageBitMap[i] == 1) {
	    continue;
	}
	for (j = 1; j < numPages; j++) {
	    if (dmaPageBitMap[i + j] == 1) {
		break;
	    }
	}
	if (j == numPages) {
	    foundIt = TRUE;
	    break;
	}
    }
    if (!foundIt) {
	return FAILURE;
    }
    pageOffset = i;
    inPtr = inScatGathPtr;
    outPtr = outScatGathPtr;
    for (i = 0; i < scatGathLength; i++) {
	srcAddr = inPtr->bufAddr;
	numPages = outPtr->length;
	for (j = 0; j < numPages; j++) {
	    dmaPageBitMap[pageOffset + j] = 1;	/* allocate page */
	    virtPage = ((unsigned int) srcAddr) >> VMMACH_PAGE_SHIFT;
	    pte = VMMACH_RESIDENT_BIT | VMMACH_KRW_PROT |
		  VirtToPhysPage(Vm_GetKernPageFrame(virtPage));
	    SET_ALL_PAGE_MAP(((pageOffset + j) * VMMACH_PAGE_SIZE_INT) +
		    VMMACH_DMA_START_ADDR, pte);
	    srcAddr += VMMACH_PAGE_SIZE;
	}
	outPtr->bufAddr = (Address) (VMMACH_DMA_START_ADDR + 
		(pageOffset * VMMACH_PAGE_SIZE_INT) + 
		(((unsigned int) srcAddr) & VMMACH_OFFSET_MASK));
	pageOffset += numPages;
	outPtr->length = inPtr->length;
	inPtr++;
	outPtr++;
    }
    return SUCCESS;
}


/*
 ----------------------------------------------------------------------
 *
 * VmMach_DMAFree --
 *
 *	Free a previously allocated set of virtual pages for a routine that
 *	used them for mapping purposes.
 *	
 * Results:
 *	None.
 *
 * Side effects:
 *	The hardware page table is modified.
 *
 *----------------------------------------------------------------------
 */
void
VmMach_DMAFree(numBytes, mapAddr)
    int		numBytes;		/* Number of bytes to map in. */
    Address	mapAddr;	/* Kernel virtual address to unmap.*/
{
    Address	beginAddr;
    Address	endAddr;
    int		numPages;
    int		i, j;
 
    /* calculate number of pages to free */
						/* beginning of first page */
    beginAddr = (Address) (((unsigned int) mapAddr) & ~VMMACH_OFFSET_MASK_INT);
						/* beginning of last page */
    endAddr = (Address) ((((unsigned int) mapAddr) + numBytes) &
	    ~VMMACH_OFFSET_MASK_INT);
    numPages = (((unsigned int) endAddr) >> VMMACH_PAGE_SHIFT_INT) -
	    (((unsigned int) beginAddr) >> VMMACH_PAGE_SHIFT_INT) + 1;

    i = (((unsigned int) mapAddr) >> VMMACH_PAGE_SHIFT_INT) -
	(VMMACH_DMA_START_ADDR >> VMMACH_PAGE_SHIFT_INT);
    for (j = 0; j < numPages; j++) {
	dmaPageBitMap[i + j] = 0;	/* free page */
	SET_ALL_PAGE_MAP(mapAddr, (VmMachPTE) 0);
	(unsigned int) mapAddr += VMMACH_PAGE_SIZE_INT;
    }
    return;
}


#if 0 /* Dead code shirriff 9/90 */
/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_GetDevicePage --
 *
 *      Allocate and validate a page at the given virtual address.  It is
 *	assumed that this page does not fall into the range of virtual 
 *	addresses used to allocate kernel code and data and that there is
 *	already a PMEG allocate for it.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The hardware segment table for the kernel is modified to validate the
 *	the page.
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_GetDevicePage(virtAddr) 
    Address	virtAddr; /* Virtual address where a page has to be 
			   * validated at. */
{
    VmMachPTE	pte;
    int		page;

    page = Vm_KernPageAllocate();
    if (page == -1) {
	panic("Vm_GetDevicePage: Couldn't get memory\n");
    }
    pte = VMMACH_RESIDENT_BIT | VMMACH_KRW_PROT | VirtToPhysPage(page);
    SET_ALL_PAGE_MAP(virtAddr, pte);
}

#endif

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_MapKernelIntoUser --
 *
 *      Map a portion of kernel memory into the user's heap segment.  
 *	It will only map objects on hardware segment boundaries.  This is 
 *	intended to be used to map devices such as video memory.
 *
 *	NOTE: It is assumed that the user process knows what the hell it is
 *	      doing.
 *
 * Results:
 *      Return the virtual address that it chose to map the memory at.
 *
 * Side effects:
 *      The hardware segment table for the user process's segment is modified
 *	to map in the addresses.
 *
 * ----------------------------------------------------------------------------
 */
ReturnStatus
VmMach_MapKernelIntoUser(kernelVirtAddr, numBytes, userVirtAddr,
			 realVirtAddrPtr) 
    unsigned	int	kernelVirtAddr;	/* Kernel virtual address to map in. */
    int	numBytes;			/* Number of bytes to map. */
    unsigned int userVirtAddr;	/* User virtual address to attempt to start 
				   mapping in at. */
    unsigned int *realVirtAddrPtr;/* Where we were able to start mapping at. */
{
    Address             newUserVirtAddr;
    ReturnStatus        status;

    status = VmMach_IntMapKernelIntoUser(kernelVirtAddr, numBytes,
            userVirtAddr, &newUserVirtAddr);

    if (status != SUCCESS) {
        return status;
    }

    return Vm_CopyOut(4, (Address) &newUserVirtAddr, (Address) realVirtAddrPtr);
}


/*
 * ----------------------------------------------------------------------------
 *
 * Vm_IntMapKernelIntoUser --
 *
 *      Map a portion of kernel memory into the user's heap segment.
 *      It will only map objects on hardware segment boundaries.  This is
 *      intended to be used to map devices such as video memory.
 *
 *      This routine can be called from within the kernel since it doesn't
 *      do a Vm_CopyOut of the new user virtual address.
 *
 *      NOTE: It is assumed that the user process knows what the hell it is
 *            doing.
 *
 * Results:
 *      SUCCESS or FAILURE status.
 *      Return the virtual address that it chose to map the memory at in
 *      an out parameter.
 *
 * Side effects:
 *      The hardware segment table for the user process's segment is modified
 *      to map in the addresses.
 *
 * ----------------------------------------------------------------------------
 */
ReturnStatus
VmMach_IntMapKernelIntoUser(kernelVirtAddr, numBytes, userVirtAddr, newAddrPtr)
    unsigned int        kernelVirtAddr;         /* Kernel virtual address
                                                 * to map in. */
    int numBytes;                               /* Number of bytes to map. */
    unsigned int        userVirtAddr;           /* User virtual address to
                                                 * attempt to start mapping
                                                 * in at. */
    Address             *newAddrPtr;            /* New user address. */
{
    int				numSegs;
    int				firstPage;
    int				numPages;
    Proc_ControlBlock		*procPtr;
    register	Vm_Segment	*segPtr;
    int				hardSegNum;
    int				i;
    unsigned int		pte;

    procPtr = Proc_GetCurrentProc();
    segPtr = procPtr->vmPtr->segPtrArray[VM_HEAP];

    numSegs = numBytes >> VMMACH_SEG_SHIFT;
    numPages = numSegs * VMMACH_SEG_SIZE / VMMACH_PAGE_SIZE;

    /*
     * Make user virtual address hardware segment aligned (round up) and 
     * make sure that there is enough space to map things.
     */
    hardSegNum = 
	    (unsigned int) (userVirtAddr + VMMACH_SEG_SIZE - 1) >> VMMACH_SEG_SHIFT;
    userVirtAddr = hardSegNum << VMMACH_SEG_SHIFT;
    if (hardSegNum + numSegs > VMMACH_NUM_SEGS_PER_CONTEXT) {
	return(SYS_INVALID_ARG);
    }

    /*
     * Make sure will fit into the kernel's VAS.  Assume that is hardware
     * segment aligned.
     */
    hardSegNum = (unsigned int) (kernelVirtAddr) >> VMMACH_SEG_SHIFT;
    if (hardSegNum + numSegs > VMMACH_NUM_SEGS_PER_CONTEXT) {
	return(SYS_INVALID_ARG);
    }

    /*
     * Invalidate all virtual memory for the heap segment of this process
     * in the given range of virtual addresses that we are to map.  This
     * assures us that there aren't any hardware pages allocated for this
     * segment in this range of addresses.
     */
    firstPage = (unsigned int) (userVirtAddr) >> VMMACH_PAGE_SHIFT;
    (void)Vm_DeleteFromSeg(segPtr, firstPage, firstPage + numPages - 1);

    /*
     * Now go into the kernel's hardware segment table and copy the
     * segment table entries into the heap segments hardware segment table.
     */
    bcopy((Address)GetHardSegPtr(vm_SysSegPtr->machPtr, hardSegNum),
	(Address)GetHardSegPtr(segPtr->machPtr,
		(unsigned int)userVirtAddr >> VMMACH_SEG_SHIFT), numSegs);
    for (i = 0; i < numSegs * VMMACH_NUM_PAGES_PER_SEG_INT; i++) {
	pte = VmMachGetPageMap((Address)(kernelVirtAddr +
		(i * VMMACH_PAGE_SIZE_INT)));
	pte &= ~VMMACH_KR_PROT;
	pte |= VMMACH_URW_PROT;
	VmMachSetPageMap((Address)(kernelVirtAddr + (i*VMMACH_PAGE_SIZE_INT)),
		pte);
    }

    /*
     * Make sure this process never migrates.
     */
    Proc_NeverMigrate(procPtr);
    
    /* 
     * Reinitialize this process's context using the new segment table.
     */
    VmMach_ReinitContext(procPtr);
    *newAddrPtr = (Address)userVirtAddr;

    return SUCCESS;
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_Trace --
 *
 *      Scan through all of the PMEGs generating trace records for those pages
 *	that have been referenced or modified since the last time that we
 *	checked.
 *  
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
ENTRY void
VmMach_Trace()
{
    register	PMEG			*pmegPtr;
    register	Vm_Segment		*segPtr;
    register	int			pmegNum;
    register	int			curTraceTime;

    MASTER_LOCK(vmMachMutexPtr);

    /*
     * Save the current trace time and then increment it to ensure that
     * any segments that get used while we are scanning memory won't get 
     * missed.
     */
    curTraceTime = vmTraceTime;
    vmTraceTime++;

    /*
     * Spin through all of the pmegs.
     */
    for (pmegNum = 0, pmegPtr = pmegArray;
	 pmegNum < VMMACH_NUM_PMEGS;
	 pmegPtr++, pmegNum++) {
	segPtr = pmegPtr->segPtr;
	if ((pmegPtr->flags & PMEG_NEVER_FREE) ||
	    segPtr == (Vm_Segment *)NIL ||
	    segPtr->traceTime < curTraceTime) {
	    continue;
	}
	vmTraceStats.machStats.pmegsChecked++;
	printedSegTrace = FALSE;
	tracePMEGPtr = pmegPtr;
	VmMachTracePMEG(pmegNum);
    }

    MASTER_UNLOCK(vmMachMutexPtr);

    VmCheckTraceOverflow();
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMachTracePage --
 *
 *      Generate a trace record for the given page.
 *  
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
INTERNAL void
VmMachTracePage(pte, pageNum)
    register	VmMachPTE	pte;	/* Page table entry to be traced. */
    unsigned	int		pageNum;/* Inverse of page within PMEG. */
{
    Vm_TraceSeg			segTrace;
    Vm_TracePage		pageTrace;
    register	PMEG		*pmegPtr;
    register	Vm_Segment	*segPtr;

    refModMap[PhysToVirtPage(pte & VMMACH_PAGE_FRAME_FIELD)] |=
			pte & (VMMACH_REFERENCED_BIT | VMMACH_MODIFIED_BIT);
    if (!printedSegTrace) {
	/*
	 * Trace of the segment.
	 */
	printedSegTrace = TRUE;
	pmegPtr = tracePMEGPtr;
	segPtr = pmegPtr->segPtr;
	segTrace.hardSegNum = pmegPtr->hardSegNum;
	segTrace.softSegNum = segPtr->segNum;
	segTrace.segType = segPtr->type;
	segTrace.refCount = segPtr->refCount;
	VmStoreTraceRec(VM_TRACE_SEG_REC, sizeof(Vm_TraceSeg), 
			(Address)&segTrace, FALSE);
    }

    /*
     * Trace the page.
     */
    pageTrace = VMMACH_NUM_PAGES_PER_SEG_INT - pageNum;
    if (pte & VMMACH_REFERENCED_BIT) {
	pageTrace |= VM_TRACE_REFERENCED;
    } 
    if (pte & VMMACH_MODIFIED_BIT) {
	pageTrace |= VM_TRACE_MODIFIED;
    }
    VmStoreTraceRec(0, sizeof(Vm_TracePage), (Address)&pageTrace, FALSE);
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_FlushPage --
 *
 *	Flush the page at the given virtual address from all caches.  We
 *	don't have to do anything on the Sun-2 and Sun-3 workstations
 *	that we have.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The given page is flushed from the caches.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
void
VmMach_FlushPage(virtAddrPtr, invalidate)
    Vm_VirtAddr	*virtAddrPtr;
    Boolean	invalidate;	/* Should invalidate the pte after flushing. */
{
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_HandleSegMigration --
 *
 *	Handle machine-dependent aspects of segment preparation for
 *	migration.  There's nothing to do on this machine.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
void
VmMach_HandleSegMigration(segPtr)
    Vm_Segment	*segPtr;
{
    return;
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_SetProtForDbg --
 *
 *	Set the protection of the kernel pages for the debugger.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The protection is set for the given range of kernel addresses.
 *
 *----------------------------------------------------------------------
 */
void
VmMach_SetProtForDbg(readWrite, numBytes, addr)
    Boolean	readWrite;	/* TRUE if should make pages writable, FALSE
				 * if should make read-only. */
    int		numBytes;	/* Number of bytes to change protection for. */
    Address	addr;		/* Address to start changing protection at. */
{
    register	Address		virtAddr;
    register	VmMachPTE 	pte;
    register	int		firstPage;
    register	int		lastPage;

    firstPage = (unsigned)addr >> VMMACH_PAGE_SHIFT;
    lastPage = ((unsigned)addr + numBytes - 1) >> VMMACH_PAGE_SHIFT;
    for (; firstPage <= lastPage; firstPage++) {
	virtAddr = (Address) (firstPage << VMMACH_PAGE_SHIFT);
	pte = VmMachGetPageMap(virtAddr);
	pte &= ~VMMACH_PROTECTION_FIELD;
	pte |= readWrite ? VMMACH_KRW_PROT : VMMACH_KR_PROT;
	SET_ALL_PAGE_MAP(virtAddr, pte);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_Cmd --
 *
 *	Machine dependent vm commands.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
ReturnStatus
VmMach_Cmd(command, arg)
    int	command;
    int arg;
{
    return(GEN_INVALID_ARG);
}


/*
 *----------------------------------------------------------------------
 *
 * VmMach_FlushCode --
 *
 *	Machine dependent vm commands.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
void
VmMach_FlushCode(procPtr, virtAddrPtr, virtPage, numBytes)
    Proc_ControlBlock	*procPtr;
    Vm_VirtAddr		*virtAddrPtr;
    unsigned		virtPage;
    int			numBytes;
{
}

/*
 * Dummy function which will turn out to be the function that the debugger
 * prints out on a backtrace after a trap.  The debugger gets confused
 * because trap stacks originate from assembly language stacks.  I decided
 * to make a dummy procedure because it was to confusing seeing the
 * previous procedure (VmMach_MapKernelIntoUser) on every backtrace.
 */
void
VmMachTrap()
{
}

/*
 *----------------------------------------------------------------------
 *
 * ByteFill --
 *
 *	Fill numBytes at the given address.  This routine is optimized to do 
 *      4-byte fills.  However, if the address is odd then it is forced to
 *      do single byte fills.
 *
 * Results:
 *	numBytes bytes of the fill byte are placed at *destPtr at the 
 *	given address.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
ByteFill(fillByte, numBytes, destPtr)
    register unsigned int fillByte;	/* The byte to be filled in. */
    register int numBytes;	/* The number of bytes to be filled in. */
    Address destPtr;		/* Where to fill. */
{
    register unsigned int fillInt = 
	(fillByte) | (fillByte << 8) | (fillByte << 16) | (fillByte << 24);

    register int *dPtr = (int *) destPtr;
    
    /*
     * If the address is on an aligned boundary then fill in as much
     * as we can in big transfers (and also avoid loop overhead by
     * storing many fill ints per iteration).  Once we have less than
     * 4 bytes to fill then it must be done by byte copies.
     */
#define WORDMASK	0x1

    if (((int) dPtr & WORDMASK) == 0) {
	while (numBytes >= 32) {
	    *dPtr++ = fillInt;
	    *dPtr++ = fillInt;
	    *dPtr++ = fillInt;
	    *dPtr++ = fillInt;
	    *dPtr++ = fillInt;
	    *dPtr++ = fillInt;
	    *dPtr++ = fillInt;
	    *dPtr++ = fillInt;
	    numBytes -= 32;
	}
	while (numBytes >= 4) {
	    *dPtr++ = fillInt;
	    numBytes -= 4;
	}
	destPtr = (char *) dPtr;
    }

    /*
     * Fill in the remaining bytes
     */

    while (numBytes > 0) {
	*destPtr++ = fillByte;
	numBytes--;
    }
}


#define ALLOC(x,s)	(sharedData->allocVector[(x)]=s)
#define FREE(x)		(sharedData->allocVector[(x)]=0)
#define SIZE(x)		(sharedData->allocVector[(x)])
#define ISFREE(x)	(sharedData->allocVector[(x)]==0)



/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_Alloc --
 *
 *      Allocates a region of shared memory;
 *
 * Results:
 *      SUCCESS if the region can be allocated.
 *	The starting address is returned in addr.
 *
 * Side effects:
 *      The allocation vector is updated.
 *
 * ----------------------------------------------------------------------------
 */
static ReturnStatus
VmMach_Alloc(sharedData, regionSize, addr)
    VmMach_SharedData	*sharedData;	/* Pointer to shared memory info.  */
    int			regionSize;	/* Size of region to allocate. */
    Address		*addr;		/* Address of region. */
{
    int numBlocks = (regionSize+VMMACH_SHARED_BLOCK_SIZE-1) /
	    VMMACH_SHARED_BLOCK_SIZE;
    int i, blockCount, firstBlock;

    if (sharedData->allocVector == (int *)NULL || sharedData->allocVector ==
	    (int *)NIL) {
	dprintf("VmMach_Alloc: allocVector uninitialized!\n");
    }

    /*
     * Loop through the alloc vector until we find numBlocks free blocks
     * consecutively.
     */
    blockCount = 0;
    for (i=sharedData->allocFirstFree;
	    i<=VMMACH_SHARED_NUM_BLOCKS-1 && blockCount<numBlocks;i++) {
	if (ISFREE(i)) {
	    blockCount++;
	} else {
	    blockCount = 0;
	    if (i==sharedData->allocFirstFree) {
		sharedData->allocFirstFree++;
	    }
	}
    }
    if (blockCount < numBlocks) {
	dprintf("VmMach_Alloc: got %d blocks of %d of %d total\n",
		blockCount,numBlocks,VMMACH_SHARED_NUM_BLOCKS);
	return VM_NO_SEGMENTS;
    }
    firstBlock = i-blockCount;
    if (firstBlock == sharedData->allocFirstFree) {
	sharedData->allocFirstFree += blockCount;
    }
    *addr = (Address)(firstBlock*VMMACH_SHARED_BLOCK_SIZE +
	    VMMACH_SHARED_START_ADDR);
    for (i = firstBlock; i<firstBlock+numBlocks; i++) {
	ALLOC(i,numBlocks);
    }
    dprintf("VmMach_Alloc: got %d blocks at %d (%x)\n",
	    numBlocks,firstBlock,*addr);
    return SUCCESS;
}


/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_Unalloc --
 *
 *      Frees a region of shared address space.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The allocation vector is updated.
 *
 * ----------------------------------------------------------------------------
 */

static void
VmMach_Unalloc(sharedData, addr)
    VmMach_SharedData	*sharedData;	/* Pointer to shared memory info. */
    Address	addr;		/* Address of region. */
{
    int firstBlock = ((int)addr-VMMACH_SHARED_START_ADDR) /
	    VMMACH_SHARED_BLOCK_SIZE;
    int numBlocks = SIZE(firstBlock);
    int i;

    dprintf("VmMach_Unalloc: freeing %d blocks at %x\n",firstBlock,addr);
    if (firstBlock < sharedData->allocFirstFree) {
	sharedData->allocFirstFree = firstBlock;
    }
    for (i=0;i<numBlocks;i++) {
	if (ISFREE(i+firstBlock)) {
	    printf("Freeing free shared address %d %d %d\n",i,i+firstBlock,
		    (int)addr);
	    return;
	}
	FREE(i+firstBlock);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_SharedStartAddr --
 *
 *      Determine the starting address for a shared segment.
 *
 * Results:
 *      Returns the proper start address for the segment.
 *
 * Side effects:
 *      Allocates part of the shared address space.
 *
 * ----------------------------------------------------------------------------
 */
ReturnStatus
VmMach_SharedStartAddr(procPtr,size,reqAddr)
    Proc_ControlBlock	*procPtr;
    int             size;           /* Length of shared segment. */
    Address         *reqAddr;        /* Requested start address. */
{
    return VmMach_Alloc(&procPtr->vmPtr->machPtr->sharedData, size, reqAddr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_SharedProcStart --
 *
 *      Perform machine dependent initialization of shared memory
 *	for this process.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The storage allocation structures are initialized.
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_SharedProcStart(procPtr)
    Proc_ControlBlock	*procPtr;
{
    VmMach_SharedData	*sharedData = &procPtr->vmPtr->machPtr->sharedData;
    dprintf("VmMach_SharedProcStart: initializing proc's allocVector\n");
    if (sharedData->allocVector != (int *)NIL) {
	panic("VmMach_SharedProcStart: allocVector not NIL\n");
    }
    sharedData->allocVector =
	    (int *)malloc(VMMACH_SHARED_NUM_BLOCKS*sizeof(int));
    sharedData->allocFirstFree = 0;
    bzero((Address) sharedData->allocVector, VMMACH_SHARED_NUM_BLOCKS*
	    sizeof(int));
    procPtr->vmPtr->sharedStart = (Address) VMMACH_SHARED_START_ADDR;
    procPtr->vmPtr->sharedEnd = (Address) VMMACH_SHARED_START_ADDR +
	    VMMACH_USER_SHARED_PAGES*VMMACH_PAGE_SIZE;
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_SharedSegFinish --
 *
 *      Perform machine dependent cleanup of shared memory
 *	for this segment.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The storage allocation structures are freed.
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_SharedSegFinish(procPtr,addr)
    Proc_ControlBlock	*procPtr;
    Address		addr;
{
    VmMach_Unalloc(&procPtr->vmPtr->machPtr->sharedData,addr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_SharedProcFinish --
 *
 *      Perform machine dependent cleanup of shared memory
 *	for this process.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The storage allocation structures are freed.
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_SharedProcFinish(procPtr)
    Proc_ControlBlock	*procPtr;
{
    dprintf("VmMach_SharedProcFinish: freeing process's allocVector\n");
    free((Address)procPtr->vmPtr->machPtr->sharedData.allocVector);
    procPtr->vmPtr->machPtr->sharedData.allocVector = (int *)NIL;
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_CopySharedMem --
 *
 *      Copies machine-dependent shared memory data structures to handle
 *      a fork.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The new process gets a copy of the shared memory structures.
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_CopySharedMem(parentProcPtr, childProcPtr)
    Proc_ControlBlock   *parentProcPtr; /* Parent process. */
    Proc_ControlBlock   *childProcPtr;  /* Child process. */
{
    VmMach_SharedData   *childSharedData =
            &childProcPtr->vmPtr->machPtr->sharedData;
    VmMach_SharedData   *parentSharedData =
            &parentProcPtr->vmPtr->machPtr->sharedData;

    VmMach_SharedProcStart(childProcPtr);

    bcopy((Address)parentSharedData->allocVector,
	    (Address)childSharedData->allocVector,
            VMMACH_SHARED_NUM_BLOCKS*sizeof(int));
    childSharedData->allocFirstFree = parentSharedData->allocFirstFree;
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_LockCachePage --
 *
 *      Perform machine dependent locking of a kernel resident file cache
 *	page.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_LockCachePage(kernelAddress)
    Address	kernelAddress;	/* Address on page to lock. */
{
    /*
     * Sun3 leaves file cache pages always available so there is no need to
     * lock or unlock them.
     */
    return;
}

/*
 * ----------------------------------------------------------------------------
 *
 * VmMach_UnlockCachePage --
 *
 *      Perform machine dependent unlocking of a kernel resident page.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *
 * ----------------------------------------------------------------------------
 */
void
VmMach_UnlockCachePage(kernelAddress)
    Address	kernelAddress;	/* Address on page to unlock. */
{
    /*
     * Sun3 leaves file cache pages always available so there is no need to
     * lock or unlock them.
     */
    return;
}
