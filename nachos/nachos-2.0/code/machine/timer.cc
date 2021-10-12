// timer.cc -- emulate a hardware timer that generates a call to an 
//	interrupt handler every TimerTicks.  Really straightforward.
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "timer.h"
#include "utility.h"
#include "system.h"
#include "interrupt.h"
#include "stats.h"

extern "C" {
long random(void);
}

// dummy function because of the way C++ is about pointers to member functions
static void TimerHandler(int arg)
{ Timer *p = (Timer *)arg; p->TimeOut(); }

Timer::Timer(VoidFunctionPtr timeOut, int callArg, bool doRandom)
{
    randomize = doRandom;
    handler = timeOut;
    arg = callArg; 
    interrupt->Schedule(TimerHandler, (int) this, ComputeTicks(), TimerInt); 
}

void 
Timer::TimeOut() 
{
    interrupt->Schedule(TimerHandler, (int) this, ComputeTicks(), TimerInt);
    (*handler)(arg);
}

int 
Timer::ComputeTicks() 
{
    if (randomize)
	return random() % (TimerTicks * 2);
    else
	return TimerTicks; 
}
