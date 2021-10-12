/*
 * sym.h --
 *
 *	Declarations for procedures exported by the symbol table
 *	module for mipsim.
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
 * $Header: /user1/ouster/mipsim/RCS/sym.h,v 1.3 89/08/31 09:10:14 ouster Exp $ SPRITE (Berkeley)
 */

#ifndef _SYM
#define _SYM

/*
 * Return values from Sym_GetSym:
 */

#define SYM_FOUND	0
#define SYM_REGISTER	1
#define SYM_AMBIGUOUS	2
#define SYM_NOT_FOUND	3
#define SYM_REG_NOT_OK	4

/*
 * Flags to pass to Sym_GetSym:
 */

#define SYM_REGS_OK		1
#define SYM_PSEUDO_OK		2

/*
 * Flags to pass to Sym_AddSymbol:
 */

/* Leave value 1 available so SYM_REGISTER from above can be used */
#define SYM_NO_ADDR	2
#define SYM_GLOBAL	4

extern int	Sym_AddSymbol();
extern void	Sym_DeleteSymbols();
extern int	Sym_GetSym();
extern int	Sym_EvalExpr();
extern char *	Sym_GetString();

#endif /* _SYM */

