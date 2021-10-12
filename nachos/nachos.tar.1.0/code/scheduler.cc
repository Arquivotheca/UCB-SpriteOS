/* scheduler.cc -- thread scheduling routines.
 *
 * Assumes interrupts are already disabled, to provide mutual exclusion.
 * NOTE: We can't use Locks to provide mutual exclusion, since
 * if we needed to wait for a Lock, we'd end up calling FindNextToRun(),
 * and that would put us in an infinite loop.
 *
 * Very simple implementation -- no priorities, straight FIFO.
 */

#include "scheduler.h"
#include "system.h"
#include "machine.h"
#include "thread.h"

Thread* currentThread;	/* thread we're currently running */

/* Put thread on ready list */ 
void
Scheduler::ReadyToRun (Thread *thread)
{
    DEBUG('t', "Putting thread %s on ready list.\n", thread->getName());

    thread->setStatus(READY);
    readyList->Append((void *)thread);
}

/* Dequeue thread from ready list, if any, and return thread.
 * If none, return NULL.
 */
Thread *
Scheduler::FindNextToRun ()
{
    return (Thread *)readyList->Remove();
}

/* Cause nextThread to start running.  */
void
Scheduler::Run (Thread *nextThread)
{
    Thread *oldThread = currentThread;
    
#ifdef HW3
    int i;
    
    // Let's save the current machine state here and restore it when we
    // return from the switch.
    int savedRegisters[NumTotalRegs];
    class AddrSpace* savedSpace = space;
    for (i = 0; i < NumTotalRegs; i++)
	savedRegisters[i] = machine->ReadRegister(i);
#endif
    
    oldThread->CheckOverflow();

    currentThread = nextThread;
    currentThread->setStatus(RUNNING);
    
    DEBUG('t', "Switching from thread \"%s\" to thread \"%s\"\n",
	  oldThread->getName(), nextThread->getName());
    
    /* This is the asm routine defined in switch.s.  You may have to think
     * a bit to figure out what happens after this, both from the point
     * of view of the thread and from the perspective of the "outside world".
     */
    SWITCH(oldThread, nextThread);
    
    DEBUG('t', "Now in thread \"%s\"\n", currentThread->getName());

    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }
    
#ifdef HW3
    // Now put the state back.
    space = savedSpace;
    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, savedRegisters[i]);
#endif
}

void 
Scheduler::Print()
{
    printf("Ready list contents:\n");
    readyList->Mapcar((VoidFunctionPtr) ThreadPrint);
}
