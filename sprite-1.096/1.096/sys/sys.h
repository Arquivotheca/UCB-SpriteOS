/*
 * sys.h --
 *
 *     Routines and types for the sys module.
 *
 * Copyright 1985 Regents of the University of California
 * All rights reserved.
 *
 * $Header: /sprite/src/kernel/sys/RCS/sys.h,v 9.8 91/05/06 14:53:14 kupfer Exp $ SPRITE (Berkeley)
 *
 */

#ifndef _SYS
#define _SYS

#ifndef _ASM

#ifdef KERNEL
#include <user/sys.h>
#include <sprite.h>
#include <status.h>
#include <spriteTime.h>
#else /* KERNEL */
#include <sys.h>
#include <sprite.h>
#include <status.h>
#include <spriteTime.h>
#endif /* KERNEL */

#endif /* _ASM */

/*
 * Stuff for system calls.
 *
 * SYS_ARG_OFFSET	Where the system calls arguments begin.
 * SYS_MAX_ARGS		The maximum number of arguments possible to be passed
 *			    to a system call.
 * SYS_ARG_SIZE		The size in bytes of each argument.
 */

#define	SYS_ARG_OFFSET	8
#define	SYS_MAX_ARGS	10
#define	SYS_ARG_SIZE	4

#ifndef _ASM
#ifdef KERNEL

extern	Boolean	sys_ShuttingDown;	/* Set when halting */
extern	Boolean	sys_ErrorShutdown;	/* Set after a bad trap or error */
extern	Boolean	sys_ErrorSync;		/* Set while syncing disks */
extern	int	sys_NumCalls[];

extern void	Sys_Init _ARGS_((void));
extern void	Sys_SyncDisks _ARGS_((int trapType));
extern int	Sys_GetHostId _ARGS_((void));
extern void	Sys_HostPrint _ARGS_((int spriteID, char *string));
extern ReturnStatus Sys_GetTimeOfDay _ARGS_((Time *timePtr,
		    int *localOffsetPtr, Boolean *DSTPtr));
extern ReturnStatus Sys_SetTimeOfDay _ARGS_((Time *timePtr, int localOffset,
		    Boolean DST));

extern int	vprintf _ARGS_(());
extern void	panic _ARGS_(());

/* Temporary declaration until prototyping is done */
extern ReturnStatus Proc_RemoteDummy();

#else

/*
 *  Declarations of system call stubs, which happen to have the
 *  same name as the user-visible routines.
 */

extern ReturnStatus Sys_GetTimeOfDay();
extern ReturnStatus Sys_SetTimeOfDay();
extern ReturnStatus Sys_DoNothing();
extern ReturnStatus Sys_Shutdown();
extern ReturnStatus Sys_GetMachineInfo();
extern ReturnStatus Sys_GetMachineInfoNew();

#endif /* KERNEL */
#endif /* _ASM */
#endif /* _SYS */
