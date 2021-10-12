/*
 * io.h --
 *
 *	Declarations for the I/O-related facilities provided as
 *	part of Mipsim.
 *
 * Copyright 1989 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: /user1/ouster/mipsim/RCS/io.h,v 1.2 89/11/20 10:57:17 ouster Exp $ SPRITE (Berkeley)
 */

#ifndef _MIPSIM_IO
#define _MIPSIM_IO

#include <sgtty.h>

/*
 * The following structure is part of each R2000 machine, and describes
 * the I/O state of the machine:
 */

typedef struct IoState {
    struct sgttyb savedState;	/* Used to save original terminal state
				 * so terminal can be put into CBREAK
				 * mode during simulation and then be
				 * restored when simulation stops. */
    char input;			/* Next input character. */
    int flags;			/* Various flag values:  see below. */
} IoState;

/*
 * Flag values in IoState structures:
 *
 * IO_TERM_INPUT_READY -	1 means there is a valid input character
 *				waiting in the "input" field.
 * IO_TERM_OUTPUT_READY -	1 means that the terminal output buffer is
 *				empty.  0 means a character has been
 *				received, but we're waiting a few instructions
 *				to simulate the actual transmission of the
 *				character.
 * IO_TERM_INPUT_IE - 		1 means interrupts are enabled on input.
 * IO_TERM_OUTPUT_IE -		1 means interrupts are enabled on output.
 */

#define IO_TERM_INPUT_READY	1
#define IO_TERM_OUTPUT_READY	2
#define IO_TERM_INPUT_IE	4
#define IO_TERM_OUTPUT_IE	8

/*
 * Addresses of simulated device registers:
 */

#define IO_RECV_CONTROL		0xfff0
#define IO_RECV_DATA		0xfff4
#define IO_TRANS_CONTROL	0xfff8
#define IO_TRANS_DATA		0xfffc

/*
 * Bit positions in simulated device registers:
 */

#define IO_READY	1
#define IO_IE		2

/*
 * Procedures exported by io.c to rest of Mipsim:
 */

extern void	Io_BeginSim();
extern void	Io_EndSim();
extern void	Io_Init();
extern int	Io_Read();
extern int	Io_Write();

#endif /* _MIPSIM_IO */
