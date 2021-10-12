/*
 * getrec.h --
 *
 *	Include file for the trace record processing.
 */

#ifndef _GETREC_H
#define _GETREC_H

#ifndef SOSP91
#define SOSP91
#endif

#include <sprite.h>

/*
 * Size of the trace record buffer.
 * If a trace record is bigger than this, bad things will happen.
 */
#define BUF_SIZE 4096

typedef struct traceFile {
    FILE *stream;		/* Stream to read from. */
    int traceRecCount;		/* Counter of records read. */
    int numRecs;		/* Counter of number of records. */
    int version;		/* File type version. */
				/* 0 = old, 1 = new. */
    int swap;			/* 0 = don't, 1 = do. */
} traceFile;

#define SWAP4(x) ((((x)>>24)&0xff)|(((x)>>8)&0xff00)|(((x)<<8)&0xff0000)|\
     (((x)<<24)&0xff000000))

int initRead _ARGS_((char *name, int argc, char **argv));
int getHeader _ARGS_((traceFile *file, Sys_TracelogHeader *hdr));
int getNextRecord _ARGS_((traceFile *file, char *buf));
int getNextRecordMerge _ARGS_((char *buf));

#endif
