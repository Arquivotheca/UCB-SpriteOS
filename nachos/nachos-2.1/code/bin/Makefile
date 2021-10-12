# use normal make for this Makefile
#
# Copyright (c) 1992 The Regents of the University of California.
# All rights reserved.  See copyright.h for copyright notice and limitation 
# of liability and disclaimer of warranty provisions.

CFLAGS= -O0 -g2 -I../hw1

all: out coff2flat

coff2flat: coff2flat.o
	cc -g2 -O0 coff2flat.o -o coff2flat

out: out.o opstrings.o
	cc -g2 -O0 out.o opstrings.o -o out
