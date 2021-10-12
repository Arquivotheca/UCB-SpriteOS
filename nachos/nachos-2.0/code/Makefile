# Copyright (c) 1992 The Regents of the University of California.
# All rights reserved.  See copyright.h for copyright notice and limitation 
# of liability and disclaimer of warranty provisions.

MAKE = gmake
all: 
	cd bin; make
	cd hw1; $(MAKE) depend
	cd hw1; $(MAKE) nachos 
	cd hw2; $(MAKE) depend
	cd hw2; $(MAKE) nachos 
	cd hw3; $(MAKE) depend
	cd hw3; $(MAKE) nachos 
	cd hw4; $(MAKE) depend
	cd hw4; $(MAKE) nachos 
	cd hw5; $(MAKE) depend
	cd hw5; $(MAKE) nachos 

clean:
	rm */*.o hw*/nachos bin/coff2flat bin/out hw*/DISK
