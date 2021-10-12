#
# This Makefile is for use when distributing Tcl to the outside world.
# It is simplified so that it doesn't include any Sprite-specific stuff.
# For HP-UX systems, use the second, commented-out, forms of LIBS and
# CFLAGS below.
#

LIBS =
#LIBS = -lBSD

CFLAGS = -g -I. -DTCL_VERSION=\"3.3\"
#CFLAGS = -g -I. -D_BSD -DTCL_VERSION=\"3.3\"

OBJS = tclAssem.o tclBasic.o tclCmdAH.o tclCmdIZ.o tclExpr.o \
	tclGet.o tclGlob.o tclHistory.o tclProc.o tclUtil.o

LIBOBJS = panic.o strerror.o strtol.o strtoul.o strspn.o \
	strpbrk.o strchr.o strstr.o

CSRCS = tclAssem.c tclBasic.c tclCmdAH.c tclCmdIZ.c tclExpr.c \
	tclGet.c tclGlob.c tclHistory.c tclProc.c tclUtil.c

tcl.a: ${OBJS} ${LIBOBJS}
	rm -f tcl.a
	ar cr tcl.a ${OBJS} ${LIBOBJS}
	ranlib tcl.a

tclTest: tclTest.o tcl.a
	cc tclTest.o tcl.a ${LIBS} -o tclTest

clean:
	rm -f ${OBJS} ${LIBOBJS} tcl.a tclTest.o tclTest
