# $Header: /sprite/src/benchmarks/itc/cpp/RCS/Makefile,v 1.2 93/02/11 17:09:48 kupfer Exp $ SPRITE (Berkeley)
#
# set to "-DUSG" if on a System V variant, "" if not.
USE_USG =

OBJS =		cccp.o cexp.o version.o

CFLAGS =	-O -I. -I../gcc $(USE_USG)

../bin/cpp: $(OBJS)
	$(CC) $(OBJS) -o ../bin/cpp

clean:
	rm -f *.o ../bin/cpp
