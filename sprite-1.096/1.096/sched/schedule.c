/* 
 * schedule.c --
 *
 *  	Routines to implement the fair share scheduler algorithm.
 *
 * Copyright 1986 Regents of the University of California
 * All rights reserved.
 */

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/kernel/sched/RCS/schedule.c,v 9.11 90/12/07 15:02:35 mgbaker Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <sprite.h>
#include <sched.h>
#include <schedInt.h>
#include <proc.h>
#include <list.h>
#include <timer.h>
#include <sync.h>
#include <sys.h>
#include <dbg.h>
#include <mach.h>
#include <bstring.h>
#include <stdio.h>

#ifdef spur
#include <devCC.h>
#endif

#ifdef sequent
#include "machSGSProc.h"
#include "devClockArbiter.h"	/* for blinky lights */
#endif /* sequent */

static int	foundOnDeck[MACH_MAX_NUM_PROCESSORS];
static int	foundInQueue[MACH_MAX_NUM_PROCESSORS];
static int	missedStack[MACH_MAX_NUM_PROCESSORS];

/*
 *  The basic philosophy is that processes that have not executed
 *  as much as other processes deserve to be run first.  Thus we
 *  keep a smoothed average of recent CPU usage (the more recent the
 *  usage, the higher the weighting).  The process with the lowest
 *  recent usage gets highest scheduling priority.  The smoothed
 *  average is maintained by adding CPU usage as the process accumulates
 *  it, then periodically (once a second) reducing all the usages of
 *  all processes by a specific factor.  Thus, if a process stops using
 *  the CPU then its average will gradually decay to zero;  if a process
 *  becomes CPU-intensive, its average will gradually increase, up to
 *  a maximum value.  The controlling parameters are:
 *
 *  FORGET_INTERVAL -	How often to reduce everyone's usage.
 *  FORGET_MULTIPLY -
 *  FORGET_SHIFT -	These two factors determine how CPU usage decays:
 *			every second, everyone's CPU usage is multiplied
 *			by FORGET_MULTIPLY, then shifted right by
 *			FORGET_SHIFT.  Right now, the combined effect of
 *			these two is to "forget" 1/8th of the process's
 *			usage.
 */

#define FORGET_MULTIPLY		14
#define FORGET_SHIFT		4
#define FORGET_INTERVAL		timer_IntOneSecond

/*  
 *  The half-life of the average in seconds can be computed using this formula:
 *
 *        half-life  = ln(2) / ln(F)
 *
 *  where F = (FORGET_MULTIPLY)/(2**FORGET_SHIFT).  For the current settings
 *  the half-life is about 5.1 seconds.  This means that if a process
 *  suddenly stops executing, its usage will decay to half its early value
 *  in about 5 seconds.  The half-life gives an idea of how responsive the
 *  scheduler is to changes in process behavior.  If it responds too slowly,
 *  then a previously-idle process could become CPU-bound and monopolize the
 *  whole CPU for a long time until its usage rises.  If the half-life is
 *  too short, then an interactive process that does anything substantial
 *  (e.g. dragging a selection) will instantly lose its scheduling priority
 *  relative to other compute-bound processes.
 */
 
/*
 * The scheduler module mutex semaphore.  Used in sync module as well,
 * since synchronization involves mucking with the process queues.
 */
Sync_Semaphore sched_Mutex ; 
Sync_Semaphore *sched_MutexPtr = &sched_Mutex;

/*
 * Flag to see if Sched_Init has been called.  Used by Sched_GatherProcessInfo
 * to know when things have been initialized. It's needed because GPI
 * is called from the timer module and possibly before Sched_Init has been
 * called.
 */
static Boolean init = FALSE;

/*
 * Global variable for the timer queue for Sched_ForgetUsage.
 */
static Timer_QueueElement forgetUsageElement;

/*
 * Structure for instrumentation.
 */
Sched_Instrument sched_Instrument;

#ifdef SOSP91
/*
 * Get overall user and system time too, rather than just per-process.
 */

Sched_OverallTimes      sched_OverallTimesPerProcessor[MACH_MAX_NUM_PROCESSORS];

#include <sospRecord.h>
Timer_Ticks nameTime[10] = {0};
#endif /* SOSP91 */



/*
 * Status of each processor.
 */
Sched_ProcessorStatus	sched_ProcessorStatus[MACH_MAX_NUM_PROCESSORS];

/*
 * Length of time that a process can run before it is preempted.  This is
 * expressed as a number of timer interrupts.  The quantum length and
 * timer interrupt interval may not divide evenly.
 */

int	sched_Quantum = SCHED_DESIRED_QUANTUM / TIMER_CALLBACK_INTERVAL_APPROX;


Sched_OnDeck	sched_OnDeck[MACH_MAX_NUM_PROCESSORS];

/*
 * Forward Declarations.
 */
static void RememberUsage _ARGS_((Proc_ControlBlock *curProcPtr));
static Proc_ControlBlock *IdleLoop _ARGS_((void));
static void QuantumEnd _ARGS_((Proc_ControlBlock *procPtr));
extern void SchedPrintSchedStats _ARGS_((Timer_Ticks time, 
				ClientData clientData));


