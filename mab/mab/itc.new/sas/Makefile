# $Header: /sprite/src/benchmarks/itc/sas/RCS/Makefile,v 1.2 93/02/11 17:17:33 kupfer Exp $ SPRITE (Berkeley)

# set to "-DUSG" if on a System V variant, "" if not.
USE_USG =

# Set the -DSUN preprocessor variable if this machine puts the
# most significant byte of a number first, and -DVAX if it puts
# the most significant byte last.
#

CFLAGS =	-O -DSUN $(USE_USG) -DLOADER=\"../../../bin/sld\" \
			-DPREPROCESSOR=\"../../../bin/cpp\"

../bin/sas:		parser.o sas.o lexer.o
	$(CC) parser.o sas.o lexer.o -o ../bin/sas

clean:
	rm -f *.o ../bin/sas
