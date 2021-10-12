/* 
 * event.c --
 *
 *	Implements monitor styles signals.
 *	Requires schedule.h and schedule.c.
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
static char rcsid[] = "$Header: /user4/eklee/sim/RCS/event.c,v 1.7 91/03/29 10:19:30 eklee Exp Locker: eklee $ SPRITE (Berkeley)";
#endif /* not lint */

#include <stddef.h>
#include "queue.h"
#include "schedule.h"
#include "event.h"

char *calloc();

InitEvent(event)
    Event	*event;
{
    InitQueue( event );
}

Event *
AllocEvent()
{
    QUEUEtype *q;

    q = (QUEUEtype *) calloc(1, sizeof(QUEUEtype));
    if (q == NULL) {
        printf("alloc failed\n");
        abort();
    }
    InitQueue(q);
    return q;
}

FreeEvent(event)
    Event	*event;
{
    free((char *) event);
}

CauseEvent(event)
    Event	*event;
{
    GenProc	*p;

    while (( p = (GenProc *) Dequeue(event) ) != NULL) {
	SchedProc(0, p);
    }
}

WaitEvent(event)
    Event	*event;
{
    Enqueue(event, _curProc);
    Dispatch();
}

WaitEventFunc(event, proc)
    Event	*event;
    FuncProc	*proc;
{
    Enqueue(event, proc);
}
