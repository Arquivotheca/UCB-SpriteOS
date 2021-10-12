/* machine.h -- Low-level machine emulation routines.  
 *
 * A Machine handles interrupts, and in later assignments,
 * will contain the interface for the MIPS simulator.
 *
 * We will be changing this file, so don't modify it!
 */

#ifndef MACHINE_H
#define MACHINE_H

#include "utility.h"
#include "memory.h"

enum InterruptStatus { InterruptsOff, InterruptsOn };

#define StackSize     (4 * 1024)      /* in words */

#ifdef HW2
/* All the different kinds of interrupts we can get.  Note that we separate
 * out the timer-based interrupts from the program-generated exceptions --
 * in a real machine they would all go in the same vector table, but this
 * is easier for our purposes.
 */
enum InterruptType { DiskInterrupt,    // A disk operation has completed.
		     ConsoleInterrupt, // A console operation has completed.
		     TimerInterrupt,   // The timer has expired.
		     NetworkInterrupt, // A network event has occurred.
		     
		     NumInterruptTypes // This is a trick to count the enums.
};

// This definition is for the function that we call when an interrupt comes.
typedef void (*InterruptHandler)(int arg);
#endif

#ifdef HW3
// Here are the program-generated exceptions.
enum ExceptionType { SyscallException,      // A program executed a system call.
		     PageFaultException,    // Valid bit in PTE not set.
		     BusErrorException,     // Translation gave invalid address.
		     AddressErrorException, // Unaligned or OOB addr ref.
		     OverflowException,     // Integer overflow in add or sub.
		     IllegalInstrException, // Unimplemented or reserved instr.
		     
		     NumExceptionTypes
};

// This definition is for the type of function that we call when
// an Exception occurs.  It's the same as InterruptHandler.
typedef void (*ExceptionHandler)(int arg);

#define StackReg	29
#define RetAddrReg	31
#define NumGPRegs	32
#define HiReg		32
#define LoReg		33
#define PCReg		34
#define NextPCReg	35
#define PrevPCReg	36
#define LoadReg		37	/* The register target of a delayed load. */
#define LoadValueReg	38	/* The value to be loaded by a delayed load. */
#define NumTotalRegs	39

// This should be somewhere else, but we need to see it here.

class DecodedInstruction {
  public:
    char opCode;     // Type of instruction.  This is NOT the same as the
    		     // opcode field from the instruction: see defs in mips.h
    char rs, rt, rd; // Three registers from instruction.
    int extra;       // Immediate or target or shamt field or offset.
                     // Immediates are sign-extended.
    char* string();  // Useful for debugging.
};

#endif

class Machine {
  public:
    Machine();
    
    /* Turn interrupts on or off, and return previous setting.
     * Increment timer everytime interrupts get enabled.
     */
    InterruptStatus InterruptLevel(InterruptStatus level);
    
    void InterruptEnable() { (void) InterruptLevel(InterruptsOn); }
    
    /* Wait for any pending interrupts. (In assignment 1, there aren't any!) */
    void Idle();
    
    /* Return TRUE iff there are any interrupts pending. */
    bool InterruptsPending();
    
    /* Quit the operating system, printing out usage statistics. */
    void Halt();
    
    int getTimerTicks() { return (timerTicks); }
    
#ifdef HW2
    /* Note that interrupt handlers must never yield.  Exception handlers
     * can, though.  Note that the "arg" parameter to the handler function
     * is given in ScheduleInterrupt() in the interrupt case, but by
     * setExceptionHandler() in the exception case.
     */

    /* This fills in the ``interrupt vector'' with the function. */
    void setInterruptHandler(InterruptType type, InterruptHandler func);
    
    /* Schedule an interrupt to happen ``when'' ticks in the future. */
    void ScheduleInterrupt(InterruptType type, int arg, int when);
#endif

#ifdef HW3
    // This sets an exception handler, similar to the above routine.
    void setExceptionHandler(ExceptionType type, ExceptionHandler func,
			     int arg);

    // Note that in the exception routine, we may have to finish a delayed load.
    void RaiseException(ExceptionType which);

    int ReadRegister(int num) {
	ASSERT((num >= 0) && (num < NumTotalRegs));
	return (registers[num]);
    }
    void WriteRegister(int num, int value) {
	ASSERT((num >= 0) && (num < NumTotalRegs));
	// DEBUG('m', "WriteRegister %d, value %d\n", num, value);
	registers[num] = value;
    }
    
    // Run until the simulated program exits (i.e., returns from main()).
    void Run();

    void DumpState();

    // Stop after every instruction if on is TRUE.
    void setSingleStepMode(bool on)    { singleStep = on; }
    
    // This is rather dubious, but it's the only way we can get context
    // switching to work.
    void YieldAfter() { shouldYield = TRUE; }
    
#endif

#ifdef HW2
    // We're breaking our rule about public data members here.
    int numDiskReads;
    int numDiskWrites;
#endif
#ifdef HW3
    int numConsoleCharsRead;
    int numConsoleCharsWritten;
    
    int badVirtualAddress;
#endif
    
  private:
    InterruptStatus interruptLevel;
    int timerTicks;      // How long we've been running.
#ifdef HW2
    int idleTicks;       // How many ticks we spent idle.
    bool insideInterruptHandler;
#endif
    
    void OneTick();       // Tick the timer once.
    
#ifdef HW2
    // Info to keep track of the interrupt handler functions and when and
    // how they should be fired.
    struct {
	int time;                 // The tick this one should be fired at.
	InterruptHandler handler; // The function to call.
	int arg;                  // The arg to the function.
    } interruptInfo[NumInterruptTypes];
    
    // Fire off any interrupt handlers that need it.  If idling is TRUE,
    // then fire the first one that comes due.  If one was fired, return
    // TRUE, else FALSE.
    bool CheckInterrupts(bool idling);
    void FireInterrupt(InterruptType which);
#endif
    
#ifdef HW3
    // This is easier for exceptions.
    struct {
	ExceptionHandler handler; // The function to call.
	int arg;             // The arg to the function.
    } exceptionInfo[NumExceptionTypes];
    
    int registers[NumTotalRegs];
    
    bool singleStep;
    int runUntilTime;
    
    bool shouldYield;  // Should we yield at the end of this tick?
    
    // This stuff is in mipssim.cc. -----------------
    
    // Do one instruction.  Returns TRUE if we should keep going, FALSE
    // if the process is finished.
    bool OneInstruction();
    
    // Decode an instruction into its fields.
    DecodedInstruction Decode(int value);
    
    // Simulate multi-register multiplies.
    void Mult(int a, int b, bool signedArith, int* hiPtr, int* loPtr);
    
#endif
};

#endif
