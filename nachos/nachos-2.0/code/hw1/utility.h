// utility.h -- some useful stuff
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef UTILITY_H
#define UTILITY_H

// Wrap everything that is actually C with an extern "C" block.
// This prevents the internal forms of the names from being
// changed by the C++ compiler.
extern "C" {
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void abort();
}

#define min(a,b)  (((a) < (b)) ? (a) : (b))
#define max(a,b)  (((a) > (b)) ? (a) : (b))

// Boolean values.  This is the same definition as in the g++ library.
typedef enum { FALSE = 0, TRUE = 1 } bool;

// This declares the type "VoidFunctionPtr" to be a "pointer to a
// function taking an integer argument and returning nothing".  With
// such a function pointer (say it is "func"), we can call it like this:
//	(*func) (17);
typedef void (*VoidFunctionPtr)(int arg); 

// Enable selected debug messages.
//   '+' -- turn on all debug messages
//   't' -- thread system
//   's' -- semaphores, locks, and conditions
//   'i' -- interrupt emulation
//   'm' -- machine emulation
//   'd' -- disk emulation (in homework 2)
//   'f' -- file system (in homework 2)
//   'a' -- address spaces (in homework 3)
//   'n' -- network emulation (in homework 5)
extern void DebugInit (char* flags);

// Is this debug flag enabled?
extern bool DebugIsEnabled(char flag);

// Print debug message if flag is enabled
extern void DEBUG (char flag, char* format, ...);

// If the condition is false, print a message and dump core. 
//
// Useful for documenting assumptions in the code.
#define ASSERT(condition)						      \
    if (!(condition)) {							      \
	fprintf(stderr, "Assertion failed: line %d, file \"%s\"\n",	      \
		__LINE__, __FILE__);					      \
	abort();							      \
    }

// From here down are some routines used to simplify the emulation software
#ifdef HW2
// open/read/write/lseek/close, and check for error
extern int Open(char *name, int flags, int mode);
extern void Read(int fd, char *buffer, int nBytes);
extern void Write(int fd, char *buffer, int nBytes);
extern void Lseek(int fd, int offset, int whence);
extern void Close(int fd);
#endif

#ifdef HW3
// Check file to see if there are any characters to be read without waiting
extern bool PollFile(int fd);
#endif

#endif
