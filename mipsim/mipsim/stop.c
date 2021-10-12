/* 
 * stop.c --
 *
 *	This procedure provides code that manipulates spies and stops
 *	for the simulator.
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
static char rcsid[] = "$Header: /user1/ouster/mipsim/RCS/stop.c,v 1.10 91/02/06 15:52:57 ouster Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include "asm.h"
#include "mips.h"
#include "sym.h"


/*
 *----------------------------------------------------------------------
 *
 * Stop_StopCmd --
 *
 *	This procedure is invoked to process the "stop" Tcl command.
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
Stop_StopCmd(machPtr, interp, argc, argv)
    R2000 *machPtr;			/* Machine description. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int length;
    register Stop *stopPtr;
    Stop *prevPtr;
    MemWord *wordPtr;

    if (argc == 1) {
	Sim_Stop(machPtr);
	return TCL_OK;
    }

    length = strlen(argv[1]);

    /*
     * "At" option:  create a new stop.
     */

    if (strncmp(argv[1], "at", length) == 0) {
	unsigned int address;
	int result;
	char *end;

	if ((argc != 3) && (argc != 4)) {
	    sprintf(interp->result,
		    "wrong # args:  should be \"%.50s at location [command]\"",
		    argv[0]);
	    return TCL_ERROR;
	}
	result = Sym_EvalExpr(machPtr, (char *) NULL, argv[2], 0,
		(int *) &address, &end);
	if (result != TCL_OK) {
	    return result;
	}
	if (*end != 0) {
	    sprintf(interp->result,
		    "bad address \"%.50s\" in \"%.50s\" command",
		    argv[2], argv[0]);
	    return TCL_ERROR;
	}
	address = ADDR_TO_INDEX(address);
	if (address >= machPtr->numWords) {
	    sprintf(interp->result,
		    "location %.50s doesn't exist in the machine's memory",
		    argv[2]);
	    return TCL_ERROR;
	}
	wordPtr = &machPtr->memPtr[address];
	for (stopPtr = wordPtr->stopList; stopPtr != NULL;
		stopPtr = stopPtr->nextPtr) {
	    if (stopPtr->command == NULL) {
		sprintf(interp->result, "stop already set at %.50s",
			argv[2]);
		return TCL_OK;
	    }
	    prevPtr = stopPtr;
	}
	stopPtr = (Stop *) malloc(sizeof(Stop));
	if (argc == 3) {
	    stopPtr->command = (char *) NULL;
	} else {
	    stopPtr->command = malloc((unsigned) (strlen(argv[3]) + 1));
	    strcpy(stopPtr->command, argv[3]);
	}
	stopPtr->number = machPtr->stopNum;
	machPtr->stopNum += 1;
	stopPtr->address = INDEX_TO_ADDR(address);
	stopPtr->nextPtr = NULL;
	stopPtr->overallPtr = NULL;
	if (wordPtr->stopList == NULL) {
	    wordPtr->stopList = stopPtr;
	} else {
	    prevPtr->nextPtr = stopPtr;
	}
	if (machPtr->stopList == NULL) {
	    machPtr->stopList = stopPtr;
	} else {
	    for (prevPtr = machPtr->stopList; prevPtr->overallPtr != NULL;
		    prevPtr = prevPtr->overallPtr) {
		/* Empty loop body. */
	    }
	    prevPtr->overallPtr = stopPtr;
	}
	return TCL_OK;
    }

    /*
     * "Delete" option:  eliminate a stop.
     */

    if (strncmp(argv[1], "delete", length) == 0) {
	int number, i;
	char *end;

	if (argc < 3) {
	    sprintf(interp->result,
		    "wrong # args:  should be \"%.50s delete num num ...\"",
		    argv[0]);
	    return TCL_ERROR;
	}
	for (i = 2; i < argc; i++) {
	    number = strtol(argv[i], &end, 0);
	    if (*end != 0) {
		sprintf(interp->result, "bad stop number \"%.50s\"", argv[i]);
		return TCL_ERROR;
	    }
	    for (stopPtr = machPtr->stopList; stopPtr != NULL;
		    stopPtr = stopPtr->overallPtr) {
		if (stopPtr->number == number) {
		    break;
		}
		prevPtr = stopPtr;
	    }
	    if (stopPtr == NULL) {
		sprintf(interp->result, "there is no stop \"%.50s\" defined",
			argv[i]);
		return TCL_ERROR;
	    }
	    if (machPtr->stopList == stopPtr) {
		machPtr->stopList = stopPtr->overallPtr;
	    } else {
		prevPtr->overallPtr = stopPtr->overallPtr;
	    }
	    wordPtr = &machPtr->memPtr[ADDR_TO_INDEX(stopPtr->address)];
	    if (wordPtr->stopList == stopPtr) {
		wordPtr->stopList = stopPtr->nextPtr;
	    } else {
		for (prevPtr = wordPtr->stopList; prevPtr != NULL;
			prevPtr = prevPtr->nextPtr) {
		    if (prevPtr->nextPtr == stopPtr) {
			prevPtr->nextPtr = stopPtr->nextPtr;
			break;
		    }
		}
	    }
	    if (stopPtr->command != NULL) {
		free(stopPtr->command);
	    }
	    free((char *) stopPtr);
	}
	return TCL_OK;
    }

    /*
     * "Info" option:  print info about all the stops that are currently
     * defined.
     */

    if (strncmp(argv[1], "info", length) == 0) {
	if (machPtr->stopList == NULL) {
	    printf("No stops are currently set.\n");
	    return TCL_OK;
	}
	printf("Num   Address               Action\n");
	for (stopPtr = machPtr->stopList; stopPtr != NULL;
		stopPtr = stopPtr->overallPtr) {
	    printf("#%-2d   %-20s", stopPtr->number,
		    Sym_GetString(machPtr, stopPtr->address));
	    if (stopPtr->command != NULL) {
		printf("  %s\n", stopPtr->command);
	    } else {
		printf("  stop\n");
	    }
	}
	return TCL_OK;
    }

    sprintf(interp->result,
	    "bad option \"%.50s\" for \"%.50s\":  %s",
	    argv[1], argv[0], "should be at, delete, or info");
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Stop_Execute --
 *
 *	Scan through a list of spies associated with a memory word,
 *	executing the Tcl commands associated with each one.  If any
 *	of the commands returns a result other than TCL_OK, then quit
 *	immediately.
 *
 * Results:
 *	TCL_OK, if the stop commands all terminated normally; otherwise
 *	the result of the first command that didn't terminate normally.
 *	MachPtr->interp gets a result stored in it too.
 *
 * Side effects:
 *	Whatever the commands do.
 *
 *----------------------------------------------------------------------
 */

