/* 
 * sym.c --
 *
 *	This file contains procedures that manipulate symbol values
 *	and expresssions involving symbols.
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
static char rcsid[] = "$Header: /user1/ouster/mipsim/RCS/sym.c,v 1.5 89/11/20 10:57:14 ouster Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <ctype.h>
#include <hash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mips.h"
#include "sym.h"

/*
 * For each defined symbol there exists a structure of the following
 * form in the machine's symbol table.  There can be several symbols
 * with the same name (in different files), so there can be a chain
 * of these structures hanging off each entry in the hash table.
 */

typedef struct Sym {
    char *fileName;		/* Name of file in which symbol is defined. */
    unsigned int address;	/* Address of symbol, if defined. */
    int flags;			/* Various flag bits:  see below. */
    struct Sym *nextPtr;	/* Next symbol with same, or NULL for end
				 * of list. */
} Sym;

/*
 * Flag bits in Sym structures:
 *
 * S_GLOBAL -			1 means this symbol has global scope.
 * S_NO_ADDR -			1 means this symbol doesn't yet have an
 *				address defined for it (S_GLOBAL is
 *				always set in this case).
 * S_REG -			1 means this symbol is a register.
 */

#define S_GLOBAL	1
#define S_NO_ADDR	2
#define S_REG		4

/*
 * The data structure below describes the state of parsing an expression.
 * It's passed among the routines in this module.
 */

typedef struct {
    R2000 *machPtr;		/* Machine information for things like
				 * symbol table and error messages. */
    char *fileName;		/* Filename for symbol lookup;  see Sym_GetSym
				 * argument of same name for more info. */
    int ignoreUndef;		/* Non-zero means don't worry about
				 * undefined symbols. */
    char *expr;			/* Position of the next character to be
				 * scanned from the expression string. */
    int token;			/* Type of the last token to be parsed from
				 * expr.  See below for definitions.
				 * Corresponds to the characters just
				 * before expr. */
    char *tokenChars;		/* Poiner to first character of expression
				 * that mapped to token (i.e. the value of
				 * expr before the call to Lex that produced
				 * token). */
    int number;			/* If token is NUMBER, gives value of
				 * the number. */
} ExprInfo;

/*
 * The token types are defined below.  In addition, there is a table
 * associating a precedence with each operator.  The order of types
 * is important.  Consult the code before changing it.
 */

#define NUMBER		0
#define OPEN_PAREN	1
#define CLOSE_PAREN	2
#define END		3

/*
 * Binary operators:
 */

#define MULT		8
#define DIVIDE		9
#define MOD		10
#define PLUS		11
#define MINUS		12
#define LEFT_SHIFT	13
#define RIGHT_SHIFT	14
#define BIT_AND		15
#define BIT_XOR		16
#define BIT_OR		17

/*
 * Unary operators:
 */

#define	UNARY_MINUS	20
#define BIT_NOT		21

/*
 * Precedence table.  The values for non-operator token types are ignored.
 */

static int exprPrecTable[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    10, 10, 10,				/* MULT, DIVIDE, MOD */
    9, 9,				/* PLUS, MINUS */
    8, 8,				/* LEFT_SHIFT, RIGHT_SHIFT */
    5,					/* BIT_AND */
    4,					/* BIT_XOR */
    3,					/* BIT_OR */
    0, 0,
    11, 11				/* UNARY_MINUS, BIT_NOT */
};

/*
 *----------------------------------------------------------------------
 *
 * Sym_DeleteSymbols --
 *
 *	Remove all symbols associated with a particular file from a
 *	machine's symbol table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All symbols for "fileName" get removed from "machPtr"s symbol
 *	table.  This is typically done before (re-) loading a file into
 *	memory.
 *
 *----------------------------------------------------------------------
 */

