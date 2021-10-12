/* 
 * schedule.c --
 *
 *	Schedules coroutines with respect to simulation time.
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
static char rcsid[] = "$Header: /user4/eklee/sim/RCS/schedule.c,v 1.10 91/03/29 10:19:57 eklee Exp Locker: eklee $ SPRITE (Berkeley)";
#endif /* not lint */

#include <assert.h>
#include <math.h>
#include <varargs.h>
#include "queue.h"
#include "schedule.h"
#include "pqarr.h"

#ifdef _XTSIM
#include <stddef.h>
#include <X11/Intrinsic.h>
#include <sys/time.h>
#endif _XTSIM


/*
 * global vars
 */
SimTime		_globalTime = 0;
ProcProc	*_curProc;
FuncProc	*_curFunc;
ProcProc	*_spawnTmp;

static int 		traceSim=0;
static PQArr		*ready_q;
static QUEUEtype	proc_free_q;
static QUEUEtype	func_free_q;
static ProcProc		p0;

#ifdef _XTSIM
#define REAL_TIME_QUANTA      20

static SimTime		sampledRealTime;
static FuncProc		*xtFunc;

/*
 * Return time in milliseconds.
 */
SimTime
GetSysTime()
{
    struct timeval    sysTime;
    gettimeofday(&sysTime, NULL);
    return sysTime.tv_sec*1000 + sysTime.tv_usec/1000;
}

void
DispatchXtEvents()
{
    SimTime	delayTime;
    XEvent	event;
    XtInputMask	inputMask;
    GenProc	*proc;

    _curFunc = xtFunc;
ProcessEvents:
    /*
     * Dispatch pending events.
     */
    while (inputMask = XtPending()) {
	if (inputMask & XtIMXEvent) {
	    XtNextEvent(&event);
	    if (traceSim) {
		printf("SIM:dispatch time:%g xev:%d\n",
			(double) _globalTime, event.type);
	    }
	    XtDispatchEvent(&event);
	} else {
	    XtProcessEvent(inputMask);
	}
    }
    /*
     * No more X events.  Check if there are any processes to run.
     */
    if ((proc = (GenProc *) PQArr_First(ready_q)) == NULL) {
	/*
	 * Nothing to do.  Wait for something.
	 */
	(void) _XtwaitForSomething(FALSE, FALSE, FALSE, TRUE,
		NULL, _XtDefaultAppContext());
	sampledRealTime = GetSysTime();
	_globalTime = sampledRealTime;
	goto ProcessEvents;
    }
    /*
     * No more X events but we have a process we can run.
     * Wait until the process can be run or until we have more X events
     * to process.
     * NOTE: Simulation time is kept in sync with real time in quantas that
     * are REAL_TIME_QUANTA in size because calling GetSysTime() is
     * expensive.
     */
    if (proc->time > sampledRealTime + REAL_TIME_QUANTA) {
	/*
	 * Simulation time is ahead of real time.  Wait for real time.
	 */
	sampledRealTime = GetSysTime();
	delayTime = proc->time - sampledRealTime;
	if (delayTime > REAL_TIME_QUANTA) {
	    (void) _XtwaitForSomething(FALSE, FALSE, FALSE, TRUE,
			&delayTime, _XtDefaultAppContext());
	    /*
	     * If we received an X event while waiting until the next process
	     * could be executed, dispatch the events.
	     */
	    if (XtPending()) {
		sampledRealTime = GetSysTime();
		_globalTime = sampledRealTime;
		goto ProcessEvents;
	    } else {
		sampledRealTime = proc->time;
		_globalTime = sampledRealTime;
	    }
	}
    }
    /*
     * There are no pending X events, at least one process that is ready
     * to be executed and it is the right time to execute the process.
     */
    _curFunc = NULL;
}
#endif _XTSIM

TraceSimOn()
{
    traceSim = 1;
}

TraceSimOff()
{
    traceSim = 0;
}

void
PrintGenProcID(proc)
    GenProc	*proc;
{
    printf("pid:%d ", proc->pid);
    switch (proc->procType) {
    case FUNCPROC:
	printf("func:%08x", ((FuncProc *) proc)->func);
	break;
    case PROCPROC:
	printf("proc:%s", ((ProcProc *) proc)->name);
	break;
    default:
	printf("ERROR: Unknown procType '%d'.", proc->procType);
	break;
    }
}

static int
ProcTimeCmp(procA, procB)
    GenProc	*procA, *procB;
{
    return procA->time < procB->time;
}

void
InitSchedule()
{
    ready_q = PQArr_Create(ProcTimeCmp);
    InitQueue(&proc_free_q);
    InitQueue(&func_free_q);
    InitGenProc(&p0, PROCPROC);
    strncpy(p0.name, "root", NAME_SIZE);
    _curProc = &p0;
}

void
InitXtSchedule()
{
    InitSchedule();
    xtFunc = MakeFunc(DispatchXtEvents, "");
    sampledRealTime = GetSysTime();
    _globalTime = sampledRealTime;
}

InitGenProc(proc, procType)
    GenProc	*proc;
    ProcType	procType;
{
    static int		pid = 0;

    proc->procType = procType;
    proc->time = 0;
    proc->pid = pid++;
}

static int numProc = 0;

