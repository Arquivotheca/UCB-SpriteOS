/*
 * cop0.h --
 *
 *	Declarations for things exported by the cop0 module to the
 *	rest of Mipsim.
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
 * $Header: /user1/ouster/mipsim/RCS/cop0.h,v 1.1 89/11/20 10:57:43 ouster Exp $ SPRITE (Berkeley)
 */

#ifndef _COP0
#define _COP0

/*
 * The following structure encapsulates the state of coprocessor 0
 * of an R2000 machine.  Note:  at present only a few of the facilities
 * of coprocessor 0 are actually implemented.
 */

#define COP0_NUM_LEVELS 6

typedef struct Cop0 {
    int cause;			/* Cause register, which identifies last
				 * interrupt. */
    int status;			/* Status register, used among other things
				 * to enable interrupts. */
    int epc;			/* EPC register:  where interrupt occurred. */
    int pending[COP0_NUM_LEVELS];
				/* How many interrupts are pending at each
				 * defined level? */
    int pendingMask;		/* Mask of levels with pending > 0, in
				 * same positions as status and cause regs. */
    int flags;			/* Various flag values:  see below. */
} Cop0;

/*
 * Definitions of various bits in the cause and status registers.  See
 * pages 5-4 and following in Kane for details.
 */

#define COP0_STATUS_REGNUM	12
#define COP0_STATUS_IMASK0	0x400
#define COP0_STATUS_IE0		0x10
#define COP0_STATUS_IEP		0x4
#define COP0_STATUS_IEC		0x1
#define COP0_STATUS_IGNORE	0xffff03ea

#define COP0_CAUSE_REGNUM	13
#define COP0_CAUSE_BD		0x80000000
#define COP0_CAUSE_IP0		0x400
#define COP0_CAUSE_CODE_INT	0x0
#define COP0_CAUSE_IGNORE	0x7fff03c3

#define COP0_EPC_REGNUM		14

/*
 * Exported procedures:
 */

extern void	Cop0_Init();
extern void	Cop0_IntPending();
extern int	Cop0_ReadReg();
extern void	Cop0_Rfe();
extern void	Cop0_WriteReg();

#endif /* _COP0 */
