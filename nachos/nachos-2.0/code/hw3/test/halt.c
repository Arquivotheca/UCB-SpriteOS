/* Simple program to shut down the OS */

/* gcc -c -G 0 halt.c; ld -N -T 0 halt.o -o halt; coff2flat halt halt.flat */

#include "syscall.h"

int
main()
{
    Syscall(SC_Halt);
    Syscall(SC_Exit);	/* shouldn't be reached */
}
