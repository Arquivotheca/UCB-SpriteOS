/* 
 * tclCkalloc.c --
 *
 *    Interface to malloc and free that provides support for debugging problems
 *    involving overwritten, double freeing memory and loss of memory.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 * 
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * This code contributed by Karl Lehenbauer and Mark Diekhans
 *
 */

static char sccsid[] = "@(#) tclCkalloc.c 1.3 94/08/15 11:41:43";

#include "tclInt.h"

#define FALSE	0
#define TRUE	1

#ifdef TCL_MEM_DEBUG
#ifndef TCL_GENERIC_ONLY
#include "tclPort.h"
#endif

#define GUARD_SIZE 8

struct mem_header {
        long               length;
        char              *file;
        int                line;
        struct mem_header *flink;
        struct mem_header *blink;
	int		   dummy;	/* Aligns body on 8-byte boundary. */
        unsigned char      low_guard[GUARD_SIZE];
        char               body[1];
};

static struct mem_header *allocHead = NULL;  /* List of allocated structures */

#define GUARD_VALUE  0341

/* static char high_guard[] = {0x89, 0xab, 0xcd, 0xef}; */

static int total_mallocs = 0;
static int total_frees = 0;
static int current_bytes_malloced = 0;
static int maximum_bytes_malloced = 0;
static int current_malloc_packets = 0;
static int maximum_malloc_packets = 0;
static int break_on_malloc = 0;
static int trace_on_at_malloc = 0;
static int  alloc_tracing = FALSE;
static int  init_malloced_bodies = TRUE;
#ifdef MEM_VALIDATE
    static int  validate_memory = TRUE;
#else
    static int  validate_memory = FALSE;
#endif

/*
 * Prototypes for procedures defined in this file:
 */

static int		MemoryCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));

/*
 *----------------------------------------------------------------------
 *
 * dump_memory_info --
 *     Display the global memory management statistics.
 *
 *----------------------------------------------------------------------
 */
static void
dump_memory_info(outFile) 
    FILE *outFile;
{
        fprintf(outFile,"total mallocs             %10d\n", 
                total_mallocs);
        fprintf(outFile,"total frees               %10d\n", 
                total_frees);
        fprintf(outFile,"current packets allocated %10d\n", 
                current_malloc_packets);
        fprintf(outFile,"current bytes allocated   %10d\n", 
                current_bytes_malloced);
        fprintf(outFile,"maximum packets allocated %10d\n", 
                maximum_malloc_packets);
        fprintf(outFile,"maximum bytes allocated   %10d\n", 
                maximum_bytes_malloced);
}

/*
 *----------------------------------------------------------------------
 *
 * ValidateMemory --
 *     Procedure to validate allocted memory guard zones.
 *
 *----------------------------------------------------------------------
 */
static void
ValidateMemory (memHeaderP, file, line, nukeGuards)
    struct mem_header *memHeaderP;
    char              *file;
    int                line;
    int                nukeGuards;
{
    unsigned char *hiPtr;
    int   idx;
    int   guard_failed = FALSE;
    int byte;
    
    for (idx = 0; idx < GUARD_SIZE; idx++) {
        byte = *(memHeaderP->low_guard + idx);
        if (byte != GUARD_VALUE) {
            guard_failed = TRUE;
            fflush (stdout);
	    byte &= 0xff;
            fprintf(stderr, "low guard byte %d is 0x%x  \t%c\n", idx, byte,
		    (isprint(UCHAR(byte)) ? byte : ' '));
        }
    }
    if (guard_failed) {
        dump_memory_info (stderr);
        fprintf (stderr, "low guard failed at %lx, %s %d\n",
                 (long unsigned int) memHeaderP->body, file, line);
        fflush (stderr);  /* In case name pointer is bad. */
        fprintf (stderr, "%ld bytes allocated at (%s %d)\n", memHeaderP->length,
		memHeaderP->file, memHeaderP->line);
        panic ("Memory validation failure");
    }

    hiPtr = (unsigned char *)memHeaderP->body + memHeaderP->length;
    for (idx = 0; idx < GUARD_SIZE; idx++) {
        byte = *(hiPtr + idx);
        if (byte != GUARD_VALUE) {
            guard_failed = TRUE;
            fflush (stdout);
	    byte &= 0xff;
            fprintf(stderr, "hi guard byte %d is 0x%x  \t%c\n", idx, byte,
		    (isprint(UCHAR(byte)) ? byte : ' '));
        }
    }

    if (guard_failed) {
        dump_memory_info (stderr);
        fprintf (stderr, "high guard failed at %lx, %s %d\n",
                 (long unsigned int) memHeaderP->body, file, line);
        fflush (stderr);  /* In case name pointer is bad. */
        fprintf (stderr, "%ld bytes allocated at (%s %d)\n",
		memHeaderP->length, memHeaderP->file,
		memHeaderP->line);
        panic ("Memory validation failure");
    }

    if (nukeGuards) {
        memset ((char *) memHeaderP->low_guard, 0, GUARD_SIZE); 
        memset ((char *) hiPtr, 0, GUARD_SIZE); 
    }

}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ValidateAllMemory --
 *     Validates guard regions for all allocated memory.
 *
 *----------------------------------------------------------------------
 */
