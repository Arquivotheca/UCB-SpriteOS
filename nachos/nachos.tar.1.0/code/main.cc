/* main.cc -- entry point into the operating system */

#include "utility.h"
#include "thread.h"
#include "synch.h"
#include "machine.h"
#include "scheduler.h"
#include "system.h"

/* A simple test case. */
void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	printf("*** thread %d looped %d times\n", which, num);
        currentThread->Yield();
    }
}

void
SimpleTest()
{
    DEBUG('t', "Entering SimpleTest");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, 1);
    SimpleThread(0);
}

/* The main routine. */
int
main (int argc, char **argv)
{
    DEBUG('t', "Entering main");
    Initialize(argc, argv);
    
    SimpleTest();
    currentThread->Finish();
    
    // Not reached.
    return(0);
}
