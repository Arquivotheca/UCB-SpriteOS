/*
 * Routines to get trace records.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sprite.h>
#include <string.h>
#include <sysStats.h>
#include "sosp.h"
#include <bstring.h>
#include <list.h>

/*
 * Size of the trace record buffer.
 * If a trace record is bigger than this, bad things will happen.
 */
#define BUF_SIZE 4096

#define SWAP4(x) ((((x)>>24)&0xff)|(((x)>>8)&0xff00)|(((x)<<8)&0xff0000)|\
     (((x)<<24)&0xff000000))


/*
 * Letters to keep track of trace versions.
 * Version 2: January 9
 */
#define VERSION1	'A'
#define VERSION2	'B'
#define VERSION3	'C'

#define VERSIONLETTER	VERSION2

long startSec, startUsec;

int version;

char *ctime();

char *extraBuf;

typedef struct fileEntry {
    List_Links links;		/* Linked list. */
    int refBootTime[2];		/* Boot time for this trace. (only used for
				reference) */
    int startTime[2];		/* Time of the first record in the trace. */
    char *fileName;		/* Name of the file. */
    Boolean	compressed;	/* Is the file compressed? */
} fileEntry;

typedef struct machineEntry {
    int timeOffset[2];			/* Offset of this machine's time.  */
    int machineID;			/* ID of this machine. */
    char kernel[SYS_TRACELOG_KERNELLEN]; /* Kernel we're running. */
    char machineType[SYS_TRACELOG_TYPELEN];/* Machine type. */
    char *buf;				/* Data buffer. */
    traceFile inTraceFile;		/* Internal state. */
    List_Links fileList;		/* List of files for this machine.  */
    int bootTime[2];		/* Boot time for this machine's trace. */
    int curRecTime[2];		/* Time of the current record in the trace. */
} machineEntry;


#define MAX_MACHINES 10
#define TOTAL_MACHINES 100

float offsetTable[TOTAL_MACHINES] = {0};

#define timeLess(a,b) (((a)[0]<(b)[0])||((a)[0]==(b)[0] && (a)[1]<(b)[1]))
#define copyTime(to,from) (to)[0]=(from)[0];(to)[1]=(from)[1]

static int checkRecord _ARGS_((Sys_TracelogRecord *tracePtr));
static void usage _ARGS_((char *name));
static int getNextRecordOld _ARGS_((traceFile *file, char *buf));
static int getNextRecordNew _ARGS_((traceFile *file, char *buf));
static void swapBuf _ARGS_((int *buf, int len));
static void addTime _ARGS_((int *a, int *b, int *res));
static void subTime _ARGS_((int *a, int *b, int *res));
static void usage _ARGS_((char *name));
static int openTraceFile _ARGS_((int machine));
static void dumpRec _ARGS_((Sys_TracelogRecord *tracePtr));

int numMachineEntries = 0;
machineEntry machineEntries[MAX_MACHINES] = {0};

int minTime = -1, maxTime = -1;

#define SECS(day,hour,min) ((((day-1)*24+(hour))*60+(min))*60)

Sys_TracelogHeader initHdr;

int hdrBootTime[2];

static char	cmdBuffer[256];

static Boolean	migrate = FALSE;

/*
 * Initialize the reading of multiple files.
 * This takes an argc,argv: [-s startdate] [-e enddate] files.
 *
 * Returns 0 for success, -1 for failure.
 */