ProcProc *
AllocProc(name)
    char	*name;
{
    ProcProc	*proc;

    if ( (proc = (ProcProc *) Dequeue(&proc_free_q)) == NULL &&
	(proc = (ProcProc *) calloc(1, sizeof(ProcProc))) == NULL ) {
	printf("Error: Calloc failed in AllocProc\n");
    }
    InitGenProc(proc, PROCPROC);
    strncpy(proc->name, name, NAME_SIZE);
    numProc++;
    return proc;
}

static int numFunc = 0;

/* VARARGS2 */
FuncProc *
MakeFunc(func, format, va_alist)
    void	(*func)();
    char	*format;
    va_dcl
{
    FuncProc    *proc;
    va_list     argList;
    va_list     userList;

    if ((proc = (FuncProc *) Dequeue(&func_free_q)) == NULL &&
        (proc = (FuncProc *) calloc(1, sizeof(FuncProc))) == NULL) {
        printf("Error: Calloc failed in AllocProc\n");
        abort();
    }
    InitGenProc(proc, FUNCPROC);
    proc->func = func;
    proc->clientData = (int *) proc;

    va_start(argList);
    userList = (char *) proc->userData;
    for (; *format != NULL; format++) {
	if ((char *) userList + sizeof(double) >
		(char *) &proc->userData[MAX_USER_DATA_NUM]) {
	    printf("ERROR: to many args\n");
	    abort();
	}
	switch (*format) {
	case 'i':
	    va_arg(userList, int) = va_arg(argList, int);
	    break;
	case 'd':
	    va_arg(userList, double) = va_arg(argList, double);
	    break;
	default:
	    printf("ERROR: Unknown type '%c'.\n", *format);
	    abort();
	    break;
	}
    }
    numFunc++;
    return proc;
}

/* VARARGS2 */
void
GetFuncArg(proc, format, va_alist)
    FuncProc	*proc;
    char	*format;
    va_dcl
{
    va_list     argList;
    va_list     userList;
    va_start(argList);

    va_start(argList);
    userList = (char *) proc->userData;
    for (; *format != NULL; format++) {
	switch (*format) {
	case 'i':
	    *va_arg(argList, int *) = va_arg(userList, int);
	    break;
	case 'd':
	    *va_arg(argList, double *) = va_arg(userList, double);
	    break;
	default:
	    printf("ERROR: Unknown type '%c'.\n", *format);
	    abort();
	    break;
	}
    }
}

void
FreeProc(proc)
    ProcProc *proc;
{
    Enqueue(&proc_free_q, proc);
    numProc--;
}

void
FreeFunc(proc)
    FuncProc *proc;
{
    Enqueue(&func_free_q, proc);
    numFunc--;
}

void
SchedProc1(dt, proc)
    SimTime	dt;
    GenProc	*proc;
{
    proc->time = _globalTime + dt;
    PQArr_Enqueue(ready_q, proc);
    if (traceSim) {
	printf("SIM:schedproc time:%g schedtime:%g ",
		(double) _globalTime, (double) proc->time);
	PrintGenProcID(proc);
	printf("\n");
    }
}

void
Delay1(dt)
    SimTime dt;
{
    if (_globalTime + dt > NextProc()->time) {
	SchedProc(dt, _curProc);
	Dispatch();
    } else {
	_globalTime += dt;
    }
}

static ProcProc *_freeProc = NULL;

void
Dispatch ()
{
    GenProc	*proc;
    ProcProc	*tmpProc;

    if (_curFunc != NULL) {
	printf("ERROR: Functions may not block.\n");
	abort();
    }
    for (;;) {
#ifdef _XTSIM
	DispatchXtEvents();
#endif _XTSIM
	if ((proc = (GenProc *) PQArr_Dequeue(ready_q)) == NULL) {
	    printf("Error: Deadlock Detected\n");
	    abort();
	}
	if (_globalTime < proc->time) {
	    _globalTime = proc->time;
	}
	if (traceSim) {
	    printf("SIM:dispatch time:%g ", (double) _globalTime, proc->pid);
	    PrintGenProcID(proc);
	    printf("\n");
	}
	switch (proc->procType) {
	case FUNCPROC:
	    _curFunc = (FuncProc *) proc;
	    ExecuteFunc(_curFunc);
	    _curFunc = NULL;
	    break;
	case PROCPROC:
	    tmpProc = _curProc;
	    _curProc = (ProcProc *) proc;
	    /*
	     * We can not free terminated ProcProc until now because we needed
	     * the stack to execute the FuncProc's.
	     */
	    if (_freeProc != NULL) {
		FreeProc(_freeProc);
		_freeProc = NULL;
	    }
	    /*
	     * _tmpProc == _curProc if the same PROCPROC on whose stack
	     * we executed one or more FUNCPROCs is scheduled to execute 
	     * immediately afterwards.
	     */
	    if (tmpProc != _curProc) {
		SwitchCoproc(&tmpProc->coproc, &_curProc->coproc);
	    }
	    return;
	default:
	    printf("Error: Unknown process type.\n");
	    abort();
	}
    }
}

void
Terminate ()
{
    _freeProc = _curProc;
    Dispatch();
}

void
PrintSimState()
{
    printf("\nSimulation State:\n");
    printf("numFunc:%d numProc:%d\n", numFunc, numProc);
}
