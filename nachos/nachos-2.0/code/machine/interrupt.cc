// interrupt.cc -- low-level machine emulation routines
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "interrupt.h"
#include "list.h"
#include "stats.h"
#include "thread.h"
#include "system.h"
#ifdef HW3
#include "machine.h"
#endif

static char *intLevelNames[] = { "off", "on"};
static char *intTypeNames[] = { "timer", "disk", "console write", 
			"console read", "network send", "network recv"};

Interrupt::Interrupt()
{
    level = IntOff;
    pending = new List();
    inHandler = FALSE;
    yieldOnReturn = FALSE;
    status = SystemMode;
}

// Change the interrupt level, without advancing the time
void
Interrupt::ChangeLevel(IntStatus old, IntStatus now)
{
    level = now;
    DEBUG('i',"\tinterrupts: %s -> %s\n",intLevelNames[old],intLevelNames[now]);
}

// Change interrupt level, and increment timer if interrupts are now enabled
IntStatus
Interrupt::SetLevel(IntStatus now)
{
    IntStatus old = level;
    
    		// by convention, interrupt handlers don't enable interrupts
    ASSERT((now == IntOff) || (inHandler == FALSE));	
    ChangeLevel(old, now);
    if ((now == IntOn) && (old == IntOff))
	OneTick();
    return old;
}

// Tick timer, firing any interrupts that are due
void
Interrupt::OneTick()
{
    if (status == SystemMode) {
        stats->totalTicks += SystemTick;
	stats->systemTicks += SystemTick;
    } else {		// ifdef HW3
	stats->totalTicks += UserTick;
	stats->userTicks += UserTick;
    }
    DEBUG('i', "\n== Tick %d ==\n", stats->totalTicks);
    ChangeLevel(IntOn, IntOff);
    while (CheckIfDue(FALSE))	// do all pending interrupts
	;
    ChangeLevel(IntOff, IntOn);
    if (yieldOnReturn) {	// handler asked for delayed yield
	yieldOnReturn = FALSE;
	currentThread->Yield();
    }
}

// Wait for any pending interrupts.  
//   In assignment 1, there aren't any!
//   In assignment 3-5, there are always pending interrupts!
void
Interrupt::Idle()
{
    DEBUG('i', "Machine idling; checking for interrupts.\n");
    status = IdleMode;
    if (CheckIfDue(TRUE)) {
	// if we do one, we should do any others that are due at the same time! 
    	while (CheckIfDue(FALSE))	
	    ;
        yieldOnReturn = FALSE;	// reset, since yield will happen anyway
        status = SystemMode;
	return;
    }
    // ifdef HW3, not reached -- halt must be invoked by user program
    DEBUG('i', "Machine idle.  No interrupts to do.\n");
    printf("All threads completed.\n");
    Halt();
}

// All done!
void
Interrupt::Halt()
{
    printf("Machine halting!\n\n");
    stats->Print();
    Cleanup();     // Never returns.
}

// Schedule an interrupt to happen ``when'' ticks in the future.
void
Interrupt::Schedule(VoidFunctionPtr handler, int arg, int fromNow, IntType type)
{
    InterruptInfo *info = new InterruptInfo;
    int when = stats->totalTicks + fromNow;

    DEBUG('i', "Scheduling interrupt handler the %s at time = %d\n", 
		intTypeNames[type], when);
    ASSERT(fromNow > 0);
    info->handler = handler;
    info->arg = arg;
    info->type = type;
    info->when = when;
    pending->SortedInsert((void *) info, when);
}

// check if we need to invoke an interrupt handler 
bool
Interrupt::CheckIfDue(bool advanceClock)
{
    MachineStatus old = status;
    int when;

    if (DebugIsEnabled('i'))
	DumpState();
    InterruptInfo *info = (InterruptInfo *)pending->SortedRemove(&when);

    ASSERT(level == IntOff);
    if (info == NULL)
	return FALSE;
    if (advanceClock && when > stats->totalTicks) {	// advance the clock
	stats->idleTicks += (when - stats->totalTicks);
	stats->totalTicks = when;
    } else if (when > stats->totalTicks) {	// not time yet, put it back
	pending->SortedInsert(info, when);
	return FALSE;
    }
#ifndef HW3	// only for homework 1 and 2
    if ((status == IdleMode) && (info->type == TimerInt) && pending->IsEmpty())
	 return FALSE;	// nothing more to do -- time to quit
#endif
    DEBUG('i', "Invoking interrupt handler for the %s at time %d\n", 
			intTypeNames[info->type], info->when);
#ifdef HW3
    machine->DelayedLoad(0, 0);
#endif
    inHandler = TRUE;
    status = SystemMode;
    (*(info->handler))(info->arg);
    status = old;
    inHandler = FALSE;
    return TRUE;
}

static void
PrintInfo(int arg)
{
    InterruptInfo *info = (InterruptInfo *)arg;

    printf("Interrupt handler %s, scheduled at %d\n", 
	intTypeNames[info->type], info->when);
}

void
Interrupt::DumpState()
{
    printf("Time: %d, interrupts %s\n", stats->totalTicks, intLevelNames[level]);
    printf("Interrupt handlers:\n");
    fflush(stdout);
    pending->Mapcar(PrintInfo);
    printf("End of handlers\n");
    fflush(stdout);
}
