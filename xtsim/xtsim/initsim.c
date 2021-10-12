/* 
 * initsim.c --
 *
 *	Initializes schedule.c, event.c, fork.c, semaphore.c and resource.c.
 *	InitSim must be called before any routines from the above modules.
 *
 * Copyright 1990 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /user4/eklee/sim/RCS/initsim.c,v 1.4 91/03/29 10:19:33 eklee Exp Locker: eklee $ SPRITE (Berkeley)";
#endif /* not lint */

#ifndef _XTSIM
InitSim ()
{
    InitSchedule();
}
#else
InitSim ()
{
    InitXtSchedule();
}
#endif _XTSIM
