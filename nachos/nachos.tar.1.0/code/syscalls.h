/* Definitions for stuff to handle system calls. */

#ifndef SYSCALLS_H
#define SYSCALLS_H

#ifdef HW3_SOL

// Here are the definitions for the types of system calls that we can do.
enum SyscallType { BadSyscall = 0,
		   GetThreadIdSyscall = 1,
		   OpenSyscall = 2,
		   ReadSyscall = 3,
		   WriteSyscall = 4,
		   SeekSyscall = 5,
		   ConsoleReadSyscall = 6,
		   ConsoleWriteSyscall = 7,
		   ExecSyscall = 8,
};

extern void HandleSyscall(int arg);

#endif

#endif
