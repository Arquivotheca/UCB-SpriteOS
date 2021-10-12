
/* $Revision: 1.2 $ on $Date: 1991/12/20 07:50:34 $
 * $Source: /home/ygdrasil/a/faustus/cs162/hw1+/RCS/utility.cc,v $
 *
 * utility.cc -- miscellaneous routines
 */

#include "utility.h"
#include <stdarg.h>

/* controls which DEBUG messages are printed */
static char *enableFlags = NULL;

/* Select which debug messages are to be printed */
void
DebugInit (char *flags)
{
    enableFlags = flags;
}

bool
DebugIsEnabled(char flag)
{
    return (enableFlags && (strchr(enableFlags, flag) ||
			    strchr(enableFlags, '+')));
}

/* Print debug message if flag is enabled, or if the + wildcard flag is given */

void DEBUG (char flag, char *format, ...)
{
    if (DebugIsEnabled(flag)) {
	va_list ap;
	// You will get an unused variable message here -- ignore it.
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fflush(stderr);
    }
}
