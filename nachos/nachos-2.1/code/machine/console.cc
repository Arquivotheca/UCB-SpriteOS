// console.cc -- emulate a tty
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "console.h"
#include "system.h"
#include "stats.h"
#include "interrupt.h"

extern "C" {
#include <sys/types.h>
#include <sys/file.h>

// int open(char *name, int flags, int mode);
}

int open(char *name, int flags, int mode);
// Dummy functions because C++ is weird about pointers to member functions
static void ConsoleReadPoll(int arg) 
{ Console *console = (Console *)arg; console->CheckCharAvail(); }
static void ConsoleWriteDone(int arg)
{ Console *console = (Console *)arg; console->WriteDone(); }

// readAvail is called when a character has arrived, ready to be "got"
// writeDone is called when the character is "put"
Console::Console(char *readFile, char *writeFile, VoidFunctionPtr readAvail, 
		VoidFunctionPtr writeDone, int callArg)
{
    if (readFile == NULL)
	readFileNo = 0;		// stdin
    else
    	readFileNo = Open(readFile, O_RDONLY, 0);
    if (writeFile == NULL)
	writeFileNo = 1;	// stdout
    else
    	writeFileNo = Open(writeFile, O_RDWR|O_CREAT|O_TRUNC, 0666);

    // set up the stuff to emulate asynchronous interrupts
    writeHandler = writeDone;
    readHandler = readAvail;
    handlerArg = callArg;
    putBusy = FALSE;
    incoming = EOF;

    // start polling for incoming packets
    interrupt->Schedule(ConsoleReadPoll, (int)this, ConsoleTime, ConsoleReadInt);
}

// If a character has been typed and there is buffer space, read it in
// and then notify user that character can be read
void
Console::CheckCharAvail()
{
    // schedule the next time to poll for a packet
    interrupt->Schedule(ConsoleReadPoll, (int)this, ConsoleTime, ConsoleReadInt);

    // do nothing if character is already buffered, or none to be read
    if ((incoming != EOF) || !PollFile(readFileNo))
	return;	  

    // otherwise, read character and tell user about it
    Read(readFileNo, &incoming, sizeof(char));
    stats->numConsoleCharsRead++;
    (*readHandler)(handlerArg);	
}

// notify user that another character can be output
void
Console::WriteDone()
{
    putBusy = FALSE;
    stats->numConsoleCharsWritten++;
    (*writeHandler)(handlerArg);
}

// read a character, if one is buffered
char
Console::GetChar()
{
   char ch = incoming;

   incoming = EOF;
   return ch;
}

// write a character to the console, schedule an interrupt to occur
// in the future, and return
void
Console::PutChar(char ch)
{
    ASSERT(putBusy == FALSE);
    Write(writeFileNo, &ch, sizeof(char));
    putBusy = TRUE;
    interrupt->Schedule(ConsoleWriteDone, (int)this,ConsoleTime,ConsoleWriteInt);
}
