/* 
 * asm.c --
 *
 *	The procedures in this file do assembly and dis-assembly of
 *	R2000 assembler instructions.
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
static char rcsid[] = "$Header: /user1/ouster/mipsim/RCS/asm.c,v 1.19 91/02/03 17:23:17 ouster Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <ctype.h>
#include <errno.h>
#include <hash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include "mips.h"
#include "asm.h"
#include "gp.h"
#include "sym.h"

/*
 * The following structure type encapsulates the state of loading one
 * or more assembler files, and is used for communication between the
 * procedures that do assembly.
 */

typedef struct {
    char *file;			/* Name of file currently being assembled
				 * (none means info to assemble comes from
				 * some source other than file). */
    int lineNum;		/* Line number within file (-1 if info being
				 * assembled doesn't come from file). */
    char *line;			/* Contents of entire line being assembled
				 * (useful for error messages). */
    unsigned int codeAddr;	/* Next address to place information in
				 * code area. */
    unsigned int dataAddr;	/* Next address to place information in
				 * data area. */
    unsigned int dot;		/* Address at which to place information
				 * (can be either code or data).

    /*
     * Information used to build up a concatenated version of all the
     * error strings that occur while reading the files:
     */

    char *message;		/* Pointer to current message (malloc-ed).
				 * NULL means no error has occurred so far. */
    char *end;			/* Address of NULL byte at end of message;
				 * append new messages here. */
    int totalBytes;		/* # of bytes allocated at message. 0 means
				 * no error has occurred yet. */
    int errorCount;		/* If too many errors occur, give up. */

    int flags;			/* Various flags:  see below for values. */
} LoadInfo;

/*
 * Flags for LoadInfo structures:
 *
 * ASM_CODE -			1 means currently assembling into code
 *				area;  0 means currently assembling into
 *				data area.
 * ASM_SIZE_ONLY -		1 means this is the first pass, where the
 *				only important thing is size (suppress
 *				all error messages).
 * ASM_ALIGN_0 -		1 means that an "align 0" command is in
 *				effect.
 */

#define ASM_CODE	1
#define ASM_SIZE_ONLY	2
#define ASM_ALIGN_0	4

#define ASM_MAX_ERRORS	20

/*
 * The #defines below specify the different classes of instructions,
 * as defined on pp. D-4 to D-6 of Kane's book.  These classes are used
 * during assembly, and indicate the different formats that may be taken
 * by operand specifiers for a particular opcode.
 * 
 * NO_ARGS -		no operands
 * LOAD_STORE -		(register, address)
 * BREAK -		(immediate), as for break
 * LI -			(dest, 32-bit expression)
 * LUI -		(dest, 16-bit expression)
 * UNARY_ARITH -	(dest, src)
 * ARITH -		(dest, src1, src2) OR (dest/src1, src2)
 *			OR (dest, src1, 16-bit immediate)
 *			OR (dest/src1, 16-bit immediate)
 * MULDIV -		same as ARITH (special subset to handle
 *			mult/divide/rem instructions)
 * NOR -		same as ARITH (special subset for nor,
 *			which has no immediate form)
 * SHIFT -		same as ARITH (special subset to handle
 *			shifting instructions)
 * SUB -		same as ARITH (special subset to handle
 *			subtraction instructions)
 * MULT -		(src1, src2)
 * BRANCH -		(label)
 * BRANCH_EQ -		(src1, src2, label)
 *			OR (src1, 16-bit immediate, label)
 * BRANCH_INEQ -	same as BRANCH_EQ (special subset to handle
 *			inequality tests)
 * BRANCH_1_OP -	(src1, label)
 * JUMP -		(label) OR (src1)
 * JALR -		(dest, src1) OR (src1)
 * SRC1 -		(src1)
 * DEST -		(dest)
 * MOVE -		(dest,src1)
 * MTC -		(rt, rd) (for MTC and MFC instructions)
 */

#define NO_ARGS		0
#define LOAD_STORE	1
#define BREAK		2
#define LI		3
#define LUI		4
#define UNARY_ARITH	5
#define ARITH		6
#define MULDIV		7
#define NOR		8
#define SHIFT		9
#define SUB		10
#define MULT		11
#define BRANCH		12
#define BRANCH_EQ	13
#define BRANCH_INEQ	14
#define BRANCH_1_OP	15
#define JUMP		16
#define JALR		17
#define SRC1		18
#define DEST		19
#define MOVE		20
#define MTC		21

/*
 * This file allows the use of the pseudo-op-codes defined on pages
 * D-4 to D-6 in Kane.  For example, it is possible to say
 * "add r2,r3" instead of the full form "add r2,r2,r3".  However,
 * for classroom teaching these convenience operators are confusing.
 * If STRICT is #defined  below, the Mipsim allows only the operators
 * defined in Appendix A of Kane (i.e. no pseudo-ops except "nop").
 */

#define STRICT 1

/*
 * The tables below give the maximum and minimum # of arguments
 * permissible for each class above.
 */

#ifndef STRICT
static int minArgs[] =
    {0, 3, 0, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1, 3, 3, 2, 1, 1, 1, 1, 2, 2};
static int maxArgs[] =
    {0, 3, 1, 2, 2, 2, 3, 3, 3, 3, 3, 2, 1, 3, 3, 2, 1, 2, 1, 1, 2, 2};
#else
static int minArgs[] =
    {0, 3, 0, 2, 2, 2, 3, 2, 3, 3, 3, 2, 1, 3, 3, 2, 1, 2, 1, 1, 2, 2};
static int maxArgs[] =
    {0, 3, 1, 2, 2, 2, 3, 2, 3, 3, 3, 2, 1, 3, 3, 2, 1, 2, 1, 1, 2, 2};
#endif

/*
 * Structures of the following type are used during assembly and
 * disassembly.  One such structure exists for each defined op code.
 */

typedef struct {
    char *name;			/* Opcode name, e.g. "add". */
    int class;			/* Class of instruction (see table above). */
    int op;			/* Bit pattern corresponding to this
				 * instruction. */
    int mask;			/* Used for disassembly:  if these bits match
				 * op, then use this opcode for disassembly.
				 * 0 means this is a synthesized instruction
				 * that doesn't exist in native form, so
				 * it should be ignored during disassembly. */
    int other;			/* This field is used when the assembler
				 * is generating multiple instructions for
				 * a single opcode, or when different
				 * instructions may be generated for the
				 * same opcode (e.g. add -> addi).  The
				 * meaning of the field depends on class;
				 * see the code in Asm_Assemble. */
    int flags;			/* OR-ed combination of bits, giving various
				 * information for use during assembly, such
				 * as for range checking.  See below for
				 * values. */
    int rangeMask;		/* Mask for use in range check:  for unsigned
				 * check, none of these bits must be set.  For
				 * sign-extended check, either all or none
				 * must be set. */
} OpcodeInfo;

/*
 * Bits for "flags" field, used for range checking:
 *
 * CHECK_LAST -			1 means check last argument, if it is
 *				an immediate.
 * CHECK_NEXT_TO_LAST -		1 means check next-to-last argument, if it
 *				is an immediate.
 * IMMEDIATE_REQ -		1 means the argument given above MUST be
 *				an immediate.
 * SIGN_EXTENDED -		1 means the immediate will be sign-extended.
 *
 * Other flag bits:
 *
 * DIV_INS -			1 means this is a divide instruction;  if
 *				two operands given, both registers, just
 *				generate native divide instruction without
 *				moving the result back from LO.
 * BRANCH_L -			1 means this branch instruction is checking
 *				for "less than".
 * BRANCH_LE -			1 means this branch instruction is checking
 *				for "less than or equal".
 * BRANCH_GE -			1 means this branch instruction is checking
 *				for "greater than or equal".
 * BRANCH_G -			1 means this branch instruction is checking
 *				for "greater than".
 */

#define CHECK_LAST		1
#define CHECK_NEXT_TO_LAST	2
#define IMMEDIATE_REQ		4
#define SIGN_EXTENDED		8

#define DIV_INS			0x10
#define BRANCH_L		0x20
#define BRANCH_LE		0x40
#define BRANCH_GE		0x80
#define BRANCH_G 		0x100

/*
 * The following value for "other" is used in the SHIFT class to
 * indicate that this instruction must always take the variable
 * form.
 */

#define ALWAYS_VAR 0xfffffffe

/*
 * Table of all known instructions:
 */

