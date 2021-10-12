/* thread.h -- Interface for users of threads.
 *
 * To fork a thread, we must first allocate a data structure for it
 * "t = new Thread", and then doing the fork "t->fork(f, arg)".
 */

#ifndef THREAD_H
#define THREAD_H

#include "utility.h"
#include "machine.h"

enum ThreadStatus { RUNNING, READY, BLOCKED };

#define MachineStateSize 10   /* 8 GP registers, FP, and RA. */

/*  Every thread has:
 *    an execution stack for activation records ("stackTop" and "stack")
 *    space to save registers while blocked 
 *    a "status" (running/ready/blocked)
 */

class Thread {
  public:
    Thread(char* threadName);	// name for convenience
    ~Thread(); 

    void Fork(VoidFunctionPtr func, int arg); 	// make thread run (*func)(arg)

    void Yield();  // relinquish the processor if any other thread is runnable
    
    void Sleep();  // put the thread to sleep and relinquish the processor
    
    void Finish();  // the thread is done executing
    
    void CheckOverflow();   // make sure thread hasn't overflowed its stack
    
    void setStatus(ThreadStatus st) { status = st; }
    
    char* getName() { return (name); }
    void Print() { printf("%s, ", name); }
    
  private:
    /* These first two members MUST be in these positions for SWITCH. */
    int* stackTop;
    int machineState[MachineStateSize];  /* All except the stackTop. */
    
    int* stack;     /* The bottom of the stack, or NULL for the main thread. */
    ThreadStatus status;
    char* name;
    
    /* This routine is used internally by the constructors. */
    void StackAllocate(VoidFunctionPtr func, int arg);
};

extern Thread* threadToBeDestroyed;  /* The thread that just finished. */

/* Initialize the thread system. */
extern void ThreadSystemInit();
extern void ThreadPrint(int arg);

/* Magical machine-dependent routines, defined in switch.s */

/* First frame on thread execution stack; 
 *   Enables interrupts, calls "func", and when that returns, calls 
 *   ThreadFinish().
 */
extern "C" void ThreadRoot();

/* Stop running oldThread and start running newThread. */
extern "C" void SWITCH(Thread *oldThread, Thread *newThread);

#endif
