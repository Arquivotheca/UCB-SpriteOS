/*
 * mips.h --
 *
 *	Declarations of structures used to simulate the MIPS
 *	architecture.
 *
 * Copyright 1989 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: /user1/ouster/mipsim/RCS/mips.h,v 1.9 91/02/03 13:25:17 ouster Exp $ SPRITE (Berkeley)
 */

#ifndef _MIPS
#define _MIPS

#ifndef _TCL
#include <tcl.h>
#endif
#ifndef _HASH
#include <hash.h>
#endif
#ifndef _MIPSIM_IO
#include "io.h"
#endif
#ifndef _COP0
#include "cop0.h"
#endif

/*
 * The following structure is used for each "stop" that has
 * been requested for a machine.
 */

typedef struct Stop {
    char *command;		/* Tcl command to execute, or NULL if
				 * this is a simple stop. */
    int number;			/* Number that identifies this stop for
				 * deletion purposes. */
    unsigned int address;	/* Address (in R2000 memory) of memory word
				 * associated with stop. */
    struct Stop *nextPtr;	/* Next in list of stops associated with
				 * same memory location (NULL for end of
				 * list). */
    struct Stop *overallPtr;	/* Next in list of all stops set for
				 * machine (NULL for end of list). */
} Stop;

/*
 * Each memory word is represented by a structure of the following
 * format.  In order to interpret instructions efficiently, they
 * get decoded into several fields on the first execution after each
 * change to the word.
 */

typedef struct {
    int value;			/* Contents of the memory location. */
    char opCode;		/* Type of instruction.  This is NOT
				 * the same as the opcode field from the
				 * instruction:  see #defines below
				 * for details. */
    char rs, rt, rd;		/* Three registers from instruction. */
    int extra;			/* Immediate or target or shamt field
				 * or offset.  Immediates are sign-extended. */
    Stop *stopList;		/* List of stops to invoke whenever
				 * this memory location is accessed. */
} MemWord;

/*
 * For each callback registered through Sim_CallBack, there is a structure
 * of the following form:
 */

typedef struct CallBack {
    int serialNum;		/* Call the procedure after executing the
				 * instruction with this serial number. */
    void (*proc)();		/* Procedure to call. */
    ClientData clientData;	/* Argument to pass to proc. */
    struct CallBack *nextPtr;	/* Next callback in list of all those
				 * associated with this machine.  NULL
				 * means end of list. */
} CallBack;

/*
 * The structure below describes the state of an R2000 machine.
 */

#define TOTAL_REGS 39
#define NUM_GPRS 32
#define HI_REG (NUM_GPRS)
#define LO_REG (NUM_GPRS+1)
#define PC_REG (NUM_GPRS+2)
#define NEXT_PC_REG (NUM_GPRS+3)
#define STATUS_REG (NUM_GPRS+4)
#define CAUSE_REG (NUM_GPRS+5)
#define EPC_REG (NUM_GPRS+6)

typedef struct {
    Tcl_Interp *interp;		/* Interpreter associated with machine (used
				 * for interpreting commands, returning
				 * errors, etc.) */
    int numWords;		/* Number of words of memory simulated for
				 * this machine. */
    MemWord *memPtr;		/* Array of MemWords, sufficient to provide
				 * memSize bytes of storage. */
    int regs[TOTAL_REGS];	/* General-purpose registers, followed by
				 * hi and lo multiply-divide registers,
				 * followed by program counter and next
				 * program counter.  Both pc's are stored
				 * as indexes into the memPtr array. */
    unsigned int badPC;		/* If an addressing error occurs during
				 * instruction fetch, this value records
				 * the bad address.  0 means no addressing
				 * error is pending. */
    int addrErrNum;		/* If badPC is non-zero, this gives the
				 * serial number (insCount value) of the
				 * instruction after which the addressing
				 * error is to be registered. */
    int loadReg;		/* For delayed loads:  register to be loaded
				 * after next instruction (0 means no delayed
				 * load in progress. */
    int loadValue;		/* Value to be placed in loadReg after next
				 * instruction completes. */
    int insCount;		/* Count of total # of instructions executed
				 * in this machine (i.e. serial number of
				 * current instruction). */
    int firstIns;		/* Serial number corresponding to first
				 * instruction executed in particular run;
				 * used to ignore stops on first ins. */
    int branchSerial;		/* Serial number of most recent branch/jump
				 * instruction;  used to set BD bit during
				 * exceptions. */
    int branchPC;		/* PC of instruction given by "branchSerial":
				 * also used during exceptions. */
    int flags;			/* Used to indicate special conditions during
				 * simulation (for greatest speed, should
				 * normally be zero).  See below for
				 * definitions. */
    int stopNum;		/* Used to assign increasing reference
				 * numbers to stops. */
    Stop *stopList;		/* First in chain of all spies and stops
				 * associated with this machine (NULL means
				 * none). */
    CallBack *callBackList;	/* First in linked list of all callbacks
				 * currently registered for this machine,
				 * sorted in increasing order of serialNum. */
    Hash_Table symbols;		/* Records addresses of all symbols read in
				 * by assembler for machine. */
    IoState ioState;		/* I/O-related information for machine (see
				 * io.h and io.c for details). */
    Cop0 cop0;			/* State of coprocessor 0 (see cop0.h and
				 * cop0.c for details). */
} R2000;

