# NOTE: this is a GNU Makefile.  You must use "gmake" rather than "make".
#
# Copyright (c) 1992 The Regents of the University of California.
# All rights reserved.  See copyright.h for copyright notice and limitation 
# of liability and disclaimer of warranty provisions.

DEFINES = -DHW2 -DHW3
INCPATH = -I../hw1 -I../hw2 -I../hw3 -I../machine
HFILES = $(HW1_H) $(HW2_H) $(HW3_H)
CFILES = $(HW1_C) $(HW2_C) $(HW3_C)
C_OFILES = $(HW1_O) $(HW2_O) $(HW3_O)

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
bitmap.o : ../hw2/bitmap.cc ../hw1/copyright.h ../hw2/bitmap.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw2/openfile.h 
directory.o : ../hw2/directory.cc ../hw1/copyright.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw2/openfile.h ../hw2/directory.h \
  ../hw2/filehdr.h ../machine/disk.h ../hw2/bitmap.h 
filehdr.o : ../hw2/filehdr.cc ../hw1/copyright.h ../hw2/bitmap.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw2/openfile.h ../hw2/filehdr.h \
  ../machine/disk.h ../hw2/synchdisk.h ../hw1/synch.h ../hw1/thread.h \
  ../hw1/list.h ../hw1/system.h 
filesys.o : ../hw2/filesys.cc ../hw1/copyright.h ../machine/disk.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw2/bitmap.h ../hw2/openfile.h \
  ../hw2/directory.h ../hw2/filehdr.h ../hw2/filesys.h 
test2.o : ../hw2/test2.cc ../hw1/copyright.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw2/filesys.h ../hw2/openfile.h \
  ../hw1/system.h ../hw1/thread.h ../machine/disk.h ../machine/stats.h 
openfile.o : ../hw2/openfile.cc ../hw1/copyright.h ../hw2/filehdr.h \
  ../machine/disk.h ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw2/bitmap.h ../hw2/openfile.h \
  ../hw2/synchdisk.h ../hw1/synch.h ../hw1/thread.h ../hw1/list.h \
  ../hw1/system.h 
synchdisk.o : ../hw2/synchdisk.cc ../hw1/copyright.h ../hw2/synchdisk.h \
  ../machine/disk.h ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/synch.h ../hw1/thread.h \
  ../hw1/list.h ../hw1/scheduler.h 
disk.o : ../machine/disk.cc ../hw1/copyright.h ../machine/disk.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../machine/interrupt.h ../hw1/list.h \
  ../hw1/system.h ../machine/stats.h \
  /uns/usr/local/lib/g++-include/sys/types.h //usr/include/sys/types.h \
  /uns/usr/local/lib/g++-include/sys/file.h //usr/include/sys/smp_lock.h \
  //usr/include/sys/file.h 
addrspace.o : ../hw3/addrspace.cc ../hw1/copyright.h ../hw3/addrspace.h \
  ../machine/pte.h ../machine/disk.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../machine/machine.h \
  ../hw2/filesys.h ../hw2/openfile.h ../hw1/system.h 
exception.o : ../hw3/exception.cc ../hw1/copyright.h ../hw1/system.h \
  ../machine/machine.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../machine/pte.h ../machine/disk.h \
  ../machine/interrupt.h ../hw1/list.h ../hw3/syscall.h 
test3.o : ../hw3/test3.cc ../hw1/copyright.h ../hw2/filesys.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw2/openfile.h ../hw3/addrspace.h \
  ../machine/pte.h ../machine/disk.h ../machine/machine.h ../hw1/synch.h \
  ../hw1/thread.h ../hw1/list.h ../machine/console.h ../hw1/system.h 
console.o : ../machine/console.cc ../hw1/copyright.h ../machine/console.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../hw1/system.h ../machine/stats.h \
  ../machine/interrupt.h ../hw1/list.h \
  /uns/usr/local/lib/g++-include/sys/types.h //usr/include/sys/types.h \
  /uns/usr/local/lib/g++-include/sys/file.h //usr/include/sys/smp_lock.h \
  //usr/include/sys/file.h 
machine.o : ../machine/machine.cc ../hw1/copyright.h ../machine/machine.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../machine/pte.h ../machine/disk.h \
  ../machine/interrupt.h ../hw1/list.h ../hw1/system.h ../machine/stats.h 
mipssim.o : ../machine/mipssim.cc ../hw1/copyright.h ../machine/machine.h \
  ../hw1/utility.h /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../machine/pte.h ../machine/disk.h \
  ../machine/mipssim.h ../hw1/system.h ../hw1/thread.h ../machine/stats.h \
  ../machine/interrupt.h ../hw1/list.h 
pte.o : ../machine/pte.cc ../hw1/copyright.h ../hw3/addrspace.h \
  ../machine/pte.h ../machine/disk.h ../hw1/utility.h \
  /uns/usr/local/lib/g++-include/stdio.h \
  /uns/usr/local/lib/g++-include/string.h \
  /uns/usr/local/lib/g++-include/std.h \
  /uns/usr/local/lib/g++-include/stddef.h \
  /uns/usr/local/lib/g++-include/ctype.h ../machine/machine.h \
  ../hw2/filesys.h ../hw2/openfile.h ../hw1/system.h ../machine/stats.h 
# DEPENDENCIES MUST END AT END OF FILE
# IF YOU PUT STUFF HERE IT WILL GO AWAY
# see make depend above