OpcodeInfo opcodes[] = {
    {"add", ARITH, 0x20, 0xfc00003f, 0x20000000,
	    CHECK_LAST|SIGN_EXTENDED, 0xffff0000},
    {"addi", ARITH, 0x20000000, 0xfc000000, 0,
	    CHECK_LAST|SIGN_EXTENDED|IMMEDIATE_REQ, 0xffff0000},
    {"addiu", ARITH, 0x24000000, 0xfc000000, 0,
	    CHECK_LAST|SIGN_EXTENDED|IMMEDIATE_REQ, 0xffff0000},
    {"addu", ARITH, 0x21, 0xfc00003f, 0x24000000,
	    CHECK_LAST|SIGN_EXTENDED, 0xffff0000},
    {"and", ARITH, 0x24, 0xfc00003f, 0x30000000,
	    CHECK_LAST, 0xffff0000},
    {"andi", ARITH, 0x30000000, 0xfc000000, 0,
	    CHECK_LAST|IMMEDIATE_REQ, 0xffff0000},
#ifndef STRICT
    {"b", BRANCH, 0x10000000, 0xffff0000, 0, 0, 0xffff8000},
#endif
    {"beq", BRANCH_EQ, 0x10000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|SIGN_EXTENDED, 0xffff8000},
#ifndef STRICT
    {"bge", BRANCH_INEQ, 0x2a, 0, 0x28000000,
	    CHECK_LAST|BRANCH_GE, 0xffff8000},
    {"bgeu", BRANCH_INEQ, 0x2b, 0, 0x2c000000,
	    CHECK_LAST|BRANCH_GE, 0xffff8000},
#endif
    {"bgez", BRANCH_1_OP, 0x04010000, 0xfc1f0000, 0, 0, 0xffff8000},
    {"bgezal", BRANCH_1_OP, 0x04110000, 0xfc1f0000, 0, 0, 0xffff8000},
#ifndef STRICT
    {"bgt", BRANCH_INEQ, 0x2a, 0, 0x28000000,
	    CHECK_LAST|BRANCH_G, 0xffff8000},
    {"bgtu", BRANCH_INEQ, 0x2b, 0, 0x2c000000,
	    CHECK_LAST|BRANCH_G, 0xffff8000},
#endif
    {"bgtz", BRANCH_1_OP, 0x1c000000, 0xfc000000, 0, 0, 0xffff8000},
#ifndef STRICT
    {"ble", BRANCH_INEQ, 0x2a, 0, 0x28000000,
	    CHECK_LAST|BRANCH_LE, 0xffff8000},
    {"bleu", BRANCH_INEQ, 0x2b, 0, 0x2c000000,
	    CHECK_LAST|BRANCH_LE, 0xffff8000},
#endif
    {"blez", BRANCH_1_OP, 0x18000000, 0xfc000000, 0, 0, 0xffff8000},
#ifndef STRICT
    {"blt", BRANCH_INEQ, 0x2a, 0, 0x28000000,
	    CHECK_LAST|BRANCH_L, 0xffff8000},
    {"bltu", BRANCH_INEQ, 0x2b, 0, 0x2c000000,
	    CHECK_LAST|BRANCH_L, 0xffff8000},
#endif
    {"bltz", BRANCH_1_OP, 0x04000000, 0xfc1f0000, 0, 0, 0xffff8000},
    {"bltzal", BRANCH_1_OP, 0x04100000, 0xfc1f0000, 0, 0, 0xffff8000},
    {"bne", BRANCH_EQ, 0x14000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|SIGN_EXTENDED, 0xffff8000},
#ifndef STRICT
    {"br", BRANCH, 0x10000000, 0xffff0000, 0, 0, 0xffff8000},
#endif
    {"break", BREAK, 0xd, 0xfc00003f, 0,
	    CHECK_LAST, 0xfff00000},
    {"div", MULDIV, 0x1a, 0xfc00003f, 0x12,
	    CHECK_LAST|SIGN_EXTENDED|DIV_INS, 0xffff0000},
    {"divu", MULDIV, 0x1b, 0xfc00003f, 0x12,
	    CHECK_LAST|DIV_INS, 0xffff0000},
    {"j", JUMP, 0x8000000, 0xfc000000, 0x8, 0, 0},
    {"jal", JUMP, 0xc000000, 0xfc000000, 0x9, 0, 0},
    {"jalr", JALR, 0x9, 0xfc00003f, 0, 0, 0},
    {"jr", SRC1, 0x8, 0xfc00003f, 0, 0, 0},
#ifndef STRICT
    {"la", LI, 0, 0, 0, 0, 0},
    {"li", LI, 0, 0, 0, 0, 0},
#endif
    {"lb", LOAD_STORE, 0x80000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|SIGN_EXTENDED, 0xffff0000},
    {"lbu", LOAD_STORE, 0x90000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|SIGN_EXTENDED, 0xffff0000},
    {"lh", LOAD_STORE, 0x84000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|SIGN_EXTENDED, 0xffff0000},
    {"lhu", LOAD_STORE, 0x94000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|SIGN_EXTENDED, 0xffff0000},
    {"lui", LUI, 0x3c000000, 0xfc000000, 0,
	    CHECK_LAST|IMMEDIATE_REQ, 0xffff0000},
    {"lw", LOAD_STORE, 0x8c000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|SIGN_EXTENDED, 0xffff0000},
    {"lwl", LOAD_STORE, 0x88000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|SIGN_EXTENDED, 0xffff0000},
    {"lwr", LOAD_STORE, 0x98000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|SIGN_EXTENDED, 0xffff0000},
    {"mfc0", MTC, 0x40000000, 0xffe003ff, 0, 0, 0},
    {"mfhi", DEST, 0x10, 0xfc00003f, 0, 0, 0},
    {"mflo", DEST, 0x12, 0xfc00003f, 0, 0, 0},
#ifndef STRICT
    {"move", MOVE, 0x20000000, 0, 0, 0, 0},
#endif
    {"mtc0", MTC, 0x40800000, 0xffe003ff, 0, 0, 0},
    {"mthi", SRC1, 0x11, 0xfc00003f, 0, 0, 0},
    {"mtlo", SRC1, 0x13, 0xfc00003f, 0, 0, 0},
#ifndef STRICT
    {"mul", MULDIV, 0x18, 0xfc00003f, 0x12,
	    CHECK_LAST|SIGN_EXTENDED, 0xffff0000},
#endif
    {"mult", MULT, 0x18, 0xfc00003f, 0, 0, 0},
    {"multu", MULT, 0x19, 0xfc00003f, 0, 0, 0},
#ifndef STRICT
    {"neg", UNARY_ARITH, 0x22, 0, 0, 0, 0},
    {"negu", UNARY_ARITH, 0x23, 0, 0, 0, 0},
#endif
    {"nop", NO_ARGS, 0x0, 0xffffffff, 0, 0, 0},
    {"nor", NOR, 0x27, 0xfc00003f, 0,
	    CHECK_LAST, 0xffff0000},
#ifndef STRICT
    {"not", UNARY_ARITH, 0x27, 0, 0, 0, 0},
#endif
    {"or", ARITH, 0x25, 0xfc00003f, 0x34000000,
	    CHECK_LAST, 0xffff0000},
    {"ori", ARITH, 0x34000000, 0xfc000000, 0,
	    CHECK_LAST|IMMEDIATE_REQ, 0xffff0000},
#ifndef STRICT
    {"rem", MULDIV, 0x1a, 0xfc00003f, 0x10,
	    CHECK_LAST|SIGN_EXTENDED, 0xffff0000},
    {"remu", MULDIV, 0x1b, 0xfc00003f, 0x10,
	    CHECK_LAST, 0xffff0000},
#endif
    {"rfe", NO_ARGS, 0x42000010, 0xffffffff, 0, 0, 0},
    {"sb", LOAD_STORE, 0xa0000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|IMMEDIATE_REQ|SIGN_EXTENDED, 0xffff0000},
    {"sh", LOAD_STORE, 0xa4000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|IMMEDIATE_REQ|SIGN_EXTENDED, 0xffff0000},
    {"sll", SHIFT, 0x0, 0xfc00003f, 0x4,
	    CHECK_LAST, 0xffffffe0},
    {"sllv", SHIFT, 0x4, 0xfc00003f, ALWAYS_VAR, 0, 0},
    {"slt", ARITH, 0x2a, 0xfc00003f, 0x28000000,
	    CHECK_LAST|SIGN_EXTENDED, 0xffff0000},
    {"slti", ARITH, 0x28000000, 0xfc000000, 0,
	    CHECK_LAST|IMMEDIATE_REQ|SIGN_EXTENDED, 0xffff0000},
    {"sltiu", ARITH, 0x2c000000, 0xfc000000, 0,
	    CHECK_LAST|IMMEDIATE_REQ|SIGN_EXTENDED, 0xffff0000},
    {"sltu", ARITH, 0x2b, 0xfc00003f, 0xfc000000,
	    CHECK_LAST, 0xffff0000},
    {"sra", SHIFT, 0x3, 0xfc00003f, 0x7,
	    CHECK_LAST, 0xffffffe0},
    {"srav", SHIFT, 0x7, 0xfc00003f, ALWAYS_VAR, 0, 0},
    {"srl", SHIFT, 0x2, 0xfc00003f, 0x6,
	    CHECK_LAST, 0xffffffe0},
    {"srlv", SHIFT, 0x6, 0xfc00003f, ALWAYS_VAR},
    {"sub", SUB, 0x22, 0xfc00003f, 0,
	    CHECK_LAST|SIGN_EXTENDED, 0xffff0000},
    {"subu", SUB, 0x23, 0xfc00003f, 0,
	    CHECK_LAST|SIGN_EXTENDED, 0xffff0000},
    {"sw", LOAD_STORE, 0xac000000, 0xfc000000, 0,
	    CHECK_NEXT_TO_LAST|IMMEDIATE_REQ|SIGN_EXTENDED, 0xffff0000},
    {"swl", LOAD_STORE, 0xa8000000, 0xfc000000,
	    CHECK_LAST|IMMEDIATE_REQ|SIGN_EXTENDED, 0xffff0000},
    {"swr", LOAD_STORE, 0xb8000000, 0xfc000000,
	    CHECK_LAST|SIGN_EXTENDED, 0xffff0000},
    {"xor", ARITH, 0x26, 0xfc00003f, 0x38000000,
	    CHECK_LAST, 0xffff0000},
    {"xori", ARITH, 0x38000000, 0xfc000000, 0,
	    CHECK_LAST|IMMEDIATE_REQ, 0xffff0000},
    {NULL, NO_ARGS, 0, 0, 0, 0, 0}
};

