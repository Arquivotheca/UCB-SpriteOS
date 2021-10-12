/* syscalls.h -- Nachos system call interface 
 */
/*
 Copyright (c) 1992 The Regents of the University of California.
 All rights reserved.  See copyright.h for copyright notice and limitation 
 of liability and disclaimer of warranty provisions.
 */

#include "copyright.h"

#ifndef SYSCALLS_H
#define SYSCALLS_H

/* system call codes */
#define SC_Halt		0
#define SC_Exit		1
#define SC_FileOpen	2
#define SC_FileClose	3
#define SC_FileRead	4
#define SC_FileWrite	5
#define SC_ConsoleRead	6
#define SC_ConsoleWrite	7
#define SC_Exec		8
#define SC_Wait		9

/* user-level assembly assist -- eg, from C, "Syscall(SC_ConsoleWrite, ch)" */
extern void *Syscall(int code, ...); 

/* User programs "call" these procedures, using Syscall
 * Nachos implements these procedures (cf. exception.cc).
 */
extern void Halt();		/* stop the OS */
extern void Exit();		/* stop this address space */

typedef int FileDesc;

extern FileDesc FileOpen(char *name);	/* open a file, return an id */
extern void FileClose(FileDesc id);	/* close file */
extern void FileWrite(FileDesc id, char *buf, int nbytes); /* write to file */
extern void FileRead(FileDesc id, char *buf, int nbytes); /* read from file */

extern char ConsoleRead(); 		/* read character from console */
extern void ConsoleWrite(char ch); 	/* write character to console */

typedef int SpaceId;

extern SpaceId Exec(char *name, int argc, char **argv); /* run program */
extern void Wait(SpaceId id); 		/* wait for program to finish */
#endif
