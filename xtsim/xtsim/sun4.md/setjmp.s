/* 
 * setjmp.s --
 *
 *	setjmp/longjmp routines for SUN4.
 *
 * Copyright 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * static char rcsid[] = "$Header: machCCRegs.s,v 1.1 88/06/15 14:18:30 mendel E
xp $ SPRITE (Berkeley)";
 *
 */

 /*
  * Define offsets in the jmp_buf block.
  */

#define	SIGMASK_OFFSET	0
#define	RTNPC_OFFSET	4
#define	SP_OFFSET	8
#define	FP_OFFSET	12
/* 
 *----------------------------------------------------------------------
 *
 * setjmp/_setjmp --
 *
 *	setjmp and _setjmp routines for SUN4.
 *
 * Results:
 *	An integer 0.
 *
 * Side effects:
 *	None.
 *
 * Calling Sequence:
 *	int val = setjmp(env) or val = _setjmp(env)
 *	jmp_buf		env;
 *
 *----------------------------------------------------------------------
 * 
 */

.text
	.align 2
.globl _Setjmp
_Setjmp:
        /*
         * Save our save pointer and return address in the jmp_buf for
         * use by longjmp.
         */
        st      %sp, [%o0 + SP_OFFSET]
        st      %fp, [%o0 + FP_OFFSET]
        st      %o7, [%o0 + RTNPC_OFFSET]
	/*
	 * Flush register windows.
	 */
	save	%sp,-64,%sp
	save	%sp,-64,%sp
	save	%sp,-64,%sp
	save	%sp,-64,%sp
	save	%sp,-64,%sp
	save	%sp,-64,%sp
	save	%sp,-64,%sp
	restore
	restore
	restore
	restore
	restore
	restore
	restore
        /*
         * Return a 0 like a good setjmp should.
         */
        retl
        mov     0,%o0

/* 
 *----------------------------------------------------------------------
 *
 * longjmp/_longjmp --
 *
 *	longjmp and _longjmp routines for SUN4.
 *
 * Results:
 *	Doesn't return normally.
 *
 * Side effects:
 *	Returns to the specified setjmp/_setjmp call.
 * 	longjmp restores the signal mask.
 *
 * Calling Sequence:
 *	longjmp(env,val) or  _setjmp(env,val)
 *	jmp_buf		env;
 *	int	val;
 *
 *----------------------------------------------------------------------
 * 
 */

	.align 2
.globl _Longjmp
_Longjmp:
        /*
         * Fake togther a call frame that can "return" in to the
         * setjmp call.
         */
        ld      [%o0 + SP_OFFSET], %fp
        sub     %fp, 64, %sp
        ld      [%o0 + RTNPC_OFFSET], %i7

        mov     %o0,%i1
        mov     %o1,%i0
        restore
        retl
        ld      [%o1 + FP_OFFSET], %fp



