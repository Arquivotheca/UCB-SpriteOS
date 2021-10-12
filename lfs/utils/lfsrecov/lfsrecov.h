/*
 * lfsrecov.h --
 *
 *	Declarations of global data structures and routines of the
 *	lfsrecov program.
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
 * $Header: /sprite/lib/forms/RCS/proto.h,v 1.7 91/02/09 13:24:52 ouster Exp $ SPRITE (Berkeley)
 */

#ifndef _LFSRECOV
#define _LFSRECOV

/* constants */

/* data structures */

/*
 * Recovery is implemented as a two pass operation on the
 * log tail. enum Pass defines which pass is active.
 */
enum Pass { PASS1, PASS2};

/*
 * For a operation from the directory log, the operands 
 * can be in one of three states:
 *	UNKNOWN - The operand is between the START and END record. The
 *		  change may or may not have made it out.
 * 	FORWARD - The operand is after the END record in the log. Change
 *		  did make it out. 
 *	BACKWARD - The operand is before the END record in the log. Change
 *		   didn't make it out.
 */
enum LogStatus { UNKNOWN, FORWARD, BACKWARD};


/*
 * Start and end points of the tail of the recovery log.
 */
extern LogPoint	logStart;	/* Start of recovery log. */
extern LogPoint	logEnd;		/* End of recovery log. */
/*
 * Arguments.
 */
extern int	blockSize;	/* File system block size. */
extern Boolean	verboseFlag;	/* Trace progress of program. */
extern Boolean	showLog ;	/* Show contents of log being processed. */
extern char	*deviceName;		/* Device to use. */
extern Boolean recreateDirEntries; 
extern Lfs	*lfsPtr;

/* procedures */

#endif /* _LFSRECOV */