int
initRead(name,argc,argv, retHdr)
char *name;
int argc;
char **argv;
Sys_TracelogHeader **retHdr;	/* Return a representative header. */
{
    int i,j;
    FILE *inFile;
    traceFile tmpTraceFile;
    fileEntry *newEntry, *entryPtr;
    int found;
    char *minReq = NULL, *maxReq = NULL;
    Boolean		compressed;

    if (retHdr != NULL) {
	*retHdr = &initHdr;
    }
    while (argc>0 && argv[0][0]=='-') {
	if (argc>1 && argv[0][1]=='s' && strlen(argv[1])==8) {
	    minReq = argv[1];
	    argc--;
	    argv++;
	} else if (argc>1 && argv[0][1]=='e' && strlen(argv[1])==8) {
	    maxReq = argv[1];
	    argc--;
	    argv++;
	} else if (argc>2 && argv[0][1]=='o') {
	    int num;
	    float offset;
	    sscanf(argv[1], "%d", &num);
	    sscanf(argv[2], "%f", &offset);
	    if (num>=TOTAL_MACHINES) {
		fprintf(stderr,"Machine #: %d too big\n", num);
		exit(-1);
	    }
	    offsetTable[num] = offset;
	    argc -= 2;
	    argv += 2;
	} else {
	    usage(name);
	    return -1;
	}
	argc -= 1;
	argv += 1;
    }
    extraBuf = malloc(BUF_SIZE);
    for (i=0;i<argc;i++) {
	if (strcmp(argv[i] + strlen(argv[i]) - 2, ".Z") == 0) {
	    compressed = TRUE;
	    sprintf(cmdBuffer, "zcat %s", argv[i]);
	    inFile = popen(cmdBuffer, "r");
	} else {
	    compressed = FALSE;
	    inFile = fopen(argv[i],"r");
	}
	if (inFile==NULL) {
	    perror(argv[i]);
	    return -1;
	}
	tmpTraceFile.stream = inFile;
	tmpTraceFile.name = argv[i];
	if (getHeader(&tmpTraceFile,&initHdr)<0) {
	    fprintf(stderr,"Couldn't get header for %s\n", argv[i]);
	    return -1;
	}
	/*
	 * See if we already have an entry for this machine.
	 * Make sure the entries are compatible.
	 */
	for (j=0;j<numMachineEntries;j++) {
	    if (initHdr.machineID == machineEntries[j].machineID) {
		if (strcmp(machineEntries[j].kernel,initHdr.kernel) ||
			strcmp(machineEntries[j].machineType,
			initHdr.machineType)) {
		    fprintf(stderr,"Warning: tracefile mismatch on %s:\n",
			    argv[i]);
		    fprintf(stderr,"Old: %s; ", machineEntries[j].kernel);
		    fprintf(stderr,"New: %s\n", initHdr.kernel);
		}
		if (bcmp((char *)machineEntries[j].bootTime, (char *)
			initHdr.bootTime, 2*sizeof(int))) {
		    fprintf(stderr,"Warning: Boot time mismatch on %s\n",
			    argv[i]);
		}
		break;
	    }
	}
	if (j==numMachineEntries) {
	    /* Not found; make new machine entry */
	    machineEntries[j].machineID = initHdr.machineID;
	    List_Init((List_Links *)&machineEntries[j].fileList);
	    bcopy(initHdr.kernel, machineEntries[j].kernel,
		    SYS_TRACELOG_KERNELLEN);
	    bcopy(initHdr.machineType, machineEntries[j].machineType,
		    SYS_TRACELOG_TYPELEN);
	    copyTime(machineEntries[j].bootTime, initHdr.bootTime);
	    machineEntries[j].buf = malloc(BUF_SIZE);
	    machineEntries[j].timeOffset[0] =
		    (int)offsetTable[initHdr.machineID];
	    machineEntries[j].timeOffset[1] = (offsetTable[initHdr.
		    machineID]-machineEntries[j].timeOffset[0])*1000000;
	    numMachineEntries++;
	    if (numMachineEntries>=MAX_MACHINES) {
		fprintf(stderr,"Too many machines\n");
		exit(-1);
	    }
	}
	/*
	 * Add the file to the list:
	 * First get a record and see when the trace starts.
	 * Then put the record in the right place in the trace.
	 */
	if (getNextRecord(&tmpTraceFile, machineEntries[j].buf)<0) {
	    return -1;
	}
	newEntry = (fileEntry *)malloc(sizeof(fileEntry));
	newEntry->fileName = argv[i];
	newEntry->compressed = compressed;
	addTime(((Sys_TracelogRecord *)machineEntries[j].buf)->time,
		initHdr.bootTime, newEntry->startTime);
	copyTime(newEntry->refBootTime, initHdr.bootTime);
	found = 0;
	LIST_FORALL(&machineEntries[j].fileList, (List_Links *)entryPtr) {
	    if (entryPtr->startTime[0]>=newEntry->startTime[0]) {
		if (entryPtr->startTime[0]==newEntry->startTime[0] &&
			entryPtr->startTime[1]==newEntry->startTime[1]) {
		    fprintf(stderr,"Warning: %s and %s have same start time\n",
			    entryPtr->fileName, newEntry->fileName);
		}
		List_Insert((List_Links *)newEntry,
			LIST_BEFORE((List_Links *)entryPtr));
		found=1;
		break;
	    }
	}
	if (!found) {
	    List_Insert((List_Links *)newEntry,
		    LIST_ATREAR(&machineEntries[j].fileList));
	}
	if (pclose(inFile) == -1) {
	    fclose(inFile);
	}
	machineEntries[j].inTraceFile.stream = NULL;
    }
    if (minReq != NULL || maxReq != NULL) {
	int startOfMonth[2];
	int monthOffset[2];
	char *date;
	/*
	 * We take the boot time, figure out how many seconds into the month
	 * it was, and subtract to compute the start of the month.
	 */
	copyTime(startOfMonth, machineEntries[0].bootTime);
	date = ctime(startOfMonth);
	monthOffset[1] = startOfMonth[1];
	date[10] = '\0';
	date[13] = '\0';
	date[16] = '\0';
	date[19] = '\0';
	monthOffset[0] = SECS(atoi(date+8),atoi(date+11),atoi(date+14)) +
		atoi(date+17);
	subTime(startOfMonth, monthOffset, startOfMonth);
	if (minReq != NULL) {
	    minReq[2]='\0';
	    minReq[5]='\0';
	    minReq[8]='\0';
	    monthOffset[0] = SECS(atoi(minReq),atoi(minReq+3),atoi(minReq+6));
	    addTime(monthOffset, startOfMonth, monthOffset);
	    minTime = monthOffset[0];
	}
	if (maxReq != NULL) {
	    maxReq[2]='\0';
	    maxReq[5]='\0';
	    maxReq[8]='\0';
	    monthOffset[0] = SECS(atoi(maxReq),atoi(maxReq+3),atoi(maxReq+6));
	    addTime(monthOffset, startOfMonth, monthOffset);
	    maxTime = monthOffset[0];
	}
    }
    /*
     * Open the first files.
     */
    for (i=0;i<numMachineEntries;i++) {
	openTraceFile(i);
    }
    copyTime(hdrBootTime,initHdr.bootTime);
    return 0;
}

