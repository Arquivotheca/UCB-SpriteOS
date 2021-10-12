/* thread.cc -- thread management routines to fork/finish/switch threads.  */

#include "utility.h"
#include "thread.h"
#include "scheduler.h"
#include "machine.h"
#include "switch.h"
#include "synch.h"

Thread* threadToBeDestroyed;  // If non-NULL, delete this after a SWITCH.

/* Use this value as a fencepost to detect stack overflow. */
#define STACK_FENCEPOST 0xdeadbeef

Thread::Thread(char* threadName)
{
    name = threadName;
    stackTop = NULL;
    stack = NULL;
}

/* The destructor */
Thread::~Thread()
{
    DEBUG('t', "Deleting thread \"%s\"\n", name);
    if (stack != NULL)
	delete stack;
}

/* Invoke (*func)(arg), allowing caller and callee to execute concurrently. 
 *
 * Initialize the thread stack to run the procedure, and then put
 * the thread on the ready queue.
 */
void 
Thread::Fork(VoidFunctionPtr func, int arg)
{
    DEBUG('t', "Forking thread \"%s\" with func = 0x%x, arg = %d\n",
	  name, (int) func, arg);
    
    StackAllocate(func, arg);

    InterruptStatus oldLevel = machine->InterruptLevel(InterruptsOff);
    scheduler->ReadyToRun(this);	/* assumes interrupts off */
    (void) machine->InterruptLevel(oldLevel);
}    

/* Check stack for overflow.  Because the compiler won't do it for us. */
void
Thread::CheckOverflow()
{
    if ((stack != NULL) && (*stack != STACK_FENCEPOST)) {
	fprintf(stderr, "Error: stack overflow in thread \"%s\"\n",
		getName());
	abort();
    }
}

/* Called by a thread when it's done executing. 
 *
 * NOTE: we don't de-allocate the thread data structure or the execution
 *  stack, because we're running in the thread and on the stack right now!
 *  Instead, we set threadToBeDestroyed, so that Scheduler::Run() will
 *  call the destructor.
 */
void
Thread::Finish ()
{
    (void) machine->InterruptLevel(InterruptsOff);
    ASSERT(this == currentThread);
    
    DEBUG('t', "Finishing thread \"%s\"\n", getName());
    
    threadToBeDestroyed = currentThread;
    Sleep(); 	
    // not reached
}

/* Relinquish the processor if any other thread is ready to run.
 * Similar to Thread::Sleep(), but a little different.
 */
void
Thread::Yield ()
{
    Thread *nextThread;
    InterruptStatus oldLevel = machine->InterruptLevel(InterruptsOff);
    
    ASSERT(this == currentThread);
    
    DEBUG('t', "Yielding thread \"%s\"\n", getName());
    
    nextThread = scheduler->FindNextToRun();
    if (nextThread != NULL) {
	scheduler->ReadyToRun(this);
	scheduler->Run(nextThread);
    }
    (void) machine->InterruptLevel(oldLevel);
}

/* Put the current thread to sleep, relinquishing processor to a ready
 * thread.  Idle the machine if there are no ready threads.
 *
 * Assumes interrupts are already disabled.
 */
void
Thread::Sleep ()
{
    Thread *nextThread;
    
    ASSERT(this == currentThread);
    
    DEBUG('t', "Sleeping thread \"%s\"\n", getName());

    status = BLOCKED;
    while ((nextThread = scheduler->FindNextToRun()) == NULL)
	machine->Idle();	/* no one to run, wait for an interrupt */
        
    scheduler->Run(nextThread); /* returns when we've been signalled */ 
}


/* We have to do this because pointers to member functions are strange. */
static void ThreadFinish()    { currentThread->Finish(); }
static void InterruptEnable() { machine->InterruptEnable(); }

/* Allocate and return an execution stack, building an initial
 *   call frame that will enable interrupts, invoke "func" with arg 
 *   as its one parameter, and call Thread::Finish() when "func" returns.
 */
void
Thread::StackAllocate (VoidFunctionPtr func, int arg)
{
    stack = new int[StackSize];
    stackTop = stack + StackSize;
    
    *stack = STACK_FENCEPOST;
    
    machineState[PCState] = (int) ThreadRoot;
    machineState[FPState] = 0;
    machineState[StartupPCState] = (int) InterruptEnable;
    machineState[InitialPCState] = (int) func;
    machineState[InitialArgState] = arg;
    machineState[WhenDonePCState] = (int) ThreadFinish;
}

/* Initialize thread system */
void
ThreadSystemInit ()
{
    threadToBeDestroyed = NULL;

    // Bootstrap by creating a thread to represent the thread we're 
    // running in as the UNIX process.
    currentThread = new Thread("main");
    currentThread->setStatus(RUNNING);
}

// for use in List::MapCar
void ThreadPrint(int arg)     
{ 
    Thread *t = (Thread *)arg;

    t->Print(); 
}
