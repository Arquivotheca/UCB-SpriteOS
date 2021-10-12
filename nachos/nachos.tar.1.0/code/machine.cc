/* machine.cc -- Low-level machine implementation routines.  
 *
 * DO NOT CHANGE THIS FILE.
 */

#include "machine.h"
#include "scheduler.h"
#ifdef HW3
#include "addrspace.h"
#include "console.h"
#endif
#ifdef HW5
#include "network.h"
#endif

/* The global machine state.  This must be created fairly early on. */
Machine* machine;

#ifdef HW2
static char* interruptNames[] = { "disk", "console", "timer", "network" };
#endif

#ifdef HW3
static char* exceptionNames[] = { "syscall", "page fault", "bus error",
				  "address error", "overflow",
				  "illegal instruction" };
#endif

Machine::Machine()
{
    interruptLevel = 0;
    timerTicks = 0;

#ifdef HW2
    idleTicks = 0;
    numDiskReads = 0;
    numDiskWrites = 0;

    int i;
    for (i = 0; i < NumInterruptTypes; i++) {
        interruptInfo[i].time = 0;        // No pending interrupt.
        interruptInfo[i].handler = NULL;  // No handler yet.
    }
    insideInterruptHandler = FALSE;
#endif

#ifdef HW3
    for (i = 0; i < NumTotalRegs; i++)
        registers[i] = 0;

    badVirtualAddress = 0;
    singleStep = FALSE;
    shouldYield = FALSE;
    
    for (i = 0; i < NumExceptionTypes; i++)
        exceptionInfo[i].handler = NULL;
#endif
}

#define LevelToString(l)	(((l) == InterruptsOff) ? "off" : "on")

/* Turn interrupts on or off, and return previous setting.
 * Increment timer every time interrupts get enabled.
 */
InterruptStatus
Machine::InterruptLevel(InterruptStatus level)
{
    InterruptStatus oldLevel = interruptLevel;
    
    interruptLevel = level;
    DEBUG('m', "\tinterrupts: %s -> %s\n",
	  LevelToString(oldLevel), LevelToString(interruptLevel));
    
    if ((interruptLevel == InterruptsOn) && (oldLevel == InterruptsOff))
	OneTick();
    
    return (oldLevel);
}

/* This routine ticks the timer once, and fires any interrupts that have
 * been scheduled for this tick.  (None for assignment #1.)
 */
void
Machine::OneTick()
{
    timerTicks++;
    DEBUG('m', "\n== Tick %d ==\n", timerTicks);
    
#ifdef HW2
    ASSERT(!insideInterruptHandler);
    DEBUG('m', "\tinterrupts: on -> off\n");
    interruptLevel = InterruptsOff;
    (void) CheckInterrupts(FALSE);
    DEBUG('m', "\tinterrupts: off -> on\n");
    interruptLevel = InterruptsOn;
#endif
    
#ifdef HW3
    if (shouldYield) {
	shouldYield = FALSE;
	currentThread->Yield();
    }
#endif
}

/* Wait for any pending interrupts.
 *   (In assignment 1, there aren't any!)
 * If there aren't any, halt the machine.
 */
void
Machine::Idle()
{
    DEBUG('m', "Machine idling.  Looking for interrupts to do.\n");
#ifdef HW2
    if (CheckInterrupts(TRUE))
	return;
#endif
#ifndef HW5
    DEBUG('m', "Machine idle.  No interrupts to do.\n");
    
    printf("All threads completed.\n");
    Halt();
#endif
}

bool
Machine::InterruptsPending()
{
#ifdef HW2
    for (int i = 0; i < NumInterruptTypes; i++)
	if ((i != TimerInterrupt) && (interruptInfo[i].time > 0))
	    return (TRUE);
#endif
    return (FALSE);
}

extern void Cleanup();  // This is in system.cc

/* All done! */
void
Machine::Halt()
{
#ifdef HW2
    printf("Machine halting, ticks: %d, idle %d, reads: %d, writes: %d.\n",
	   timerTicks, idleTicks, numDiskReads, numDiskWrites);
#else
    printf("Machine halting, ticks: %d.\n", timerTicks);
#endif
    Cleanup();     // Never returns.
}

#ifdef HW2

/* This fills in the ``interrupt vector'' with the function. */
void
Machine::setInterruptHandler(InterruptType type, InterruptHandler func)
{
    ASSERT((type >= 0) && (type < NumInterruptTypes));
    interruptInfo[type].handler = func;
}