Boolean
migrateChildren(new)
    Boolean	new;
{
    Boolean	old;
    old = migrate;
    migrate = new;
    return old;
}

/*
 * Start a new trace file.
 * Returns -1 on failure; 0 on success.
 */
static int
openTraceFile(index)
int index;
{
    FILE *inFile;
    fileEntry *entry;
    Sys_TracelogHeader tmpHdr;

    if (List_IsEmpty(&machineEntries[index].fileList)) {
	return -1;
    }
    entry = (fileEntry *) List_First(&machineEntries[index].fileList);
    if (entry->compressed) {
	if (migrate) {
	    sprintf(cmdBuffer, "migrate zcat %s", entry->fileName);
	} else {
	    sprintf(cmdBuffer, "zcat %s", entry->fileName);
	}
	inFile = popen(cmdBuffer, "r");
	if ((inFile == NULL) && (migrate)) {
	    printf("Migration of zcat failed, doing it locally\n");
	    sprintf(cmdBuffer, "zcat %s", entry->fileName);
	    inFile = popen(cmdBuffer, "r");
	}
    } else {
	inFile = fopen(entry->fileName,"r");
    }
    if (inFile==NULL) {
	perror(entry->fileName);
	return -1;
    }
    if (machineEntries[index].inTraceFile.stream != NULL) {
	if (pclose(machineEntries[index].inTraceFile.stream) == -1) {
	    fclose(machineEntries[index].inTraceFile.stream);
	}
    }
    machineEntries[index].inTraceFile.stream = inFile;
    machineEntries[index].inTraceFile.name = entry->fileName;
    if (getHeader(&machineEntries[index].inTraceFile,&tmpHdr)<0) {
	return -1;
    }
    if (getNextRecord(&machineEntries[index].inTraceFile,
	    machineEntries[index].buf)<0) {
	return -1;
    }
    copyTime(machineEntries[index].bootTime,entry->refBootTime);
    subTime(machineEntries[index].bootTime,machineEntries[index].timeOffset,
	   machineEntries[index].bootTime);
#if 0
    printf("Modifying by %d/%d\n", machineEntries[index].timeOffset[0],
	    machineEntries[index].timeOffset[1]);
#endif
    addTime(((Sys_TracelogRecord *)machineEntries[index].buf)->time,
	    machineEntries[index].bootTime, machineEntries[index].curRecTime);
    startSec = hdrBootTime[0];
    startUsec = hdrBootTime[1];
    return 0;
}

