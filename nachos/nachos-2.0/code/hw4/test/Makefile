CC = gcc
CFLAGS = -c -G 0 -I../../hw3/Makefile
LDFLAGS = -N -T 0

perftest: perftest.o assist.o
	ld $(LDFLAGS) perftest.o assist.o -o perftest.coff
	../../bin/coff2flat perftest.coff perftest