/* Schedule an interrupt to happen ``when'' ticks in the future. */
void
Machine::ScheduleInterrupt(InterruptType type, int arg, int when)
{
    ASSERT((type >= 0) && (type < NumInterruptTypes));
    ASSERT(when > 0);
    interruptInfo[type].time = timerTicks + when;
    interruptInfo[type].arg = arg;
    
    DEBUG('m', "Scheduling interrupt type %d for time = %d, arg 0x%x\n",
	  type, interruptInfo[type].time, arg);
}

/* Note that when interrupts are disabled, there are no timer ticks and
 * thus no possibility of interrupts happening.
 */

bool
Machine::CheckInterrupts(bool idling)
{
#ifdef HW3
    if (console)
	console->CheckActive();
#endif
#ifdef HW5
    if (network)
	network->CheckActive();
#endif
    
    int i;
    bool wasFired = FALSE;
    
//    printf("checking interrupts at %d:\n", timerTicks);
//    for (i = 0; i < NumInterruptTypes; i++)
//	printf("%d: %d\n", i, interruptInfo[i].time);
    
    if (idling) {
	int earliestTime = 0;
	for (i = 0; i < NumInterruptTypes; i++) {
	    if ((interruptInfo[i].time > 0) &&
		((interruptInfo[i].time < earliestTime) ||
		 (earliestTime == 0)))
		earliestTime = interruptInfo[i].time;
	}
	
	if (earliestTime == 0) {
	    DEBUG('m', "Returning from CheckInterrupts, false\n");
	    return (FALSE);
	}
	
	// Make sure we haven't skipped any interrupts somehow.
	ASSERT(earliestTime >= timerTicks);
	
	idleTicks += earliestTime - timerTicks;
	timerTicks = earliestTime;  // Advance the clock.
	
	for (i = 0; i < NumInterruptTypes; i++) {
	    if (interruptInfo[i].time == earliestTime) {
		FireInterrupt((InterruptType) i);
		if (i == TimerInterrupt) {
		    for (int j = 0; j < NumInterruptTypes; j++)
			if ((j != TimerInterrupt) &&
			    (interruptInfo[j].time > 0))
			    wasFired = TRUE;
		} else
		    wasFired = TRUE;
	    }
	}
    } else {
	for (i = 0; i < NumInterruptTypes; i++) {
	    if (interruptInfo[i].time == timerTicks) {
		FireInterrupt((InterruptType) i);
		if (i != TimerInterrupt)
		    wasFired = TRUE;
	    } else {
		// Make sure we haven't skipped any interrupts somehow.
		if (!((interruptInfo[i].time == 0) ||
		      (interruptInfo[i].time > timerTicks))) {
		    printf("%s interrupt should have fired at %d, now = %d\n",
			   interruptNames[i], interruptInfo[i].time,
			   timerTicks);
		    abort();
		}
	    }
	}
    }
    DEBUG('m', "Returning from CheckInterrupts, val: %d\n", wasFired);
    return (wasFired);
}

void
Machine::FireInterrupt(InterruptType which)
{
    insideInterruptHandler = TRUE;
    
    ASSERT(interruptLevel == InterruptsOff);
    DEBUG('m', "Interrupt: %s, time %d, arg 0x%x\n", interruptNames[which],
	  interruptInfo[which].time, interruptInfo[which].arg);
    
#ifdef HW3
    /* First, do delayed load */
    registers[registers[LoadReg]] = registers[LoadValueReg];
    registers[LoadReg] = 0;
    registers[LoadValueReg] = 0;
#endif
    
    if (!interruptInfo[which].handler) {
	fprintf(stderr, "Panic: no interrupt handler defined!\n");
	exit(1);
    }
    interruptInfo[which].time = 0;  // The handler might schedule a new one.
    (*(interruptInfo[which].handler)) (interruptInfo[which].arg);
    DEBUG('m', "Returning from FireInterrupt\n");

    insideInterruptHandler = FALSE;    
}
		
#endif

#ifdef HW3

