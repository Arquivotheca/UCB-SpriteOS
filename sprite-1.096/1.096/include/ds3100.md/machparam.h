/*
 * machparam.h --
 *
 *	This file contains various machine-dependent parameters needed
 *	by UNIX programs running under Sprite.  This file includes parts
 *	of the UNIX header files "machine/machparm.h" and
 *	"machine/endian.h".  Many of things in the UNIX file are only
 *	useful for the kernel;  stuff gets added to this file only
 *	when it's clear that it is needed for user programs.
 *
 * Copyright (C) 1989 by Digital Equipment Corporation, Maynard MA
 *
 *			All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its 
 * documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in 
 * supporting documentation, and that the name of Digital not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  
 *
 * Digitial disclaims all warranties with regard to this software, including
 * all implied warranties of merchantability and fitness.  In no event shall
 * Digital be liable for any special, indirect or consequential damages or
 * any damages whatsoever resulting from loss of use, data or profits,
 * whether in an action of contract, negligence or other tortious action,
 * arising out of or in connection with the use or performance of this
 * software.
 *
 * $Header: /sprite/src/lib/include/ds3100.md/RCS/machparam.h,v 1.1 89/07/08 14:56:04 nelson Exp $ SPRITE (Berkeley)
 */

#ifndef _MACHPARAM
#define _MACHPARAM

#ifndef _LIMITS
#include <limits.h>
#endif

/*
 *----------------------
 * Taken from endian.h:
 *----------------------
 */

/*
 * Definitions for byte order,
 * according to byte significance from low address to high.
 */
#define LITTLE_ENDIAN   1234    /* least-significant byte first (vax) */
#define BIG_ENDIAN      4321    /* most-significant byte first (IBM, net) */
#define PDP_ENDIAN      3412    /* LSB first in word, MSW first in long (pdp) */

#define BYTE_ORDER      LITTLE_ENDIAN   /* byte order on vax */

/*
 *----------------------
 * Miscellaneous:
 *----------------------
 */

/*
 * The bits of a address that should not be set if word loads and stores
 * are done on the address. This mask intended for fast byte manipulation
 * routines.
 */

#define	WORD_ALIGN_MASK	0x3

/*
 * Size of a page.
 */

#define PAGSIZ 0x1000

#endif _MACHPARAM
