/* synch.cc -- Thread synchronization routines -- semaphores, locks 
 *   and condition variables.
 *
 * Semaphores disable/enable interrupts for mutual exclusive 
 *   access to the queue of threads waiting for the semaphore.
 *
 * Assignment #1 is to implement locks and condition variables using 
 *   semaphores for synchronization.    
 */

#include "synch.h"
#include "utility.h"
#include "thread.h"
#include "scheduler.h"
#include "machine.h"

/* Semaphores ---------------------------------------------------------- */

/* Initialize a new Semaphore. */
Semaphore::Semaphore(char* semaphoreName, int val)
{
    name = semaphoreName;
    value = val;
    queue = new List;
}

/* Block until semaphore value > 0; then decrement it and return. */
void
Semaphore::P ()
{
    InterruptStatus oldLevel = machine->InterruptLevel(InterruptsOff);
    
    /* Between when a thread is woken and when it starts to run,
     * another thread may have P()'ed that semaphore.  Thus, the thread 
     * checks each time to see if the semaphore is still available.
     */
    while (value == 0) {
	queue->Append((void *)currentThread);
	currentThread->Sleep();
    }
    value--;
    (void) machine->InterruptLevel(oldLevel);
}

/* Increment semaphore value; wake up a waiting thread, if any.  */
void
Semaphore::V ()
{
    Thread *thread;
    InterruptStatus oldLevel = machine->InterruptLevel(InterruptsOff);

    value++;
    thread = (Thread *)queue->Remove();
    if (thread != NULL)
	scheduler->ReadyToRun(thread);
    (void) machine->InterruptLevel(oldLevel);
}

#ifdef HW1_SOLN
/* Locks ---------------------------------------------------------- */

/* Initialize a new Lock. */
Lock::Lock(char* lockName)
{
    name = lockName;
    semaphore = new Semaphore(lockName, 1);
}

/* Acquire lock; relinquish processor if lock is currently held.  */
void
Lock::Acquire ()
{
    semaphore->P();
}

/* Release lock, waking up any threads waiting for the lock.  */
void
Lock::Release ()
{
    semaphore->V();
}

/* Conditions ---------------------------------------------------------- */

/* Initialize a new Condition. */
Condition::Condition(char* condName)
{
    name = condName;
    semaphore = new Semaphore(condName, 0);
    numWaiting = 0;
}

/* Wait on condition, relinquishing the processor.
 *   Lock must be held when called; it is released when the
 *   processor is relinquished and re-acquired when the thread wakes up.
 */
void
Condition::Wait (Lock *conditionLock)
{
    numWaiting++;
    conditionLock->Release();
    semaphore->P();
    conditionLock->Acquire();
}

/* Wake up a thread (if any) who is waiting on the condition.  Lock must 
 * be held when called.  */
void
Condition::Signal (Lock *conditionLock)
{
    if (numWaiting > 0)
      {
      numWaiting--;
      semaphore->V();
      }
}

/* Wake up all threads waiting on the condition.  Lock must be held when called.
 */
void
Condition::Broadcast (Lock *conditionLock)
{
    while (numWaiting > 0)
      {
      numWaiting--;
      semaphore->V();
      }
}

#endif
