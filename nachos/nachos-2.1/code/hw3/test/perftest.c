/* Program to test multiprogramming performance */

/* gcc -c -G 0 perftest.c; ld -N -T 0 perftest.o -o perftest; 
	coff2flat perftest perftest.flat */

#include "syscall.h"

int
main()
{
    AddrSpace addr1 = Exec("io", 0, (char **)NULL);
    AddrSpace addr2 = Exec("cpu", 0, (char **)NULL);

    Wait(addr1);
    Wait(addr2);
    Syscall(SC_Halt);		/* and then we're done */
    Syscall(SC_Exit);		/* not reached */
}