/*
 * Opcode values that are used in the code of this module:
 */

#define SUBU_OP			0x23
#define ADDI_OP			0x20000000
#define ADDIU_OP		0x24000000
#define LUI_OP			0x3c000000
#define ORI_OP			0x34000000
#define BEQ_OP			0x10000000
#define BNE_OP			0x14000000
#define LOAD_IMM(reg, x)	(0x20000000 | ((reg) << 16) | ((x) & 0xffff))
#define LOAD_IMM_UNS(reg, x)	(0x34000000 | ((reg) << 16) | ((x) & 0xffff))

/*
 * Table mapping from register number to register name.
 */

char *Asm_RegNames[] = {
    "r0", "r1", "r2", "r3",
    "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11",
    "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19",
    "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27",
    "r28", "sp", "r30", "r31",
    "hi", "lo", "pc", "npc",
    "status", "cause", "epc"
};

/*
 * Size of largest instruction we can assemble, in words:
 */

#define ASM_MAX_WORDS 5

/*
 * Forward declarations for procedures defined in this file:
 */

static void	AddErrMsg();
static void	IndicateError();
static void	ReadFile();
static int	StoreWords();

/*
 *----------------------------------------------------------------------
 *
 * Asm_Assemble --
 *
 *	Given a string describing an assembler instruction, return
 *	the binary code corresponding to the instruction.
 *
 * Results:
 *	The return value is a standard Tcl result (normally TCL_OK plus
 *	an empty string).  If the assembly completed successfully, then
 *	*sizePtr gets filled in with the # of instruction words assembled
 *	(may be more than 1 for special pseudo-instructions), and the
 *	word(s) at *codePtr get filled in with the actual instruction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Asm_Assemble(machPtr, fileName, string, dot, wholeLine, sizeOnly,
	sizePtr, codePtr)
    R2000 *machPtr;			/* Machine for which assembly is done:
					 * used for symbol table and error
					 * reporting. */
    char *fileName;			/* File name to use for symbol
					 * lookups.  See GetSym procedure in
					 * sym.c for more information. */
    char *string;			/* R2000 assembler instruction. */
    unsigned int dot;			/* Where in memory instruction(s) will
					 * be placed. */
    char *wholeLine;			/* Entire line containing string;  used
					 * when printing error messages. */
    int sizeOnly;			/* Non-zero means this is the first
					 * pass, so ignore errors and just
					 * compute the size of the instruction.
					 */
    int *sizePtr;			/* Fill in with # words assembled
					 * for this instruction. */
    int *codePtr;			/* Pointer to ASM_MAX_WORDS words
					 * storage, which get filled in with
					 * assembled code. */
{
    register OpcodeInfo *insPtr;	/* Info about instruction. */
    register char *p;			/* Current character in string. */
    char *errMsg;
    char *opStart;
    int length;
    char msg[100];
    int isReg[3];			/* Tells whether each operand in the
					 * instruction is a register. */
    int operands[3];			/* Value of each operand (reg #,
					 * immediate, shift amount, etc.). */
    char *argStart[3];			/* First chars. of arguments (for
					 * error reporting). */
    int numOps;				/* Number of operands in the
					 * instruction. */

    /*
     * Parse off the instruction name, and look it up in the table.
     */

    for (p = string; (*p == ' ') || (*p == '\t'); p++) {
	/* Empty loop body. */
    }
    opStart = p;
    for ( ; isalnum(*p); p++) {
	/* Empty loop body. */
    }
    length = p-opStart;
    if (length > 0) {
	for (insPtr = opcodes; insPtr->name != NULL; insPtr++) {
	    if ((insPtr->name[0] == opStart[0])
		    && (strncmp(insPtr->name, opStart, length) == 0)
		    && (insPtr->name[length] == 0)) {
		codePtr[0] = insPtr->op;
		goto gotIns;
	    }
	}
    }
    errMsg= "unknown opcode";
    p =  opStart;
    goto error;

    /*
     * Parse up to three operand fields in the instruction, storing
     * information in isReg[], operands[], and numOps.
     */

    gotIns:
    isReg[0] = isReg[1] = isReg[2] = 0;
    operands[0] = operands[1] = operands[2] = 0;
    for (numOps = 0; numOps < 3; numOps++) {
	char *end, savedChar;
	int result;

	/*
	 * Find the starting character for this operand specifier.
	 */

	while ((*p == ' ') || (*p == '\t')) {
	    p++;
	}
	argStart[numOps] = p;

	/*
	 * The code below is a special case to handle the second
	 * specifier for instructions in the load-store class.  Accept
	 * an optional expression followed by an optional register
	 * name in parentheses.
	 */

	if ((numOps == 1) && (insPtr->class == LOAD_STORE)) {
	    if (*p == '(') {
		operands[1] = 0;
	    } else {
		if (Sym_EvalExpr(machPtr, fileName, p, sizeOnly,
			&operands[1], &end) != TCL_OK) {
		    errMsg = machPtr->interp->result;
		    goto error;
		}
		p = end;
	    }
	    while ((*p == ' ') || (*p == '\t')) {
		p++;
	    }
	    argStart[2] = p;
	    isReg[2] = 1;
	    operands[2] = 0;
	    if (*p == '(') {
		for (p++; *p != ')'; p++) {
		    if ((*p == 0)  || (*p == '#')) {
			p = argStart[2];
			errMsg = "missing ) after base register";
			goto error;
		    }
		}
		savedChar = *p;
		*p = 0;
		result = Sym_GetSym(machPtr, fileName, argStart[2]+1,
			SYM_REGS_OK, (unsigned int *) &operands[2]);
		*p = savedChar;
		if (result != SYM_REGISTER) {
		    p = argStart[2]+1;
		    errMsg = "bad base register name";
		    goto error;
		}
		p++;
	    }
	    numOps = 3;
	    break;
	}

	/*
	 * Back to the normal case.  Find the end of the current
	 * operand specifier.
	 */

	while ((*p != ',') && (*p != '#') && (*p != 0) && (*p != '\n')) {
	    p++;
	}
	end = p;
	if (p == argStart[numOps]) {
	    if (numOps == 0) {
		break;
	    }
	    errMsg = "empty operand specifier";
	    goto error;
	}
	for (p--; (*p == ' ') || (*p == '\t'); p--) {
	    /* Null loop body;  just backspace over space */
	}
	p++;

	/*
	 * Figure out what kind of operand this is.
	 */

	savedChar = *p;
	*p = 0;
	result = Sym_GetSym(machPtr, fileName, argStart[numOps], SYM_REGS_OK,
	    (unsigned int *) &operands[numOps]);
	if (result == SYM_REGISTER) {
	    isReg[numOps] = 1;
	} else if (result != SYM_FOUND) {
	    char *term;

	    if (Sym_EvalExpr(machPtr, (char *) NULL, argStart[numOps],
		    sizeOnly, (int *) &operands[numOps], &term) != TCL_OK) {
		*p = savedChar;
		p = argStart[numOps];
		errMsg = "unrecognizable operand specifier";
		goto error;
	    }
	    if (*term != 0) {
		*p = savedChar;
		p = term;
		errMsg = "unknown garbage in expression";
		goto error;
	    }
	}
	*p = savedChar;

	/*
	 * See if this is the last argument.  If not, skip over the
	 * separating comma.
	 */

	p = end;
	if (*p != ',') {
	    numOps++;
	    break;
	}
	if (numOps == 2) {
	    errMsg = "more than three operands";
	    goto error;
	}
	p++;
    }

    /*
     * Check argument count for propriety.
     */

    if ((numOps < minArgs[insPtr->class])
	    || (numOps > maxArgs[insPtr->class])) {
	if (minArgs[insPtr->class] == maxArgs[insPtr->class]) {
	    sprintf(msg, "wrong # operands (must be %d)",
		    minArgs[insPtr->class]);
	} else {
	    sprintf(msg, "wrong # operands (must be %d or %d)",
		    minArgs[insPtr->class], maxArgs[insPtr->class]);
	}
	p = argStart[0];
	errMsg = msg;
	goto error;
    }

    /*
     * Check immediate arguments for proper range.
     */

    if (insPtr->flags & (CHECK_LAST | CHECK_NEXT_TO_LAST)) {
	int i;

	if (insPtr->flags & CHECK_LAST) {
	    i = numOps-1;
	} else {
	    i = numOps-2;
	}
	if (i >= 0) {
	    if (isReg[i]) {
		if (insPtr->flags & IMMEDIATE_REQ) {
		    p = argStart[i];
		    regIllegal:
		    errMsg = "register operand not allowed";
		    goto error;
		}
	    } else {
		int j;
		j = operands[i] & insPtr->rangeMask;
		if (j != 0) {
		    j = insPtr->rangeMask >> 1;
		    if (!(insPtr->flags & SIGN_EXTENDED)
			    || ((operands[i] & j) != j)) {
			p = argStart[i];
			sprintf(msg, "immediate operand 0x%x out of range",
				operands[i]);
			errMsg = msg;
			goto error;
		    }
		}
	    }
	}
    }

    /*
     * Dispatch based on the class of instruction, and handle everything
     * else in a class-specific fashion.
     */

    *sizePtr = 1;
    switch (insPtr->class) {
	case NO_ARGS:
	    codePtr[0] = insPtr->op;
	    break;

	case LOAD_STORE:
	    codePtr[0] = insPtr->op | (operands[0] << 16)
		    | (operands[1] & 0xffff) | (operands[2] << 21);
	    break;

	case BREAK:
	    codePtr[0] = insPtr->op | (operands[0] << 6);
	    break;

	case LI:
	    if (!isReg[0]) {
		p = argStart[0];
		goto regRequired;
	    }
	    if (isReg[1]) {
		p = argStart[1];
		goto regIllegal;
	    }
	    if (!(operands[1] & 0xffff0000)) {
		codePtr[0] = LOAD_IMM_UNS(operands[0], operands[1]);
	    } else if ((operands[1] & 0xffff8000) == 0xffff8000) {
		codePtr[0] = LOAD_IMM(operands[0], operands[1]);
	    } else {
		*sizePtr = 2;
		codePtr[0] = LUI_OP | (operands[0] << 16)
			| ((operands[1] >> 16) & 0xffff);
		codePtr[1] = ORI_OP | (operands[0] << 16)
			| (operands[0] << 21) | (operands[1] & 0xffff);
	    }
	    break;

	case LUI:
	    codePtr[0] = insPtr->op | (operands[0] << 16)
		    | (operands[1] & 0xffff);
	    break;

	case UNARY_ARITH:
	    if (!isReg[0]) {
		p = argStart[0];
		goto regRequired;
	    }
	    if (numOps == 1) {
		isReg[1] = isReg[0];
		argStart[1] = argStart[0];
		operands[1] = operands[0];
	    }
	    if (!isReg[1]) {
		p = argStart[1];
		goto regRequired;
	    }
	    codePtr[0] = insPtr->op | (operands[0] << 11)
		    | (operands[1] << 16);
	    break;

	/*
	 * The main class of arithmetic instructions can get assembled
	 * in many different ways.  Most instructions can end using either
	 * the normal register-to-register opcode, or an immediate opcode,
	 * which is stored in insPtr->other.  If the instruction MUST use
	 * only the immediate form, a special value of insPtr->other
	 * indicates this fact.
	 */

	case ARITH:
	case MULDIV:
	case NOR:
	case SHIFT:
	case SUB:
	    if (!isReg[0]) {
		p = argStart[0];
		regRequired:
		errMsg = "operand must be a register";
		goto error;
	    }

	    /*
	     * If two operands given, duplicate first register to make
	     * three operands (except for special case of div/divu and
	     * two register operands;  then generate native div/divu
	     * instruction).
	     */

	    if ((numOps == 2) && !((insPtr->flags & DIV_INS) && isReg[1])) {
		isReg[2] = isReg[1];
		operands[2] = operands[1];
		argStart[2] = argStart[1];
		isReg[1] = isReg[0];
		operands[1] = operands[0];
		argStart[1] = argStart[0];
		numOps = 3;
	    } else if (!isReg[1]) {
		p = argStart[1];
		goto regRequired;
	    }
	    if (insPtr->class == ARITH) {
		if (isReg[2]) {
		    codePtr[0] = insPtr->op | (operands[0] << 11)
			    | (operands[1] << 21) | (operands[2] << 16);
		} else if (insPtr->flags & IMMEDIATE_REQ) {
		    codePtr[0] = insPtr->op | (operands[0] << 16)
			    | (operands[1] << 21) | (operands[2] & 0xffff);
		} else {
#ifdef STRICT
		    p = argStart[2];
		    goto regRequired;
#else
		    codePtr[0] = insPtr->other | (operands[0] << 16)
			    | (operands[1] << 21) | (operands[2] & 0xffff);
#endif
		}
	    } else if (insPtr->class == SUB) {
		if (isReg[2]) {
		    codePtr[0] = insPtr->op | (operands[0] << 11)
			    | (operands[1] << 21) | (operands[2] << 16);
		} else {
#ifdef STRICT
		    p = argStart[2];
		    goto regRequired;
#else
		    /*
		     * Sub with immediate:  turn into add with the
		     * negative immediate (but be careful for the one
		     * immediate value that can't be negated!).
		     */

		    if (operands[2] == 0xffff8000) {
			p = argStart[2];
			errMsg = "immediate value 0xffff8000 is out of range";
			goto error;
		    }
		    if (insPtr->op == SUBU_OP) {
			codePtr[0] = ADDIU_OP;
		    } else {
			codePtr[0] = ADDI_OP;
		    }
		    codePtr[0] |= (operands[0] << 16)
			    | (operands[1] << 21) | ((-operands[2]) & 0xffff);
#endif
		}
	    } else if (insPtr->class == SHIFT) {
		if (isReg[2]) {
		    if (insPtr->other == ALWAYS_VAR) {
			codePtr[0] = insPtr->op | (operands[0] << 11)
				| (operands[1] << 16) | (operands[2] << 21);
		    } else {
#ifdef STRICT
			p = argStart[2];
			errMsg = "operand must be immediate";
			goto error;
#else
			codePtr[0] = insPtr->other | (operands[0] << 11)
				| (operands[1] << 16) | (operands[2] << 21);
#endif
		    }
		} else {
		    if (insPtr->other == ALWAYS_VAR) {
			p = argStart[2];
			goto regRequired;
		    }
		    codePtr[0] = insPtr->op | (operands[0] << 11)
			| (operands[1] << 16) | (operands[2] << 6);
		}
	    } else if (insPtr->class == NOR) {
		if (isReg[2]) {
		    codePtr[0] = insPtr->op | (operands[0] << 11)
			    | (operands[1] << 21) | (operands[2] << 16);
		} else {
#ifdef STRICT
		    p = argStart[2];
		    goto regRequired;
#else
		    *sizePtr = 2;
		    codePtr[0] = LOAD_IMM_UNS(1, operands[2]);
		    codePtr[1] = insPtr->op | (operands[0] << 11)
			    | (operands[1] << 21) | (1 << 16);
#endif
		}
	    } else if (insPtr->class == MULDIV) {

		/*
		 * Multiplies and divides and remainders can result in up
		 * to three instructions:  one to load a constant into r1,
		 * one to multiply or divide, and one to load the result
		 * back from LO or HI.  The "other" fields contains the
		 * opcode (MFLO or MFHI) to retrieve the result.
		 */

		if (numOps == 2) {
		    codePtr[0] = insPtr->op | (operands[0] << 21)
			    | (operands[1] << 16);
		} else if (isReg[2]) {
		    *sizePtr = 2;
		    codePtr[0] = insPtr->op | (operands[1] << 21)
			    | (operands[2] << 16);
		    codePtr[1] = insPtr->other | (operands[0] << 11);
		} else {
		    *sizePtr = 3;
		    codePtr[0] = LOAD_IMM(1, operands[2]);
		    codePtr[1] = insPtr->op | (operands[1] << 21) | (1 << 16);
		    codePtr[2] = insPtr->other | (operands[0] << 11);
		}
	    }
	    break;

	case MULT:
	    if (!isReg[0]) {
		p = argStart[0];
		goto regRequired;
	    }
	    if (!isReg[1]) {
		p = argStart[1];
		goto regRequired;
	    }
	    codePtr[0] = insPtr->op | (operands[0] << 21) | (operands[1] << 16);
	    break;

	/*
	 * Branches:  generate (and check) the branch displacement, which
	 * is done the same for all branch instructions.  Then handle
	 * different sub-classes differently.  Two-instruction sequences
	 * get generated for some of the BRANCH_EQ and all of the
	 * BRANCH_INEQ instructions.
	 */

	case BRANCH:
	case BRANCH_EQ:
	case BRANCH_INEQ:
	case BRANCH_1_OP: {
	    int disp;

	    if (isReg[numOps-1]) {
		p = argStart[numOps-1];
		goto regIllegal;
	    }
	    disp = operands[numOps-1];
	    if (disp & 0x3) {
		p = argStart[numOps-1];
		errMsg = "branch target not word-aligned";
		goto error;
	    }
	    disp = (disp - (dot+4)) >> 2;
	    if ((disp & 0x3fff8000) && ((disp & 0x3fff8000) != 0x3fff8000)) {
		badDisp:
		p = argStart[numOps-1];
		sprintf(msg, "branch target too far away (offset 0x%x)", disp);
		errMsg = msg;
		goto error;
	    }
	    if (insPtr->class == BRANCH) {
		codePtr[0] = insPtr->op | (disp & 0xffff);
		break;
	    }
	    if (!isReg[0]) {
		p = argStart[0];
		goto regRequired;
	    }
	    if (insPtr->class == BRANCH_EQ) {
		if (isReg[1]) {
		    codePtr[0] = insPtr->op | (disp & 0xffff)
			    | (operands[0] << 21) | (operands[1] << 16);
		} else {
#ifdef STRICT
		    p = argStart[2];
		    goto regRequired;
#else
		    /*
		     * Generate a two-word sequence.  This moves the branch
		     * instruction down, which changes the branch offset.
		     */

		    if (disp == 0xffff8000) {
			goto badDisp;
		    }
		    disp -= 1;
		    codePtr[0] = LOAD_IMM(1, operands[1]);
		    codePtr[1] = insPtr->op | (disp & 0xffff)
			    | (operands[0] << 21) | (1 << 16);
		    *sizePtr = 2;
#endif
		}
	    } else if (insPtr->class == BRANCH_INEQ) {

		/*
		 * Two-operand branch with an inequality.  Must
		 * synthesize two instructions, the first of which is
		 * in the slt class and the second of which is beq or
		 * bne.  This is tricky because the only comparison
		 * available is <.  To handle >=, do < but take NOT of
		 * result ("other" field selects beq/bne to handle this).
		 * To handle <=, offset an immediate argument by 1 or
		 * reverse registers for comparison.  The four flags
		 * BRANCH_G, BRANCH_GE, BRANCH_L, and BRANCH_LE select
		 * among these options.
		 *
		 * Note also that the branch offset has to change to
		 * reflect the fact that this is a two-instruction
		 * sequence
		 */

		if (disp == 0xffff8000) {
		    goto badDisp;
		}
		disp -= 1;
		if (isReg[1]) {
		    if (insPtr->flags & (BRANCH_GE|BRANCH_L)) {
			codePtr[0] = insPtr->op | (operands[0] << 21)
				| (operands[1] << 16) | (1 << 11);
		    } else {
			codePtr[0] = insPtr->op | (operands[1] << 21)
				| (operands[0] << 16) | (1 << 11);
		    }
		    if (insPtr->flags & (BRANCH_L|BRANCH_G)) {
			codePtr[1] = BNE_OP | (1 << 21) | (disp & 0xffff);
		    } else {
			codePtr[1] = BEQ_OP | (1 << 21) | (disp & 0xffff);
		    }
		} else {
		    if (insPtr->flags & (BRANCH_LE|BRANCH_G)) {
			if (operands[1] == 0x7fff) {
			    p = argStart[1];
			    errMsg = "immediate operand out of rangs";
			    goto error;
			}
			codePtr[0] = insPtr->other | (operands[0] << 21)
				| (1 << 16) | ((operands[1]+1) & 0xffff);
		    } else {
			codePtr[0] = insPtr->other | (operands[0] << 21)
				| (1 << 16) | (operands[1] & 0xffff);
		    }
		    if (insPtr->flags & (BRANCH_L|BRANCH_LE)) {
			codePtr[1] = BNE_OP | (1 << 21) | (disp & 0xffff);
		    } else {
			codePtr[1] = BEQ_OP | (1 << 21) | (disp & 0xffff);
		    }
		}
		*sizePtr = 2;
	    } else {
		codePtr[0] = insPtr->op | (disp & 0xffff) | (operands[0] << 21);
	    }
	    break;
	}

	case JUMP:
	    if (isReg[0]) {
#ifdef STRICT
		p = argStart[0];
		errMsg = "register operand illegal (must be address)";
		goto error;
#else
		codePtr[0] = insPtr->other | (operands[0] << 21);
#endif
	    } else {
		if (operands[0] & 0x3) {
		    p = argStart[0];
		    errMsg = "jump target not word-aligned";
		    goto error;
		}
		if ((operands[0] & 0xf0000000) != (dot & 0xf0000000)) {
		    p = argStart[0];
		    errMsg = "jump target too far away";
		    goto error;
		}
		codePtr[0] = insPtr->op | ((operands[0] >> 2) & 0x3ffffff);
	    }
	    break;

	case JALR:
	    if (!isReg[0]) {
		p = argStart[0];
		goto regRequired;
	    }
	    if (numOps == 1) {
		codePtr[0] = insPtr->op | (31 << 11) | (operands[0] << 21);
	    } else {
		if (!isReg[1]) {
		    p = argStart[1];
		    goto regRequired;
		}
		codePtr[0] = insPtr->op | (operands[0] << 11)
			| (operands[1] << 21);
	    }
	    break;

	case SRC1:
	    if (!isReg[0]) {
		p = argStart[0];
		goto regRequired;
	    }
	    codePtr[0] = insPtr->op | (operands[0] << 21);
	    break;

	case DEST:
	    if (!isReg[0]) {
		p = argStart[0];
		goto regRequired;
	    }
	    codePtr[0] = insPtr->op | (operands[0] << 11);
	    break;

	case MOVE:
	    if (!isReg[0]) {
		p = argStart[0];
		goto regRequired;
	    }
	    if (!isReg[1]) {
		p = argStart[1];
		goto regRequired;
	    }
	    codePtr[0] = insPtr->op | (operands[0] << 16) | (operands[1] << 21);
	    break;

	case MTC:
	    if (!isReg[0]) {
		p = argStart[0];
		goto regRequired;
	    }
	    if (!isReg[1]) {
		p = argStart[1];
		goto regRequired;
	    }
	    codePtr[0] = insPtr->op | (operands[0] << 16) | (operands[1] << 11);
	    break;

	default:
	    errMsg = "internal error:  unknown class for instruction";
	    goto error;
    }

    /*
     * Make sure that there's no garbage left on the line after the
     * instruction.
     */

    while (isspace(*p)) {
	p++;
    }
    if ((*p != 0) && (*p != '#')) {
	errMsg = "extra junk at end of line";
	goto error;
    }
    return TCL_OK;

    /*
     * Generate a reasonably-human-understandable error message.
     */

    error:
    IndicateError(machPtr->interp, errMsg, wholeLine, p);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Asm_Disassemble --
 *
 *	Given an instruction, return a string describing the instruction
 *	in assembler format.
 *
 * Results:
 *	The return value is a string, which either describes the
 *	instruction or contains a message saying that the instruction
 *	didn't make sense.  The string is statically-allocated, meaning
 *	that it will change on the next call to this procedure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Asm_Disassemble(machPtr, ins, pc)
    R2000 *machPtr;		/* Machine to use for symbol table info. */
    int ins;			/* The contents of the instruction. */
    unsigned int pc;		/* Memory address at which instruction is
				 * located. */
{
    register OpcodeInfo *opPtr;
    OpcodeInfo *bestPtr;
    static char string[200];
    int field, bestMask;

    /*
     * Match this instruction against our instruction table to find
     * out what instruction it is.  Look
     */

    for (bestMask = 0, opPtr = opcodes; opPtr->name != NULL; opPtr++) {
	if (opPtr->mask == 0) {
	    continue;
	}
	if ((ins & opPtr->mask) != (opPtr->op)) {
	    continue;
	}
	if ((bestMask & opPtr->mask) != opPtr->mask) {
	    bestMask = opPtr->mask;
	    bestPtr = opPtr;
	}
    }
    if (bestMask == 0) {
	sprintf(string, "unrecognized instruction (0x%x)", ins);
	return string;
    }
    opPtr = bestPtr;

    /*
     * Dispatch on the type of the instruction.
     */

    switch (opPtr->class) {
	case NO_ARGS:
	    sprintf(string, "%s", opPtr->name);
	    break;

	case LOAD_STORE:
	    field = ins & 0xffff;
	    if (field & 0x8000) {
		field |= 0xffff0000;
	    }
	    sprintf(string, "%s %s,%s(%s)", opPtr->name,
		    Asm_RegNames[(ins >> 16) & 0x1f],
		    Sym_GetString(machPtr, (unsigned) field),
		    Asm_RegNames[(ins >> 21) & 0x1f]);
	    break;

	case BREAK:
	    field = (ins >> 6) & 0xfffff;
	    sprintf(string, "%s 0x%x", opPtr->name, field);
	    break;

	case LUI:
	    field = ins & 0xffff;
	    sprintf(string, "%s %s,0x%x", opPtr->name, 
		    Asm_RegNames[(ins >> 16) & 0x1f], field);
	    break;

	case ARITH:
	case NOR:
	case SUB:
	    if (opPtr->flags & IMMEDIATE_REQ) {
		field = ins & 0xffff;
		sprintf(string, "%s %s,%s,0x%x", opPtr->name,
			Asm_RegNames[(ins >> 16) & 0x1f],
			Asm_RegNames[(ins >> 21) & 0x1f], field);
	    } else {
		sprintf(string, "%s %s,%s,%s", opPtr->name,
			Asm_RegNames[(ins >> 11) & 0x1f],
			Asm_RegNames[(ins >> 21) & 0x1f],
			Asm_RegNames[(ins >> 16) & 0x1f]);
	    }
	    break;

	case MULT:
	case MULDIV:
	    sprintf(string, "%s %s,%s", opPtr->name,
		    Asm_RegNames[(ins >> 21) & 0x1f],
		    Asm_RegNames[(ins >> 16) & 0x1f]);
	    break;

	case SHIFT:
	    if (opPtr->other == ALWAYS_VAR) {
		sprintf(string, "%s %s,%s,%s", opPtr->name,
			Asm_RegNames[(ins >> 11) & 0x1f],
			Asm_RegNames[(ins >> 16) & 0x1f],
			Asm_RegNames[(ins >> 21) & 0x1f]);
	    } else {
		field = (ins >> 6) & 0x1f;
		sprintf(string, "%s %s,%s,%d", opPtr->name,
			Asm_RegNames[(ins >> 11) & 0x1f],
			Asm_RegNames[(ins >> 16) & 0x1f], field);
	    }
	    break;

	case BRANCH:
	case BRANCH_EQ:
	case BRANCH_1_OP:
	    field = (ins & 0xffff);
	    if (field & 0x8000) {
		field |= 0xffff0000;
	    }
	    field = (field << 2) + pc + 4;
	    if (opPtr->class == BRANCH_EQ) {
		sprintf(string, "%s %s,%s,%s", opPtr->name,
			Asm_RegNames[(ins >> 21) & 0x1f],
			Asm_RegNames[(ins >> 16) & 0x1f],
			Sym_GetString(machPtr, (unsigned) field));
	    } else if (opPtr->class == BRANCH_1_OP) {
		sprintf(string, "%s %s,%s", opPtr->name,
			Asm_RegNames[(ins >> 21) & 0x1f],
			Sym_GetString(machPtr, (unsigned) field));
	    } else {
		sprintf(string, "%s %s", opPtr->name,
			Sym_GetString(machPtr, (unsigned) field));
	    }
	    break;

	case JUMP:
	    field = ((ins & 0x3ffffff) << 2) | (pc & 0xf0000000);
	    sprintf(string, "%s %s", opPtr->name,
		    Sym_GetString(machPtr, (unsigned) field));
	    break;

	case JALR:
	    sprintf(string, "%s %s,%s", opPtr->name,
		    Asm_RegNames[(ins >> 11) & 0x1f],
		    Asm_RegNames[(ins >> 21) & 0x1f]);
	    break;

	case SRC1:
	    sprintf(string, "%s %s", opPtr->name,
		    Asm_RegNames[(ins >> 21) & 0x1f]);
	    break;

	case DEST:
	    sprintf(string, "%s %s", opPtr->name,
		    Asm_RegNames[(ins >> 11) & 0x1f]);
	    break;

	case MTC:
	    sprintf(string, "%s %s,%s", opPtr->name,
		    Asm_RegNames[(ins >> 16) & 0x1f],
		    Asm_RegNames[(ins >> 11) & 0x1f]);
	    break;

	default:
	    sprintf(string, "instruction confused dis-assembler (0x%x)", ins);
	    break;
    }
    return string;
}

/*
 *----------------------------------------------------------------------
 *
 * Asm_AsmCmd --
 *
 *	This procedure is invoked to process the "asm" Tcl command.
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

/* ARGSUSED */
int
Asm_AsmCmd(machPtr, interp, argc, argv)
    R2000 *machPtr;			/* Machine description. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int result;
    unsigned int pc;
    char *end;
    int size, code[ASM_MAX_WORDS];

    if ((argc != 2) && (argc != 3)) {
	sprintf(interp->result,
		"wrong # args:  should be \"%.50s\" instruction [pc]",
		argv[0]);
	return TCL_ERROR;
    }

    if (argc == 3) {
	pc = strtoul(argv[2], &end, 0);
	if ((*end != 0) || (end == argv[2])) {
	    sprintf(interp->result, "bad pc \"%.50s\"", argv[2]);
	    return TCL_ERROR;
	}
    } else {
	pc = 0;
    }

    result = Asm_Assemble(machPtr, (char *) NULL, argv[1], pc, argv[1], 1,
	    &size, code);
    if (result != TCL_OK) {
	return result;
    }
    sprintf(interp->result, "0x%x", code[0]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Asm_LoadCmd --
 *
 *	This procedure is invoked to process the "load" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl return result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
int
Asm_LoadCmd(machPtr, interp, argc, argv)
    R2000 *machPtr;		/* Machine whose memory should be loaded. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Count of number of files in argv. */
    char **argv;		/* Array containing names of files to
				 * assemble. */
{
    unsigned int codeStart, dataStart;
    char *p, *end;
    LoadInfo info;
    int i;

    /*
     * Figure out the starting addresses for code and data (check for
     * variables "codeStart" and "dataStart", and use their values if
     * they're defined;  otherwise use defaults).
     */

    codeStart = 0x100;
    p = Tcl_GetVar(machPtr->interp, "codeStart", 1);
    if (p != NULL) {
	codeStart = strtoul(p, &end, 0);
	if (*end != 0) {
	    sprintf(machPtr->interp->result,
		    "\"codeStart\" variable doesn't contain an address: \"%.50s\"",
		    p);
	    return TCL_ERROR;
	}
    }
    dataStart = 0x1000;
    p = Tcl_GetVar(machPtr->interp, "dataStart", 1);
    if (p != NULL) {
	dataStart = strtoul(p, &end, 0);
	if (*end != 0) {
	    sprintf(machPtr->interp->result,
		    "\"dataStart\" variable doesn't contain an address: \"%.50s\"",
		    p);
	    return TCL_ERROR;
	}
    }

    /*
     * Pass 1: delete old symbol definitions.
     */

    for (i = 1; i < argc; i++) {
	Sym_DeleteSymbols(machPtr, argv[i]);
    }

    /*
     * Pass 2:  read through all of the files to build the symbol table.
     */

    info.codeAddr = codeStart;
    info.dataAddr = dataStart;
    info.message = NULL;
    info.end = NULL;
    info.totalBytes = 0;
    info.errorCount = 0;
    info.flags = ASM_SIZE_ONLY;
    for (i = 1; i < argc; i++) {
	ReadFile(argv[i], machPtr, &info);
    }

    /*
     * Pass 3: read through the files a second time to actually assemble
     * the code.
     */

    info.codeAddr = codeStart;
    info.dataAddr = dataStart;
    info.flags = 0;
    for (i = 1; i < argc; i++) {
	ReadFile(argv[i], machPtr, &info);
	if (info.errorCount > ASM_MAX_ERRORS) {
	    break;
	}
    }

    if (info.message == NULL) {
	return TCL_OK;
    }
    Tcl_Return(machPtr->interp, info.message, TCL_DYNAMIC);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ReadFile --
 *
 *	Read in an assembler file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets loaded into *machPtr's memory, and *infoPtr
 *	gets modified (to point to an error message, for example).
 *
 *----------------------------------------------------------------------
 */

static void
ReadFile(fileName, machPtr, infoPtr)
    char *fileName;		/* Name of assembler file to read. */
    R2000 *machPtr;		/* Machine into whose memory the information
				 * is to be loaded. */
    register LoadInfo *infoPtr;	/* Information about the state of the
				 * assembly process. */
{
#define MAX_LINE_SIZE 200
#define MAX_NAME_SIZE 10
    char line[MAX_LINE_SIZE];
    char pseudoOp[MAX_NAME_SIZE+1];
    FILE *f;
    register char *p;
    int i, nullTerm;
    char *end, *curToken;
    char savedChar;

    f = fopen(fileName, "r");
    if (f == NULL) {
	if (infoPtr->flags & ASM_SIZE_ONLY) {
	    return;
	}
	sprintf(machPtr->interp->result,
		"couldn't open file \"%.50s\": %.100s", fileName,
		strerror(errno));
	AddErrMsg(machPtr->interp, infoPtr, 0);
	return;
    }

    /*
     * Process the file one line at a time.
     */

    infoPtr->file = fileName;
    infoPtr->dot = (infoPtr->codeAddr + 3) & ~3;
    infoPtr->flags |= ASM_CODE;
    for (infoPtr->lineNum = 1; ; infoPtr->lineNum++) {
	infoPtr->line = fgets(line, MAX_LINE_SIZE, f);
	if (infoPtr->line == NULL) {
	    if (!feof(f)) {
		sprintf(machPtr->interp->result, "error reading file: %.100s",
			strerror(errno));
		AddErrMsg(machPtr->interp, infoPtr, 1);
	    }
	    break;
	}

	/*
	 * Skip leading blanks.
	 */

	for (p = line; (*p == ' ') || (*p == '\t'); p++) {
	    /* Null body:  just skip spaces. */
	}

	/*
	 * Parse off an optional symbol at the beginning of the line.
	 * Note:  force symbol-related error messages to be output
	 * during pass 1, even though most other error messages get
	 * ignored during pass 1.
	 */

	if (isalpha(*p) || (*p == '_') || (*p == '$')) {
	    curToken = p;
	    for (p++; isalnum(*p) || (*p == '_') || (*p == '$'); p++) {
		/* Null body:  just skip past symbol. */
	    }
	    if (*p == ':') {
		*p = 0;
		if (infoPtr->flags & ASM_SIZE_ONLY) {
		    Sym_AddSymbol(machPtr, fileName, curToken, infoPtr->dot,
			    0);
		    if (*machPtr->interp->result != 0) {
			AddErrMsg(machPtr->interp, infoPtr, 1);
		    }
		}
		*p = ':';
		p++;
		while ((*p == ' ') || (*p == '\t')) {
		    p++;
		}
	    } else {
		p = curToken;
	    }
	}

	/*
	 * Skip empty lines.
	 */

	if ((*p == '\n') || (*p == 0)) {
	    continue;
	}

	/*
	 * If this isn't an assembler pseudo-op, just assemble the
	 * instruction and move on.
	 */

	while ((*p == ' ') || (*p == '\t')) {
	    p++;
	}
	if (*p == '#') {
	    continue;
	}
	if (*p != '.') {
	    int size, code[ASM_MAX_WORDS], result;

	    infoPtr->dot = (infoPtr->dot + 3) & ~3;
	    result = Asm_Assemble(machPtr, fileName, p, infoPtr->dot,
		    line, infoPtr->flags & ASM_SIZE_ONLY, &size, code);
	    if (result == TCL_OK) {
		result = StoreWords(machPtr, infoPtr->dot, code, size);
	    }
	    infoPtr->dot += size*4;
	    goto endOfLine;
	}

	/*
	 * Handle an assembler pseudo-op.
	 */

	curToken = p;
	for (i = 0, p++; (i < MAX_NAME_SIZE) && isalpha(*p); i++, p++) {
	    pseudoOp[i] = *p;
	}
	if (i >= MAX_NAME_SIZE) {
	    IndicateError(machPtr->interp, "pseudo-op name too long",
		    line, curToken);
	    goto endOfLine;
	}
	pseudoOp[i] = 0;
	while ((*p == ' ') || (*p == '\t')) {
	    p++;
	}
	if ((pseudoOp[0] == 'a') && (strcmp(pseudoOp, "align") == 0)) {
	    if (Sym_EvalExpr(machPtr, fileName, p, 0, &i, &end) != TCL_OK) {
		IndicateError(machPtr->interp, machPtr->interp->result,
			line, p);
		goto endOfLine;
	    }
	    p = end;
	    if (i == 0) {
		machPtr->interp->result = "\".align 0\" not supported";
		goto endOfLine;
	    } else {
		i = (1 << i) - 1;
		infoPtr->dot = (infoPtr->dot + i) & ~i;
	    }
	} else if ((pseudoOp[0] == 'a') && (strcmp(pseudoOp, "ascii") == 0)) {
	    nullTerm = 0;

	    /*
	     * Read one or more ASCII strings from the input line.  Each
	     * must be surrounded by quotes, and they must be separated
	     * by commas.
	     */

	    doString:
	    while (1) {
		while ((*p == ' ') || (*p == '\t')) {
		    p++;
		}
		if (*p != '"') {
		    IndicateError(machPtr->interp,
			    "missing \" at start of string", line, p);
		    goto endOfLine;
		}
		p++;
		i = Gp_PutString(machPtr, p, '"', infoPtr->dot, nullTerm, &end);
		if (*end != '"') {
		    IndicateError(machPtr->interp,
			    "missing \" at end of string", line, end-1);
		    goto endOfLine;
		}
		p = end+1;
		infoPtr->dot += i;
		while ((*p == ' ') || (*p == '\t')) {
		    p++;
		}
		if (*p != ',') {
		    break;
		}
		p++;
	    }
	} else if ((pseudoOp[0] == 'a') && (strcmp(pseudoOp, "asciiz") == 0)) {
	    nullTerm = 1;
	    goto doString;
	} else if ((pseudoOp[0] == 'b') && (strcmp(pseudoOp, "byte") == 0)) {
	    while (1) {
		curToken = p;
		if (Sym_EvalExpr(machPtr, fileName, p, 0, &i, &end)
			!= TCL_OK) {
		    IndicateError(machPtr->interp, machPtr->interp->result,
			    line, p);
		    goto endOfLine;
		}
		Gp_PutByte(machPtr, infoPtr->dot, i);
		infoPtr->dot += 1;
		for (p = end; (*p == ' ') || (*p == '\t'); p++) {
		    /* Null body;  just skip space. */
		}
		if (*p != ',') {
		    break;
		}
		p++;
	    }
	} else if ((pseudoOp[0] == 'd') && (strcmp(pseudoOp, "data") == 0)) {
	    if (infoPtr->flags & ASM_CODE) {
		infoPtr->codeAddr = infoPtr->dot;
	    } else {
		infoPtr->dataAddr = infoPtr->dot;
	    }
	    if (Sym_EvalExpr(machPtr, fileName, p, 0, &i, &end) != TCL_OK) {
		Tcl_Return(machPtr->interp, (char *) NULL, TCL_STATIC);
	    } else {
		p = end;
		infoPtr->dataAddr = i;
	    }
	    infoPtr->dot = infoPtr->dataAddr;
	    infoPtr->flags &= ~ASM_CODE;
	} else if ((pseudoOp[0] == 'g') && (strcmp(pseudoOp, "globl") == 0)) {
	    if (!isalpha(*p)) {
		IndicateError(machPtr->interp,
			"symbol name must start with letter", line, p);
		goto endOfLine;
	    }
	    curToken = p;
	    while (isalnum(*p) || (*p == '_') || (*p == '$')) {
		p++;
	    }
	    savedChar = *p;
	    *p = 0;
	    if (infoPtr->flags & ASM_SIZE_ONLY) {
		Sym_AddSymbol(machPtr, fileName, curToken, 0,
			SYM_GLOBAL|SYM_NO_ADDR);
		if (*machPtr->interp->result != 0) {
		    AddErrMsg(machPtr->interp, infoPtr, 1);
		}
	    }
	    *p = savedChar;
	} else if ((pseudoOp[0] == 's') && (strcmp(pseudoOp, "space") == 0)) {
	    if (Sym_EvalExpr(machPtr, fileName, p, 0, &i, &end) != TCL_OK) {
		IndicateError(machPtr->interp, machPtr->interp->result,
			line, p);
		goto endOfLine;
	    }
	    p = end;
	    while (i > 0) {
		Gp_PutByte(machPtr, infoPtr->dot, 0);
		infoPtr->dot += 1;
		i -= 1;
	    }
	} else if ((pseudoOp[0] == 't') && (strcmp(pseudoOp, "text") == 0)) {
	    if (infoPtr->flags & ASM_CODE) {
		infoPtr->codeAddr = infoPtr->dot;
	    } else {
		infoPtr->dataAddr = infoPtr->dot;
	    }
	    if (Sym_EvalExpr(machPtr, fileName, p, 0, &i, &end) != TCL_OK) {
		Tcl_Return(machPtr->interp, (char *) NULL, TCL_STATIC);
	    } else {
		p = end;
		infoPtr->codeAddr = i;
	    }
	    infoPtr->dot = infoPtr->codeAddr;
	    infoPtr->flags |= ASM_CODE;
	} else if ((pseudoOp[0] == 'w') && (strcmp(pseudoOp, "word") == 0)) {
	    while (1) {
		curToken = p;
		if (Sym_EvalExpr(machPtr, fileName, p, 0, &i, &end)
			!= TCL_OK) {
		    IndicateError(machPtr->interp, machPtr->interp->result,
			    line, p);
		    goto endOfLine;
		}
		infoPtr->dot = (infoPtr->dot + 3) & ~3;
		(void) StoreWords(machPtr, infoPtr->dot, &i, 1);
		infoPtr->dot += 4;
		for (p = end; (*p == ' ') || (*p == '\t'); p++) {
		    /* Null body;  just skip space. */
		}
		if (*p != ',') {
		    break;
		}
		p++;
	    }
	} else {
	    IndicateError(machPtr->interp, "unknown pseudo-op", line,
		    curToken);
	    goto endOfLine;
	}

	/*
	 * Check for extraneous garbage at the end of the line.
	 */

	while (isspace(*p)) {
	    p++;
	}
	if ((*p != '#') && (*p != 0)) {
	    IndicateError(machPtr->interp, "extra junk at end of line",
		    line, p);
	}

	/*
	 * Done with the line.  If there has been an error, add it onto
	 * the list of error messages that has accumulated during the
	 * assembly.  Increase the storage allocated to error messages
	 * if necessary to accommodate the new message.
	 */

	endOfLine:
	if (*machPtr->interp->result != 0) {
	    if (infoPtr->flags & ASM_SIZE_ONLY) {
		Tcl_Return(machPtr->interp, (char *) NULL, TCL_STATIC);
	    } else {
		AddErrMsg(machPtr->interp, infoPtr, 1);
		if (infoPtr->errorCount > ASM_MAX_ERRORS) {
		    goto endOfFile;
		}
	    }
	}
    }

    endOfFile:
    fclose(f);
    if (infoPtr->flags & ASM_CODE) {
	infoPtr->codeAddr = infoPtr->dot;
    } else {
	infoPtr->dataAddr = infoPtr->dot;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * AddErrMsg --
 *
 *	Given an error message in an interpreter, add it onto a list of
 *	error messages being accumulated for an assembly and clear the
 *	interpreter's message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The message is added to the list of messages in infoPtr, and
 *	the interpreter's result is re-initialized.
 *
 *----------------------------------------------------------------------
 */

static void
AddErrMsg(interp, infoPtr, addHeader)
    Tcl_Interp *interp;			/* Interpreter containing error
					 * message. */
    register LoadInfo *infoPtr;		/* State of assembly, to which error
					 * message is to be added. */
    int addHeader;			/* Non-zero means tack on message
					 * header identifying file and line
					 * number. */
{
    int length, hdrLength, totalLength;
    char header[100];

    length = strlen(interp->result);
    if (length == 0) {
	return;
    }
    if (addHeader) {
	sprintf(header, "%.50s(%d): ", infoPtr->file,
		infoPtr->lineNum);
    } else {
	header[0] = 0;
    }
    hdrLength = strlen(header);
    totalLength = hdrLength + length + 2;

    /*
     * Grow the error message area if the current area isn't large
     * enough.
     */

    if (totalLength > ((infoPtr->message + infoPtr->totalBytes)
	    - (infoPtr->end + 1))) {
	char *newMsg;

	if (infoPtr->totalBytes == 0) {
	    infoPtr->totalBytes = 4*totalLength;
	} else {
	    infoPtr->totalBytes = 2*(infoPtr->totalBytes + totalLength);
	}
	newMsg = malloc((unsigned) infoPtr->totalBytes);
	if (infoPtr->message != NULL) {
	    strcpy(newMsg, infoPtr->message);
	    infoPtr->end += newMsg - infoPtr->message;
	} else {
	    infoPtr->end = newMsg;
	}
	infoPtr->message = newMsg;
    }
    if (infoPtr->end != infoPtr->message) {
	*infoPtr->end = '\n';
	infoPtr->end += 1;
    }
    sprintf(infoPtr->end, "%s%s", header, interp->result);
    infoPtr->end += hdrLength + length;
    infoPtr->errorCount += 1;

    Tcl_Return(interp, (char *) NULL, TCL_STATIC);
}

/*
 *----------------------------------------------------------------------
 *
 * StoreBytes --
 *
 *	Place a given range of bytes in the memory of a machine.
 *
 * Results:
 *	A standard Tcl result (normally TCL_OK plus empty string);  error
 *	information is returned through machPtr->interp.
 *
 * Side effects:
 *	MachPtr's memory is modified to hold new information.
 *
 *----------------------------------------------------------------------
 */

static int
StoreWords(machPtr, address, wordPtr, numWords)
    register R2000 *machPtr;		/* Machine into which to store. */
    unsigned int address;		/* Word-aligned byte address in
					 * machine's memory. */
    int *wordPtr;			/* Words to store into machine's
					 * memory. */
    int numWords;			/* Number of words to store. */
{
    int index;
    register MemWord *memPtr;

    for ( ; numWords > 0; wordPtr++, address += 4, numWords--) {
	index = ADDR_TO_INDEX(address);
	if (index >= machPtr->numWords) {
	    sprintf(machPtr->interp->result,
		    "can't store at address 0x%x:  no such memory location",
		    address);
	    return TCL_ERROR;
	}
	memPtr = machPtr->memPtr + index;
	memPtr->value = *wordPtr;
	memPtr->opCode = OP_NOT_COMPILED;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * IndicateError --
 *
 *	Generate an error message that also points out the position
 *	in a string where the error was detected.
 *
 * Results:
 *	There is no return value.  Interp's result is modified to hold
 *	errMsg followed by string, with position pos highlighted in
 *	string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
IndicateError(interp, errMsg, string, pos)
    Tcl_Interp *interp;		/* Interpreter to hold error message.  The
				 * result area must be in the initial
				 * empty state. */
    char *errMsg;		/* Message describing the problem. */
    char *string;		/* Input string that contained the problem. */
    char *pos;			/* Location in string of the character where
				 * problem was detected. */
{
    int msgLength, stringLength;
    char *newMsg;

    msgLength = strlen(errMsg);
    stringLength = strlen(string);
    if (string[stringLength-1] == '\n') {
	stringLength -= 1;
    }

    /*
     * Always allocate new storage for the new message.  This is needed
     * because (a) the space required may exceed the size of the static
     * result buffer, and (b) "errMsg" may actually be in the static
     * buffer so we have to be careful not to trash it while generating
     * the new message.
     */

    newMsg = malloc((unsigned) (msgLength + stringLength + 10));
    sprintf(newMsg, "%s: %.*s => %.*s", errMsg, pos-string,
	    string, stringLength - (pos-string), pos);
    Tcl_Return(interp, newMsg, TCL_DYNAMIC);
}
