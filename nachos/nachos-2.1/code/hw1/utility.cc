// utility.cc -- miscellaneous routines
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "utility.h"
#include <stdarg.h>

static char *enableFlags = NULL; // controls which DEBUG messages are printed 

// Select which debug messages are to be printed
void
DebugInit (char *flags)
{
    enableFlags = flags;
}

bool
DebugIsEnabled(char flag)
{
    return (enableFlags && (strchr(enableFlags, flag) ||
			    strchr(enableFlags, '+')));
}

// Print debug message if flag is enabled, or if the + wildcard flag is given
void 
DEBUG (char flag, char *format, ...)
{
    if (DebugIsEnabled(flag)) {
	va_list ap;
	// You will get an unused variable message here -- ignore it.
	va_start(ap, format);
	vfprintf(stdout, format, ap);
	va_end(ap);
	fflush(stdout);
    }
}

// From here down are some routines used to simplify the emulation software

#ifdef HW2

extern "C" {
// int open(char *name, int flags, int mode);
// int read(int filedes, char *buf, int numBytes);
// int write(int filedes, char *buf, int numBytes);
// int lseek(int filedes, int offset, int whence);
// int close(int filedes);
// int select(int numBits, void *readFds, void *writeFds, void *exceptFds, struct timeval *timeout);
}

// open file and check for error
int
Open(char *name, int flags, int mode)
{
    int fd = open(name, flags, mode);

    ASSERT(fd >= 0); 
    return fd;
}

// read from file and check for error
void
Read(int fd, char *buffer, int nBytes)
{
    int retVal = read(fd, buffer, nBytes);
    ASSERT(retVal == nBytes);
}

// write file and check for error
void
Write(int fd, char *buffer, int nBytes)
{
    int retVal = write(fd, buffer, nBytes);
    ASSERT(retVal == nBytes);
}

// lseek and check for error
void 
Lseek(int fd, int offset, int whence)
{
    int retVal = lseek(fd, offset, whence);
    ASSERT(retVal >= 0);
}

// close file and check for error
void 
Close(int fd)
{
    int retVal = close(fd);
    ASSERT(retVal >= 0); 
}
#endif

#ifdef HW3
extern "C" {
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/file.h>
}

#include "interrupt.h"
#include "system.h"

// Check file/socket to see if we can read any characters without waiting
bool
PollFile(int fd)
{
    int rfd = (1 << fd), wfd = 0, xfd = 0, retVal;
    struct timeval pollTime;

    pollTime.tv_sec = 0;
    if (interrupt->getStatus() == IdleMode)
        pollTime.tv_usec = 20000;              // let other machine run
    else
        pollTime.tv_usec = 0;                 // simple poll
    retVal = select(32, &rfd, &wfd, &xfd, &pollTime);
    ASSERT((retVal == 0) || (retVal == 1));
    if (retVal == 0)
	return FALSE;                 // no character waiting to be read
    return TRUE;
}
#endif

