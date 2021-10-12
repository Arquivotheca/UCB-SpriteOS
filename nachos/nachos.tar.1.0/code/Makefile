# $Revision: 1.1 $ on $Date: 1992/01/03 03:59:27 $
# $Source: /home/ygdrasil/a/faustus/cs162/hw2/RCS/Makefile,v $
#
# NOTE: this is a GNU Makefile.  You must use "gnumake" rather than
# "make".
#
# Whenever you add new .cc files, put them in the CFILES list below.
# When you create new .h files, you should put them in HFILES,
# or "gnumake depend" may not work properly.  Whenever you change the
# include structure of your program, you should do a gnumake depend -- this
# will modify the Makefile.
#

# You might want to play with the CFLAGS, but if you use -O it may
# break the thread system.  You might want to use -fno-inline if
# you need to call some inline functions from the debugger.
INCPATH = 
DEFINES = -DHW1_SOLN -DHW2
CFLAGS =-g -Wall -Wshadow -fwritable-strings $(INCPATH) $(DEFINES)
LDFLAGS =

# These definitions may change as the software is updated.
CC = g++
LD = g++
AS = as

PROGRAM = nachos

H1FILES = list.h machine.h scheduler.h switch.h synch.h system.h \
	thread.h utility.h

C1FILES = main.cc list.cc machine.cc scheduler.cc synch.cc \
	system.cc thread.cc utility.cc

H2FILES = bitmap.h directory.h disk.h filehdr.h filesys.h list.h \
	machine.h openfile.h scheduler.h switch.h synch.h synchdisk.h \
 	system.h thread.h utility.h

C2FILES = main2.cc bitmap.cc directory.cc disk.cc filehdr.cc filesys.cc \
	list.cc machine.cc openfile.cc scheduler.cc synch.cc synchdisk.cc \
	system.cc thread.cc utility.cc

H5FILES = addrspace.h bitmap.h console.h directory.h disk.h filehdr.h \
 	filesys.h list.h machine.h memory.h memorymgr.h mipssim.h \
 	network.h openfile.h post.h scheduler.h switch.h synch.h synchdisk.h \
 	syscalls.h system.h thread.h user_syscalls.h utility.h

C5FILES = main5.cc addrspace.cc bitmap.cc console.cc directory.cc disk.cc \
	filehdr.cc filesys.cc list.cc machine.cc memory.cc memorymgr.cc \
	mipssim.cc network.cc openfile.cc post.cc scheduler.cc synch.cc \
	synchdisk.cc syscalls.cc system.cc thread.cc utility.cc

HFILES = $(H2FILES)
CFILES = $(C2FILES)
AFILES = switch.s

# You should not have to modify anything below this line.
# -----------------------------------------------------------------

C_OFILES = $(subst .cc,.o,$(CFILES))
A_OFILES = $(subst .s,.o,$(AFILES))

OFILES = $(C_OFILES) $(A_OFILES)
SRCFILES = $(CFILES) $(AFILES)

$(PROGRAM): $(OFILES)
	$(LD) $(OFILES) $(LDFLAGS) -o $(PROGRAM)

$(C_OFILES): %.o: %.cc
	$(CC) $(CFLAGS) -c $<

$(A_OFILES): %.o: %.s
	$(AS) -o $@ $<

depend: $(CFILES) $(HFILES)
	$(CC) -M $(INCPATH) $(CFILES) > makedep
	echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	ed - Makefile < eddep
	rm eddep makedep 
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> Makefile
	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile
	echo '# see make depend above' >> Makefile

#-----------------------------------------------------------------
# DO NOT DELETE THIS LINE -- make depend uses it
# DEPENDENCIES MUST END AT END OF FILE
main2.o : main2.cc utility.h /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h thread.h machine.h memory.h synch.h \
  list.h system.h scheduler.h 
bitmap.o : bitmap.cc bitmap.h utility.h \
  /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h openfile.h 
directory.o : directory.cc utility.h /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h openfile.h directory.h filehdr.h \
  disk.h thread.h machine.h memory.h bitmap.h 
disk.o : disk.cc disk.h thread.h utility.h \
  /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h machine.h memory.h system.h 
filehdr.o : filehdr.cc disk.h thread.h utility.h \
  /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h machine.h memory.h bitmap.h \
  openfile.h filehdr.h synchdisk.h synch.h list.h system.h 
filesys.o : filesys.cc disk.h thread.h utility.h \
  /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h machine.h memory.h bitmap.h \
  openfile.h directory.h filehdr.h filesys.h 
list.o : list.cc list.h utility.h /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h 
machine.o : machine.cc machine.h utility.h \
  /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h memory.h scheduler.h list.h thread.h 
openfile.o : openfile.cc filehdr.h disk.h thread.h utility.h \
  /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h machine.h memory.h bitmap.h \
  openfile.h system.h synchdisk.h synch.h list.h 
scheduler.o : scheduler.cc scheduler.h list.h utility.h \
  /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h thread.h machine.h memory.h system.h 
synch.o : synch.cc synch.h thread.h utility.h \
  /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h machine.h memory.h list.h system.h \
  scheduler.h 
synchdisk.o : synchdisk.cc synchdisk.h disk.h thread.h utility.h \
  /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h machine.h memory.h synch.h list.h \
  system.h scheduler.h 
system.o : system.cc system.h utility.h \
  /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h thread.h machine.h memory.h synch.h \
  list.h scheduler.h 
thread.o : thread.cc utility.h /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h thread.h machine.h memory.h \
  scheduler.h list.h switch.h synch.h system.h 
utility.o : utility.cc utility.h /usr/local/lib/g++/g++-include/stdio.h \
  /usr/local/lib/g++/g++-include/string.h \
  /usr/local/lib/g++/g++-include/std.h \
  /usr/local/lib/g++/g++-include/stddef.h \
  /usr/local/lib/g++/g++-include/ctype.h \
  /usr/local/lib/g++/g++-include/stdarg.h 
# DEPENDENCIES MUST END AT END OF FILE
# IF YOU PUT STUFF HERE IT WILL GO AWAY
# see make depend above