/*
 * Get the next record, merging multiple streams.
 * Return machine ID for success, -1 for failure.
 */
int
getNextRecordMerge(buf)
char **buf;
{
    int i;
    int best;
    int done = 0;
    int bestTime[2];
    while (!done) {
	best = -1;
	for (i=0;i<numMachineEntries;i++) {
	    if (!List_IsEmpty(&machineEntries[i].fileList)) {
		if (best==-1 ||
			timeLess(machineEntries[i].curRecTime,bestTime)) {
		    copyTime(bestTime,machineEntries[i].curRecTime);
		    best = i;
		}
	    }
	}
	if (best==-1) {
	    return -1;
	} else {
	    if ((minTime<0 || minTime<=machineEntries[best].curRecTime[0]) &&
		    (maxTime<0 || maxTime>machineEntries[best].curRecTime[0])){
		copyTime(((Sys_TracelogRecord *)machineEntries[best].buf)->
			time, machineEntries[best].curRecTime);
		subTime(((Sys_TracelogRecord *)machineEntries[best].buf)->time,
		    hdrBootTime,
		    ((Sys_TracelogRecord *)machineEntries[best].buf)->time);
		*buf = machineEntries[best].buf;
		version = machineEntries[best].inTraceFile.version;
		machineEntries[best].buf = extraBuf;
		startSec = hdrBootTime[0];
		startUsec = hdrBootTime[1];
		extraBuf = *buf;
		done = 1;
	    }
	    /*
	     * Read another record.
	     */
	    if (getNextRecord(&machineEntries[best].inTraceFile,
		    machineEntries[best].buf)<0) {
		/* Start reading a new trace file. */
		List_Remove(List_First(&machineEntries[best].fileList));
		(void) openTraceFile(best);
	    } else {
		addTime(((Sys_TracelogRecord *)machineEntries[best].buf)->time,
			machineEntries[best].bootTime,
			machineEntries[best].curRecTime);
	    }
	}
    }
    return machineEntries[best].machineID;
}

static void
usage(name)
char *name;
{
    fprintf(stderr,"Usage: %s [-s day.hour.min] [-e day.hour.min] [-o machine secs] files\n",
	    name);
}

/*
 * Get the header.
 * Return 0 for success, -1 for failure.
 */
int
getHeader(file, hdr)
traceFile *file;
Sys_TracelogHeader *hdr;
{
    int status;
    int magic;
    int found = 0;
    (void) fread(&magic, sizeof(int), 1, file->stream);
    if (magic == TRACELOG_MAGIC) {
	file->version = 1;
	file->swap = 0;
	found=1;
    } else if (magic == SWAP4(TRACELOG_MAGIC)) {
#if 0
	fprintf(stderr,"Warning: byteswapping input file\n");
#endif
	file->version = 1;
	file->swap = 1;
	found=1;
    } else if ((magic&~0xff) == (TRACELOG_MAGIC&~0xff)) {
	file->version = magic&0xff;
	file->swap = 0;
	found=1;
    } else if ((SWAP4(magic)&~0xff) == (TRACELOG_MAGIC&~0xff)) {
#if 0
	fprintf(stderr,"Warning: byteswapping input file\n");
#endif
	file->version = SWAP4(magic)&0xff;
	file->swap = 1;
	found=1;
    }
    version = file->version;
    if (found) {
	status =fread(hdr, sizeof(Sys_TracelogHeader), 1, file->stream);
    } else {
	file->version = 0;
	bcopy((char *)&magic, (char *)hdr, 4);
	status =fread(((char *)hdr)+4, sizeof(Sys_TracelogHeaderKern)-4,
		1, file->stream);
	hdr->traceDir[0] = hdr->traceDir[1] = hdr->traceDir[2] =
		hdr->traceDir[3] = -1;
	/*
	 * Do some consistency
	 */
	if (hdr->machineID<0 || hdr->machineID>100 || hdr->numRecs<0 ||
	    hdr->numRecs>1000) {
	    fprintf(stderr,"Invalid header on input file %s\n", 
		file->name);
	    fprintf(stderr,"magic: %x, M: %x, S: %x\n", magic,
		    TRACELOG_MAGIC, SWAP4(TRACELOG_MAGIC));
	    exit(-1);
	}
	fprintf(stderr,"Warning: obsolete input format\n");
    }
    if (status == 0) {
	return -1;
    }
    if (file->swap) {
	/*
	 * Swap input.  Then unswap strings.
	 */
	swapBuf((int *)hdr, sizeof(Sys_TracelogHeader));
	swapBuf((int *)hdr->kernel, sizeof(hdr->kernel));
	swapBuf((int *)hdr->machineType, sizeof(hdr->machineType));
    }
    if (hdr->lostRecords>0) {
	fprintf(stderr,"*** Lost %d records\n", hdr->lostRecords);
    }
    file->traceRecCount = 0;
    file->numRecs = hdr->numRecs;
    startSec = hdr->bootTime[0];
    startUsec = hdr->bootTime[1];
    return 0;
}