void
Machine::Run()
{
    printf("Starting thread \"%s\" at time %d\n",
	   currentThread->getName(), timerTicks);
    while (OneInstruction()) {
	OneTick();
	if (singleStep && (runUntilTime <= timerTicks)) {
	    if (runUntilTime) {
		ASSERT(runUntilTime == timerTicks);
		runUntilTime = 0;
	    }
	    DumpState();
	    printf("%d> ", timerTicks);
	    fflush(stdout);
	    char buf[80];
	    fgets(buf, 80, stdin);
	    int num;
	    if (sscanf(buf, "%d", &num) == 1)
		runUntilTime = num;
	    else
		switch (*buf) {
		  case '\n':
		    break;
		    
		  case 'c':
		    singleStep = FALSE;
		    break;
		    
		  case '?':
		    printf("Machine commands:\n");
		    printf("    <return>  execute one instruction\n");
		    printf("    <number>  run until the given timer tick\n");
		    printf("    c         run until completion\n");
		    printf("    ?         print help message\n");
		    break;
		}
	}
    }
}


void
Machine::setExceptionHandler(ExceptionType type, ExceptionHandler func,
			     int arg)
{
    ASSERT((type >= 0) || (type < NumExceptionTypes));
    exceptionInfo[type].handler = func;
    exceptionInfo[type].arg = arg;
}

void
Machine::RaiseException (ExceptionType which)
{
    DEBUG('m', "Exception: %s\n", exceptionNames[which]);
    
    /* First, do delayed load */
    registers[registers[LoadReg]] = registers[LoadValueReg];
    registers[LoadReg] = 0;
    registers[LoadValueReg] = 0;
    
    /* Then call exception handler */
    if (!exceptionInfo[which].handler) {
	fprintf(stderr, "Panic: no exception handler defined for %s!\n",
		exceptionNames[which]);
	exit(1);
    }
    // Note that we *don't* disable interrupts here.  Exceptions can be
    // interrupted, since unlike interrupts they are synchronous.
    (*exceptionInfo[which].handler) (exceptionInfo[which].arg);
    
    // So this duck walks into a drugstore and says, "Gimme a chapstick".
    // The salesperson says, "Will that be cash or charge", and the duck
    // says, "Just put it on my bill."
}

void
Machine::DumpState()
{
    int i;
    
    printf("Machine: %d tick%s, interrupts %s\n", timerTicks,
	   (timerTicks != 1) ? "s" : "", interruptLevel ? "on" : "off");
    printf("    interrupt handlers:\n");
    for (i = 0; i < NumInterruptTypes; i++)
	printf("\t%d:  %d, 0x%x, 0x%x\n", i, interruptInfo[i].time,
	       interruptInfo[i].handler, interruptInfo[i].arg);
    printf("    exception handlers:\n");
    for (i = 0; i < NumExceptionTypes; i++)
	printf("\t%d:  0x%x, 0x%x\n", i, exceptionInfo[i].handler,
	       exceptionInfo[i].arg);
    printf("    registers:\n");
    for (i = 0; i < NumGPRegs; i++)
	switch (i) {
	  case StackReg:
	    printf("\tSP(%d):\t0x%x%s", i, registers[i],
		   ((i % 4) == 3) ? "\n" : "");
	    break;
	    
	  case RetAddrReg:
	    printf("\tRA(%d):\t0x%x%s", i, registers[i],
		   ((i % 4) == 3) ? "\n" : "");
	    break;
	  
	  default:
	    printf("\t%d:\t0x%x%s", i, registers[i],
		   ((i % 4) == 3) ? "\n" : "");
	    break;
	}
    
    printf("\tHi:\t0x%x", registers[HiReg]);
    printf("\tLo:\t0x%x\n", registers[LoReg]);
    printf("\tPC:\t0x%x", registers[PCReg]);
    printf("\tNextPC:\t0x%x", registers[NextPCReg]);
    printf("\tPrevPC:\t0x%x\n", registers[PrevPCReg]);
    printf("\tLoad:\t0x%x", registers[LoadReg]);
    printf("\tLoadV:\t0x%x\n", registers[LoadValueReg]);
#if 0
    printf("    pagetable 0x%x (0x%x)\n", (int) pageTable, pageTableSize);
    for (i = 0; i < pageTableSize; i++)
	printf("\t0x%x%s", pageTable[i].physicalFrame,
	       ((i % 8) == 7) ? "\n" : "");
    if ((i % 8) != 7)
	printf("\n");
    printf("\tStack:\t");
    for (i = pageTableSize * PageSize - 4; i > registers[StackReg]; i -= 4)
	printf("%x\t", ReadWord(i));
#endif
    
    printf("\n");
}

#endif
