/* switch.s -- Machine dependent routines.  DO NOT TOUCH THESE! */

#define ASSEMBLER

#include "asm.h"
#include "switch.h"

/* Symbolic register names */
#define a0	$4	/* argument registers */
#define a1	$5
#define s0	$16	/* callee saved */
#define s1	$17
#define s2	$18
#define s3	$19
#define s4	$20
#define s5	$21
#define s6	$22
#define s7	$23
#define sp	$29	/* stack pointer */
#define fp	$30	/* frame pointer */
#define ra	$31	/* return address */

        .text   
        .align  2

LEAF(ThreadRoot)
	jal	StartupPC
	move	a0, InitialArg
	jal	InitialPC			/* call procedure */
	jal 	WhenDonePC			/* when we're done, clean up */

	# NOT REACHED
	END(ThreadRoot)

	# a0 -- pointer to old Thread
	# a1 -- pointer to new Thread
LEAF(SWITCH)
	sw	sp, SP(a0)		# save new stack pointer
	sw	s0, S0(a0)
	sw	s1, S1(a0)
	sw	s2, S2(a0)
	sw	s3, S3(a0)
	sw	s4, S4(a0)
	sw	s5, S5(a0)
	sw	s6, S6(a0)
	sw	s7, S7(a0)
	sw	fp, FP(a0)		# save return address
	sw	ra, PC(a0)		# save return address
	
	lw	sp, SP(a1)		# t0 contains new sp
	lw	s0, S0(a1)
	lw	s1, S1(a1)
	lw	s2, S2(a1)
	lw	s3, S3(a1)
	lw	s4, S4(a1)
	lw	s5, S5(a1)
	lw	s6, S6(a1)
	lw	s7, S7(a1)
	lw	fp, FP(a1)
	lw	ra, PC(a1)		# save return address	

	j	ra
	END(SWITCH)
