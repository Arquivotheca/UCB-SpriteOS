

CFLAGS=	-DTARGET_MACHINE=TARGET_SUN3

ld: ld.o getopt.o getopt1.o cplus-dem.o
	cc -o ld ld.o getopt.o getopt1.o cplus-dem.o

ld.o: ld.c

getopt.o: getopt.c

getopt1.o: getopt1.c

cplus-dem.o: cplus-dem.c

