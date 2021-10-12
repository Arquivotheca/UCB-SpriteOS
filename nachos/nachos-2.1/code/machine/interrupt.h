// interrupt.h -- low-level interrupt emulation routines
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "utility.h"
#include "list.h"

enum IntStatus { IntOff, IntOn };

enum MachineStatus {IdleMode, SystemMode, UserMode};

enum IntType { TimerInt, DiskInt, ConsoleWriteInt, ConsoleReadInt, 
				NetworkSendInt, NetworkRecvInt};

typedef struct {
    VoidFunctionPtr handler;    // The function to call.
    int arg;                    // The arg to the function.
    int when;			// when the interrupt is supposed to fire
    IntType type;		// for debugging
} InterruptInfo;

class Interrupt {
  public:
    Interrupt();
    
	// Turn interrupts on or off, and return previous setting.
    IntStatus SetLevel(IntStatus level); 
    
    void Enable() { (void) SetLevel(IntOn); }
    void Idle(); 	// Wait for any pending interrupts 
    void Halt(); 	// quit, printing out performance statistics
    
    // Call this to, on return from the interrupt handler, switch
    // to the next thread on the ready queue (by convention, most OS's
    // do not yield within interrupt handlers, but rather do the switch
    // as part of the return from interrupt
    void YieldOnReturn() { ASSERT(inHandler == TRUE);  yieldOnReturn = TRUE; }
    
    // Schedule an interrupt to happen ``when'' ticks in the future.
    // Called by the device emulators
    void Schedule(VoidFunctionPtr handler, int arg, int when, IntType type);
    
    // Advance emulated time
    void OneTick();       
    void DumpState();

    MachineStatus getStatus() { return status; }
    void setStatus(MachineStatus st) { status = st; }
    
  private:
    IntStatus level;	// are interrupts on or off?
    List *pending;	// the list of scheduled interrupts
    bool inHandler;	// if executing in the interrupt handler
    bool yieldOnReturn; // call Thread::Yield() when the handler returns
    MachineStatus status;

    // Check if it's time to invoke an interrupt handlers.
    // If advanceClock is TRUE, then advance time to the the first interrupt 
    // that's due.  If one was fired, return TRUE, else FALSE.
    bool CheckIfDue(bool advanceClock);

    // SetLevel, without advancing the clock
    void ChangeLevel(IntStatus old, IntStatus now);  
};

#endif
