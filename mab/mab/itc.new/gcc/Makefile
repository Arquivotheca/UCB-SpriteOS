# $Header: /sprite/src/benchmarks/itc/gcc/RCS/Makefile,v 1.2 93/02/11 17:12:41 kupfer Exp $ SPRITE (Berkeley)
#
# If your system has a system include file "alloca.h", make sure there
# is no file by that name in this directory (rename it to alloca.h.dummy).
# If your system doesn't define the include file "alloca.h", make sure a
# dummy version exists in this directory (raname alloca.h.dummy to
# alloca.h if alloca.h isn't here already).
#
# Repeat for stddef.h.

# set to "-DUSG" if on a System V variant, "" if not.
USE_USG = 

OBJS =		gcc.o obstack.o version.o

CFLAGS =	-O $(USE_USG)

../bin/gcc:	$(OBJS)
	$(CC) $(OBJS) -o ../bin/gcc

clean:
	rm -f *.o ../bin/gcc
