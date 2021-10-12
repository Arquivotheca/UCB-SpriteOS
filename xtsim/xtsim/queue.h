/*
 * queue.h --
 *
 *	Declarations of ...
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
 * $Header: /user4/eklee/sim/RCS/queue.h,v 1.8 91/03/29 10:20:15 eklee Exp Locker: eklee $ SPRITE (Berkeley)
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>

typedef struct NODEtype {
	struct NODEtype *next;
} NODEtype;

typedef NODEtype QUEUEtype;

NODEtype *DeleteNd();

#define NT(q)		((NODEtype *) (q))
#define First(q)	(NT(q)->next)
#define Next(q)		(NT(q)->next)
#define FirstElem(q)	(NT(q)->next == NT(q) ? NULL : NT(q)->next)
#define NextElem(q, p)	(NT(p)->next == NT(q) ? NULL : NT(p)->next)
#define Enqueue(q, nd)	{InsertNd(q, nd);}
#define Dequeue(q)	(NT(q)->next == NT(q) ? NULL : DeleteNd(q))
#define InsertNd(prev, nd) \
	(NT(nd)->next = NT(prev)->next, NT(prev)->next = NT(nd))

extern void InitQueue();
extern void EnqueueProc();
extern NODEtype *DequeueProc();
extern void InitStack();
extern NODEtype *PushProc();
extern NODEtype *PopProc();

#endif QUEUE_H
