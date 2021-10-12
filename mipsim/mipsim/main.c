/* 
 * main.c --
 *
 *	Main program for "mipsim" simulator for R2000 architecture.
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
static char rcsid[] = "$Header: /user1/ouster/mipsim/RCS/main.c,v 1.9 91/01/10 11:39:11 ouster Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <tcl.h>
#include "mips.h"

static Tcl_Interp *interp;
static R2000 *machPtr;

/*
 * Forward references to procedures declared later in this file:
 */

static void Interrupt();

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	Top-level procedure for mipsim simulator.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tons:  read the user manual for details.
 *
 *----------------------------------------------------------------------
 */

main()
{
#define LINE_SIZE 200
    char line[LINE_SIZE];
    char tmp[200];
    char *cmd, *p;
    int c, result, gotPartial;
    Tcl_CmdBuf buffer;

    interp = Tcl_CreateInterp();
    machPtr = Sim_Create(4096, interp);
    (void) signal(SIGINT, Interrupt);

    /*
     * Read a ".mipsim" file if one exists.  Check first in the home
     * directory, then in the current directory.
     */

    p = getenv("HOME");
    if (p != NULL) {
	sprintf(tmp, "%.150s/.mipsim", p);
	if (access(tmp, R_OK) == 0) {
	    sprintf(tmp, "source %.150s/.mipsim", p);
	    result = Tcl_Eval(interp, tmp, 0, (char **) 0);
	    if (*interp->result != 0) {
		printf("%s\n", interp->result);
	    }
	}
    }
    if (access(".mipsim", R_OK) == 0) {
	struct stat homeStat, cwdStat;

	/*
	 * Don't process the .mipsim file in the current directory if
	 * the current directory is the same as the home directory:
	 * it will already have been processed above.
	 */

	(void) stat(p, &homeStat);
	(void) stat(".", &cwdStat);
	if (bcmp((char *) &homeStat, (char *) &cwdStat,
		sizeof(cwdStat))) {
	    result = Tcl_Eval(interp, "source .mipsim", 0, (char **) 0);
	    if (*interp->result != 0) {
		printf("%s\n", interp->result);
	    }
	}
    }

    buffer = Tcl_CreateCmdBuf();
    while (1) {
	clearerr(stdin);
	p = Tcl_GetVar(interp, "prompt", 1);
	if ((p == NULL) || (Tcl_Eval(interp, p, 0, (char **) 0) != TCL_OK)) {
	    fputs("(mipsim) ", stdout);
	} else {
	    fputs(interp->result, stdout);
	}
	fflush(stdout);

	if (fgets(line, LINE_SIZE, stdin) == NULL) {
	    if (!gotPartial) {
		exit(0);
	    }
	    line[0] = 0;
	}
	cmd = Tcl_AssembleCmd(buffer, line);
	if (cmd == NULL) {
	    gotPartial = 1;
	    continue;
	}

	gotPartial = 0;
	result = Tcl_RecordAndEval(interp, cmd, 0);
	if (*interp->result != 0) {
	    printf("%s\n", interp->result);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Interrupt --
 *
 *	This procedure is invoked when the interrupt key is typed:
 *	it causes the simulation to stop.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Causes simultor to stop after next instruction.
 *
 *----------------------------------------------------------------------
 */

static void
Interrupt()
{
    Sim_Stop(machPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Main_QuitCmd --
 *
 *	This procedure is invoked to process the "quit" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	None:  this command never returns.
 *
 * Side effects:
 *	The program exits.
 *
 *----------------------------------------------------------------------
 */

/* ARGSUSED */
int
Main_QuitCmd(machPtr, interp, argc, argv)
    R2000 *machPtr;			/* Machine description. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    exit(0);
    return TCL_OK;			/* Never gets executed. */
}
