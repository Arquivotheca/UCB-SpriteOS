# use normal make for this Makefile

CC = gcc
INCDIR =-I../../hw3 -I../../machine
CFLAGS = -c -G 0 $(INCDIR)
LDFLAGS = -N -T 0

AS=as
ASFLAGS = $(INCDIR)

all: halt perftest cpu io

halt: halt.o assist.o
	ld $(LDFLAGS) halt.o assist.o -o halt.coff
	../../bin/coff2flat halt.coff halt

perftest: perftest.o assist.o
	ld $(LDFLAGS) perftest.o assist.o -o perftest.coff
	../../bin/coff2flat perftest.coff perftest
cpu: cpu.o assist.o
	ld $(LDFLAGS) cpu.o assist.o -o cpu.coff
	../../bin/coff2flat cpu.coff cpu
io: io.o assist.o
	ld $(LDFLAGS) io.o assist.o -o io.coff
	../../bin/coff2flat io.coff io