void
Sym_DeleteSymbols(machPtr, fileName)
    R2000 *machPtr;		/* Machine to manipulate. */
    char *fileName;		/* Name of file whose symbols should be
				 * deleted. */
{
    Hash_Entry *hPtr;
    register Sym *symPtr;
    register Sym *prevPtr;
    Hash_Search search;

    for (hPtr = Hash_EnumFirst(&machPtr->symbols, &search);
	    hPtr != NULL; hPtr = Hash_EnumNext(&search)) {
	for (symPtr = (Sym *) Hash_GetValue(hPtr), prevPtr = NULL;
		symPtr != NULL; prevPtr = symPtr, symPtr = symPtr->nextPtr) {
	    if (strcmp(symPtr->fileName, fileName) != 0) {
		continue;
	    }
	    if (prevPtr == NULL) {
		Hash_SetValue(hPtr, symPtr->nextPtr);
	    } else {
		prevPtr->nextPtr = symPtr->nextPtr;
	    }
	    free(symPtr->fileName);
	    free((char *) symPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Sym_AddSymbol --
 *
 *	Add a new symbol to the symbol table for a machine.
 *
 * Results:
 *	A standard Tcl result is returned (normally TCL_OK), and an
 *	error string may be added to machPtr's interpreter.
 *
 * Side effects:
 *	Name is entered into the symbol table, if it isn't already
 *	there.  If address is -1 and global is 1, then the name is
 *	entered as a global symbol but no address is assigned (that
 *	will ostensibly come later).
 *
 *----------------------------------------------------------------------
 */

int
Sym_AddSymbol(machPtr, fileName, name, address, flags)
    R2000 *machPtr;		/* Machine to manipulate. */
    char *fileName;		/* Name of file in which symbol defined. */
    char *name;			/* Name of symbol. */
    unsigned int address;	/* Address corresponding to symbol, or
				 * SYM_NO_ADDR if symbol is being declared
				 * global before it has been defined. */
    int flags;			/* OR-ed combination of flag bits: any of
				 * SYM_REGISTER, SYM_GLOBAL, SYM_NO_ADDR. */
{
    register Sym *symPtr;
    Hash_Entry *hPtr;

    hPtr = Hash_CreateEntry(&machPtr->symbols, (Address) name,
	    (Boolean *) NULL);
    symPtr = (Sym *) Hash_GetValue(hPtr);
    if (symPtr != NULL) {
	Sym *globalPtr, *localPtr;

	/*
	 * See if there are other symbols that might conflict with this
	 * one.  If there's an error due to a conflict, it is handled
	 * farther down.
	 */

	globalPtr = localPtr = NULL;
	for ( ; symPtr != NULL; symPtr = symPtr->nextPtr) {
	    if (strcmp(symPtr->fileName, fileName) == 0) {
		localPtr = symPtr;
	    } else {
		if ((symPtr->flags & (S_GLOBAL|S_NO_ADDR)) == S_GLOBAL) {
		    globalPtr = symPtr;
		}
	    }
	}

	if (localPtr != NULL) {

	    /*
	     * Is this call just updating information in a previously-entered
	     * symbol?  If so, then do it.
	     */

	    if (flags & SYM_GLOBAL) {
		localPtr->flags |= S_GLOBAL;
	    }
	    if (!(flags & SYM_NO_ADDR)) {
		if (!(localPtr->flags & S_NO_ADDR)) {
		    sprintf(machPtr->interp->result,
			    "symbol \"%.50s\" already defined in %.50s",
			    name, fileName);
		    return TCL_ERROR;
		}
		localPtr->address = address;
		localPtr->flags &= ~S_NO_ADDR;
	    }

	    /*
	     * Does this new symbol result in a conflict between two
	     * globally-defined symbols?
	     */

	    if ((globalPtr != NULL)
		    && ((localPtr->flags & (S_GLOBAL|S_NO_ADDR)) == S_GLOBAL)) {
		globalConflict:
		sprintf(machPtr->interp->result,
			"symbol \"%.50s\" already defined in %.50s",
			name, globalPtr->fileName);
		return TCL_ERROR;
	    }
	    return TCL_OK;
	}

	if ((globalPtr != NULL)
		&& ((flags & (SYM_GLOBAL|SYM_NO_ADDR)) == S_GLOBAL)) {
	    goto globalConflict;
	}
    }

    /*
     * Create and initialize the new symbol.
     */

    symPtr = (Sym *) malloc(sizeof(Sym));
    symPtr->fileName = (char *) malloc((unsigned) (strlen(fileName) + 1));
    strcpy(symPtr->fileName, fileName);
    symPtr->address = address;
    if (flags & SYM_GLOBAL) {
	symPtr->flags |= S_GLOBAL;
    }
    if (flags & SYM_NO_ADDR) {
	symPtr->flags |= S_NO_ADDR;
    }
    if (flags & SYM_REGISTER) {
	symPtr->flags |= S_REG;
    }
    symPtr->nextPtr = (Sym *) Hash_GetValue(hPtr);
    Hash_SetValue(hPtr, symPtr);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Sym_GetSym --
 *
 *	Lookup a symbol and return its address.
 *
 * Results:
 *	The return value is one of SYM_FOUND (the normal case, where the
 *	symbol exists), SYM_REGISTER (if the symbol was found and is a
 *	register), SYM_NOT_FOUND if no such symbol could be found,
 *	SYM_AMBIGUOUS if the symbol is multiply-defined, or
 *	SYM_REG_NOT_OK if the name referred to a register but
 *	registers are not acceptable here.  If SYM_FOUND or SYM_REGISTER
 *	is returned, *addrPtr is modified to hold the symbol's memory
 *	address or register number.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Sym_GetSym(machPtr, fileName, name, flags, addrPtr)
    R2000 *machPtr;		/* Machine to use for name lookup. */
    char *fileName;		/* Non-null means only consider symbols
				 * that may be referenced from this file.
				 * NULL means consider any global or
				 * unambiguous symbol. */
    char *name;			/* Name of desired symbol. */
    int flags;			/* Zero or one of the following: 
				 * SYM_REGS_OK means consider normal registers;
				 * SYM_PSEUDO_OK means accept pseudo-reg
				 * names (e.g. "hi" and "lo") also. */
    unsigned int *addrPtr;	/* Address of symbol (or register number)
				 * gets stored here. */
{
    Hash_Entry *hPtr;
    register Sym *symPtr;
    Sym *matchPtr;
    int matchCount;

    matchPtr = NULL;
    matchCount = 0;
    hPtr = Hash_FindEntry(&machPtr->symbols, name);
    if (hPtr != NULL) {

	/*
	 * Loop through all the symbols with the given name, looking for
	 * a clear match (global symbol or symbol defined in given file).
	 */
    
	for (symPtr = (Sym *) Hash_GetValue(hPtr); symPtr != NULL;
		symPtr = symPtr->nextPtr) {
	    if (symPtr->address == SYM_NO_ADDR) {
		continue;
	    }
	    if (symPtr->flags & S_GLOBAL) {
		found:
		*addrPtr = symPtr->address;
		if (symPtr->flags & S_REG) {
		    if (flags & (SYM_REGS_OK|SYM_PSEUDO_OK)) {
			return SYM_REGISTER;
		    }
		    return SYM_REG_NOT_OK;
		}
		return SYM_FOUND;
	    }
	    if (fileName != NULL) {
		if (strcmp(fileName, symPtr->fileName) == 0) {
		    goto found;
		}
	    } else {
		matchPtr = symPtr;
		matchCount++;
	    }
	}
    }

    /*
     * If there's been no match so far, see if the name refers to a
     * standard register name.
     */

    if (matchCount == 0) {
	int num;
	char *end;

	if ((*name == 'r') || (*name == '$')) {
	    num = strtoul(name+1, &end, 10);
	    if ((end != (name+1)) && (*end == 0) && (num <= 31)
		    && (num >= 0)) {
		gotReg:
		if (num >= 32) {
		    if (!(flags & SYM_PSEUDO_OK)) {
			return SYM_NOT_FOUND;
		    }
		} else if (!(flags & (SYM_PSEUDO_OK|SYM_REGS_OK))) {
		    return SYM_REG_NOT_OK;
		}
		*addrPtr = num;
		return SYM_REGISTER;
	    }
	}
	if (strcmp(name, "sp") == 0) {
	    num = 29; goto gotReg;
	} else if (strcmp(name, "gp") == 0) {
	    num = 28; goto gotReg;
	} else if ((flags & SYM_PSEUDO_OK) && (strcmp(name, "hi") == 0)) {
	    num = HI_REG; goto gotReg;
	} else if ((flags & SYM_PSEUDO_OK) && (strcmp(name, "lo") == 0)) {
	    num = LO_REG; goto gotReg;
	} else if ((flags & SYM_PSEUDO_OK) && (strcmp(name, "pc") == 0)) {
	    num = PC_REG; goto gotReg;
	} else if ((flags & SYM_PSEUDO_OK) && (strcmp(name, "npc") == 0)) {
	    num = NEXT_PC_REG; goto gotReg;
	} else if (strcmp(name, "at") == 0) {
	    num = 1; goto gotReg;
	} else if ((flags & SYM_PSEUDO_OK) && (strcmp(name, "status") == 0)) {
	    num = STATUS_REG; goto gotReg;
	} else if ((flags & SYM_PSEUDO_OK) && (strcmp(name, "cause") == 0)) {
	    num = CAUSE_REG; goto gotReg;
	} else if ((flags & SYM_PSEUDO_OK) && (strcmp(name, "epc") == 0)) {
	    num = EPC_REG; goto gotReg;
	}
    }

    /*
     * One last check:  if there's an unambiguous local symbol, and that's
     * permitted, then return it.
     */

    if ((fileName == NULL) && (matchCount == 1)) {
	symPtr = matchPtr;
	goto found;
    }
    if (matchCount > 1) {
	return SYM_AMBIGUOUS;
    }
    return SYM_NOT_FOUND;
}

/*
 *----------------------------------------------------------------------
 *
 * Lex --
 *
 *	Lexical analyzer for expression parser.
 *
 * Results:
 *	TCL_OK is returned unless an error occurred while doing lexical
 *	analysis or executing an embedded command.  In that case a
 *	standard Tcl error is returned, using machPtr->interp->result to hold
 *	an error message.  In the event of a successful return, the token
 *	and (possibly) number fields in infoPtr are updated to refer to
 *	the next symbol in the expression string, and the expr field is
 *	advanced.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Lex(infoPtr)
    register ExprInfo *infoPtr;		/* Describes the state of the parse. */
{
    register char *p, c;

    /*
     * The next token is either:
     * (a)	A number (convert it from ASCII to binary).
     * (b)	A symbol name (lookup the symbol).
     * (c)	Space (skip it).
     * (d)	An operator (see what kind it is).
     */

    p = infoPtr->expr;
    infoPtr->tokenChars = p;
    c = *p;
    while (isspace(c)) {
	p++;  c = *p;
    }
    infoPtr->expr = p;
    if (!isascii(c)) {
	infoPtr->token = END;
	return TCL_OK;
    }
    if (isdigit(c)) {
	infoPtr->token = NUMBER;
	infoPtr->number = strtol(p, &infoPtr->expr, 0);
	return TCL_OK;
    }
    if (isalpha(c)) {
	char savedChar;
	int result;

	for (p++; isalnum(*p) || (*p == '_') || (*p == '$'); p++) {
	    /* Null loop body;  just skip to end of symbol. */
	}
	savedChar = *p;
	*p = 0;
	result = Sym_GetSym(infoPtr->machPtr, infoPtr->fileName,
		infoPtr->expr, 0, (unsigned int *) &infoPtr->number);
	if (result == SYM_NOT_FOUND) {
	    if (infoPtr->ignoreUndef) {
		infoPtr->number = 0;
	    } else {
		sprintf(infoPtr->machPtr->interp->result,
			"undefined symbol \"%.50s\"", infoPtr->expr);
		*p = savedChar;
		return TCL_ERROR;
	    }
	} else if (result == SYM_AMBIGUOUS) {
	    sprintf(infoPtr->machPtr->interp->result,
		    "symbol \"%.50s\" multiply defined", infoPtr->expr);
	    *p = savedChar;
	    return TCL_ERROR;
	} else if (result == SYM_REG_NOT_OK) {
	    sprintf(infoPtr->machPtr->interp->result,
		    "can't use register name in expression");
	    *p = savedChar;
	    return TCL_ERROR;
	}
	infoPtr->token = NUMBER;
	*p = savedChar;
	infoPtr->expr = p;
	return TCL_OK;
    }

    infoPtr->expr = p+1;
    switch (c) {
	case '(':
	    infoPtr->token = OPEN_PAREN;
	    return TCL_OK;

	case ')':
	    infoPtr->token = CLOSE_PAREN;
	    return TCL_OK;

	case '*':
	    infoPtr->token = MULT;
	    return TCL_OK;

	case '/':
	    infoPtr->token = DIVIDE;
	    return TCL_OK;

	case '%':
	    infoPtr->token = MOD;
	    return TCL_OK;

	case '+':
	    infoPtr->token = PLUS;
	    return TCL_OK;

	case '-':
	    infoPtr->token = MINUS;
	    return TCL_OK;

	case '<':
	    if (p[1] == '<') {
		infoPtr->expr = p+2;
		infoPtr->token = LEFT_SHIFT;
	    } else {
		infoPtr->token = END;
	    }
	    return TCL_OK;

	case '>':
	    if (p[1] == '>') {
		infoPtr->expr = p+2;
		infoPtr->token = RIGHT_SHIFT;
	    } else {
		infoPtr->token = END;
	    }
	    return TCL_OK;

	case '&':
	    infoPtr->token = BIT_AND;
	    return TCL_OK;

	case '^':
	    infoPtr->token = BIT_XOR;
	    return TCL_OK;

	case '|':
	    infoPtr->token = BIT_OR;
	    return TCL_OK;

	case '~':
	    infoPtr->token = BIT_NOT;
	    return TCL_OK;

	case 0:
	default:
	    infoPtr->token = END;
	    infoPtr->expr = p;
	    return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetValue --
 *
 *	Parse a "value" from the remainder of the expression in infoPtr.
 *
 * Results:
 *	Normally TCL_OK is returned.  The value of the parsed number is
 *	returned in infoPtr->number.  If an error occurred, then
 *	interp->result contains an error message and TCL_ERROR is returned.
 *
 * Side effects:
 *	Information gets parsed from the remaining expression, and the
 *	expr and token and tokenChars fields in infoPtr get updated.
 *	Information is parsed until either the end of the expression is
 *	reached (null character or close paren), an error occurs, or a
 *	binary operator is encountered with precedence <= prec.  In any
 *	of these cases, infoPtr->token will be left pointing to the token
 *	AFTER the expression.
 *
 *----------------------------------------------------------------------
 */

static int
GetValue(infoPtr, prec)
    register ExprInfo *infoPtr;		/* Describes the state of the parse
					 * just before the value (i.e. Lex
					 * will be called to get first token
					 * of value). */
    int prec;				/* Treat any un-parenthesized operator
					 * with precedence <= this as the end
					 * of the expression. */
{
    int result, operator, operand;
    int gotOp;				/* Non-zero means already lexed the
					 * operator (while picking up value
					 * for unary operator).  Don't lex
					 * again. */
    char *savedPtr;

    /*
     * There are two phases to this procedure.  First, pick off an initial
     * value.  Then, parse (binary operator, value) pairs until done.
     */

    gotOp = 0;
    result = Lex(infoPtr);
    if (result != TCL_OK) {
	return result;
    }
    if (infoPtr->token == OPEN_PAREN) {

	/*
	 * Parenthesized sub-expression.
	 */

	result = GetValue(infoPtr, -1);
	if (result != TCL_OK) {
	    return result;
	}
	if (infoPtr->token != CLOSE_PAREN) {
	    Tcl_Return(infoPtr->machPtr->interp, (char *) NULL, TCL_STATIC);
	    sprintf(infoPtr->machPtr->interp->result,
		    "unmatched parenthesis in expression");
	    return TCL_ERROR;
	}
    } else {
	if (infoPtr->token == MINUS) {
	    infoPtr->token = UNARY_MINUS;
	}
	if (infoPtr->token >= UNARY_MINUS) {

	    /*
	     * Process unary operators.
	     */

	    operator = infoPtr->token;
	    savedPtr = infoPtr->expr;
	    result = GetValue(infoPtr, exprPrecTable[infoPtr->token]);
	    if (result != TCL_OK) {
		return result;
	    }
	    if (infoPtr->tokenChars == savedPtr) {
		sprintf(infoPtr->machPtr->interp->result,
			"missing value after operator in expression");
		return TCL_ERROR;
	    }
	    switch (operator) {
		case UNARY_MINUS:
		    infoPtr->number = -infoPtr->number;
		    break;
		case BIT_NOT:
		    infoPtr->number = ~infoPtr->number;
		    break;
	    }
	    gotOp = 1;
	} else if (infoPtr->token != NUMBER) {
	    return TCL_OK;
	}
    }

    /*
     * Got the first operand.  Now fetch (operator, operand) pairs.
     */

    if (!gotOp) {
	result = Lex(infoPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }
    while (1) {
	operand = infoPtr->number;
	operator = infoPtr->token;
	if ((operator < MULT) || (operator >= UNARY_MINUS)) {
	    return TCL_OK;
	}
	if (exprPrecTable[operator] <= prec) {
	    return TCL_OK;
	}

	savedPtr = infoPtr->expr;
	result = GetValue(infoPtr, exprPrecTable[operator]);
	if (infoPtr->tokenChars == savedPtr) {
	    sprintf(infoPtr->machPtr->interp->result,
		    "missing value after operator in expression");
	    return TCL_ERROR;
	}
	if (result != TCL_OK) {
	    return result;
	}
	switch (operator) {
	    case MULT:
		infoPtr->number = operand * infoPtr->number;
		break;
	    case DIVIDE:
		if (infoPtr->number == 0) {
		    Tcl_Return(infoPtr->machPtr->interp,
			    "divide by zero in expression", TCL_STATIC);
		    return TCL_ERROR;
	        }
		infoPtr->number = operand / infoPtr->number;
		break;
	    case MOD:
		if (infoPtr->number == 0) {
		    Tcl_Return(infoPtr->machPtr->interp,
			    "divide by zero in expression", TCL_STATIC);
		    return TCL_ERROR;
	        }
		infoPtr->number = operand % infoPtr->number;
		break;
	    case PLUS:
		infoPtr->number = operand + infoPtr->number;
		break;
	    case MINUS:
		infoPtr->number = operand - infoPtr->number;
		break;
	    case LEFT_SHIFT:
		infoPtr->number = operand << infoPtr->number;
		break;
	    case RIGHT_SHIFT:
		infoPtr->number = operand >> infoPtr->number;
		break;
	    case BIT_AND:
		infoPtr->number = operand & infoPtr->number;
		break;
	    case BIT_XOR:
		infoPtr->number = operand ^ infoPtr->number;
		break;
	    case BIT_OR:
		infoPtr->number = operand | infoPtr->number;
		break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Sym_EvalExpr --
 *
 *	Given an expression including symbol names and constants, compute
 *	the value of the expression.
 *
 * Results:
 *	The result is a standard Tcl return value.  MachPtr's interp may
 *	get a result added to it also.  *valuePtr is modified to hold
 *	the value of the expression, and *endPtr is modified to point to
 *	the character that terminated the expression (i.e. the first
 *	character not in the expression).  If an error occurs, *endPtr
 *	is set to exprString.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Sym_EvalExpr(machPtr, fileName, exprString, ignoreUndef, valuePtr, endPtr)
    R2000 *machPtr;		/* Machine to manipulate. */
    char *fileName;		/* Name of file to use for symbol lookup.
				 * See declaration of Sym_GetSym arg for more
				 * information. */
    char *exprString;		/* Expression string to evaluate. */
    int ignoreUndef;		/* Non-zero means treat undefined symbols
				 * as having address 0. */
    int *valuePtr;		/* Store the expression value here. */
    char **endPtr;		/* Store address of terminating character
				 * here. */
{
    ExprInfo info;
    int result;

    info.machPtr = machPtr;
    info.fileName = fileName;
    info.ignoreUndef = ignoreUndef;
    info.expr = exprString;
    result = GetValue(&info, -1);
    if (result != TCL_OK) {
	*valuePtr = 0;
	*endPtr = exprString;
	return result;
    }
    *valuePtr = info.number;

    /*
     * If the expression contained no information at all, then
     * it's an error.
     */

    if (info.tokenChars == exprString) {
	sprintf(machPtr->interp->result, "missing expression");
	return TCL_ERROR;
    }

    /*
     * Skip over blank space at the end of the expression.
     */

    while (isspace(*(info.tokenChars))) {
	info.tokenChars++;
    }
    *endPtr = info.tokenChars;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Sym_GetString --
 *
 *	Given an address, return a string describing the address.
 *
 * Results:
 *	The return value is a textual description of address.  If
 *	possible, the address is identified symbolically (or with
 *	an offset).  In the worst case, it may simply be identified
 *	with a hex address.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Sym_GetString(machPtr, address)
    R2000 *machPtr;		/* Machine to manipulate. */
    unsigned int address;	/* Address for which an identifying string
				 * is returned. */
{
    Hash_Search search;
    Hash_Entry *hPtr;
    register Sym *symPtr;
    int distance, closestDistance;
    char *closestName;
    static char string[100];

    closestDistance = 10000;
    closestName = NULL;

    for (hPtr = Hash_EnumFirst(&machPtr->symbols, &search);
	    hPtr != NULL; hPtr = Hash_EnumNext(&search)) {
	for (symPtr = (Sym *) Hash_GetValue(hPtr); symPtr != NULL;
		symPtr = symPtr->nextPtr) {
	    distance = address - symPtr->address;
	    if ((distance < 0) || (distance >= closestDistance)) {
		continue;
	    }
	    if (distance == 0) {
		return hPtr->key.name;
	    }
	    closestDistance = distance;
	    closestName = hPtr->key.name;
	}
    }
    if (closestName == NULL) {
	sprintf(string, "0x%x", address);
    } else {
	sprintf(string, "%.50s+0x%x", closestName, closestDistance);
    }
    return string;
}
