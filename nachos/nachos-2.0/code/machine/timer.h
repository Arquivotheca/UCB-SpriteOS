// timer.h -- emulate a hardware timer that generates a call to an 
//	interrupt handler every TimerTicks.  Really straightforward.
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef TIMER_H
#define TIMER_H

#include "utility.h"

class Timer {
  public:
    Timer(VoidFunctionPtr timeOut, int callArg, bool doRandom);
    ~Timer() {}

    void TimeOut();
    int ComputeTicks();

  private:
    bool randomize;		// true if choose timeout randomly
    VoidFunctionPtr handler;	// what to call after each timeout
    int arg;
};

#endif