int
Stop_Execute(machPtr, stopPtr)
    R2000 *machPtr;		/* Machine being executed. */
    register Stop *stopPtr;	/* First in list of stops. */
{
    int result = TCL_OK;
    int stopFlag = 0;
    int stopNum = -1;

    /*
     * The code below checks each individual stop operation to
     * see if it caused an actual stop (the first to request a
     * stop is reported as the triggered stop).
     */

    for ( ; stopPtr != NULL; stopPtr = stopPtr->nextPtr) {
	if (machPtr->flags & STOP_REQUESTED) {
	    stopFlag |= STOP_REQUESTED;
	    machPtr->flags &= ~STOP_REQUESTED;
	}
	if (stopPtr->command == NULL) {
	    Sim_Stop(machPtr);
	} else {
	    result = Tcl_Eval(machPtr->interp, stopPtr->command, 0,
		    (char **) NULL);
	    if (result != TCL_OK) {
		return result;
	    } else {
		if (*machPtr->interp->result != 0) {
		    printf("%s\n", machPtr->interp->result);
		}
		Tcl_Return(machPtr->interp, (char *) NULL, TCL_STATIC);
	    }
	}
	if ((machPtr->flags & STOP_REQUESTED) && (stopNum == -1)) {
	    stopNum = stopPtr->number;
	}
    }
    machPtr->flags |= stopFlag;
    if (stopNum != -1) {
	unsigned int pcAddr;
	char number[20];

	pcAddr = INDEX_TO_ADDR(machPtr->regs[PC_REG]);
	sprintf(number, "%d", stopNum);
	Tcl_AppendResult(machPtr->interp, "stop ", number, ", pc = ",
		Sym_GetString(machPtr, pcAddr), (char *) NULL);
	Tcl_AppendResult(machPtr->interp, ": ",  Asm_Disassemble(machPtr,
		machPtr->memPtr[machPtr->regs[PC_REG]].value,
		pcAddr), (char *) NULL);
    }
    return TCL_OK;
}
