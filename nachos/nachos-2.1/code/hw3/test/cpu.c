/* Simple CPU-bound program */

/* gcc -c -G 0 cpu.c; ld -N -T 0 cpu.o -o cpu; coff2flat cpu cpu.flat */

#include "syscall.h"

int
main()
{
    int i, count;

    for (count = i = 0; i < 10000; i++)
	count += i;
    Syscall(SC_Exit);
}
