/* 
 * getput.c --
 *
 *	This file implements commands to query or change the state
 *	of the R2000 machine being simulated.
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
static char rcsid[] = "$Header: /user1/ouster/mipsim/RCS/getput.c,v 1.15 91/02/03 13:26:07 ouster Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include "asm.h"
#include "gp.h"
#include "mips.h"
#include "sym.h"

/*
 * Forward declarations for procedures declared later in this file:
 */

static int	GetAddress();
static char *	GetString();

/*
 *----------------------------------------------------------------------
 *
 * Gp_GetCmd --
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
Gp_GetCmd(machPtr, interp, argc, argv)
    R2000 *machPtr;			/* Machine description. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    unsigned int address;
    int isReg, result, totalSpace, spaceNeeded;
    enum {BINARY, HEX, DECIMAL, CHAR, STRING, INS} format = HEX;
    int length = 4;			/* # bytes in each value to print. */
    int count = 1;			/* # of values to print. */
    int addrIsValue = 0;		/* Print address instead of value. */
    char *p;				/* Points to NULL char. at end of
					 * current result. */
    char *locString, *valueString;
    char locBuf[100], valueBuf[100];

    if ((argc != 2) && (argc != 3)) {
	sprintf(interp->result,
		"wrong # args:  should be \"%.50s location [flags]\"",
		argv[0]);
	return TCL_ERROR;
    }
    result = GetAddress(machPtr, argv[1], &address, &isReg);
    if (result != TCL_OK) {
	return result;
    }

    /*
     * Parse flags.
     */

    if (argc == 3) {
	for (p = argv[2]; *p != 0; p++) {
	    if (*p == 'b') {
		length = 1;
	    } else if (*p == 'h') {
		length = 2;
	    } else if (*p == 'w') {
		length = 4;
	    } else if (*p == 'x') {
		format = HEX;
	    } else if (*p == 'd') {
		format = DECIMAL;
	    } else if (*p == 'B') {
		format = BINARY;
	    } else if (*p == 'c') {
		format = CHAR;
		length = 1;
	    } else if (*p == 's') {
		if (isReg) {
		    sprintf(interp->result, "can't get \"%.50s\" as string",
			    argv[1]);
		    return TCL_ERROR;
		}
		format = STRING;
		length = 1;
	    } else if (*p == 'i') {
		format = INS;
	    } else if (isdigit(*p)) {
		char *end;

		count = strtoul(p, &end, 10);
		p = end-1;
	    } else if (*p == 'v') {
		addrIsValue = 1;
	    } else {
		sprintf(interp->result, "bad flag \"%c\" in \"%.50s\" command",
			*p, argv[0]);
		return TCL_ERROR;
	    }
	}
    }

    /*
     * Enter a loop to format <location value> pairs.  Round the address
     * down to an even boundary appropriate for the output type.
     */

    if (!isReg && !addrIsValue) {
	address &= ~(length-1);
    }
    p = interp->result;
    totalSpace = TCL_RESULT_SIZE;
    while (1) {
	int value;

	/*
	 * Get the value to print.
	 */

	if (addrIsValue) {
	    value = address;
	} else if (isReg) {
	    if (address == PC_REG) {
		value = Sim_GetPC(machPtr);
	    } else if (address == NEXT_PC_REG) {
		value = INDEX_TO_ADDR(value);
	    } else if (address == STATUS_REG) {
		value = Cop0_ReadReg(machPtr, COP0_STATUS_REGNUM);
	    } else if (address == CAUSE_REG) {
		value = Cop0_ReadReg(machPtr, COP0_CAUSE_REGNUM);
	    } else if (address == EPC_REG) {
		value = Cop0_ReadReg(machPtr, COP0_EPC_REGNUM);
	    } else {
		value = machPtr->regs[address];
	    }
	} else {
	    if (ADDR_TO_INDEX(address) < machPtr->numWords) {
		value = machPtr->memPtr[ADDR_TO_INDEX(address)].value;
	    } else if (Io_Read(machPtr, (address & ~0x3), &value) == 0) {
		sprintf(interp->result,
			"location \"%.50s\" doesn't exist in memory",
			argv[1]);
		return TCL_ERROR;
	    }
	    if (length == 1) {
		switch (address & 0x3) {
		    case 0:
			value >>= 24;
			break;
		    case 1:
			value >>= 16;
			break;
		    case 2:
			value >>= 8;
			break;
		}
	    } else if ((length == 2) && !(address & 0x2)) {
		value >>= 16;
	    }
	}
	if (length == 1) {
	    value &= 0xff;
	    if ((format == DECIMAL) && (value & 0x80)) {
		value |= 0xffffff00;
	    }
	}
	if (length == 2) {
	    value &= 0xffff;
	    if ((format == DECIMAL) && (value & 0x8000)) {
		value |= 0xffff0000;
	    }
	}

	/*
	 * Format the value's address, leaving a pointer to it
	 * in locString.
	 */

	locString = locBuf;
	if (addrIsValue) {
	    locString = "";
	} else if (isReg) {
	    (void) sprintf(locBuf, "%s:\t", Asm_RegNames[address]);
	} else {
	    (void) sprintf(locBuf, "%s:\t", Sym_GetString(machPtr, address));
	}

	/*
	 * Format the value itself, leaving a pointer in valueString.
	 */

	valueString = valueBuf;
	switch (format) {
	    case HEX:
		(void) sprintf(valueBuf, "0x%08x", value);
		break;
	    case DECIMAL:
		(void) sprintf(valueBuf, "%d", value);
		break;
	    case BINARY: {
		int i;
		for (i = 31; i >= 0; i--) {
		    if (value & (1<<i)) {
			*valueString = '1';
			valueString++;
		    } else if (valueString != valueBuf) {
			*valueString = '0';
			valueString++;
		    }
		}
		if (valueString == valueBuf) {
		    *valueString = '0';
		    valueString++;
		}
		*valueString = 0;
		valueString = valueBuf;
		break;
	    }
	    case CHAR:
		if (isprint(value)) {
		    (void) sprintf(valueBuf, "%c (0x%x)", (char) value,
			    value);
		} else {
		    (void) sprintf(valueBuf, "0x%x", value);
		}
		break;
	    case STRING:
		valueString = GetString(machPtr, address, &length);
		break;
	    case INS:
		valueString = Asm_Disassemble(machPtr, value, address);
		break;
	}

	/*
	 * Make sure there's enough space for this value in the
	 * result buffer (grow the output buffer if necessary),
	 * then put the value into the buffer.
	 */

	spaceNeeded = (p - interp->result) + strlen(locString)
		+ strlen(valueString);
	while (totalSpace <= spaceNeeded) {
	    char *newSpace;
	    int spaceUsed;

	    totalSpace *= 2;
	    newSpace = malloc((unsigned) totalSpace);
	    spaceUsed = p-interp->result;
	    bcopy(interp->result, newSpace, spaceUsed);
	    if (interp->dynamic) {
		free(interp->result);
	    }
	    interp->dynamic = 1;
	    interp->result = newSpace;
	    p = newSpace + spaceUsed;
	}
	strcpy(p, locString);
	strcat(p, valueString);
	p += strlen(p);

	/*
	 * Go on to the next value, if there are any more.
	 */

	count -= 1;
	if (count == 0) {
	    break;
	}
	if (isReg) {
	    address += 1;
	    if (address >= TOTAL_REGS) {
		break;
	    }
	} else {
	    address += length;
	    if (ADDR_TO_INDEX(address) >= machPtr->numWords) {
		break;
	    }
	}
	*p = '\n';
	p++;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Gp_PutCmd --
 *
 *	This procedure is invoked to process the "put" Tcl command.
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
Gp_PutCmd(machPtr, interp, argc, argv)
    R2000 *machPtr;			/* Machine description. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    unsigned int address;
    int isReg, value, result;
    char *end;

    if (argc != 3) {
	sprintf(interp->result,
		"wrong # args:  should be \"%.50s location value\"",
		argv[0]);
	return TCL_ERROR;
    }
    result = GetAddress(machPtr, argv[1], &address, &isReg);
    if (result != TCL_OK) {
	return result;
    }

    result = Sym_EvalExpr(machPtr, (char *) NULL, argv[2], 0,
	    &value, &end);
    if (result != TCL_OK) {
	return result;
    }
    if (*end != 0) {
	sprintf(interp->result,
		"bad value \"%.50s\" in \"%.50s\" command",
		argv[2], argv[0]);
	return TCL_ERROR;
    }
    if (isReg) {
	if ((address == 0) ||
		((address >= STATUS_REG) && (address <= EPC_REG))){
	    sprintf(interp->result, "can't modify %s", Asm_RegNames[address]);
	    return TCL_ERROR;
	}
	if ((address == PC_REG) || (address == NEXT_PC_REG)) {
	    if (value & 0x3) {
		sprintf(interp->result,
			"address 0x%x not properly aligned for pc or npc",
			value);
		return TCL_ERROR;
	    }
	    value = ADDR_TO_INDEX(value);
	}
	machPtr->regs[address] = value;
    } else {
	if (ADDR_TO_INDEX(address) < machPtr->numWords) {
	    MemWord *wordPtr;

	    wordPtr = &machPtr->memPtr[ADDR_TO_INDEX(address)];
	    wordPtr->value = value;
	    wordPtr->opCode = OP_NOT_COMPILED;
	} else if (Io_Write(machPtr, (address & ~0x3), value, 4) == 0) {
	    sprintf(interp->result,
		    "location \"%.50s\" doesn't exist in memory",
		    argv[1]);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Gp_PutstringCmd --
 *
 *	This procedure is invoked to process the "putstring" Tcl command.
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
Gp_PutstringCmd(machPtr, interp, argc, argv)
    R2000 *machPtr;			/* Machine description. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    unsigned int address;
    int isReg, result, byteNum;
    char *p;
    MemWord *wordPtr;
    static int mask[] = {0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff};
    static int shift[] = {24, 16, 8, 0};

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		argv[0], " location string\"", (char *) NULL);
	return TCL_ERROR;
    }
    result = GetAddress(machPtr, argv[1], &address, &isReg);
    if (result != TCL_OK) {
	return result;
    }
    if (isReg) {
	sprintf(interp->result, "can't put string in a register");
	return TCL_ERROR;
    }

    for (p = argv[2]; ; p++, address++) {
	if (ADDR_TO_INDEX(address) >= machPtr->numWords) {
	    char string[10];

	    sprintf(string, "0x%x", address);
	    Tcl_AppendResult(interp, "location ", string,
		    " doesn't exist in memory", (char *) NULL);
	    return TCL_ERROR;
	}
	byteNum = address & 0x3;
	wordPtr = &machPtr->memPtr[ADDR_TO_INDEX(address)];
	wordPtr->value = (wordPtr->value & ~mask[byteNum])
		| ((*p << shift[byteNum]) & mask[byteNum]);
	wordPtr->opCode = OP_NOT_COMPILED;
	if (*p == '\0') {
	    break;
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetAddress --
 *
 *	Convert a string to information about an address, suitable for
 *	reading or writing the location.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 * Side effects:
 *	Generates an error in interp if string can't be parsed into
 *	an address.
 *
 *----------------------------------------------------------------------
 */

static int
GetAddress(machPtr, string, addressPtr, regPtr)
    R2000 *machPtr;			/* Machine address will be used with.
					 * Also used for error reporting. */
    char *string;			/* Specification of address. */
    unsigned int *addressPtr;		/* Store address of value here. */
    int *regPtr;			/* Store 1 here if address refers to
					 * a register, 0 for memory location. */
{
    unsigned int result;
    char *end;

    result = Sym_GetSym(machPtr, (char *) NULL, string, SYM_PSEUDO_OK,
	    addressPtr);
    if (result == SYM_REGISTER) {
	*regPtr = 1;
	return TCL_OK;
    }
    if (result == SYM_FOUND) {
	*regPtr = 0;
	return TCL_OK;
    }

    result = Sym_EvalExpr(machPtr, (char *) NULL, string, 0,
	    (int *) addressPtr, &end);
    if (result != TCL_OK) {
	return result;
    }
    if (*end != 0) {
	sprintf(machPtr->interp->result,
		"mistyped expression \"%.50s\"", string);
	return TCL_ERROR;
    }
    *regPtr = 0;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetString --
 *
 *	Given an address, return the ASCII string at that address.
 *
 * Results:
 *	The return value is a pointer to ASCII string that's at
 *	"address" in machPtr's memory.  The integer at *countPtr
 *	is overwritten with the number of bytes in the string,
 *	including the terminating NULL character.  If the string
 *	is very long, the return value may be truncated to hold only
 *	the first few characters of the string (*countPtr will also
 *	be truncated).  The return value is stored in a static buffer
 *	that will be overwritten on the next call to this procedure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
GetString(machPtr, address, countPtr)
    register R2000 *machPtr;	/* Machine whose memory is to be examined. */
    unsigned int address;	/* Address of string. */
    int *countPtr;		/* Fill in string length (including NULL
				 * character, if any) here. */
{
#define MAX_LENGTH 200
    static char buffer[(4*MAX_LENGTH)+20];
    char *p;
    unsigned int index;
    int value, count;

    buffer[0] = '"';
    p = buffer+1;
    for (count = 0; ; count++, address++) {
	index = ADDR_TO_INDEX(address);
	if (index >= machPtr->numWords) {
	    *countPtr = count;
	    break;
	}
	value = machPtr->memPtr[index].value;
	switch (address & 0x3) {
	    case 0:
		value >>= 24;
		break;
	    case 1:
		value >>= 16;
		break;
	    case 2:
		value >>= 8;
		break;
	}
	value &= 0xff;
	if (value == 0) {
	    *countPtr = count+1;
	    break;
	}
	if (count == MAX_LENGTH) {
	    *countPtr = MAX_LENGTH;
	    strcpy(p, "...");
	    p += 3;
	    break;
	}
	if (value == '\\') {
	    strcpy(p, "\\\\");
	    p += 2;
	} else if (value == '"') {
	    strcpy(p, "\\\"");
	    p += 2;
	} else if (isascii(value) && isprint(value)) {
	    *p = value;
	    p++;
	} else if (value == '\n') {
	    strcpy(p, "\\n");
	    p += 2;
	} else if (value == '\t') {
	    strcpy(p, "\\t");
	    p += 2;
	} else if (value == '\b') {
	    strcpy(p, "\\b");
	    p += 2;
	} else if (value == '\r') {
	    strcpy(p, "\\r");
	    p += 2;
	} else {
	    sprintf(p, "\\x%02x", value);
	    p += 4;
	}
    }
    *p = '"';
    p[1] = 0;
    return buffer;
}

/*
 *----------------------------------------------------------------------
 *
 * Gp_PutString --
 *
 *	Given an ASCII string, store it in the memory of a machine.
 *
 * Results:
 *	The return value is the number of bytes actually stored
 *	in machPtr's memory.  If endPtr isn't NULL, *endPtr is
 *	filled in with the address of the character that terminated
 *	the string (either term or 0).
 *
 * Side effects:
 *	The bytes of "string" are stored in machPtr's memory starting
 *	at "address".  Standard Tcl backslash sequences are interpreted.
 *
 *----------------------------------------------------------------------
 */

int
Gp_PutString(machPtr, string, term, address, addNull, endPtr)
    register R2000 *machPtr;		/* Machine whose memory is to
					 * be modified. */
    register char *string;		/* String to store. */
    char term;				/* Character that terminates string. */
    unsigned int address;		/* Where in machPtr's memory to store.*/
    int addNull;			/* If non-zero, add a terminating
					 * NULL character to memory after
					 * the string. */
    char **endPtr;			/* If non-NULL, fill in with address
					 * of terminating character. */
{
    int backslashCount;
    int size;

    size = 0;
    while (1) {
	if (*string == '\\') {
	    Gp_PutByte(machPtr, address,
		    Tcl_Backslash(string, &backslashCount));
	    string += backslashCount;
	} else if ((*string == 0) || (*string == term)) {
	    break;
	} else {
	    Gp_PutByte(machPtr, address, *string);
	    string++;
	}
	address += 1;
	size += 1;
    }
    if (addNull) {
	Gp_PutByte(machPtr, address, 0);
	size += 1;
    }
    if (endPtr != 0) {
	*endPtr = string;
    }
    return size;
}

/*
 *----------------------------------------------------------------------
 *
 * Gp_PutByte --
 *
 *	Store a particular byte at a particular address in memory.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	MachPtr's memory gets modified.
 *
 *----------------------------------------------------------------------
 */

void
Gp_PutByte(machPtr, address, value)
    register R2000 *machPtr;		/* Machine whose memory to modify. */
    unsigned int address;		/* Where to store value. */
    int value;				/* Value to store as byte at address. */
{
    MemWord *wordPtr;
    int index;

    index = ADDR_TO_INDEX(address);
    if (index < machPtr->numWords) {
	wordPtr = &machPtr->memPtr[index];
	switch (address & 0x3) {
	    case 0:
		wordPtr->value = (wordPtr->value & 0xffffff)
			| (value << 24);
		break;
	    case 1:
		wordPtr->value = (wordPtr->value & 0xff00ffff)
			| ((value << 16) & 0xff0000);
		break;
	    case 2:
		wordPtr->value = (wordPtr->value & 0xffff00ff)
			| ((value << 8) & 0xff00);
		break;
	    case 3:
		wordPtr->value = (wordPtr->value & 0xffffff00)
			| (value & 0xff);
		break;
	}
	wordPtr->opCode = OP_NOT_COMPILED;
    }
}
