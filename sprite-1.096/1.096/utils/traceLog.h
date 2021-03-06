/*
 * traceLog.h --
 *
 *	Definitions for the generalized tracing facility.
 *
 *	These routines are for the SOSP91 paper.
 *
 * Copyright 1991 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that this copyright
 * notice appears in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: /sprite/src/kernel/utils/RCS/traceLog.h,v 1.6 91/06/27 12:16:32 shirriff Exp $ SPRITE (Berkeley)
 */

#ifndef _TRACELOG
#define _TRACELOG

#include "sysStats.h"
#include "spriteTime.h"

/*
 * Information applicable to an entire circular buffer.
 */

typedef struct TraceLog_Header {
    int numBuffers;		/* The number of buffers */
    int	firstNewBuffer;		/* The first buffer with new data */
    int	currentBuffer;		/* The number of the current buffer */
    int	currentOffset;		/* Our offset in the buffer */
    int flags;			/* TRACE_INHIBIT, TRACE_NO_TIMES */
    int dataSize;		/* The size of each buffer */
    int	lostRecords;		/* Records lost due to overflow. */
    int	blocked;		/* Set if we're blocked on buffer full. */
    int totalNumRecords;	/* Total records given to record. */
    int totalLostRecords;	/* Total records lost. */
    Sys_TracelogHeaderKern hdr;	/* The header for the user. */
    struct TraceLog_Buffer *buffers;  /* pointer to array of buffers */
} TraceLog_Header;

/*
 * Trace Header Flags:
 *
 *	TRACELOG_INHIBIT		- Don't do traces.
 *	TRACELOG_NO_TIMES		- Don't take time stamps, faster.
 *	TRACELOG_NO_BUF			- Return records immediately.
 */

#define TRACELOG_INHIBIT		0x0100
#define TRACELOG_NO_TIMES		0x0200
#define TRACELOG_NO_BUF			0x0400

/*
 * Information stored per-record.
 */

typedef struct TraceLog_Buffer {
    int size;			/* Size in bytes of the actual data. */
				/* Top byte = flags. */
    int numRecords;		/* Number of records. */
    int	lostRecords;		/* Records lost here due to overflow. */
    int	mode;			/* Inuse, done, unused (for consistency) */
    Address data;		/* Pointer to the data */
} TraceLog_Buffer;

#define INUSE 1
#define DONE 2
#define UNUSED 3

extern void		TraceLog_Init _ARGS_((TraceLog_Header *tracerPtr,
					   int numBuffers, int size,
					   int flags, int version));
extern void		TraceLog_Insert _ARGS_((TraceLog_Header *traceHdrPtr,
					     Address dataPtr, int size,
					     int flags));
extern ReturnStatus	TraceLog_Dump _ARGS_((TraceLog_Header *traceHdrPtr,
				       Address  dataAddr, Address hdrAddr));
extern void		TraceLog_Reset _ARGS_((TraceLog_Header *traceHdrPtr));
extern void		TraceLog_Finish _ARGS_((TraceLog_Header *traceHdrPtr));

#endif /* _TRACELOG */
