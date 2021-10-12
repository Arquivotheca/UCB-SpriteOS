/*
 * asm.h --
 *
 *	Definitions used to assemble and disassemble R2000 instructions.
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
 * $Header: /user1/ouster/mipsim/RCS/asm.h,v 1.6 89/09/19 11:55:36 ouster Exp $ SPRITE (Berkeley)
 */

#ifndef _ASM_HDR
#define _ASM_HDR

/*
 * Variables and procedures exported to rest of simulator:
 */

extern char *Asm_RegNames[];

extern int	Asm_Assemble();
extern int	Asm_AsmCmd();
extern char *	Asm_Disassemble();
extern int	Asm_GetExpr();
extern int	Asm_LoadCmd();

#endif /* _ASM_HDR */
