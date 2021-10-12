/* switch.h:  Machine dependent routines for kernel context switching.
 *
 * This file supports the pmax mips architecture.
 */
/*
 Copyright (c) 1992 The Regents of the University of California.
 All rights reserved.  See copyright.h for copyright notice and limitation 
 of liability and disclaimer of warranty provisions.
 */

#include "copyright.h"

#ifndef SWITCH_H
#define SWITCH_H

/* These are the offsets from the beginning of the Thread object, in bytes,
   used in switch.s */
#define SP 0
#define S0 4
#define S1 8
#define S2 12
#define S3 16
#define S4 20
#define S5 24
#define S6 28
#define S7 32
#define FP 36
#define PC 40

#define InitialPC	s0
#define InitialArg	s1
#define WhenDonePC	s2
#define StartupPC	s3

/* These definitions are used in Thread::AllocateStack(). */
#define PCState		(PC/4-1)
#define FPState		(FP/4-1)
#define InitialPCState	(S0/4-1)
#define InitialArgState	(S1/4-1)
#define WhenDonePCState	(S2/4-1)
#define StartupPCState	(S3/4-1)

/* If you have problems with stack overflow, make this larger, but
 * be sure you really need the extra space.
 */
#define StackSize	(4 * 1024)	/* in words */

#endif
