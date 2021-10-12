/*
 * coproc.h --
 *
 * Copyright 1990 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: /sprite/lib/forms/RCS/proto.h,v 1.5 90/01/12 12:03:25 douglis Exp $ SPRITE (Berkeley)
 */

#ifndef COPROC_H
#define COPROC_H

#include <setjmp.h>
#include <stddef.h>

#define STACKSIZE	2000
#define WARNSTACKLIMIT	 400 

typedef struct {
    jmp_buf	env;
    int		sp[STACKSIZE];
} COPROCtype;

#define CreateCoproc(p) (!_setjmp((p)->env) ? CreateCoproc2(p) : 0)

extern COPROCtype *CreateCoproc2();
extern void SwitchCoproc();

#endif COPROC_H
