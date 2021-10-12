# NOTE: this is a GNU Makefile.  You must use "gmake" rather than "make".
#
# Copyright (c) 1992 The Regents of the University of California.
# All rights reserved.  See copyright.h for copyright notice and limitation 
# of liability and disclaimer of warranty provisions.

DEFINES =
INCPATH = -I../hw1 -I../machine
HFILES = $(HW1_H)
CFILES = $(HW1_C)
C_OFILES = $(HW1_O)

include ../Makefile.src
#-----------------------------------------------------------------
# DO NOT DELETE THIS LINE -- make depend uses it
# DEPENDENCIES MUST END AT END OF FILE
main.o : ../hw1/main.cc ../hw1/copyright.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/system.h ../hw1/thread.h 
list.o : ../hw1/list.cc ../hw1/copyright.h ../hw1/list.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h 
scheduler.o : ../hw1/scheduler.cc ../hw1/copyright.h ../hw1/scheduler.h \
  ../hw1/list.h ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/thread.h ../hw1/system.h 
synch.o : ../hw1/synch.cc ../hw1/copyright.h ../hw1/synch.h ../hw1/thread.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/list.h ../hw1/scheduler.h \
  ../machine/interrupt.h ../hw1/system.h 
synchlist.o : ../hw1/synchlist.cc ../hw1/copyright.h ../hw1/synchlist.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/list.h ../hw1/synch.h \
  ../hw1/thread.h 
system.o : ../hw1/system.cc ../hw1/copyright.h \
  /uns/usr/local/lib/g++-include/signal.h /usr/include/sys/signal.h \
  ../hw1/system.h ../hw1/thread.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/scheduler.h ../hw1/list.h \
  ../machine/interrupt.h ../machine/stats.h ../machine/timer.h 
thread.o : ../hw1/thread.cc ../hw1/copyright.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/thread.h ../hw1/scheduler.h \
  ../hw1/list.h ../machine/interrupt.h ../hw1/switch.h ../hw1/synch.h \
  ../hw1/system.h 
utility.o : ../hw1/utility.cc ../hw1/copyright.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h \
  /uns/usr/local/lib/g++-include/stdarg.h 
test1.o : ../hw1/test1.cc ../hw1/copyright.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/thread.h ../hw1/system.h 
interrupt.o : ../machine/interrupt.cc ../hw1/copyright.h \
  ../machine/interrupt.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/list.h ../machine/stats.h \
  ../hw1/thread.h ../hw1/system.h 
timer.o : ../machine/timer.cc ../hw1/copyright.h ../machine/timer.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/system.h \
  ../machine/interrupt.h ../hw1/list.h ../machine/stats.h 
# DEPENDENCIES MUST END AT END OF FILE
# IF YOU PUT STUFF HERE IT WILL GO AWAY
# see make depend above
