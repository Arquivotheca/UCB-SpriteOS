/* 
 * queue.c --
 *
 *	Implements routines for inserting and deleting elements from queues.
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
static char rcsid[] = "$Header: /user4/eklee/sim/RCS/queue.c,v 1.5 91/03/29 10:19:53 eklee Exp Locker: eklee $ SPRITE (Berkeley)";
#endif /* not lint */

/*
 * assumes sizeof(int) == sizeof(WORD)
 * this is really a stack and not a queue
 */

#include "queue.h"

void
InitQueue ( q )
    NODEtype *q;
{
    q->next = q;
}

/* defined as macro
void
InsertNd ( prev, nd )
    NODEtype *prev, *nd;
{
    nd->next = prev->next;
    prev->next = nd;
}
*/

NODEtype *DeleteNd ( prev )
    NODEtype *prev;
{
    NODEtype *nd;
    
    nd = prev->next;
    prev->next = nd->next;
    return nd;
}

void
EnqueueProc (q, nd)
    QUEUEtype *q;
    NODEtype  *nd;
{
    InsertNd( q, nd );
}

NODEtype *
DequeueProc (q) 
    QUEUEtype *q;
{
    return ( NT(q)->next == NT(q) ? NULL : DeleteNd(q) );
}
