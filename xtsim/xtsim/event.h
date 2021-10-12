/*
 * event.h --
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
 * $Header: /user4/eklee/sim/RCS/event.h,v 1.5 91/03/29 10:20:02 eklee Exp Locker: eklee $ SPRITE (Berkeley)
 */

#ifndef _EVENT_H
#define _EVENT_H

#include "queue.h"
#include "schedule.h"

typedef QUEUEtype  Event;

extern Event *AllocEvent();

#endif _EVENT_H
