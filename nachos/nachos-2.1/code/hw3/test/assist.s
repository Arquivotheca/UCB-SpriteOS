/* simple assembly language assist for making system calls from user programs */

#include "asm.h"

LEAF(Syscall)
	syscall
	j	$31
 	END(Syscall)