/*
 * Do some simple consistency checks.
 * Returns 1 for success, 0 for failure.
 */
static int
checkRecord(tracePtr)
Sys_TracelogRecord *tracePtr;
{
    int flags;
    int type;
    int len;
    flags = (tracePtr->recordLen&TRACELOG_FLAGMASK)>>28;
    type = (tracePtr->recordLen&TRACELOG_TYPEMASK)>>16;
    len = tracePtr->recordLen&TRACELOG_BYTEMASK;
    if (len <=0 || len > 500) {
	fprintf(stderr,"Bad record length: %d\n", len);
	dumpRec(tracePtr);
	return 0;
    }
    if (type<=0 || type>LOST_TYPE) {
	fprintf(stderr,"Bad type: %d\n", type);
	dumpRec(tracePtr);
	return 0;
    }
    if (type != (int)tracePtr->data) {
	fprintf(stderr,"Record type mismatch: %d vs %d\n", type,
		(int)tracePtr->data);
	dumpRec(tracePtr);
	return 0;
    }
#if 0
    if (tracePtr->time[0] > 60*60*24*7) {
	fprintf(stderr,"Bad time field: %d\n", tracePtr->time[0]);
	dumpRec(tracePtr);
	return 0;
    }
#endif
    return 1;
}

/*
 * Dump record to stderr.
 */
static void
dumpRec(tracePtr)
Sys_TracelogRecord *tracePtr;

/*
 * Get the next record, using the old format.
 * Return 0 for success, -1 for failure.
 */
{
    int i;
    fprintf(stderr,"Header is: len: %d, type: %d, time: %d/%d:\n",
	    tracePtr->recordLen&TRACELOG_BYTEMASK,
	    (tracePtr->recordLen&TRACELOG_TYPEMASK)>>16,
	    tracePtr->time[0], tracePtr->time[1]);
    fprintf(stderr,"Record is:\n");
    for (i=0;i<64;i+=8) {
	int *ip = ((int *)&tracePtr->data)+i;
	fprintf(stderr,"%8.8x %8.8x %8.8x %8.8x %8.8x %8.8x %8.8x %8.8x\n",
		ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7]);
    }
}

static int
getNextRecordOld(file,buf)
traceFile *file;
char *buf;
{
    int status;
    int recordLen;
    Sys_TracelogHeader hdr;
    Sys_TracelogRecord *tracePtr = (Sys_TracelogRecord *)buf;

    if (file->traceRecCount == file->numRecs) {
	if (getHeader(file,&hdr)<0) {
	    return -1;
	}
    }
    status = fread(buf,sizeof(int),1,file->stream);
    if (status==0) {
	if (file->traceRecCount == file->numRecs) {
	    return -1;
	} else {
	    fprintf(stderr,"Too early end of log at %d instead of %d\n",
		   file->traceRecCount, file->numRecs);
	    exit(-1);
	}
    }
    file->traceRecCount++;
    if (file->traceRecCount > file->numRecs) {
	fprintf(stderr,"Warning: too many records in log\n");
    }
    recordLen = tracePtr->recordLen&TRACELOG_BYTEMASK;
    if (recordLen <=0 || recordLen > 500) {
	fprintf(stderr,"Record too long: %d (%x)\n", recordLen,
		tracePtr->recordLen);
	exit(-1);
    }
    status = fread(buf+sizeof(int), recordLen-sizeof(int), 1, file->stream);
    if (status==0) {
	fprintf(stderr,"Truncated record\n");
	exit(-1);
    }
    return 0;
}

