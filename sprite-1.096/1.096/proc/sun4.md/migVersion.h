/*
 * migVersion.h --
 *
 *	Define the migration version, which is machine-dependent.
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
 * $Header: /sprite/src/kernel/proc/sun4.md/RCS/migVersion.h,v 1.8 91/06/29 16:32:12 mgbaker Exp $ SPRITE (Berkeley)
 */

/*
 * Set the migration version number.  Machines can only migrate to other
 * machines of the same architecture and version number.
 */
#ifndef PROC_MIGRATE_VERSION
#define PROC_MIGRATE_VERSION 1016
#endif /* PROC_MIGRATE_VERSION */

