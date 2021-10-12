# $Header: /sprite/src/benchmarks/itc/sranlib/RCS/Makefile,v 1.2 93/02/11 17:22:50 kupfer Exp $ SPRITE (Berkeley)

# set to "-DUSG" if on a System V variant, "" if not.
USE_USG = 

# The FIRST_BYTE preprocessor variable should be set to HIGH if the
# host machine puts the most significant byte of a number first (e.g.
# 68000s), and otherwise should be set to LOW (e.g. VAXes).
# 
# The FLOATING_FORM variable should be set to IEEE if the host machine
# uses IEEE format, or else to OTHER.
#

CFLAGS		= -O $(USE_USG) -DFIRST_BYTE=HIGH -DFLOATING_FORM=IEEE \
			-DTARGET=HOST -I../sas

../bin/sranlib:		sranlib.o
	$(CC) sranlib.o -o ../bin/sranlib

clean:
	rm -f *.o ../bin/sranlib
