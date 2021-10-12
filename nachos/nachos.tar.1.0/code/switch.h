/* switch.h:  Machine dependent routines for kernel context switching.
 *
 * This file supports the pmax mips architecture.
 */

#ifndef SWITCH_H
#define SWITCH_H

/* Offsets in context switch frame, in bytes */

#ifndef ASSEMBLER
#define s0 0
#define s1 4
#define s2 8
#define s3 12
#endif

/* These are the offsets from the beginning of the Thread object, in bytes. */
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

/* These definitions are used in switch.s */
#define InitialPC	s0
#define InitialArg	s1
#define WhenDonePC	s2
#define StartupPC	s3

/* These definitions are used in Thread::AllocateStack(). */
#define PCState		(PC/4-1)
#define FPState		(FP/4-1)
#define InitialPCState	(s0/4)
#define InitialArgState	(s1/4)
#define WhenDonePCState	(s2/4)
#define StartupPCState	(s3/4)

/* If you have problems with stack overflow, make this larger, but
 * be sure you really need the extra space.
 */
#define StackSize	(4 * 1024)	/* in words */

#endif
