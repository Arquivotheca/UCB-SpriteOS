/* utility.h -- useful stuff.  */

#ifndef UTILITY_H
#define UTILITY_H

/* Wrap everything that is actually C with an extern "C" block.
 * This prevents the internal forms of the names from being
 * changed by the C++ compiler.
 */
extern "C" {
#include <stdio.h>
#include <string.h>
#include <ctype.h>
}

#define min(a,b)  (((a) < (b)) ? (a) : (b))
#define max(a,b)  (((a) > (b)) ? (a) : (b))

/* Boolean values.  This is the same definition as in the G++ library. */
typedef enum { FALSE = 0, TRUE = 1 } bool;

// This declares the type "VoidFunctionPtr" to be a "pointer to a
// function taking an integer argument and returning nothing".  With
// such a function pointer (say it is "func"), we can call it like this:
//	(*func) (17);

typedef void (*VoidFunctionPtr)(int arg); 
typedef bool (*BoolFunctionPtr)(void *arg1, void *arg2); 

/* Debugging routines.   Note that these are C routines, in debug.c.
 * It is like this because Ultrix G++ doesn't like varargs at this point.
 */

/* Enabled selected debug messages.
 *   '+' -- turn on all debug messages
 *   't' -- thread system
 *   's' -- semaphores, locks, and conditions
 *   'm' -- machine emulation
 *   'f' -- file system (in homework 2)
 *   'a' -- address spaces (in homework 3)
 */
extern void DebugInit (char* flags);

/* Is this debug flag enabled? */
extern bool DebugIsEnabled(char flag);

/* Print debug message if flag is enabled. */
extern void DEBUG (char flag, char* format, ...);

/* If the condition is false, print a message and dump core. 
 *
 * Useful for documenting assumptions in the code.
 */
#define ASSERT(condition)						      \
    if (!(condition)) {							      \
	fprintf(stderr, "Assertion failed: line %d, file \"%s\"\n",	      \
		__LINE__, __FILE__);					      \
	abort();							      \
    }

#endif
