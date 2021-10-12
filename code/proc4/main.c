/*
 * Program to process a trace file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sprite.h>
#include <sysStats.h>
#include <sosp.h>
#include <namehash.h>

char *prefixList = NULL;

int thisLRU = 0;

extern long sec;
long firstSecs = -1;
int debug = 0;


void
main(argc,argv)
int argc;
char **argv;
{
    char buf[1000];
    Sys_TracelogRecord *tracePtr = (Sys_TracelogRecord *)buf;
    char *name = argv[0];
    int machine;
    char *file = NULL;
    int newArgc;
    char **newArgv;

    initHash();
    while (argc > 1 && argv[1][0]=='-') {
	if (argv[1][1]=='l' && argc>2) {
	    thisLRU = atoi(argv[2]);
	    printf("Doing LRU list %d\n", thisLRU);
	    argc--;
	    argv++;
	} else if (argv[1][1]=='i' && argc>2) {
	    loadHash(argv[2],0);
	    argc--;
	    argv++;
	} else if (argv[1][1]=='p' && argc>2) {
	    prefixList = argv[2];
	    loadHash(prefixList,1);
	    argc--;
	    argv++;
	} else if (argv[1][1]=='d') {
	    debug++;
	} else if (argv[1][1]=='s') {
	    break;
	} else if (argv[1][1]=='f') {
	    file = argv[2];
	    argc--;
	    argv++;
	}
	argc--;
	argv++;
    }
    if (argc<2 && file==NULL) {
	fprintf(stderr,"Usage: sospread [-l #] [-i #] [-p prefixes] [-d]\n");
	fprintf(stderr,"  [-f file] [-s dd.hh.mm -e dd.hh.mm] filenames(s)\n");
	exit(1);
    }
    if (file != NULL) {
        int i;
	FILE *nameFile;
#define MAX 500
        nameFile = fopen(file,"r");
	if (nameFile==NULL) {
	    perror(file);
	    exit(-1);
	}
	newArgv = (char **)malloc(MAX*sizeof(char *));
	for (i=0;i<argc-1;i++) {
	    newArgv[i] = (char *)malloc(strlen(argv[i+1])+1);
	    strcpy(newArgv[i], argv[i+1]);
	}
	for (;i<MAX;i++) {
	    if (fgets(buf,100,nameFile)==NULL) break;
	    newArgv[i] = (char *)malloc(strlen(buf)+1);
	    strcpy(newArgv[i], buf);
	    newArgv[i][strlen(buf)-1]='\0';
	}
	newArgc = i;
    } else {
	newArgc = argc-1;
	newArgv = argv+1;
    }
    if (initRead(name, newArgc, newArgv, NULL)<0) {
	fprintf(stderr,"Initialization failed\n");
	exit(-1);
    }
    while (1) {
	machine = getNextRecordMerge((char *)&tracePtr);
	if (machine<0) break;
	dorec(tracePtr,machine);
	if (firstSecs==-1) {
	    firstSecs = sec;
	}
    }
    fprintf(stderr,"Last machine was %d\n", machine);
    donerecs();
}
