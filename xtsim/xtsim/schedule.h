/*
 * schedule.h --
 *
 * This module schedules coroutines based on simulation time.
 * InitSim() must be called before using any routine from this module.
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
 * $Header: /user4/eklee/sim/RCS/schedule.h,v 1.11 91/03/29 10:20:18 eklee Exp Locker: eklee $ SPRITE (Berkeley)
 */

#ifndef _SCHEDULE_H
#define _SCHEDULE_H

#include "queue.h"
#include "coproc.h"

typedef unsigned long SimTime;

typedef enum { PROCPROC=1, FUNCPROC } ProcType;

#define MAX_USER_QUEUE_NUM 8
typedef struct {
    int         *userQueue[MAX_USER_QUEUE_NUM];
    SimTime     time;
    ProcType    procType;
    int		pid;
} GenProc;

#define NAME_SIZE 80
typedef struct {
    GenProc     genProc;
    COPROCtype  coproc;
    char	name[NAME_SIZE];
} ProcProc;

#define MAX_USER_DATA_NUM 8
typedef struct {
    GenProc     genProc;
    void        (*func)();
    int         *clientData;
    int		userData[MAX_USER_DATA_NUM];
} FuncProc;

extern SimTime	_globalTime;
extern ProcProc	*_spawnTmp;
extern ProcProc	*_curProc;
extern FuncProc	*_curFunc;

/*
 *----------------------------------------------------------------------
 *
 * Spawn --
 *	Spawn is analogous to Unix fork, but duplicates only the current
 *	procedure's *local* variables; therefore, the child process should
 *	never execute a 'return' (call Terminate() instead).
 *	If the child process needs access to the procedure's formal parameters,  *	the formals should be copied to locals before calling Spawn.
 *	Any storage to be shared between processes must be declared global or 
 *	shared via *pointers* which are initialized before calling Spawn.
 *
 * Results:
 *	Spawn returns the process id of the newly created child process to its
 *	parent and zero to the child.
 *
 * Side effects:
 *	Creates and schedules new coroutine thread.
 *
 *----------------------------------------------------------------------
 */

#define Spawn(name) (							\
	SchedProc(0, _spawnTmp = AllocProc(name)),			\
	( CreateCoproc( &(_spawnTmp)->coproc ) ? _spawnTmp : NULL )	\
)

/*
 * Return current simulation time.
 */
#define LocalTime()	(_globalTime)

/*
 * Spend simulation time.
 */
#define Delay(dt)		(Delay1((SimTime) (dt)))
#define SchedProc(dt, proc)	(SchedProc1((SimTime) (dt), (GenProc *) (proc)))
#define ExecuteFunc(p)		(((FuncProc *)(p))->func((p)->clientData));

/*
 * Internal use only.
 */
#define NextProc()	( (GenProc *) First( &ready_q ) )

/*
 * Attach property prop identified by propID to current process.
 * propID may range from 0 to MAX_USER_QUEUE_NUM - 1
 * (To be phased out; use GetCurQueueNode instead.)
 */
#define PutCurProcProp(propID, prop)	\
    (cur_proc->userQueue[MAX_USER_QUEUE_NUM - 1 - (propID)] = (int *) (prop))

#define PutProcProp(proc, propID, prop)	\
    (((GenProc *)(proc))->userQueue[MAX_USER_QUEUE_NUM - 1 - (propID)] = (int *) (prop))

/*
 * Retreave property identified by propID from process proc.
 * (To be phased out; use GetCurQueueNode instead.)
 */
#define GetProcProp(proc, propID)	\
    (((GenProc *) proc)->userQueue[MAX_USER_QUEUE_NUM - 1 - (propID)])

#define GetCurProcQueueNode() (_curProc->userQueue)
#define GetProcQueueNode(proc) (((GenProc *) (proc))->userQueue)

extern void InitSchedule();
extern ProcProc *AllocProc();
extern FuncProc *MakeFunc();
extern void GetFuncArgs();
extern void FreeProc();
extern void FreeFunc();
extern void SchedProc1();
extern void Delay1();
extern void Dispatch();
extern void Terminate();

#endif _SCHEDULE_H
