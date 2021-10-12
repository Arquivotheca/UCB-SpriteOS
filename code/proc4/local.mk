#
# This file is included by Makefile.  Makefile is generated automatically
# by mkmf, and this file provides additional local personalization.  The
# variable SYSMAKEFILE is provided by Makefile;  it's a system Makefile
# that must be included to set up various compilation stuff.
#

CFLAGS += -DSOSP91
#CFLAGS += -pg
MANPAGES = NONE

#LIBS += -ldmalloc

#if empty(TM:Mds3100)
 XCFLAGS += -Wall
#else
 NOOPTIMIZATION = no -O please
#endif


#include	<$(SYSMAKEFILE)>
