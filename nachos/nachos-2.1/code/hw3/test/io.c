/* Simple I/O-bound program -- UNIX echo */

/* gcc -c -G 0 io.c; ld -N -T 0 io.o -o io; coff2flat io io.flat */

#include "syscall.h"

int
main()
{
    char ch;

    for (int i = 0; i < 1000; i++) {
	ch = ConsoleRead();
	ConsoleWrite(ch);
    }
    Syscall(SC_Exit);
}
