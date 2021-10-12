/* 
 * sim.c --
 *
 *	This file contains a simple Tcl-based simulator for an abridged
 *	version of the MIPS R2000 architecture.
 *
 * Copyright 1989 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /user1/ouster/mipsim/RCS/sim.c,v 1.20 91/02/03 13:25:24 ouster Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include "asm.h"
#include "mips.h"
#include "sym.h"

/*
 * The table below is used to translate bits 31:26 of the instruction
 * into a value suitable for the "opCode" field of a MemWord structure,
 * or into a special value for further decoding.
 */

#define SPECIAL 100
#define BCOND	101

#define IFMT 1
#define JFMT 2
#define RFMT 3

typedef struct {
    int opCode;		/* Translated op code. */
    int format;		/* Format type (IFMT or JFMT or RFMT) */
} OpInfo;

OpInfo opTable[] = {
    {SPECIAL, RFMT}, {BCOND, IFMT}, {OP_J, JFMT}, {OP_JAL, JFMT},
    {OP_BEQ, IFMT}, {OP_BNE, IFMT}, {OP_BLEZ, IFMT}, {OP_BGTZ, IFMT},
    {OP_ADDI, IFMT}, {OP_ADDIU, IFMT}, {OP_SLTI, IFMT}, {OP_SLTIU, IFMT},
    {OP_ANDI, IFMT}, {OP_ORI, IFMT}, {OP_XORI, IFMT}, {OP_LUI, IFMT},
    {OP_MTC0, IFMT}, {OP_UNIMP, IFMT}, {OP_UNIMP, IFMT}, {OP_UNIMP, IFMT},
    {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_RES, IFMT},
    {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_RES, IFMT},
    {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_RES, IFMT},
    {OP_LB, IFMT}, {OP_LH, IFMT}, {OP_LWL, IFMT}, {OP_LW, IFMT},
    {OP_LBU, IFMT}, {OP_LHU, IFMT}, {OP_LWR, IFMT}, {OP_RES, IFMT},
    {OP_SB, IFMT}, {OP_SH, IFMT}, {OP_SWL, IFMT}, {OP_SW, IFMT},
    {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_SWR, IFMT}, {OP_RES, IFMT},
    {OP_UNIMP, IFMT}, {OP_UNIMP, IFMT}, {OP_UNIMP, IFMT}, {OP_UNIMP, IFMT},
    {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_RES, IFMT},
    {OP_UNIMP, IFMT}, {OP_UNIMP, IFMT}, {OP_UNIMP, IFMT}, {OP_UNIMP, IFMT},
    {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_RES, IFMT}, {OP_RES, IFMT}
};

/*
 * The table below is used to convert the "funct" field of SPECIAL
 * instructions into the "opCode" field of a MemWord.
 */

int specialTable[] = {
    OP_SLL, OP_RES, OP_SRL, OP_SRA, OP_SLLV, OP_RES, OP_SRLV, OP_SRAV,
    OP_JR, OP_JALR, OP_RES, OP_RES, OP_UNIMP, OP_BREAK, OP_RES, OP_RES,
    OP_MFHI, OP_MTHI, OP_MFLO, OP_MTLO, OP_RES, OP_RES, OP_RES, OP_RES,
    OP_MULT, OP_MULTU, OP_DIV, OP_DIVU, OP_RES, OP_RES, OP_RES, OP_RES,
    OP_ADD, OP_ADDU, OP_SUB, OP_SUBU, OP_AND, OP_OR, OP_XOR, OP_NOR,
    OP_RES, OP_RES, OP_SLT, OP_SLTU, OP_RES, OP_RES, OP_RES, OP_RES,
    OP_RES, OP_RES, OP_RES, OP_RES, OP_RES, OP_RES, OP_RES, OP_RES,
    OP_RES, OP_RES, OP_RES, OP_RES, OP_RES, OP_RES, OP_RES, OP_RES
};

/*
 * The following value is used to handle virtually all special cases
 * while simulating.  The simulator normally executes in a fast path
 * where it ignores all special cases.  However, after executing each
 * instruction it checks the current serial number (total # of instructions
 * executed) agains the value below.  If that serial number has been
 * executed, then the simulator pauses to check for all possible special
 * conditions (stops, callbacks, errors, etc.).  Thus anyone that wants
 * to get a special condition handled must be sure to set checkNum below
 * so that the special-check code will be executed.  This facility means
 * that the simulator only has to check a single condition in its fast
 * path.
 */

static int checkNum;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int	AddressError();
static int	BusError();
static void	Compile();
static void	Mult();
static int	Overflow();
static int	ReadMem();
static int	Simulate();
static int	WriteMem();

/*
 *----------------------------------------------------------------------
 *
 * Sim_Create --
 *
 *	Create a description of an R2000 machine.
 *
 * Results:
 *	The return value is a pointer to the description of the R2000
 *	machine.
 *
 * Side effects:
 *	The R2000 structure gets allocated and initialized.  Several
 *	Tcl commands get registered for interp.
 *
 *----------------------------------------------------------------------
 */

