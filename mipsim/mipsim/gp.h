/*
 * gp.h --
 *
 *	Declarations for procedures exported by the gp ("get and put")
 *	module of mipsim.
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
 * $Header: /user1/ouster/mipsim/RCS/gp.h,v 1.2 91/02/03 13:26:09 ouster Exp $ SPRITE (Berkeley)
 */

#ifndef _GP
#define _GP

extern int	Gp_GetCmd();
extern void	Gp_PutByte();
extern int	Gp_PutCmd();
extern int	Gp_PutstringCmd();
extern int	Gp_PutString();

#endif /* _GP */
