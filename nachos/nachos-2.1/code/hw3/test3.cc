// test3.cc -- Address spaces and multiprogramming
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "filesys.h"
#include "openfile.h"
#include "addrspace.h"
#include "synch.h"
#include "console.h"
#include "system.h"
#include "thread.h"

void
StartProcess(char *filename)
{
    OpenFile *binary = fileSystem->Open(filename);
    AddrSpace* as;

    if (binary == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    currentThread->space = new AddrSpace(binary);    
    delete binary;	// close file

    machine->Run();
    delete as;
}

// data structures needed for the console test
static Console *console;
static Semaphore *readAvail;
static Semaphore *writeDone;

static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

void 
ConsoleTest (char *in, char *out)
{
    char ch;

    console = new Console(in, out, ReadAvail, WriteDone, 0);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    
    for (;;) {
	readAvail->P();		// wait for character to arrive
	ch = console->GetChar();
	console->PutChar(ch);		// echo it!
	if (ch == 'q') return;  // if q, quit
    }
}
