/* 
 * io.c --
 *
 *	This file implements a couple of simple memory-mapped I/O
 *	devices for the MIPS R2000 simulator.
 *
 * Copyright 1989 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /user1/ouster/mipsim/RCS/io.c,v 1.3 89/12/07 18:00:21 ouster Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include "mips.h"

/*
 * Terminal input is handled by polling the terminal for input every
 * once-in-a-while during simulation (the simulator is already busy-
 * waiting during input so this doesn't make things any worse).  For
 * terminal output, the character gets output to the terminal
 * immediately, but the terminal output device doesn't get marked as
 * ready until many instructions later.
 */

/*
 * How many instructions to wait between checks for terminal input:
 */

#define INPUT_WAIT	5000

/*
 * How many instructions must elapse between a character is received
 * for output and its transmission is considered complete.
 */

#define OUTPUT_DELAY	500

/*
 * Forward declarations for procedures defined in this file:
 */

static void	CheckInput();
static void	InputTimerProc();
static void	MarkOutput();

/*
 *----------------------------------------------------------------------
 *
 * Io_Init --
 *
 *	This procedure is called whenever a new machine is created.
 *	Its job is to initialize the part of the machine structure
 *	related to I/O.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the I/O devices (caller need not know about this).
 *
 *----------------------------------------------------------------------
 */

