// console.h -- emulate a terminal
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef CONSOLE_H
#define CONSOLE_H

#include "utility.h"

class Console {
  public:
	// readAvail is called when a character has arrived, ready to be "got"
	// writeDone is called when the next character can be "put"
    Console(char *readFile, char *writeFile, VoidFunctionPtr readAvail, 
		VoidFunctionPtr writeDone, int callArg);
    ~Console();

    // write a character to the console, return immediately, and
    // call "writeHandler" interrupt when next char can go
    void PutChar(char ch); 

    // poll console input -- return char if any,  EOF if none
    // "readHandler" is called whenever there is a char to be gotten
    char GetChar();	   

    void WriteDone();	 // internal emulation routines 
    void CheckCharAvail();

  private:
    int readFileNo;
    int writeFileNo;
    VoidFunctionPtr writeHandler; // call when put finishes 
    VoidFunctionPtr readHandler; // call when char can be read
    int handlerArg;
    bool putBusy;    	// Is a putchar operation in progress?
    char incoming;    	// EOF if no char to read, otherwise the char
};

#endif