/*
 * Get the next record, using the new format.
 * Return 0 for success, -1 for failure.
 */
static int
getNextRecordNew(file,buf)
traceFile *file;
char *buf;
{
    int status;
    int recordLen;
    Sys_TracelogRecord *tracePtr = (Sys_TracelogRecord *)buf;

    status = fread(buf,sizeof(int),1,file->stream);
    if (status==0) {
	if (fgetc(file->stream)==EOF) {
	    return -1;
	} else {
	    fprintf(stderr,"Truncated file %s: pos = %ld\n", 
		file->name, ftell(file->stream));
	    exit(-1);
	}
    }
    if (file->swap) {
	swapBuf((int *)buf, sizeof(int));
    }
    recordLen = tracePtr->recordLen&TRACELOG_BYTEMASK;
    if (recordLen <=0 || recordLen > 500) {
	fprintf(stderr,"Record too long in %s: %d (%x)\n", 
		file->name, recordLen, tracePtr->recordLen);
	exit(-1);
    }
    if (file->version==VERSION1 || (file->version==VERSION2 &&
	    ((tracePtr->recordLen&TRACELOG_TYPEMASK)>>16)!=SOSP_LOOKUP)) {
	/*
	 * We have to regenerate the type from the first field.
	 * I managed to mess up VERSION2 so that lookup records are in
	 * the old format, while everything else is in the new format. -Ken
	 */
	(void) fread(buf+sizeof(int), 2*sizeof(int), 1, file->stream);
	status = fread(buf+4*sizeof(int), recordLen-3*sizeof(int), 1,
		file->stream);
	if (file->swap) {
	    swapBuf(((int *)buf)+1, 2*sizeof(int));
	    swapBuf(((int *)buf)+4, recordLen-3*sizeof(int));
	}
	tracePtr->data = (ClientData)((tracePtr->recordLen&TRACELOG_TYPEMASK)
		>>16);
	tracePtr->recordLen = (tracePtr->recordLen&~TRACELOG_BYTEMASK) |
		(tracePtr->recordLen+4);
    } else {
	status = fread(buf+sizeof(int), recordLen-sizeof(int), 1, file->stream);
	if (file->swap) {
	    swapBuf(((int *)buf)+1, recordLen-sizeof(int));
	}
    }
    if (status==0) {
	fprintf(stderr,"Truncated record in %s\n", file->name);
	return -1;
    }
    if (!checkRecord(tracePtr)) {
	fprintf(stderr,"Bad record:\n");
	fprintf(stderr,"flags/type/len: %x\n", tracePtr->recordLen);
	fprintf(stderr,"time: %x %x\n", tracePtr->time[0], tracePtr->time[1]);
	fprintf(stderr,"data: %x\n", (int)tracePtr->data);
	exit(-1);
    }
    return 0;
}

/*
 * Get the next record.
 * Return 0 for success, -1 for failure.
 */
int
getNextRecord(file,buf)
traceFile *file;
char *buf;
{
    if (file->version==0) {
	return getNextRecordOld(file,buf);
    } else if (file->version==1 || file->version==VERSION1 ||
	    file->version==VERSION2) {
	return getNextRecordNew(file,buf);
    } else {
	fprintf(stderr,"Unknown file version: %d\n", file->version);
	exit(-1);
    }
    /*NOTREACHED*/
    return -1;
}

/*
 * Do byte swapping.
 */
static void
swapBuf(buf, len)
int *buf;
int len;
{
    int i;
    if ((len&3)!=0) {
	fprintf(stderr,"Unaligned swap buffer!\n");
	exit(-1);
    }
    len /= 4;
    for (i=0;i<len;i++) {
	*buf = SWAP4(*buf);
	buf++;
    }
}

/*
 * Add two times.
 */
static void addTime(a,b,res)
int *a, *b, *res;
{
    res[0] = a[0]+b[0];
    res[1] = a[1]+b[1];
    if (res[1]>1000000) {
	res[1] -= 1000000;
	res[0] += 1;
    }
}

/*
 * Subtract two times.
 */
static void subTime(a,b,res)
int *a, *b, *res;
{
    res[0] = a[0]-b[0];
    res[1] = a[1]-b[1];
    if (res[1]<0) {
	res[1] += 1000000;
	res[0] -= 1;
    }
}
