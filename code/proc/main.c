/*
 * Program to process a trace file.
 */
#include <stdio.h>
#include <signal.h>
#include <sprite.h>
#include <sysStats.h>
#include <getrec.h>

main(argc,argv)
int argc;
char **argv;
{
    int status;
    int args[2];
    int i;
    char *dataPtr;
    char buf[1000];
    Sys_TracelogRecord *tracePtr = (Sys_TracelogRecord *)buf;
    Sys_TracelogHeader hdr;
    FILE *inFile;
    traceFile inTraceFile;

    if (argv[1][0]=='-' && argv[1][1]=='i') {
	initHash(argv[2]);
	argc -= 2;
	argv += 2;
    }
    if (argc<2) {
	fprintf(stderr,"Usage: sospread filename(s)\n");
	exit(1);
    }
    for (i=1;i<argc;i++) {
	if (argc != 1) {
	    printf("--- Processing %s ---\n", argv[i]);
	}
	inFile = fopen(argv[i],"r");
	if (inFile==NULL) {
	   perror("open");
	   exit(-1);
	}
	inTraceFile.stream = inFile;

	getHeader(&inTraceFile, &hdr);
	printf("From %d (%s) %32.32s\n", hdr.machineID,
		hdr.machineType, hdr.kernel);
	while (getNextRecord(&inTraceFile,buf)==0) {
	    dorec(tracePtr);
	}
	fclose(inFile);
    }
}