/*
 * ----------------------------------------------------------------------------
 *
 * Sched_Init --
 *
 *      Initialize data structures and variables for the scheduler.
 *	Cause Sched_ForgetUsage to be called from timer callback queue.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Global variables are initialized.  Run queue is initialized.
 *
 * ----------------------------------------------------------------------------
 */

void
Sched_Init()
{
    int	cpu;

    sched_ProcessorStatus[0] = SCHED_PROCESSOR_ACTIVE;
    for(cpu = 0; cpu < MACH_MAX_NUM_PROCESSORS; cpu++) {
	sched_ProcessorStatus[cpu] = SCHED_PROCESSOR_NOT_STARTED;
	sched_OnDeck[cpu].procPtr = (Proc_ControlBlock *) NIL;
    }
    bzero((Address) &(sched_Instrument),sizeof(sched_Instrument));

    List_Init(schedReadyQueueHdrPtr);
    Sync_SemInitDynamic(sched_MutexPtr, "sched_Mutex");
    Sync_SemRegister(sched_MutexPtr);

    forgetUsageElement.routine		= Sched_ForgetUsage; 
    forgetUsageElement.clientData	= 0;
    forgetUsageElement.interval		= FORGET_INTERVAL;
    Timer_ScheduleRoutine(&forgetUsageElement, TRUE);

    init = TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * Sched_ForgetUsage --
 *
 *	Adjusts the priority for all user processes on the system.
 *  
 *	This routine is called at regular intervals by the 
 *	Timer module TimeOut routine.
 *
 *
 * Results:
 *	none.
 *
 * Side Effects:
 *	Priorities of user processes are modified.
 *
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
void
Sched_ForgetUsage(time, clientData)
    Timer_Ticks time;	/* The absolute time when this routine is called. 
			 * (not used). */
    ClientData	clientData;	/* 0 - not used. */
{
    register Proc_ControlBlock *procPtr;
    register int i;

    /*
     *  Gain exclusive access to usage fields in the process table.
     */
     MASTER_LOCK(sched_MutexPtr);

    /*
     *  Loop through all the processes on the system and
     *  forget some of the CPU usage for them.
     */
    for (i = 0; i < proc_MaxNumProcesses; i++) {
	procPtr = proc_PCBTable[i];
	if (procPtr->state == PROC_UNUSED) {
	    continue;
	}
        procPtr->unweightedUsage = 
		(procPtr->unweightedUsage * FORGET_MULTIPLY) >> FORGET_SHIFT;

	procPtr->weightedUsage =
		(procPtr->weightedUsage * FORGET_MULTIPLY) >> FORGET_SHIFT;
    }

    /*
     *  Schedule this procedure to be called again later.
     */
    Timer_ScheduleRoutine(&forgetUsageElement, TRUE);

    MASTER_UNLOCK(sched_MutexPtr);
}


/*
 *----------------------------------------------------------------------
 *
 *  Sched_GatherProcessInfo --
 *
 *	This routine is called at every timer interrupt. It collects
 *	statistics about the running process such as the state of CPU and
 *	CPU usage. 
 *
 *  Results:
 *	None.
 *
 *  Side Effects:
 *	Various statistics about the running process are collected in the
 *      process's control block.
 *
 *
 *----------------------------------------------------------------------
 */
void
Sched_GatherProcessInfo(interval)
    unsigned int interval;	/* Number of ticks since last invocation. */
{
    register Proc_ControlBlock  *curProcPtr;
    register int		cpu;

    if (!init) {
	return;
    }

    MASTER_LOCK(sched_MutexPtr);

    /*
     *  Get a pointer to the current process from the array that keeps
     *  track of running processes on each processor.
     */
    for (cpu = 0; cpu < mach_NumProcessors; cpu++) {

	curProcPtr = proc_RunningProcesses[cpu];

	/*
	 * If no process is currently running on this processor, don't
	 * charge the usage to a particular process but keep track of it.
	 */
	if (curProcPtr == (Proc_ControlBlock *) NIL) {
	    Timer_AddIntervalToTicks(
		    sched_Instrument.processor[cpu].noProcessRunning, 
		    interval,
		    &(sched_Instrument.processor[cpu].noProcessRunning));
	    continue;
	}

	/*
	 *  We want to gather statistics about how much CPU time is spent in
	 *  kernel and user states.  The processor state is determined by
	 *  calling a machine-dependent routine.
	 */
	if (Mach_ProcessorState(cpu) == MACH_KERNEL) {
	    Timer_AddIntervalToTicks(curProcPtr->kernelCpuUsage.ticks, interval,
		           &(curProcPtr->kernelCpuUsage.ticks));
#ifdef SOSP91
            Timer_AddIntervalToTicks(
		    sched_OverallTimesPerProcessor[cpu].kernelTime, interval,
                    &(sched_OverallTimesPerProcessor[cpu].kernelTime));
	    {
		int n;
		n = curProcPtr->SOSP_IN_NAME_LOOKUP;
		if (n>=0 && n<6) {
		    Timer_AddIntervalToTicks( nameTime[n], interval,
			    &nameTime[n]);
		} else {
		    /*
		     * We weren't initialized.
		     */
		    curProcPtr->SOSP_IN_NAME_LOOKUP = 0;
		}
	    }
#endif SOSP91
	} else {
	    Timer_AddIntervalToTicks(curProcPtr->userCpuUsage.ticks, interval,
		           &(curProcPtr->userCpuUsage.ticks));
#ifdef SOSP91
            Timer_AddIntervalToTicks(
		    sched_OverallTimesPerProcessor[cpu].userTime, interval,
                    &(sched_OverallTimesPerProcessor[cpu].userTime));
	    if (curProcPtr->genFlags & PROC_FOREIGN) {
		Timer_AddIntervalToTicks(
			sched_OverallTimesPerProcessor[cpu].userTimeMigrated,
			interval, &(
			sched_OverallTimesPerProcessor[cpu].userTimeMigrated));
	    }
#endif SOSP91
	}

	/*
	 *  Update the CPU usage for scheduling priority calculations
	 *  for the current process.
	 */
	curProcPtr->recentUsage += interval;

	/*
	 * See if the quantum has expired for the process.  It can go
	 * negative if the user process happened to be running in kernel mode
	 * when the quantum expired for the first time and the process has
	 * not reentered the kernel voluntarily.
	 */
	if ((curProcPtr->genFlags & PROC_USER) && 
	    (curProcPtr->billingRate != PROC_NO_INTR_PRIORITY)) {
	    if (curProcPtr->schedQuantumTicks != 0) {
		curProcPtr->schedQuantumTicks--;
	    }
	    if (curProcPtr->schedQuantumTicks == 0) {
		QuantumEnd(curProcPtr);
	    }
	}
    }

    MASTER_UNLOCK(sched_MutexPtr);
}


/*
 * ----------------------------------------------------------------------------
 *
 * Sched_ContextSwitchInt --
 *
 *	Change to a new process.  Set the state of the current process
 *	to the state argument.
 *
 *	If no process is runnable, then loop with interrupts enabled and
 *	the master lock released until one is found.
 *
 *	The master lock is assumed to be held with sched_Mutex when
 *	this routine is called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new process is made runnable.  Counters of context switches are
 *	incremented.
 *
 * ----------------------------------------------------------------------------
 */

void
Sched_ContextSwitchInt(state)
    register	Proc_State state;	/* New state of current process */
{
    register Proc_ControlBlock	*curProcPtr;  	/* PCB for currently runnning 
						 * process. */
    register Proc_ControlBlock	*newProcPtr;  	/* PCB for new process. */
    Proc_ControlBlock		*tnewProcPtr;
    register int cpu;

    cpu = Mach_GetProcessorNumber();
    sched_Instrument.processor[cpu].numContextSwitches++;

    curProcPtr = Proc_GetCurrentProc();
    /*
     * If we have a context switch pending get rid of it.
     */
    curProcPtr->schedFlags &= ~SCHED_CONTEXT_SWITCH_PENDING;

    /*
     * Adjust scheduling priorities.
     */
    RememberUsage(curProcPtr);
    if (state == PROC_READY) {
	/*
	 * If the current process is PROC_READY, add it to the ready queue and
	 * get the next runnable process.  If that happens to be the current
	 */
	curProcPtr->numQuantumEnds++; 
	if (List_IsEmpty(schedReadyQueueHdrPtr)) {
	    curProcPtr->schedQuantumTicks = sched_Quantum;
	    return;
	}

	curProcPtr->state = PROC_READY;
	Sched_InsertInQueue(curProcPtr, &tnewProcPtr);
	newProcPtr = tnewProcPtr;
	if (newProcPtr == (Proc_ControlBlock *) NIL) {
	    newProcPtr = IdleLoop();
	} else if (newProcPtr == curProcPtr) {
	    curProcPtr->schedQuantumTicks = sched_Quantum;
	    curProcPtr->state = PROC_RUNNING;
	    return;
	} 
	/*
 	 * Don't run this process if another processor is already using
	 * its stack.
	 */
	if (newProcPtr->schedFlags & SCHED_STACK_IN_USE) {
	    Sched_InsertInQueue(newProcPtr, (Proc_ControlBlock **) NIL);
	    newProcPtr = IdleLoop();
	} 
    } else {
	if (state == PROC_WAITING) {
	    curProcPtr->numWaitEvents++; 
	}
	curProcPtr->state = state;
	/*
	 * Drop into the idle loop and come out with a runnable process.
	 * This procedure exists to try and capture idle time when profiling.
	 */
	newProcPtr = IdleLoop();
    }

    /*
     * Set the state of the new process.  
     */
    newProcPtr->state = PROC_RUNNING;
    newProcPtr->processor = cpu;
    Proc_SetCurrentProc(newProcPtr);

    /*
     * Set up the quantum as the number of clocks ticks that this process 
     * is allowed to run berfore it is context-switched.
     * (This field is ignored for kernel processes and user processes with 
     * a billing rate of PROC_NO_INTR_PRIORITY, which allows them to run 
     * forever.)
     */
    newProcPtr->schedQuantumTicks = sched_Quantum;

    /*
     * If the current process is continuing, then don't bother to 
     * to do full context switch.  
     */
    if (newProcPtr == curProcPtr) { 
	return;
    }

    sched_Instrument.processor[cpu].numFullCS++;

    /*
     * Perform the hardware context switch.  After switching, make
     * sure that there is a context for this process.
     */
    newProcPtr->schedFlags |= SCHED_STACK_IN_USE;
    curProcPtr->schedFlags &= ~SCHED_STACK_IN_USE;
    Mach_ContextSwitch(curProcPtr, newProcPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * RememberUsage --
 *
 *	Adjusts the weighted and unweighted CPU usages for a kernel or
 *	and user process. A process with the billingRate of 
 *	PROC_NO_INTR_PRIORITY does not get charged for weighted CPU usage,
 *	which is used in deciding priority in the run queue.
 *	
 *	This routine assumes the sched_Mutex master lock is held.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	CPU usages of the process are modified.
 *
 *----------------------------------------------------------------------
 */

static void
RememberUsage(curProcPtr)
    register Proc_ControlBlock *curProcPtr;	/* The process that will be 
						 * adjusted */
{
    register int billingRate = curProcPtr->billingRate;

    /*
     *  We want to calculate the process's CPU usage at this moment.
     *  There are 2 smoothed usage averages that we maintain: an
     *  unweighted value and a weighted value.  The weighted usage is used
     *  for calculating scheduling priority.  The unweighted usage keeps
     *  track of the real smoothed usage.
     */ 

    curProcPtr->unweightedUsage += curProcPtr->recentUsage;

    /*
     *  The billing rate basically specifies a process's scheduling 
     *  priority. It it used to modify the amount of the recent usage
     *  that gets added to the weighted usage.
     *
     *  If the billing rate equals the normal value then the recent usage
     *  is not multiplied or divided by any factor.  If the billing rate
     *  is greater than the normal value then only a faction of the recent
     *  usage is added to the weighted usage.  If the billing rate is less
     *  than the normal value then the recent usage is multiplied by a
     *  power of 2 before it is added to the weighted  usage.
     *
     *  A process with a billing rate of PROC_NO_INTR_PRIORITY does
     *  not get charged for CPU usage.
     */


    if (billingRate >= PROC_NORMAL_PRIORITY) {
	if (billingRate != PROC_NO_INTR_PRIORITY) {
	    curProcPtr->weightedUsage += curProcPtr->recentUsage >> billingRate;
	}
    } else {
	curProcPtr->weightedUsage += curProcPtr->recentUsage << -(billingRate);
    }

    /*
     *  Reset the recent usage back to zero.
     */

    curProcPtr->recentUsage = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * IdleLoop --
 *
 *	This fetches a runnable process from the ready queue and returns it.
 *	If none are available this goes into an idle loop, enabling and
 *	disabling interrupts, and waits for something to become runnable.
 *
 * Results:
 *	A pointer to the next process to run.
 *
 * Side effects:
 *	Momentarily enables interrupts.
 *
 *----------------------------------------------------------------------
 */

static Proc_ControlBlock *
IdleLoop()
{
    register Proc_ControlBlock	*procPtr;
    register int cpu;
    register List_Links		*queuePtr;
    register Boolean		foundOne;
    Proc_ControlBlock		*lastProcPtr = Proc_GetCurrentProc();
    Boolean			onReadyQueue;
#ifdef spur 
	/* Turn off perf counters. */
    Dev_CCSetCounters(COUNTERS_OFF);
#endif

    cpu = Mach_GetProcessorNumber();
    queuePtr = schedReadyQueueHdrPtr;
    if (sched_ProcessorStatus[cpu] == SCHED_PROCESSOR_ACTIVE) {
	foundOne = FALSE;
	procPtr = (Proc_ControlBlock *) List_First(queuePtr);
	while (!List_IsAtEnd(queuePtr,(List_Links *) procPtr)) {
	    if (!(procPtr->schedFlags & SCHED_STACK_IN_USE) ||
		 (procPtr->processor == cpu)) {
		foundOne = TRUE;
		break; 
	    }
	    if (procPtr->schedFlags & SCHED_STACK_IN_USE) {
		missedStack[cpu]++;
	    }
	    procPtr = (Proc_ControlBlock *)List_Next((List_Links *)procPtr);
	}
	if (foundOne) {
	    /*
	     * We found a READY process for us, break out of the
	     * idle loop.
	     */
	    onReadyQueue = TRUE;
	    foundInQueue[cpu]++;
#ifdef spur
	    Mach_InstCountOff(0);
#endif
	    goto exit;
	}
    }
    Proc_SetCurrentProc((Proc_ControlBlock *) NIL);
#ifdef spur
    Mach_InstCountEnd(0);
#endif
    MASTER_UNLOCK(sched_MutexPtr);

    if (Mach_IntrNesting(cpu) != 0) {
	int i;

	Mach_EnableIntr();
	i = Mach_IntrNesting(cpu);
	mach_NumDisableIntrsPtr[cpu] = 0;
	Mach_EnableIntr();
	panic("Interrupt level at %d going into idle loop.\n", i);
    }

#ifdef sequent
    /*
     * Really going idle, turn off the front panel light
     * and the processor board light.
     */
    if (light_show) {
	if (fp_lights) {
	    FP_LIGHTOFF(cpu);
	}
	*(int *)PHYS_LED = 0;
    }
#endif /* sequent */

    while (1) {
	/*
	 * Wait for a process to become runnable.  
	 */
	if (((List_IsEmpty(queuePtr) == FALSE) ||
	     (sched_OnDeck[cpu].procPtr != (Proc_ControlBlock *) NIL)) &&
	    ((sched_ProcessorStatus[cpu] == SCHED_PROCESSOR_ACTIVE) ||
	     (sched_ProcessorStatus[cpu] == SCHED_PROCESSOR_COUNTING_TICKS) ||
	     (lastProcPtr->state == PROC_READY))) {
	    /*
	     * Looks like there might be something in the queue. We don't
	     * have sched_Mutex down at this point, so this is only a hint.
	     */
	    MASTER_LOCK(sched_MutexPtr);
#ifdef spur
	    Mach_InstCountStart(2);
#endif
	    /*
	     * Look and see if there is anything for us on deck.
	     */
	    procPtr = sched_OnDeck[cpu].procPtr;
	    if (procPtr != (Proc_ControlBlock *) NIL) {
		if ((procPtr->schedFlags & SCHED_STACK_IN_USE) &&
		    (procPtr->processor != cpu)) {
		    panic("Process with stack in use in the staging area.");
		}
		sched_OnDeck[cpu].procPtr = (Proc_ControlBlock *) NIL;
		onReadyQueue = FALSE;
		foundOnDeck[cpu]++;
#ifdef spur
		Mach_InstCountOff(2);
#endif
		break;
	    }
	    /*
	     * If we are counting ticks then we are waiting for one 
	     * specific process to wake up, and it will show up in the
	     * staging area.  If we didn't find one there then skip to
	     * the bottom of the loop.
	     */
	    if (sched_ProcessorStatus[cpu] != SCHED_PROCESSOR_COUNTING_TICKS) {
		/*
		 * Make sure queue is not empty. If there is a ready process
		 * take a peek at it to insure that we can execute it. The
		 * only condition preventing a processor from executing a
		 * process is that its stack is being used by another processor.
		 */
		foundOne = FALSE;
		procPtr = (Proc_ControlBlock *) List_First(queuePtr);
		while (!List_IsAtEnd(queuePtr,(List_Links *) procPtr)) {
		    if (!(procPtr->schedFlags & SCHED_STACK_IN_USE) ||
			 (procPtr->processor == cpu)) {
			foundOne = TRUE;
			break; 
		    }
		    if (procPtr->schedFlags & SCHED_STACK_IN_USE) {
			missedStack[cpu]++;
		    }
		    procPtr = (Proc_ControlBlock *)
			List_Next((List_Links *)procPtr);
		}
		if (foundOne) {
		    /*
		     * We found a READY processor for us, break out of the
		     * idle loop.
		     */
		     onReadyQueue = TRUE;
		     foundInQueue[cpu]++;
#ifdef spur
		    Mach_InstCountOff(2);
#endif
		    break;
		}
	    }
	    sync_InstrumentPtr[cpu]->sched_MutexMiss++;
#ifdef spur
	    Mach_InstCountEnd(2);
#endif
	    MASTER_UNLOCK(sched_MutexPtr);
	}
	/*
	 * Count Idle ticks.  
	 */
	if (sched_Instrument.processor[cpu].idleTicksLow ==
					(unsigned) 0xffffffff) {
	    sched_Instrument.processor[cpu].idleTicksLow = 0;
	    sched_Instrument.processor[cpu].idleTicksOverflow++;
	} else {
	    sched_Instrument.processor[cpu].idleTicksLow++;
	}
    }
exit:
#ifdef spur
    Mach_InstCountStart(1);
#endif
#ifdef spur
    Dev_CCSetCounters(COUNTERS_RESTORE); /* Restore perf counters. */
#endif

    if (procPtr->state != PROC_READY) {
	/*
	 * Unlock sched_Mutex because panic tries to grab it somewhere.
	 * Do the panic by hand, without syncing the disks, because
	 * we still deadlock someplace.
	 */
	MASTER_UNLOCK(sched_MutexPtr);
	printf("Fatal Error: Non-ready process found in ready queue.\n");
	DBG_CALL;
	MASTER_LOCK(sched_MutexPtr);
    }
    if (onReadyQueue == TRUE) {
	((List_Links *)procPtr)->prevPtr->nextPtr =
					    ((List_Links *)procPtr)->nextPtr;
	((List_Links *)procPtr)->nextPtr->prevPtr =
					    ((List_Links *)procPtr)->prevPtr;
    /*
	List_Remove((List_Links *)procPtr);
    */
	sched_Instrument.numReadyProcesses -= 1;
    }

#ifdef sequent
    /*
     * Leaving idle, turn on the front panel light
     * and the processor board light.
     */
    if (light_show) {
	if (fp_lights) {
	    FP_LIGHTON(cpu);
	}
	*(int *)PHYS_LED = 1;
    }
#endif /* sequent */

    return(procPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Sched_TimeTicks --
 *
 *	Idle for a few seconds and count the ticks recorded in IdleLoop.
 *	For now, we only do this for one processor. All we're trying to get
 *	is a rough estimate of idleTicksPerSecond.
 *
 *      This procedure is called during boot. The results are pretty much
 *	meaningless if it is not.
 *	
 *	For best results all interrupts except for timer interrupts should
 *	be off.  If we are on a multiprocessor then we idle all processors
 *	for first so they don't interfere as badly.  Interrupts will still
 *	screw us up (they are handled by processor 1 but they still use
 *	locks we may need),  but I don't think turning off interrupts
 *	for 5 seconds on a live system is a good idea.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Momentarily enables interrupts.
 *
 *----------------------------------------------------------------------
 */

void
Sched_TimeTicks()
{
    register int lowTicks;
    register int cpu;
    Time time;
    int i;
    Boolean	wasIdled[MACH_MAX_NUM_PROCESSORS];

    cpu = Mach_GetProcessorNumber(); 
    if (cpu != 0) {
	sched_ProcessorStatus[cpu] = SCHED_PROCESSOR_COUNTING_TICKS;
	for (i = 0; i < mach_NumProcessors; i++) {
	     if (sched_ProcessorStatus[i] == SCHED_PROCESSOR_ACTIVE) {
		 (void) Sched_IdleProcessor(i);
		 wasIdled[i] = TRUE;
	     } else {
		 wasIdled[i] = FALSE;
	     }
	 }
    }
    Time_Multiply(time_OneSecond, 5, &time);
    printf("Idling processor %d for 5 seconds...",cpu);
    lowTicks = sched_Instrument.processor[cpu].idleTicksLow;
    (void) Sync_WaitTime(time);
    lowTicks = sched_Instrument.processor[cpu].idleTicksLow - lowTicks;
    printf(" %d ticks\n", lowTicks);
    sched_Instrument.processor[cpu].idleTicksPerSecond = lowTicks / 5;
    sched_ProcessorStatus[cpu] = SCHED_PROCESSOR_ACTIVE;
    if (cpu != 0) {
	for (i = 0; i < mach_NumProcessors; i++) {
	     if (wasIdled[i]) {
		 (void) Sched_StartProcessor(i);
	     }
	 }
     }
}


/*
 *----------------------------------------------------------------------
 *
 * QuantumEnd --
 *
 *	Called by Sched_GatherProcessInfo when a process's quantum has expired.
 *	A global flag is set to indicate that the current process should
 *	be involuntarily context switched at the next available moment.
 *	If the process is executing in kernel mode, then don't force a context
 *	switch now, but instead mark the process as having a context switch
 *	pending.
 *
 *	N.B. This routine assumes the sched mutex is already locked.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A context switch is initiated.
 *
 *----------------------------------------------------------------------
 */

static void
QuantumEnd(procPtr)
    register	Proc_ControlBlock 	*procPtr;
{
    procPtr->schedFlags |= SCHED_CONTEXT_SWITCH_PENDING;
    procPtr->specialHandling = 1;
    if (procPtr->processor != Mach_GetProcessorNumber()) {
	/* 
	 * If the process whose quantum has ended is running on a different
	 * processor we need to poke the processor and force it into the
	 * kernel. On its way back to user mode the special handling flag
	 * will be checked and a context switch will occur (assuming that
	 * the offending process is still running).
	 */
	Mach_CheckSpecialHandling(procPtr->processor);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Sched_PrintStat --
 *
 *	Print the sched module statistics.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Do the prints.
 *
 *----------------------------------------------------------------------
 */
void
Sched_PrintStat()
{
    Time  tmp;
    int   i;

    printf("Sched Statistics\n");
    for(i = 0; i < mach_NumProcessors;i++) {
	printf("Processor: %d\n",i);
	printf("numContextSwitches = %d\n",
	       sched_Instrument.processor[i].numContextSwitches);
	printf("numFullSwitches    = %d\n",
	       sched_Instrument.processor[i].numFullCS);
	printf("numInvoluntary     = %d\n",
	       sched_Instrument.processor[i].numInvoluntarySwitches);
	Timer_TicksToTime(sched_Instrument.processor[i].noProcessRunning, &tmp);
	printf("Idle Time          = %d.%06d seconds\n", 
  	       tmp.seconds, tmp.microseconds);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Sched_LockAndSwitch --
 *
 *	Acquires the Master Lock and performs a context switch.
 *	Called when a process's quantum has expired and a trace trap
 *	exception has arisen with the sched_ContextSwitchInProgress flag set.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A context switch is performed.  The count of involuntary switches is
 *	incremented.
 *
 *----------------------------------------------------------------------
 */

void
Sched_LockAndSwitch()
{
    MASTER_LOCK(sched_MutexPtr);
    sched_Instrument.processor[Mach_GetProcessorNumber()].
				numInvoluntarySwitches++;
    Sched_ContextSwitchInt(PROC_READY);
#ifdef spur
    Mach_InstCountEnd(1);
#endif

    MASTER_UNLOCK(sched_MutexPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Sched_ContextSwitch --
 *
 *	Acquires the Master Lock and performs a context switch.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A context switch is performed.
 *
 *----------------------------------------------------------------------
 */

void
Sched_ContextSwitch(state)
    Proc_State	state;
{

    MASTER_LOCK(sched_MutexPtr);
    Sched_ContextSwitchInt(state);
#ifdef spur
    Mach_InstCountEnd(1);
#endif
    MASTER_UNLOCK(sched_MutexPtr);

}



/*
 *----------------------------------------------------------------------
 *
 * Sched_StartKernProc --
 *
 *	Start a process by unlocking the master lock and calling the
 *	function whose address has been passed to us as an argument.
 *	If the function returns then exit.
 *
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The master lock is released.
 *
 *----------------------------------------------------------------------
 */
void
Sched_StartKernProc(func)
    void	(*func)();
{
#ifdef spur
    Mach_InstCountEnd(1);
#endif
    MASTER_UNLOCK(sched_MutexPtr);
    func();
    Proc_Exit(0);
}


/*
 *----------------------------------------------------------------------
 *
 * Sched_MakeReady --
 *
 *	Put the process on the ready queue.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	State of given process changed to ready.
 *
 *----------------------------------------------------------------------
 */

void
Sched_MakeReady(procPtr)
    register	Proc_ControlBlock	*procPtr;
{
    MASTER_LOCK(sched_MutexPtr);
    procPtr->state = PROC_READY;
    Sched_InsertInQueue(procPtr, (Proc_ControlBlock **) NIL);
    MASTER_UNLOCK(sched_MutexPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * Sched_StartUserProc --
 *
 *	Start a user process running.  This is the first thing that is
 * 	called when a newly created process begins execution.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Sched_StartUserProc(pc)
    Address	pc;	/* Program counter where process is to start
			 * executing. */
{
    register     	Proc_ControlBlock *procPtr;

#ifdef spur
    Mach_InstCountEnd(1);
#endif
    MASTER_UNLOCK(sched_MutexPtr);
    procPtr = Proc_GetCurrentProc();

#ifdef notdef
    Proc_Lock(procPtr);
    procPtr->genFlags |= PROC_DONT_MIGRATE;
    Proc_Unlock(procPtr);
#endif
    
    /*
     * Start the process running.  This does not return.
     */
    Mach_StartUserProc(procPtr, pc);
}
#if (MACH_MAX_NUM_PROCESSORS != 1)


/*
 *----------------------------------------------------------------------
 *
 * ProcessorStartProcess --
 *
 *	The initial process of a processor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	
 *
 *----------------------------------------------------------------------
 */

static 
void ProcessorStartProcess()
{
       /*
         * Detach from parent so that cleanup will occur when the
         * processor exits with this process. Also set the SCHED_STACK_IN_USE
         * flag so that cleanup wont happen too early.
         */
        Proc_Detach(SUCCESS);
        Sched_ContextSwitch(PROC_WAITING);
	Proc_Exit(0);
}



/*
 *----------------------------------------------------------------------
 *
 * StartProcessor --
 *
 *	Start up a processor..
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	
 *
 *----------------------------------------------------------------------
 */

ReturnStatus
StartProcessor(pnum)
    int		pnum;		/* Processor number to start. */
{
    Proc_PID    pid;
    Proc_ControlBlock *procPtr;
    char        procName[128];
    ReturnStatus status;

    /*
     * Startup an initial process for the processor pnum.  
     */
    sprintf(procName,"Processor%dStart",pnum);
    Proc_NewProc((Address)ProcessorStartProcess, PROC_KERNEL, FALSE, &pid,
				    procName);
    procPtr = Proc_GetPCB(pid);
    /*
     * Wait for this processor to go into the WAIT state.
     */
    while (procPtr->state != PROC_WAITING) {
	(void) Sync_WaitTimeInterval(10 * timer_IntOneMillisecond);
    }

    /*
     * Wait for its stack to become free .
     */
    while (procPtr->schedFlags & SCHED_STACK_IN_USE) {
	(void) Sync_WaitTimeInterval(10 * timer_IntOneMillisecond);
    }
    Sched_ContextSwitch(PROC_READY);
    printf("Starting processor %d with pid 0x%x\n",pnum,pid);
    status = Mach_SpinUpProcessor(pnum,procPtr);
    if (status != SUCCESS) {
	printf("Warning: Processor %d not started.\n",pnum);
    }
    return (status);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * Sched_StartProcessor --
 *
 *	Start a processor running processes.
 *
 * Results:
 *	A return status.
 *
 * Side effects:
 *	A processor maybe started.
 *
 *----------------------------------------------------------------------
 */
ReturnStatus
Sched_StartProcessor(pnum)
    int		pnum;	/* Processor number to start. */
{
    ReturnStatus	status;
    /*
     * Insure that processor number is in range.   
     * 
     */
    if (pnum >= MACH_MAX_NUM_PROCESSORS) {
	return (GEN_INVALID_ARG);
    }
    MASTER_LOCK(sched_MutexPtr);
    switch (sched_ProcessorStatus[pnum]) { 
	case SCHED_PROCESSOR_IDLE: {
	    sched_ProcessorStatus[pnum] = SCHED_PROCESSOR_ACTIVE;
	    /*
	     * Fall thru .
	     */
	}
	case SCHED_PROCESSOR_STARTING:
	case SCHED_PROCESSOR_ACTIVE: {
		status = SUCCESS;
		break;
	}
	case SCHED_PROCESSOR_NOT_STARTED: {
#if (MACH_MAX_NUM_PROCESSORS != 1)
	    sched_ProcessorStatus[pnum] == SCHED_PROCESSOR_STARTING;
	    MASTER_UNLOCK(sched_MutexPtr);
	    status = StartProcessor(pnum);
	    return (status);
#endif
	} 
	default: {
	    printf("Warning: Unknown processor state %d for processor %d\n",
		    (int) sched_ProcessorStatus[pnum], pnum);
	    status = FAILURE;
	}
    }
    MASTER_UNLOCK(sched_MutexPtr);
    return (status);
}



/*
 *----------------------------------------------------------------------
 *
 * Sched_IdleProcessor --
 *
 *	Put a processor into the idle state so it wont be scheduled for
 *	anymore processes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A processor will be idled started.
 *
 *----------------------------------------------------------------------
 */
ReturnStatus
Sched_IdleProcessor(pnum)
    int		pnum;	/* Processor number to start. */
{
    ReturnStatus	status;
    /*
     * Insure that processor number is in range.   
     * 
     */

#ifdef sequent
    if ((pnum < 0) || (pnum >= mach_NumProcessors)) {
        return GEN_INVALID_ARG;
    }
#else /* sequent */
    if (pnum >= MACH_MAX_NUM_PROCESSORS) {
	return (GEN_INVALID_ARG);
    }
#endif /* sequent */
    MASTER_LOCK(sched_MutexPtr);
    switch (sched_ProcessorStatus[pnum]) { 
	case SCHED_PROCESSOR_ACTIVE: 
	    sched_ProcessorStatus[pnum] = SCHED_PROCESSOR_IDLE;
	    /*
	     * Fall thru.
	     */
	case SCHED_PROCESSOR_IDLE: {
		status = SUCCESS;
		break;
	}
	case SCHED_PROCESSOR_NOT_STARTED: 
	case SCHED_PROCESSOR_STARTING: {
		status = GEN_INVALID_ARG;
		break;
	}
	default: {
	    printf("Warning: Unknown processor state %d for processor %d\n",
		    (int) sched_ProcessorStatus[pnum], pnum);
	    status = FAILURE;
	}
    }
    MASTER_UNLOCK(sched_MutexPtr);
    return (status);
}


/*
 *----------------------------------------------------------------------
 *
 * Sched_DumpReadyQueue --
 *
 *	Print out the contents of the ready queue.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Output goes to the screen.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
void
Sched_DumpReadyQueue(dummy)
    ClientData dummy;
{
    List_Links *itemPtr;
    Proc_ControlBlock *snapshot[SCHED_MAX_DUMP_SIZE];
    int snapshotCnt;
    int overflow;
    int i;

    if (List_IsEmpty(schedReadyQueueHdrPtr)) {
	printf("\nReady queue is empty.\n");
    } else {
	printf("\n%8s %5s %10s %10s %8s %8s   %s\n",
	    "ID", "wtd", "user", "kernel", "event", "state", "name");
	overflow = FALSE;
	snapshotCnt = 0;
	MASTER_LOCK(sched_MutexPtr);
	LIST_FORALL(schedReadyQueueHdrPtr,itemPtr) {
	    if (snapshotCnt >= SCHED_MAX_DUMP_SIZE) {
		overflow = TRUE;
		break;
	    }
	    snapshot[snapshotCnt++] = (Proc_ControlBlock *) itemPtr;
	}
	MASTER_UNLOCK(sched_MutexPtr);
	for (i = 0; i <snapshotCnt; i++) {
	    Proc_DumpPCB(snapshot[i]);
	}
	if (overflow) {
	    printf("Ready queue too large to snapshot.\n");
	}
    }
}


/*
 * Temporary call-back for printing sched statistics for recovery.
 */
Timer_QueueElement      schedStatElement;
Boolean                 getSchedStats = FALSE;

/*ARGSUSED*/
void
SchedPrintSchedStats(time, clientData)
    Timer_Ticks time;
    ClientData  clientData;
{
    int                 i;

    /* print stuff */
    Sched_PrintStat();
    for (i = 0; i < mach_NumProcessors; i++) {
	printf("processor %d:\n", i);
        printf("idleTicksLow: %d\n",
		sched_Instrument.processor[i].idleTicksLow);
        printf("idleTicksOverflow: %d\n",
		sched_Instrument.processor[i].idleTicksOverflow);
    }
    printf("\n");

    if (getSchedStats) {
        Timer_ScheduleRoutine(&schedStatElement, TRUE);
    }
    return;
}



/*
 *----------------------------------------------------------------------
 *
 * Sched_StartSchedStats --
 *
 *      Start up the kernel's periodic printing of sched stats.
 *      Temporary routine for recovery statistics.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Call-back routine scheduled.
 *
 *----------------------------------------------------------------------
 */
void
Sched_StartSchedStats()
{
    schedStatElement.routine = SchedPrintSchedStats;
    schedStatElement.clientData = 0;
    schedStatElement.interval = timer_IntOneSecond * 10;
    getSchedStats = TRUE;
    Timer_ScheduleRoutine(&schedStatElement, TRUE);

    return;
}


/*
 *----------------------------------------------------------------------
 *
 * Sched_StopSchedStats --
 *
 *      Stop the kernel's periodic printing of sched stats.
 *      Temporary routine for recovery statistics.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Call-back routine descheduled.
 *
 *----------------------------------------------------------------------
 */
void
Sched_StopSchedStats()
{
    getSchedStats = FALSE;
    (void) Timer_DescheduleRoutine(&schedStatElement);

    return;
}

