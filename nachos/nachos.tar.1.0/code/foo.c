
/* gcc -c -G 0 foo.c; ld -N -T 0 foo.o -o foo; out foo */

#include "user_syscalls.h"

/* There is a problem with routines that call subroutines.   Only use
 * leaf procedures.
 */

#if exec_test
int
main()
{
    int ret;
    char* s1 = "here i am";
    int n1 = 10;
    char* foo = "foo";
    
    consoleWrite(s1, n1, ret);
    exec(foo);
    return (13);
}
#endif

#if read_test
main()
{
    int f;
    char* bar = "bar";
    char buf[4];
    int ten = 4;
    int one = 1;
    int ret;
    
    open(bar, f);
    seek(f, one, ret);
    read(f, buf, ten, ret);
    consoleWrite(buf, ten, ret);
}
#endif

#if exec_test
main()
{
    char buf[10];
    int ten = 10;
    int ret;
    int i;
    
    consoleRead(buf, ten, ret);
    consoleWrite(buf, ret);
    exec(buf, ret);
}
#endif

main()
{
    int i, j = 0;
    
    for (i = 0; i < 5; i++)
	j += i;
    
    return (j);
}
