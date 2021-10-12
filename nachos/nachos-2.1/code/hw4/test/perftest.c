/* Program to test virtual memory performance, by running
 * a single program doing matrix multiplication
 */

/* gcc -c -G 0 perftest.c; ld -N -T 0 perftest.o -o perftest; 
	coff2flat perftest perftest.flat */

#include "syscall.h"

#define Dim 	32	/* each array is as big as physical memory */
int A[Dim][Dim] = {0};
int B[Dim][Dim] = {0};
int C[Dim][Dim] = {0};

int
main()
{
    int i, j, k;

    for (i = 0; i < Dim; i++)		/* first initialize the matrices */
	for (j = 0; j < Dim; j++) {
	     A[i][j] = i;
	     B[i][j] = j;
	     C[i][j] = 0;
	}

    for (i = 0; i < Dim; i++)		/* then multiply them together */
	for (j = 0; j < Dim; j++)
            for (k = 0; k < Dim; k++)
		 C[i][j] += A[i][k] * B[j][k];

    Syscall(SC_Halt);		/* and then we're done */
    Syscall(SC_Exit);		/* not reached */
}
