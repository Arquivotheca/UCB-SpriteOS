# This Makefile is for use when distributing mipsim to UNIX systems.
# It is simplified not to include any Sprite-specific stuff, and it
# makes copies of library source files that may not be present on
# all UNIX systems.  For HP-UX systems, use the second definition
# of LIBS below.

LIBS = tcl/tcl.a
#LIBS = tcl/tcl.a -lBSD

CFLAGS = -g -I. -Itcl

OBJS = asm.o cop0.o getput.o io.o main.o sim.o stop.o sym.o

LIBOBJS = Hash.o HashChainSearch.o Hash_CreateEntry.o Hash_DeleteEntry.o \
	Hash_EnumFirst.o Hash_EnumNext.o Hash_FindEntry.o Hash_InitTable.o \
	List_Init.o List_Insert.o List_Remove.o

CSRCS = asm.c cop0.c getput.c io.c main.c sim.c stop.c sym.c

mipsim: ${OBJS} ${LIBOBJS} ${LIBS}
	rm -f mipsim
	cc -g ${OBJS} ${LIBOBJS} ${LIBS} -o mipsim

tcl/tcl.a:
	cd tcl; make

install: mipsim
	rm -f ../bin/mipsim.old
	mv ../bin/mipsim ../bin/mipsim.old
	chmod 755 mipsim
	mv mipsim ../bin/mipsim

clean:
	rm -f ${OBJS} ${LIBOBJS} mipsim