/*
 * Flag values for R2000 structures:
 *
 * STOP_REQUESTED:		1 means that the "stop" command has been
 *				executed and execution should stop ASAP.
 *				
 */

#define STOP_REQUESTED		0x4

/*
 * OpCode values for MemWord structs.  These are straight from the MIPS
 * manual except for the following special values:
 *
 * OP_NOT_COMPILED -	means the value of the memory location has changed
 *			so the instruction needs to be recompiled.
 * OP_UNIMP -		means that this instruction is legal, but hasn't
 *			been implemented in the simulator yet.
 * OP_RES -		means that this is a reserved opcode (it isn't
 *			supported by the architecture).
 */

#define OP_ADD		1
#define OP_ADDI		2
#define OP_ADDIU	3
#define OP_ADDU		4
#define OP_AND		5
#define OP_ANDI		6
#define OP_BEQ		7
#define OP_BGEZ		8
#define OP_BGEZAL	9
#define OP_BGTZ		10
#define OP_BLEZ		11
#define OP_BLTZ		12
#define OP_BLTZAL	13
#define OP_BNE		14
#define OP_BREAK	15
#define OP_DIV		16
#define OP_DIVU		17
#define OP_J		18
#define OP_JAL		19
#define OP_JALR		20
#define OP_JR		21
#define OP_LB		22
#define OP_LBU		23
#define OP_LH		24
#define OP_LHU		25
#define OP_LUI		26
#define OP_LW		27
#define OP_LWL		28
#define OP_LWR		29
#define OP_MFC0		30
#define OP_MFHI		31
#define OP_MFLO		32
#define OP_MTC0		33
#define OP_MTHI		34
#define OP_MTLO		35
#define OP_MULT		36
#define OP_MULTU	37
#define OP_NOR		38
#define OP_OR		39
#define OP_ORI		40
#define OP_RFE		41
#define OP_SB		42
#define OP_SH		43
#define OP_SLL		44
#define OP_SLLV		45
#define OP_SLT		46
#define OP_SLTI		47
#define OP_SLTIU	48
#define OP_SLTU		49
#define OP_SRA		50
#define OP_SRAV		51
#define OP_SRL		52
#define OP_SRLV		53
#define OP_SUB		54
#define OP_SUBU		55
#define OP_SW		56
#define OP_SWL		57
#define OP_SWR		58
#define OP_XOR		59
#define OP_XORI		60

#define OP_NOT_COMPILED	80
#define OP_UNIMP	81
#define OP_RES		82

/*
 * Conversion between R2000 addresses and indexes, which are stored
 * in pc/nextPc fields of R2000 structures and also used to address
 * the memPtr values:
 */

#define INDEX_TO_ADDR(index)	((unsigned) (index << 2))
#define ADDR_TO_INDEX(addr)	(addr >> 2)

/*
 * Miscellaneous definitions:
 */

#define SIGN_BIT	0x80000000
#define R31		31

/*
 * Tcl command procedures provided by the simulator:
 */

extern int Gp_GetCmd(), Gp_PutCmd(), Gp_PutstringCmd();
extern int Sim_GoCmd(), Sim_StepCmd();
extern int Stop_StopCmd();

/*
 * Other procedures that are exported from one file to another:
 */

extern void		Sim_CallBack();
extern R2000 *		Sim_Create();
extern unsigned int	Sim_GetPC();
extern void		Sim_Stop();

extern int		Stop_Execute();

#endif /* _MIPS */