void
Tcl_ValidateAllMemory (file, line)
    char  *file;
    int    line;
{
    struct mem_header *memScanP;

    for (memScanP = allocHead; memScanP != NULL; memScanP = memScanP->flink)
        ValidateMemory (memScanP, file, line, FALSE);

}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DumpActiveMemory --
 *     Displays all allocated memory to stderr.
 *
 * Results:
 *     Return TCL_ERROR if an error accessing the file occures, `errno' 
 *     will have the file error number left in it.
 *----------------------------------------------------------------------
 */
int
Tcl_DumpActiveMemory (fileName)
    char *fileName;
{
    FILE              *fileP;
    struct mem_header *memScanP;
    char              *address;

    fileP = fopen (fileName, "w");
    if (fileP == NULL)
        return TCL_ERROR;

    for (memScanP = allocHead; memScanP != NULL; memScanP = memScanP->flink) {
        address = &memScanP->body [0];
        fprintf (fileP, "%8lx - %8lx  %7ld @ %s %d",
		(long unsigned int) address,
                 (long unsigned int) address + memScanP->length - 1,
		 memScanP->length, memScanP->file, memScanP->line);
	(void) fputc('\n', fileP);
    }
    fclose (fileP);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbCkalloc - debugging ckalloc
 *
 *        Allocate the requested amount of space plus some extra for
 *        guard bands at both ends of the request, plus a size, panicing 
 *        if there isn't enough space, then write in the guard bands
 *        and return the address of the space in the middle that the
 *        user asked for.
 *
 *        The second and third arguments are file and line, these contain
 *        the filename and line number corresponding to the caller.
 *        These are sent by the ckalloc macro; it uses the preprocessor
 *        autodefines __FILE__ and __LINE__.
 *
 *----------------------------------------------------------------------
 */
char *
Tcl_DbCkalloc(size, file, line)
    unsigned int size;
    char        *file;
    int          line;
{
    struct mem_header *result;

    if (validate_memory)
        Tcl_ValidateAllMemory (file, line);

    result = (struct mem_header *)malloc((unsigned)size + 
                              sizeof(struct mem_header) + GUARD_SIZE);
    if (result == NULL) {
        fflush(stdout);
        dump_memory_info(stderr);
        panic("unable to alloc %d bytes, %s line %d", size, file, 
              line);
    }

    /*
     * Fill in guard zones and size.  Also initialize the contents of
     * the block with bogus bytes to detect uses of initialized data.
     * Link into allocated list.
     */
    if (init_malloced_bodies) {
        memset ((VOID *) result, GUARD_VALUE,
		size + sizeof(struct mem_header) + GUARD_SIZE);
    } else {
	memset ((char *) result->low_guard, GUARD_VALUE, GUARD_SIZE);
	memset (result->body + size, GUARD_VALUE, GUARD_SIZE);
    }
    result->length = size;
    result->file = file;
    result->line = line;
    result->flink = allocHead;
    result->blink = NULL;
    if (allocHead != NULL)
        allocHead->blink = result;
    allocHead = result;

    total_mallocs++;
    if (trace_on_at_malloc && (total_mallocs >= trace_on_at_malloc)) {
        (void) fflush(stdout);
        fprintf(stderr, "reached malloc trace enable point (%d)\n",
                total_mallocs);
        fflush(stderr);
        alloc_tracing = TRUE;
        trace_on_at_malloc = 0;
    }

    if (alloc_tracing)
        fprintf(stderr,"ckalloc %lx %d %s %d\n",
		(long unsigned int) result->body, size, file, line);

    if (break_on_malloc && (total_mallocs >= break_on_malloc)) {
        break_on_malloc = 0;
        (void) fflush(stdout);
        fprintf(stderr,"reached malloc break limit (%d)\n", 
                total_mallocs);
        fprintf(stderr, "program will now enter C debugger\n");
        (void) fflush(stderr);
	abort();
    }

    current_malloc_packets++;
    if (current_malloc_packets > maximum_malloc_packets)
        maximum_malloc_packets = current_malloc_packets;
    current_bytes_malloced += size;
    if (current_bytes_malloced > maximum_bytes_malloced)
        maximum_bytes_malloced = current_bytes_malloced;

    return result->body;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DbCkfree - debugging ckfree
 *
 *        Verify that the low and high guards are intact, and if so
 *        then free the buffer else panic.
 *
 *        The guards are erased after being checked to catch duplicate
 *        frees.
 *
 *        The second and third arguments are file and line, these contain
 *        the filename and line number corresponding to the caller.
 *        These are sent by the ckfree macro; it uses the preprocessor
 *        autodefines __FILE__ and __LINE__.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_DbCkfree(ptr, file, line)
    char *  ptr;
    char     *file;
    int       line;
{
    struct mem_header *memp = 0;  /* Must be zero for size calc */

    /*
     * Since header ptr is zero, body offset will be size
     */
#ifdef _CRAYCOM
    memp = (struct mem_header *)((char *) ptr  - (sizeof(int)*((unsigned)&(memp->body))));
#else
    memp = (struct mem_header *)(((char *) ptr) - (int)memp->body);
#endif

    if (alloc_tracing)
        fprintf(stderr, "ckfree %lx %ld %s %d\n",
		(long unsigned int) memp->body, memp->length, file, line);

    if (validate_memory)
        Tcl_ValidateAllMemory (file, line);

    ValidateMemory (memp, file, line, TRUE);
    if (init_malloced_bodies) {
	memset((VOID *) ptr, GUARD_VALUE, memp->length);
    }

    total_frees++;
    current_malloc_packets--;
    current_bytes_malloced -= memp->length;

    /*
     * Delink from allocated list
     */
    if (memp->flink != NULL)
        memp->flink->blink = memp->blink;
    if (memp->blink != NULL)
        memp->blink->flink = memp->flink;
    if (allocHead == memp)
        allocHead = memp->flink;
    free((char *) memp);
    return 0;
}

/*
 *--------------------------------------------------------------------
 *
 * Tcl_DbCkrealloc - debugging ckrealloc
 *
 *	Reallocate a chunk of memory by allocating a new one of the
 *	right size, copying the old data to the new location, and then
 *	freeing the old memory space, using all the memory checking
 *	features of this package.
 *
 *--------------------------------------------------------------------
 */
char *
Tcl_DbCkrealloc(ptr, size, file, line)
    char *ptr;
    unsigned int size;
    char *file;
    int line;
{
    char *new;
    unsigned int copySize;
    struct mem_header *memp = 0;  /* Must be zero for size calc */

#ifdef _CRAYCOM
    memp = (struct mem_header *)((char *) ptr  - (sizeof(int)*((unsigned)&(memp->body))));
#else
    memp = (struct mem_header *)(((char *) ptr) - (int)memp->body);
#endif
    copySize = size;
    if (copySize > memp->length) {
	copySize = memp->length;
    }
    new = Tcl_DbCkalloc(size, file, line);
    memcpy((VOID *) new, (VOID *) ptr, (int) copySize);
    Tcl_DbCkfree(ptr, file, line);
    return(new);
}

/*
 *----------------------------------------------------------------------
 *
 * MemoryCmd --
 *     Implements the TCL memory command:
 *       memory info
 *       memory display
 *       break_on_malloc count
 *       trace_on_at_malloc count
 *       trace on|off
 *       validate on|off
 *
 * Results:
 *     Standard TCL results.
 *
 *----------------------------------------------------------------------
 */
	/* ARGSUSED */
static int
MemoryCmd (clientData, interp, argc, argv)
    ClientData  clientData;
    Tcl_Interp *interp;
    int         argc;
    char      **argv;
{
    char *fileName;
    Tcl_DString buffer;
    int result;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		argv[0], " option [args..]\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (strcmp(argv[1],"trace") == 0) {
        if (argc != 3) 
            goto bad_suboption;
        alloc_tracing = (strcmp(argv[2],"on") == 0);
        return TCL_OK;
    }
    if (strcmp(argv[1],"init") == 0) {
        if (argc != 3)
            goto bad_suboption;
        init_malloced_bodies = (strcmp(argv[2],"on") == 0);
        return TCL_OK;
    }
    if (strcmp(argv[1],"validate") == 0) {
        if (argc != 3)
             goto bad_suboption;
        validate_memory = (strcmp(argv[2],"on") == 0);
        return TCL_OK;
    }
    if (strcmp(argv[1],"trace_on_at_malloc") == 0) {
        if (argc != 3) 
            goto argError;
        if (Tcl_GetInt(interp, argv[2], &trace_on_at_malloc) != TCL_OK)
                return TCL_ERROR;
         return TCL_OK;
    }
    if (strcmp(argv[1],"break_on_malloc") == 0) {
        if (argc != 3) 
            goto argError;
        if (Tcl_GetInt(interp, argv[2], &break_on_malloc) != TCL_OK)
                return TCL_ERROR;
        return TCL_OK;
    }

    if (strcmp(argv[1],"info") == 0) {
        dump_memory_info(stdout);
        return TCL_OK;
    }
    if (strcmp(argv[1],"active") == 0) {
        if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args:  should be \"",
		    argv[0], " active file", (char *) NULL);
	    return TCL_ERROR;
	}
	fileName = Tcl_TildeSubst(interp, argv[2], &buffer);
	if (fileName == NULL) {
	    return TCL_ERROR;
	}
	result = Tcl_DumpActiveMemory (fileName);
	Tcl_DStringFree(&buffer);
	if (result != TCL_OK) {
	    Tcl_AppendResult(interp, "error accessing ", argv[2], 
		    (char *) NULL);
	    return TCL_ERROR;
	}
	return TCL_OK;
    }
    Tcl_AppendResult(interp, "bad option \"", argv[1],
	    "\":  should be info, init, active, break_on_malloc, ",
	    "trace_on_at_malloc, trace, or validate", (char *) NULL);
    return TCL_ERROR;

argError:
    Tcl_AppendResult(interp, "wrong # args:  should be \"", argv[0],
	    " ", argv[1], "count\"", (char *) NULL);
    return TCL_ERROR;

bad_suboption:
    Tcl_AppendResult(interp, "wrong # args:  should be \"", argv[0],
	    " ", argv[1], " on|off\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InitMemory --
 *     Initialize the memory command.
 *
 *----------------------------------------------------------------------
 */
void
Tcl_InitMemory(interp)
    Tcl_Interp *interp;
{
Tcl_CreateCommand (interp, "memory", MemoryCmd, (ClientData) NULL, 
                  (Tcl_CmdDeleteProc *) NULL);
}

#else


/*
 *----------------------------------------------------------------------
 *
 * Tcl_Ckalloc --
 *     Interface to malloc when TCL_MEM_DEBUG is disabled.  It does check
 *     that memory was actually allocated.
 *
 *----------------------------------------------------------------------
 */
VOID *
Tcl_Ckalloc (size)
    unsigned int size;
{
        char *result;

        result = malloc(size);
        if (result == NULL) 
                panic("unable to alloc %d bytes", size);
        return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TckCkfree --
 *     Interface to free when TCL_MEM_DEBUG is disabled.  Done here rather
 *     in the macro to keep some modules from being compiled with 
 *     TCL_MEM_DEBUG enabled and some with it disabled.
 *
 *----------------------------------------------------------------------
 */
void
Tcl_Ckfree (ptr)
    VOID *ptr;
{
        free (ptr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InitMemory --
 *     Dummy initialization for memory command, which is only available 
 *     if TCL_MEM_DEBUG is on.
 *
 *----------------------------------------------------------------------
 */
	/* ARGSUSED */
void
Tcl_InitMemory(interp)
    Tcl_Interp *interp;
{
}

#endif