void
Io_Init(machPtr)
    register R2000 *machPtr;		/* New machine. */
{
    machPtr->ioState.flags = IO_TERM_OUTPUT_READY;

    /*
     * Arrange for periodic callbacks to check for terminal input.
     */

    Sim_CallBack(machPtr, INPUT_WAIT, InputTimerProc, (ClientData) machPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Io_Read --
 *
 *	Read an I/O device register, if one exists at the given
 *	address.
 *
 * Results:
 *	The return value is non-zero if there is an I/O register at
 *	address;  otherwise the return value is zero.  If the register
 *	exists, its contents are stored at *valuePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Io_Read(machPtr, address, valuePtr)
    register R2000 *machPtr;	/* Machine whose memory is being read. */
    unsigned int address;	/* Desired address of I/O register. */
    int *valuePtr;		/* Store contents of I/O register here. */
{
    int result = 0;

    switch (address) {

	case IO_RECV_CONTROL:
	    if (machPtr->ioState.flags & IO_TERM_INPUT_READY) {
		result |= IO_READY;
	    }
	    if (machPtr->ioState.flags & IO_TERM_INPUT_IE) {
		result |= IO_IE;
	    }
	    break;

	case IO_RECV_DATA:
	    result = machPtr->ioState.input & 0xff;
	    if (machPtr->ioState.flags & IO_TERM_INPUT_READY) {
		machPtr->ioState.flags &= ~IO_TERM_INPUT_READY;
		if (machPtr->ioState.flags & IO_TERM_INPUT_IE) {
		    Cop0_IntPending(machPtr, 0, -1);
		}
		CheckInput(machPtr);
	    }
	    break;

	case IO_TRANS_CONTROL:
	    if (machPtr->ioState.flags & IO_TERM_OUTPUT_READY) {
		result |= IO_READY;
	    }
	    if (machPtr->ioState.flags & IO_TERM_OUTPUT_IE) {
		result |= IO_IE;
	    }
	    break;

	case IO_TRANS_DATA:
	    break;

	default:
	    return 0;
    }

    *valuePtr = result;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Io_Write --
 *
 *	Write an I/O device register, if one exists at the given
 *	address.
 *
 * Results:
 *	The return value is non-zero if there is an I/O register at
 *	address;  otherwise the return value is zero.  If the register
 *	exists, its contents are overwritten with value.
 *
 * Side effects:
 *	The given memory location get modified, which may start an
 *	I/O device or do any of several other things.
 *
 *----------------------------------------------------------------------
 */

int
Io_Write(machPtr, address, value, size)
    register R2000 *machPtr;	/* Machine whose memory is being written. */
    unsigned int address;	/* Desired address of I/O register. */
    int value;			/* New value for I/O register. */
    int size;			/* Size of value in bytes (1, 2, or 4). */
{
    switch (address & ~0x3) {

	case IO_RECV_CONTROL:
	    if (((address & 0x3) + size) < 4) {
		break;
	    }
	    if (value & IO_IE) {
		if (!(machPtr->ioState.flags & IO_TERM_INPUT_IE)) {
		    machPtr->ioState.flags |= IO_TERM_INPUT_IE;
		    if (machPtr->ioState.flags & IO_TERM_INPUT_READY) {
			Cop0_IntPending(machPtr, 0, 1);
		    }
		}
	    } else {
		if ((machPtr->ioState.flags &
			(IO_TERM_INPUT_IE|IO_TERM_INPUT_READY))
			== (IO_TERM_INPUT_IE|IO_TERM_INPUT_READY)) {
		    Cop0_IntPending(machPtr, 0, -1);
		}
		machPtr->ioState.flags &= ~IO_TERM_INPUT_IE;
	    }
	    break;

	case IO_RECV_DATA:
	    break;

	case IO_TRANS_CONTROL:
	    if (((address & 0x3) + size) < 4) {
		break;
	    }
	    if (value & IO_IE) {
		if (!(machPtr->ioState.flags & IO_TERM_OUTPUT_IE)) {
		    if (machPtr->ioState.flags & IO_TERM_OUTPUT_READY) {
			Cop0_IntPending(machPtr, 0, 1);
		    }
		    machPtr->ioState.flags |= IO_TERM_OUTPUT_IE;
		}
	    } else {
		if ((machPtr->ioState.flags &
			(IO_TERM_OUTPUT_IE|IO_TERM_OUTPUT_READY))
			== (IO_TERM_OUTPUT_IE|IO_TERM_OUTPUT_READY)) {
		    Cop0_IntPending(machPtr, 0, -1);
		}
		machPtr->ioState.flags &= ~IO_TERM_OUTPUT_IE;
	    }
	    break;

	case IO_TRANS_DATA:
	    if (((address & 0x3) + size) < 4) {
		break;
	    }
	    if (machPtr->ioState.flags & IO_TERM_OUTPUT_READY) {
		char c;

		/*
		 * Start transmitting data, mark transmitter not ready, and
		 * arrange for transmitter to be marked ready again later.
		 */
    
		c = value;
		if (write(1, &c, 1) != 1) {
		    fprintf(stderr, "Internal error writing to stdout!\n");
		    exit(1);
		}
		machPtr->ioState.flags &= ~IO_TERM_OUTPUT_READY;
		if (machPtr->ioState.flags & IO_TERM_OUTPUT_IE) {
		    Cop0_IntPending(machPtr, 0, -1);
		}
		Sim_CallBack(machPtr, OUTPUT_DELAY, MarkOutput,
			(ClientData) machPtr);
	    }
	    break;

	default:
	    return 0;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Io_BeginSim --
 *
 *	This procedure is invoked just before a simulation begins,
 *	so that anything I/O-related (such as resetting terminal
 *	characteristics) may be done.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	I/O-related stuff is set up (e.g. terminal characteristics
 *	are reset).
 *
 *----------------------------------------------------------------------
 */

void
Io_BeginSim(machPtr)
    register R2000 *machPtr;		/* Machine being simulated. */
{
    int flags;

    /*
     * Save terminal state, and put it into a raw-er mode during
     * the simulation.
     */

    ioctl(0, TIOCGETP, (char *) &(machPtr->ioState.savedState));
    flags = machPtr->ioState.savedState.sg_flags;
    machPtr->ioState.savedState.sg_flags = (flags | CBREAK) & ~(CRMOD|ECHO);
    ioctl(0, TIOCSETP, (char *) &machPtr->ioState.savedState);
    machPtr->ioState.savedState.sg_flags = flags;
}

/*
 *----------------------------------------------------------------------
 *
 * Io_EndSim --
 *
 *	This procedure is invoked just after a simulation command
 *	completes, so that anything I/O-related (such as resetting
 *	terminal characteristics for Mipsim command processing) may
 *	be done.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	I/O-related stuff is reset (e.g. terminal characteristics).
 *
 *----------------------------------------------------------------------
 */

void
Io_EndSim(machPtr)
    register R2000 *machPtr;		/* Machine being simulated. */
{
    /*
     * Read a pending input character, if any, and restore terminal
     * state.
     */

    CheckInput(machPtr);
    ioctl(0, TIOCSETP, (char *) &machPtr->ioState.savedState);
}

/*
 *----------------------------------------------------------------------
 *
 * CheckInput --
 *
 *	Check to see if an input character has arrived;  if so, read it
 *	in and update the I/O control registers.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	I/O device state may change, and interrupts may occur.
 *
 *----------------------------------------------------------------------
 */

static void
CheckInput(machPtr)
    register R2000 *machPtr;		/* Machine to check for I/O. */
{
    char c;
    int count;

    /*
     * See if there is an input character on the terminal.
     */

    if (!(machPtr->ioState.flags & IO_TERM_INPUT_READY)) {
	ioctl(0, FIONREAD, (char *) &count);
	if (count > 0) {
	    if (read(0, &c, 1) != 1) {
		fprintf(stderr, "Internal error reading stdin!\n");
		exit(1);
	    }
	    machPtr->ioState.input = c;
	    machPtr->ioState.flags |= IO_TERM_INPUT_READY;
	    if (machPtr->ioState.flags & IO_TERM_INPUT_IE) {
		Cop0_IntPending(machPtr, 0, 1);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InputTimerProc --
 *
 *	This procedure is invoked periodically during input to see
 *	if any I/O has completed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	I/O device state may change, and interrupts may occur.
 *
 *----------------------------------------------------------------------
 */

static void
InputTimerProc(machPtr)
    register R2000 *machPtr;		/* Machine to check for I/O. */
{
    CheckInput(machPtr);

    /*
     * Re-register ourselves to get called again later.
     */

    Sim_CallBack(machPtr, INPUT_WAIT, InputTimerProc, (ClientData) machPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * MarkOutput --
 *
 *	This procedure is called back by the simulator after a number
 *	of instructions have been executed since starting an output
 *	operation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The transmitter unit is marked as "ready" again.
 *
 *----------------------------------------------------------------------
 */

static void
MarkOutput(machPtr)
    R2000 *machPtr;			/* Machine whose transmitter should be
					 * marked ready again. */
{
    machPtr->ioState.flags |= IO_TERM_OUTPUT_READY;
    if (machPtr->ioState.flags & IO_TERM_OUTPUT_IE) {
	Cop0_IntPending(machPtr, 0, 1);
    }
}