R2000 *
Sim_Create(memSize, interp)
    int memSize;		/* Number of bytes of memory to be
				 * allocated for the machine. */
    Tcl_Interp *interp;		/* Interpreter to associate with machine. */
{
    register R2000 *machPtr;
    register MemWord *wordPtr;
    int i;
    extern int Main_QuitCmd();

    machPtr = (R2000 *) malloc(sizeof(R2000));
    machPtr->interp = interp;
    machPtr->numWords = (memSize+3) & ~0x3;
    machPtr->memPtr = (MemWord *)
	    malloc((unsigned) (sizeof(MemWord) * machPtr->numWords));
    for (i = machPtr->numWords, wordPtr = machPtr->memPtr; i > 0;
	    i--, wordPtr++) {
	wordPtr->value = 0;
	wordPtr->opCode = OP_NOT_COMPILED;
	wordPtr->stopList = NULL;
    }
    for (i = 0; i < NUM_GPRS; i++) {
	machPtr->regs[i] = 0;
    }
    machPtr->regs[HI_REG] = machPtr->regs[LO_REG] = 0;
    machPtr->regs[PC_REG] = 0;
    machPtr->regs[NEXT_PC_REG] = 1;
    machPtr->badPC = 0;
    machPtr->addrErrNum = 0;
    machPtr->loadReg = 0;
    machPtr->loadValue = 0;
    machPtr->insCount = 0;
    machPtr->firstIns = 0;
    machPtr->branchSerial = -1;
    machPtr->branchPC = 0;
    machPtr->flags = 0;
    machPtr->stopNum = 1;
    machPtr->stopList = NULL;
    machPtr->callBackList = NULL;
    Hash_InitTable(&machPtr->symbols, 0, HASH_STRING_KEYS);
    Io_Init(machPtr);
    Cop0_Init(machPtr);

    Tcl_CreateCommand(interp, "asm", Asm_AsmCmd, (ClientData) machPtr,
	    (void (*)()) NULL);
    Tcl_CreateCommand(interp, "get", Gp_GetCmd, (ClientData) machPtr,
	    (void (*)()) NULL);
    Tcl_CreateCommand(interp, "go", Sim_GoCmd, (ClientData) machPtr,
	    (void (*)()) NULL);
    Tcl_CreateCommand(interp, "load", Asm_LoadCmd, (ClientData) machPtr,
	    (void (*)()) NULL);
    Tcl_CreateCommand(interp, "put", Gp_PutCmd, (ClientData) machPtr,
	    (void (*)()) NULL);
    Tcl_CreateCommand(interp, "putstring", Gp_PutstringCmd,
	    (ClientData) machPtr, (void (*)()) NULL);
    Tcl_CreateCommand(interp, "quit", Main_QuitCmd, (ClientData) machPtr,
	    (void (*)()) NULL);
    Tcl_CreateCommand(interp, "step", Sim_StepCmd, (ClientData) machPtr,
	    (void (*)()) NULL);
    Tcl_CreateCommand(interp, "stop", Stop_StopCmd, (ClientData) machPtr,
	    (void (*)()) NULL);
    return machPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Sim_GoCmd --
 *
 *	This procedure is invoked to process the "go" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Sim_GoCmd(machPtr, interp, argc, argv)
    R2000 *machPtr;			/* Machine description. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc > 2) {
	sprintf(interp->result,
		"too many args:  should be \"%.50s\" [address]", argv[0]);
	return TCL_ERROR;
    }

    if (argc == 2) {
	char *end;
	int newPc;

	if (Sym_EvalExpr(machPtr, (char *) NULL, argv[1], 0, &newPc, &end)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	if ((*end != 0) || (newPc & 0x3)) {
	    sprintf(interp->result,
		    "\"%.50s\" isn't a valid starting address", argv[1]);
	    return TCL_ERROR;
	}
	machPtr->regs[PC_REG] = ADDR_TO_INDEX(newPc);
	machPtr->regs[NEXT_PC_REG] = machPtr->regs[PC_REG] + 1;
	machPtr->loadReg = 0;
	machPtr->flags = 0;
	machPtr->badPC = 0;
    }

    return Simulate(machPtr, interp, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * Sim_StepCmd --
 *
 *	This procedure is invoked to process the "step" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Sim_StepCmd(machPtr, interp, argc, argv)
    R2000 *machPtr;			/* Machine description. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    if (argc > 2) {
	sprintf(interp->result,
		"too many args:  should be \"%.50s\" [address]", argv[0]);
	return TCL_ERROR;
    }

    if (argc == 2) {
	char *end;
	int newPc;

	if (Sym_EvalExpr(machPtr, (char *) NULL, argv[1], 0, &newPc, &end)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	if ((*end != 0) || (newPc & 0x3)) {
	    sprintf(interp->result,
		    "\"%.50s\" isn't a valid address", argv[1]);
	    return TCL_ERROR;
	}
	machPtr->regs[PC_REG] = ADDR_TO_INDEX(newPc);
	machPtr->regs[NEXT_PC_REG] = machPtr->regs[PC_REG] + 1;
	machPtr->loadReg = 0;
	machPtr->flags = 0;
	machPtr->badPC = 0;
    }

    return Simulate(machPtr, interp, 1);
}

/*
 *----------------------------------------------------------------------
 *
 * Sim_CallBack --
 *
 *	Arrange for a particular procedure to be invoked after a given
 *	number of instructions have been simulated.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	After numIns instructions have been executed, proc will be
 *	invoked in the following way:
 *
 *	void
 *	proc(clientData, machPtr)
 *	    ClientData clientData;
 *	    R2000 *machPtr;
 *	{
 *	}
 *
 *	The clientData and machPtr arguments will be the same as those
 *	passed to this procedure.
 *----------------------------------------------------------------------
 */

void
Sim_CallBack(machPtr, numIns, proc, clientData)
    R2000 *machPtr;		/* Machine of interest. */
    int numIns;			/* Call proc after this many instructions
				 * have been executed in machPtr. */
    void (*proc)();		/* Procedure to call. */
    ClientData clientData;	/* Arbitrary one-word value to pass to proc. */
{
    register CallBack *cbPtr;

    cbPtr = (CallBack *) malloc(sizeof(CallBack));
    cbPtr->serialNum = machPtr->insCount + numIns;
    cbPtr->proc = proc;
    cbPtr->clientData = clientData;
    if ((machPtr->callBackList == NULL) ||
	    (cbPtr->serialNum < machPtr->callBackList->serialNum)) {
	cbPtr->nextPtr = machPtr->callBackList;
	machPtr->callBackList = cbPtr;
    } else {
	register CallBack *cbPtr2;

	for (cbPtr2 = machPtr->callBackList; cbPtr2->nextPtr != NULL;
		cbPtr2 = cbPtr2->nextPtr) {
	    if (cbPtr->serialNum < cbPtr2->nextPtr->serialNum) {
		break;
	    }
	}
	cbPtr->nextPtr = cbPtr2->nextPtr;
	cbPtr2->nextPtr = cbPtr;
    }
    if (cbPtr->serialNum < checkNum) {
	checkNum = cbPtr->serialNum;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Sim_Stop --
 *
 *	Arrange for the execution of the machine to stop after the
 *	current instruction.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The machine will stop executing (if it was executing in the
 *	first place).
 *
 *----------------------------------------------------------------------
 */

void
Sim_Stop(machPtr)
    R2000 *machPtr;			/* Machine to stop. */
{
    machPtr->flags |= STOP_REQUESTED;
    checkNum = machPtr->insCount + 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Sim_GetPC --
 *
 *	This procedure computes the current program counter for
 *	machPtr.
 *
 * Results:
 *	The return value is the current program counter for the
 *	machine.  This is a bit tricky to compute because the PC
 *	is stored as an index, and there may have been an unaligned
 *	value put in the PC.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

unsigned int
Sim_GetPC(machPtr)
    register R2000 *machPtr;		/* Machine whose PC is wanted. */
{
    if ((machPtr->badPC != 0) && (machPtr->insCount >= machPtr->addrErrNum)) {
	return machPtr->badPC;
    }
    return INDEX_TO_ADDR(machPtr->regs[PC_REG]);
}

/*
 *----------------------------------------------------------------------
 *
 * ReadMem --
 *
 *	Read a word from R2000 memory.
 *
 * Results:
 *	Under normal circumstances, the result is 1 and the word at
 *	*valuePtr is modified to contain the R2000 word at the given
 *	address.  If no such memory address exists, or if a stop is
 *	set on the memory location, then 0 is returned to signify that
 *	simulation should stop.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ReadMem(machPtr, address, valuePtr)
    register R2000 *machPtr;	/* Machine whose memory is being read. */
    unsigned int address;	/* Desired word address. */
    int *valuePtr;		/* Store contents of given word here. */
{
    int index, result;
    register MemWord *wordPtr;

    index = ADDR_TO_INDEX(address);
    if (index < machPtr->numWords) {
	wordPtr = &machPtr->memPtr[index];
	if ((wordPtr->stopList != NULL)
		&& (machPtr->insCount != machPtr->firstIns)) {
	    result = Stop_Execute(machPtr, wordPtr->stopList);
	    if ((result != TCL_OK) || (machPtr->flags & STOP_REQUESTED)) {
		return 0;
	    }
	}
	*valuePtr = wordPtr->value;
	return 1;
    }

    /*
     * The word isn't in the main memory.  See if it is an I/O
     * register.
     */

    if (Io_Read(machPtr, (address & ~0x3), valuePtr) == 1) {
	return 1;
    }

    /*
     * The word doesn't exist.  Register a bus error.  If interrupts
     * ever get implemented for bus errors, this code will have to
     * change a bit.
     */

    (void) BusError(machPtr, address, 0);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * WriteMem --
 *
 *	Write a value into the R2000's memory.
 *
 * Results:
 *	If the write completed successfully then 1 is returned.  If
 *	any sort of problem occurred (such as an addressing error or
 *	a stop) then 0 is returned;  the caller should stop simulating.
 *
 * Side effects:
 *	The R2000 memory gets updated with a new byte, halfword, or word
 *	value.
 *
 *----------------------------------------------------------------------
 */

static int
WriteMem(machPtr, address, size, value)
    register R2000 *machPtr;	/* Machine whose memory is being read. */
    unsigned int address;	/* Desired word address. */
    int size;			/* Size to be written (1, 2, or 4 bytes). */
    int value;			/* New value to write into memory. */
{
    int index, result;
    register MemWord *wordPtr;

    if (((size == 4) && (address & 0x3)) || ((size == 2) && (address & 0x1))) {
	(void) AddressError(machPtr, address, 0);
	return 0;
    }
    index = ADDR_TO_INDEX(address);
    if (index < machPtr->numWords) {
	wordPtr = &machPtr->memPtr[index];
	if ((wordPtr->stopList != NULL)
		&& (machPtr->insCount != machPtr->firstIns)) {
	    result = Stop_Execute(machPtr, wordPtr->stopList);
	    if ((result != TCL_OK) || (machPtr->flags & STOP_REQUESTED)) {
		return 0;
	    }
	}
	if (size == 4) {
	    wordPtr->value = value;
	} else if (size == 2) {
	    if (address & 0x2) {
		wordPtr->value = (wordPtr->value & 0xffff0000)
			| (value & 0xffff);
	    } else {
		wordPtr->value = (wordPtr->value & 0xffff)
			| (value << 16);
	    }
	} else {
	    switch (address & 0x3) {
		case 0:
		    wordPtr->value = (wordPtr->value & 0x00ffffff)
			    | (value << 24);
		    break;
		case 1:
		    wordPtr->value = (wordPtr->value & 0xff00ffff)
			    | ((value & 0xff) << 16);
			    break;
		case 2:
		    wordPtr->value = (wordPtr->value & 0xffff00ff)
			    | ((value & 0xff) << 8);
		    break;
		case 3:
		    wordPtr->value = (wordPtr->value & 0xffffff00)
			    | (value & 0xff);
		    break;
	    }
	}
	wordPtr->opCode = OP_NOT_COMPILED;
	return 1;
    }

    /*
     * Not in main memory.  See if it's an I/O device register.
     */

    if (Io_Write(machPtr, address, value, size) == 1) {
	return 1;
    }

    (void) BusError(machPtr, address, 0);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Simulate --
 *
 *	This procedure forms the core of the simulator.  It executes
 *	instructions until either a break occurs or an error occurs
 *	(or until a single instruction has been executed, if single-
 *	stepping is requested).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The state of *machPtr changes in response to the simulation.
 *	Return information may be left in *interp.
 *
 *----------------------------------------------------------------------
 */

static int
Simulate(machPtr, interp, singleStep)
    register R2000 *machPtr;		/* Machine description. */
    Tcl_Interp *interp;			/* Tcl interpreter, for results and
					 * break commands. */
    int singleStep;			/* Non-zero means execute exactly
					 * one instruction, regardless of
					 * breaks found. */
{
    register MemWord *wordPtr;		/* Memory word for instruction. */
    register unsigned int pc;		/* Current ins. address, then new
					 * nextPc value. */
    int nextLoadReg, nextLoadValue;	/* New values for "loadReg" and
					 * "loadValue" fields of *machPtr. */
    unsigned int tmp;
    int i, result;
    char *errMsg, msg[20];

    /*
     * Can't continue from an addressing error on the program counter.
     */

    if ((machPtr->badPC != 0) && (machPtr->addrErrNum == machPtr->insCount)) {
	sprintf(interp->result,
		"address error on instruction fetch, pc = 0x%x",
		machPtr->badPC);
	return TCL_ERROR;
    }

    machPtr->flags &= ~STOP_REQUESTED;
    machPtr->firstIns = machPtr->insCount;
    Io_BeginSim(machPtr);

    setCheckNum:
    if (machPtr->callBackList != NULL) {
	checkNum = machPtr->callBackList->serialNum;
    } else {
	checkNum = machPtr->insCount+100000;
    }
    if ((machPtr->badPC != 0) && (machPtr->addrErrNum > machPtr->insCount)) {
	if (checkNum > machPtr->addrErrNum) {
	    checkNum = machPtr->addrErrNum;
	}
    } else {
	machPtr->badPC = 0;
    }
    if (singleStep) {
	checkNum = machPtr->insCount+1;
    }
    while (1) {
	nextLoadReg = 0;

	/*
	 * Fetch an instruction, and compute the new next pc (but don't
	 * store it yet, in case the instruction doesn't complete).
	 */

	pc = machPtr->regs[PC_REG];
	if (pc >= machPtr->numWords) {
	    result = BusError(machPtr, INDEX_TO_ADDR(pc), 1);
	    if (result != TCL_OK) {
		goto stopSimulation;
	    } else {
		goto endOfIns;
	    }
	}
	wordPtr = &machPtr->memPtr[pc];
	pc = machPtr->regs[NEXT_PC_REG]+1;

	/*
	 * Handle breaks on the instruction, if this isn't the first
	 * instruction executed.
	 */

	if ((wordPtr->stopList != NULL)
		&& (machPtr->insCount != machPtr->firstIns)) {
	    result = Stop_Execute(machPtr, wordPtr->stopList);
	    if ((result != TCL_OK) || (machPtr->flags & STOP_REQUESTED)) {
		goto stopSimulation;
	    }
	}

	/*
	 * Execute the instruction.
	 */

	execute:
	switch (wordPtr->opCode) {

	    case OP_ADD: {
		int sum;
		sum = machPtr->regs[wordPtr->rs] + machPtr->regs[wordPtr->rt];
		if (!((machPtr->regs[wordPtr->rs] ^ machPtr->regs[wordPtr->rt])
			& SIGN_BIT) && ((machPtr->regs[wordPtr->rs]
			^ sum) & SIGN_BIT)) {
		    result = Overflow(machPtr);
		    if (result != TCL_OK) {
			goto stopSimulation;
		    } else {
			goto endOfIns;
		    }
		}
		machPtr->regs[wordPtr->rd] = sum;
		break;
	    }

	    case OP_ADDI: {
		int sum;
		sum = machPtr->regs[wordPtr->rs] + wordPtr->extra;
		if (!((machPtr->regs[wordPtr->rs] ^ wordPtr->extra)
			& SIGN_BIT) && ((wordPtr->extra ^ sum) & SIGN_BIT)) {
		    result = Overflow(machPtr);
		    if (result != TCL_OK) {
			goto stopSimulation;
		    } else {
			goto endOfIns;
		    }
		}
		machPtr->regs[wordPtr->rt] = sum;
		break;
	    }

	    case OP_ADDIU:
		machPtr->regs[wordPtr->rt] = machPtr->regs[wordPtr->rs]
			+ wordPtr->extra;
		break;

	    case OP_ADDU:
		machPtr->regs[wordPtr->rd] = machPtr->regs[wordPtr->rs]
			+ machPtr->regs[wordPtr->rt];
		break;

	    case OP_AND:
		machPtr->regs[wordPtr->rd] = machPtr->regs[wordPtr->rs]
			& machPtr->regs[wordPtr->rt];
		break;

	    case OP_ANDI:
		machPtr->regs[wordPtr->rt] = machPtr->regs[wordPtr->rs]
			& (wordPtr->extra & 0xffff);
		break;

	    case OP_BEQ:
		if (machPtr->regs[wordPtr->rs] == machPtr->regs[wordPtr->rt]) {
		    pc = machPtr->regs[NEXT_PC_REG] + wordPtr->extra;
		}
		machPtr->branchSerial = machPtr->insCount;
		machPtr->branchPC = INDEX_TO_ADDR(machPtr->regs[PC_REG]);
		break;

	    case OP_BGEZAL:
		machPtr->regs[R31] =
			INDEX_TO_ADDR(machPtr->regs[NEXT_PC_REG] + 1);
	    case OP_BGEZ:
		if (!(machPtr->regs[wordPtr->rs] & SIGN_BIT)) {
		    pc = machPtr->regs[NEXT_PC_REG] + wordPtr->extra;
		}
		machPtr->branchSerial = machPtr->insCount;
		machPtr->branchPC = INDEX_TO_ADDR(machPtr->regs[PC_REG]);
		break;

	    case OP_BGTZ:
		if (machPtr->regs[wordPtr->rs] > 0) {
		    pc = machPtr->regs[NEXT_PC_REG] + wordPtr->extra;
		}
		machPtr->branchSerial = machPtr->insCount;
		machPtr->branchPC = INDEX_TO_ADDR(machPtr->regs[PC_REG]);
		break;

	    case OP_BLEZ:
		if (machPtr->regs[wordPtr->rs] <= 0) {
		    pc = machPtr->regs[NEXT_PC_REG] + wordPtr->extra;
		}
		machPtr->branchSerial = machPtr->insCount;
		machPtr->branchPC = INDEX_TO_ADDR(machPtr->regs[PC_REG]);
		break;

	    case OP_BLTZAL:
		machPtr->regs[R31] =
			INDEX_TO_ADDR(machPtr->regs[NEXT_PC_REG] + 1);
	    case OP_BLTZ:
		if (machPtr->regs[wordPtr->rs] & SIGN_BIT) {
		    pc = machPtr->regs[NEXT_PC_REG] + wordPtr->extra;
		}
		machPtr->branchSerial = machPtr->insCount;
		machPtr->branchPC = INDEX_TO_ADDR(machPtr->regs[PC_REG]);
		break;

	    case OP_BNE:
		if (machPtr->regs[wordPtr->rs] != machPtr->regs[wordPtr->rt]) {
		    pc = machPtr->regs[NEXT_PC_REG] + wordPtr->extra;
		}
		machPtr->branchSerial = machPtr->insCount;
		machPtr->branchPC = INDEX_TO_ADDR(machPtr->regs[PC_REG]);
		break;

	    case OP_BREAK:
		errMsg = "break instruction";
		goto error;

	    case OP_DIV:
		if (machPtr->regs[wordPtr->rt] == 0) {
		    machPtr->regs[LO_REG] = 0;
		    machPtr->regs[HI_REG] = 0;
		} else {
		    machPtr->regs[LO_REG] =  machPtr->regs[wordPtr->rs]
			    / machPtr->regs[wordPtr->rt];
		    machPtr->regs[HI_REG] = machPtr->regs[wordPtr->rs]
			    % machPtr->regs[wordPtr->rt];
		}
		break;

	    case OP_DIVU: {
		unsigned int rs, rt, tmp;

		rs = (unsigned int) machPtr->regs[wordPtr->rs];
		rt = (unsigned int) machPtr->regs[wordPtr->rt];
		if (rt == 0) {
		    machPtr->regs[LO_REG] = 0;
		    machPtr->regs[HI_REG] = 0;
		} else {
		    tmp = rs / rt;
		    machPtr->regs[LO_REG] = (int) tmp;
		    tmp = rs % rt;
		    machPtr->regs[HI_REG] = (int) tmp;
		}
		break;
	    }

	    case OP_JAL:
		machPtr->regs[R31] =
			INDEX_TO_ADDR(machPtr->regs[NEXT_PC_REG] + 1);
	    case OP_J:
		pc = (pc & 0xfc000000) | wordPtr->extra;
		machPtr->branchSerial = machPtr->insCount;
		machPtr->branchPC = INDEX_TO_ADDR(machPtr->regs[PC_REG]);
		break;

	    case OP_JALR:
		machPtr->regs[wordPtr->rd] =
			INDEX_TO_ADDR(machPtr->regs[NEXT_PC_REG] + 1);
	    case OP_JR:
		tmp = machPtr->regs[wordPtr->rs];
		pc = ADDR_TO_INDEX(tmp);
		if ((tmp & 0x3) && (machPtr->badPC == 0)) {
		    machPtr->badPC = tmp;
		    machPtr->addrErrNum = machPtr->insCount + 2;
		    if (checkNum > machPtr->addrErrNum) {
			checkNum = machPtr->addrErrNum;
		    }
		}
		machPtr->branchSerial = machPtr->insCount;
		machPtr->branchPC = INDEX_TO_ADDR(machPtr->regs[PC_REG]);
		break;

	    case OP_LB:
	    case OP_LBU: {
		int value;
		tmp = machPtr->regs[wordPtr->rs] + wordPtr->extra;
		if (ReadMem(machPtr, tmp, &value) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}

		switch (tmp & 0x3) {
		    case 0:
			value >>= 24;
			break;
		    case 1:
			value >>= 16;
			break;
		    case 2:
			value >>= 8;
		}
		if ((value & 0x80) && (wordPtr->opCode == OP_LB)) {
		    value |= 0xffffff00;
		} else {
		    value &= 0xff;
		}
		nextLoadReg = wordPtr->rt;
		nextLoadValue = value;
		break;
	    }

	    case OP_LH:
	    case OP_LHU: {
		int value;
		tmp = machPtr->regs[wordPtr->rs] + wordPtr->extra;
		if (tmp & 0x1) {
		    result = AddressError(machPtr, tmp, 1);
		    if (result != TCL_OK) {
			goto stopSimulation;
		    } else {
			goto endOfIns;
		    }
		}
		if (ReadMem(machPtr, tmp, &value) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		if (!(tmp & 0x2)) {
		    value >>= 16;
		}
		if ((value & 0x8000) && (wordPtr->opCode == OP_LH)) {
		    value |= 0xffff0000;
		} else {
		    value &= 0xffff;
		}
		nextLoadReg = wordPtr->rt;
		nextLoadValue = value;
		break;
	    }

	    case OP_LUI:
		machPtr->regs[wordPtr->rt] = wordPtr->extra << 16;
		break;

	    case OP_LW: {
		int value;
		tmp = machPtr->regs[wordPtr->rs] + wordPtr->extra;
		if (tmp & 0x3) {
		    result = AddressError(machPtr, tmp, 1);
		    if (result != TCL_OK) {
			goto stopSimulation;
		    } else {
			goto endOfIns;
		    }
		}
		if (ReadMem(machPtr, tmp, &value) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		nextLoadReg = wordPtr->rt;
		nextLoadValue = value;
		break;
	    }

	    case OP_LWL: {
		int value;
		tmp = machPtr->regs[wordPtr->rs] + wordPtr->extra;
		if (ReadMem(machPtr, tmp, &value) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		if (machPtr->loadReg == wordPtr->rt) {
		    nextLoadValue = machPtr->loadValue;
		} else {
		    nextLoadValue = machPtr->regs[wordPtr->rt];
		}
		switch (tmp & 0x3) {
		    case 0:
			nextLoadValue = value;
			break;
		    case 1:
			nextLoadValue = (nextLoadValue & 0xff)
				| (value << 8);
			break;
		    case 2:
			nextLoadValue = (nextLoadValue & 0xffff)
				| (value << 16);
			break;
		    case 3:
			nextLoadValue = (nextLoadValue & 0xffffff)
				| (value << 24);
			break;
		}
		nextLoadReg = wordPtr->rt;
		break;
	    }

	    case OP_LWR: {
		int value;
		tmp = machPtr->regs[wordPtr->rs] + wordPtr->extra;
		if (ReadMem(machPtr, tmp, &value) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		if (machPtr->loadReg == wordPtr->rt) {
		    nextLoadValue = machPtr->loadValue;
		} else {
		    nextLoadValue = machPtr->regs[wordPtr->rt];
		}
		switch (tmp & 0x3) {
		    case 0:
			nextLoadValue = (nextLoadValue & 0xffffff00)
				| ((value >> 24) & 0xff);
			break;
		    case 1:
			nextLoadValue = (nextLoadValue & 0xffff0000)
				| ((value >> 16) & 0xffff);
			break;
		    case 2:
			nextLoadValue = (nextLoadValue & 0xff000000)
				| ((value >> 8) & 0xffffff);
			break;
		    case 3:
			nextLoadValue = value;
			break;
		}
		nextLoadReg = wordPtr->rt;
		break;
	    }

	    case OP_MFC0:
		machPtr->regs[wordPtr->rt] = Cop0_ReadReg(machPtr,
			wordPtr->rd);
		break;

	    case OP_MFHI:
		machPtr->regs[wordPtr->rd] = machPtr->regs[HI_REG];
		break;

	    case OP_MFLO:
		machPtr->regs[wordPtr->rd] = machPtr->regs[LO_REG];
		break;

	    case OP_MTC0:
		Cop0_WriteReg(machPtr, wordPtr->rd,
			machPtr->regs[wordPtr->rt]);
		break;

	    case OP_MTHI:
		machPtr->regs[HI_REG] = machPtr->regs[wordPtr->rs];
		break;

	    case OP_MTLO:
		machPtr->regs[LO_REG] = machPtr->regs[wordPtr->rs];
		break;

	    case OP_MULT:
		Mult(machPtr->regs[wordPtr->rs], machPtr->regs[wordPtr->rt],
			1, &machPtr->regs[HI_REG], &machPtr->regs[LO_REG]);
		break;

	    case OP_MULTU:
		Mult(machPtr->regs[wordPtr->rs], machPtr->regs[wordPtr->rt],
			0, &machPtr->regs[HI_REG], &machPtr->regs[LO_REG]);
		break;

	    case OP_NOR:
		machPtr->regs[wordPtr->rd] = ~(machPtr->regs[wordPtr->rs]
			| machPtr->regs[wordPtr->rt]);
		break;

	    case OP_OR:
		machPtr->regs[wordPtr->rd] = machPtr->regs[wordPtr->rs]
			| machPtr->regs[wordPtr->rt];
		break;

	    case OP_ORI:
		machPtr->regs[wordPtr->rt] = machPtr->regs[wordPtr->rs]
			| (wordPtr->extra & 0xffff);
		break;

	    case OP_RFE:
		Cop0_Rfe(machPtr);
		break;

	    case OP_SB:
		if (WriteMem(machPtr, (unsigned) (machPtr->regs[wordPtr->rs]
				+ wordPtr->extra),
			1, machPtr->regs[wordPtr->rt]) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		break;

	    case OP_SH:
		if (WriteMem(machPtr, (unsigned) (machPtr->regs[wordPtr->rs]
				+ wordPtr->extra),
			2, machPtr->regs[wordPtr->rt]) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		break;

	    case OP_SLL:
		machPtr->regs[wordPtr->rd] = machPtr->regs[wordPtr->rt]
			<< wordPtr->extra;
		break;

	    case OP_SLLV:
		machPtr->regs[wordPtr->rd] = machPtr->regs[wordPtr->rt]
			<< (machPtr->regs[wordPtr->rs] & 0x1f);
		break;

	    case OP_SLT:
		if (machPtr->regs[wordPtr->rs] < machPtr->regs[wordPtr->rt]) {
		    machPtr->regs[wordPtr->rd] = 1;
		} else {
		    machPtr->regs[wordPtr->rd] = 0;
		}
		break;

	    case OP_SLTI:
		if (machPtr->regs[wordPtr->rs] < wordPtr->extra) {
		    machPtr->regs[wordPtr->rt] = 1;
		} else {
		    machPtr->regs[wordPtr->rt] = 0;
		}
		break;

	    case OP_SLTIU: {
		unsigned int rs, imm;

		rs = machPtr->regs[wordPtr->rs];
		imm = wordPtr->extra;
		if (rs < imm) {
		    machPtr->regs[wordPtr->rt] = 1;
		} else {
		    machPtr->regs[wordPtr->rt] = 0;
		}
		break;
	    }

	    case OP_SLTU: {
		unsigned int rs, rt;

		rs = machPtr->regs[wordPtr->rs];
		rt = machPtr->regs[wordPtr->rt];
		if (rs < rt) {
		    machPtr->regs[wordPtr->rd] = 1;
		} else {
		    machPtr->regs[wordPtr->rd] = 0;
		}
		break;
	    }

	    case OP_SRA:
		machPtr->regs[wordPtr->rd] = machPtr->regs[wordPtr->rt]
			>> wordPtr->extra;
		break;

	    case OP_SRAV:
		machPtr->regs[wordPtr->rd] = machPtr->regs[wordPtr->rt]
			>> (machPtr->regs[wordPtr->rs] & 0x1f);
		break;

	    case OP_SRL:
		tmp = machPtr->regs[wordPtr->rt];
		tmp >>= wordPtr->extra;
		machPtr->regs[wordPtr->rd] = tmp;
		break;

	    case OP_SRLV:
		tmp = machPtr->regs[wordPtr->rt];
		tmp >>= (machPtr->regs[wordPtr->rs] & 0x1f);
		machPtr->regs[wordPtr->rd] = tmp;
		break;

	    case OP_SUB: {
		int diff;

		diff = machPtr->regs[wordPtr->rs] - machPtr->regs[wordPtr->rt];
		if (((machPtr->regs[wordPtr->rs] ^ machPtr->regs[wordPtr->rt])
			& SIGN_BIT) && ((machPtr->regs[wordPtr->rs]
			^ diff) & SIGN_BIT)) {
		    result = Overflow(machPtr);
		    if (result != TCL_OK) {
			goto stopSimulation;
		    } else {
			goto endOfIns;
		    }
		}
		machPtr->regs[wordPtr->rd] = diff;
		break;
	    }

	    case OP_SUBU:
		machPtr->regs[wordPtr->rd] = machPtr->regs[wordPtr->rs]
			- machPtr->regs[wordPtr->rt];
		break;

	    case OP_SW:
		if (WriteMem(machPtr, (unsigned) (machPtr->regs[wordPtr->rs]
				+ wordPtr->extra),
			4, machPtr->regs[wordPtr->rt]) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		break;

	    case OP_SWL: {
		int value;
		tmp = machPtr->regs[wordPtr->rs] + wordPtr->extra;
		if (ReadMem(machPtr, (tmp & ~0x3), &value) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		switch (tmp & 0x3) {
		    case 0:
			value = machPtr->regs[wordPtr->rt];
			break;
		    case 1:
			value = (value & 0xff000000)
				| ((machPtr->regs[wordPtr->rt] >> 8) & 0xffffff);
			break;
		    case 2:
			value = (value & 0xffff0000)
				| ((machPtr->regs[wordPtr->rt] >> 16) & 0xffff);
			break;
		    case 3:
			value = (value & 0xffffff00)
				| ((machPtr->regs[wordPtr->rt] >> 24) & 0xff);
			break;
		}
		if (WriteMem(machPtr, (tmp & ~0x3), 4, value) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		break;
	    }

	    case OP_SWR: {
		int value;
		tmp = machPtr->regs[wordPtr->rs] + wordPtr->extra;
		if (ReadMem(machPtr, (tmp & ~0x3), &value) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		switch (tmp & 0x3) {
		    case 0:
			value = (value & 0xffffff)
				| (machPtr->regs[wordPtr->rt] << 24);
			break;
		    case 1:
			value = (value & 0xffff)
				| (machPtr->regs[wordPtr->rt] << 16);
			break;
		    case 2:
			value = (value & 0xff)
				| (machPtr->regs[wordPtr->rt] << 8);
			break;
		    case 3:
			value = machPtr->regs[wordPtr->rt];
			break;
		}
		if (WriteMem(machPtr, (tmp & ~0x3), 4, value) == 0) {
		    result = TCL_ERROR;
		    goto stopSimulation;
		}
		break;
	    }

	    case OP_XOR:
		machPtr->regs[wordPtr->rd] = machPtr->regs[wordPtr->rs]
			^ machPtr->regs[wordPtr->rt];
		break;

	    case OP_XORI:
		machPtr->regs[wordPtr->rt] = machPtr->regs[wordPtr->rs]
			^ (wordPtr->extra & 0xffff);
		break;

	    case OP_NOT_COMPILED:
		Compile(wordPtr);
		goto execute;
		break;

	    case OP_RES:
		errMsg = "reserved operation";
		goto error;

	    case OP_UNIMP:
		errMsg = "instruction not implemented in simulator";
		goto error;

	    default:
		i = wordPtr->opCode;
		sprintf(interp->result,
			"internal error in Simulate():  bad opCode %d, pc = %.100s",
			i, Sym_GetString(machPtr, Sim_GetPC(machPtr)));
		result = TCL_ERROR;
		goto stopSimulation;
	}

	/*
	 * Simulate effects of delayed load:
	 */
    
	machPtr->regs[machPtr->loadReg] = machPtr->loadValue;
	machPtr->loadReg = nextLoadReg;
	machPtr->loadValue = nextLoadValue;
    
	/*
	 * Make sure R0 stays zero.
	 */
    
	machPtr->regs[0] = 0;
    
	/*
	 * Advance program counters.
	 */
    
	machPtr->regs[PC_REG] = machPtr->regs[NEXT_PC_REG];
	machPtr->regs[NEXT_PC_REG] = pc;

	/*
	 * Check flags for special actions to perform after the instruction.
	 */

	if ((machPtr->insCount += 1) >= checkNum) {
	    while (machPtr->callBackList != NULL) {
		register CallBack *cbPtr;

		cbPtr = machPtr->callBackList;
		if (machPtr->insCount < cbPtr->serialNum) {
		    break;
		}
		machPtr->callBackList = cbPtr->nextPtr;
		(*cbPtr->proc)(cbPtr->clientData, machPtr);
		free((char *) cbPtr);
	    }
	    if ((machPtr->badPC != 0)
		    && (machPtr->insCount == machPtr->addrErrNum)) {
		result = AddressError(machPtr, machPtr->badPC, 1);
		if (result != TCL_OK) {
		    goto stopSimulation;
		}
	    }
	    if (singleStep) {
		tmp = Sim_GetPC(machPtr);
		Tcl_AppendResult(interp, "stopped after single step, pc = ",
			Sym_GetString(machPtr, tmp), (char *) NULL);
		Tcl_AppendResult(interp, ": ",  Asm_Disassemble(machPtr,
			machPtr->memPtr[machPtr->regs[PC_REG]].value,
			tmp & ~0x3), (char *) NULL);
		result = TCL_OK;
		goto stopSimulation;
	    }
	    if (machPtr->flags & STOP_REQUESTED) {
		errMsg = "execution stopped";
		goto error;
	    }
	    goto setCheckNum;
	}

	endOfIns:
	continue;
    }

    error:
    tmp = Sim_GetPC(machPtr);
    Tcl_AppendResult(interp, errMsg, ", pc = ", Sym_GetString(machPtr, tmp),
	    (char *) NULL);
    Tcl_AppendResult(interp, ": ",  Asm_Disassemble(machPtr,
	    machPtr->memPtr[machPtr->regs[PC_REG]].value,
	    tmp & ~0x3), (char *) NULL);
    result = TCL_ERROR;

    /*
     * Before returning, store the current instruction serial number
     * in a Tcl variable.
     */

    stopSimulation:
    Io_EndSim(machPtr);
    sprintf(msg, "%d", machPtr->insCount);
    Tcl_SetVar(machPtr->interp, "insCount", msg, 1);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Compile --
 *
 *	Given a memory word, decode it into a set of fields that
 *	permit faster interpretation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of *wordPtr are modified.
 *
 *----------------------------------------------------------------------
 */

static void
Compile(wordPtr)
    register MemWord *wordPtr;		/* Memory location to be compiled. */
{
    register OpInfo *opPtr;

    wordPtr->rs = (wordPtr->value >> 21) & 0x1f;
    wordPtr->rt = (wordPtr->value >> 16) & 0x1f;
    wordPtr->rd = (wordPtr->value >> 11) & 0x1f;
    opPtr = &opTable[(wordPtr->value >> 26) & 0x3f];
    wordPtr->opCode = opPtr->opCode;
    if (opPtr->format == IFMT) {
	wordPtr->extra = wordPtr->value & 0xffff;
	if (wordPtr->extra & 0x8000) {
	    wordPtr->extra |= 0xffff0000;
	}
    } else if (opPtr->format == RFMT) {
	wordPtr->extra = (wordPtr->value >> 6) & 0x1f;
    } else {
	wordPtr->extra = wordPtr->value & 0x3ffffff;
    }
    if (wordPtr->opCode == SPECIAL) {
	wordPtr->opCode = specialTable[wordPtr->value & 0x3f];
    } else if (wordPtr->opCode == BCOND) {
	int i;
	i = wordPtr->value & 0x1f0000;
	if (i == 0) {
	    wordPtr->opCode = OP_BLTZ;
	} else if (i == 0x10000) {
	    wordPtr->opCode = OP_BGEZ;
	} else if (i == 0x100000) {
	    wordPtr->opCode = OP_BLTZAL;
	} else if (i == 0x110000) {
	    wordPtr->opCode = OP_BGEZAL;
	} else {
	    wordPtr->opCode = OP_UNIMP;
	}
    } else if (wordPtr->opCode == OP_MTC0) {
	if (wordPtr->value == 0x42000010) {
	    wordPtr->opCode = OP_RFE;
	} else if ((wordPtr->value & 0x3e00000) == 0) {
	    wordPtr->opCode = OP_MFC0;
	} else if ((wordPtr->value & 0x3e00000) == 0x800000) {
	    wordPtr->opCode = OP_MTC0;
	} else {
	    wordPtr->opCode = OP_UNIMP;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * BusError --
 *
 *	Handle bus error execptions.
 *
 * Results:
 *	A standard Tcl return value.  If TCL_OK, then there is no return
 *	string and it's safe to keep on simulating.
 *
 * Side effects:
 *	May simulate a trap for the machine.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
static int
BusError(machPtr, address, iFetch)
    R2000 *machPtr;		/* Machine description. */
    unsigned int address;	/* Location that was referenced but doesn't
				 * exist. */
    int iFetch;			/* 1 means error occurred during instruction
				 * fetch.  0 means during a load or store. */
{
    unsigned int pcAddr;
    char badAddr[20];

    pcAddr = Sim_GetPC(machPtr);
    if (iFetch) {
	sprintf(machPtr->interp->result,
		"bus error: tried to fetch instruction at 0x%x",
		address);
    } else {
	sprintf(badAddr, "0x%x", address);
	Tcl_AppendResult(machPtr->interp, "error: referenced ", badAddr,
		", pc = ", Sym_GetString(machPtr, pcAddr), (char *) NULL);
	Tcl_AppendResult(machPtr->interp, ": ",  Asm_Disassemble(machPtr,
		machPtr->memPtr[machPtr->regs[PC_REG]].value,
		pcAddr & ~0x3), (char *) NULL);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * AddressError --
 *
 *	Handle address error execptions.
 *
 * Results:
 *	A standard Tcl return value.  If TCL_OK, then there is no return
 *	string and it's safe to keep on simulating.
 *
 * Side effects:
 *	May simulate a trap for the machine.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
static int
AddressError(machPtr, address, load)
    R2000 *machPtr;		/* Machine description. */
    unsigned int address;	/* Location that was referenced but doesn't
				 * exist. */
    int load;			/* 1 means error occurred during instruction
				 * fetch or load, 0 means during a store. */
{
    sprintf(machPtr->interp->result,
	    "address error:  referenced 0x%x, pc = %.100s", address,
	    Sym_GetString(machPtr, Sim_GetPC(machPtr)));
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Overflow --
 *
 *	Handle arithmetic overflow execptions.
 *
 * Results:
 *	A standard Tcl return value.  If TCL_OK, then there is no return
 *	string and it's safe to keep on simulating.
 *
 * Side effects:
 *	May simulate a trap for the machine.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
static int
Overflow(machPtr)
    R2000 *machPtr;		/* Machine description. */
{
    unsigned int pcAddr;

    pcAddr = Sim_GetPC(machPtr);
    Tcl_AppendResult(machPtr->interp, "arithmetic overflow, pc = ",
	    Sym_GetString(machPtr, pcAddr), (char *) NULL);
    Tcl_AppendResult(machPtr->interp, ": ",  Asm_Disassemble(machPtr,
	    machPtr->memPtr[machPtr->regs[PC_REG]].value,
	    pcAddr & ~0x3), (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Mult --
 *
 *	Simulate R2000 multiplication.
 *
 * Results:
 *	The words at *hiPtr and *loPtr are overwritten with the
 *	double-length result of the multiplication.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
Mult(a, b, signedArith, hiPtr, loPtr)
    int a, b;			/* Two operands to multiply. */
    int signedArith;		/* Zero means perform unsigned arithmetic,
				 * one means perform signed arithmetic. */
    int *hiPtr, *loPtr;		/* Place the result in the two words pointed
				 * to by these arguments. */
{
    unsigned int hi, lo, bLo, bHi;
    int i;
    int negative;

    if ((a == 0) || (b == 0)) {
	*hiPtr = *loPtr = 0;
	return;
    }

    /*
     * Compute the sign of the result, then make everything positive
     * so unsigned computation can be done in the main loop.
     */

    negative = 0;
    if (signedArith) {
	if (a < 0) {
	    negative = !negative;
	    a = -a;
	}
	if (b < 0) {
	    negative = !negative;
	    b = -b;
	}
    }

    /*
     * Compute the result in unsigned arithmetic (check a's bits one at
     * a time, and add in a shifted value of b).
     */

    bLo = (unsigned int) b;
    bHi = 0;
    lo = hi = 0;
    for (i = 0; i < 32; i++) {
	if (a & 1) {
	    lo += bLo;
	    if (lo < bLo) {		/* Carry out of the low bits? */
		hi += 1;
	    }
	    hi += bHi;
	    if ((a & 0xfffffffe) == 0) {
		break;
	    }
	}
	bHi <<= 1;
	if (bLo & 0x80000000) {
	    bHi |= 1;
	}
	bLo <<= 1;
	a >>= 1;
    }

    /*
     * If the result is supposed to be negative, compute the two's
     * complement of the double-word result.
     */

    if (negative) {
	hi = ~hi;
	lo = ~lo;
	lo += 1;
	if (lo == 0) {
	    hi += 1;
	}
    }

    *hiPtr = (int) hi;
    *loPtr = (int) lo;
}
