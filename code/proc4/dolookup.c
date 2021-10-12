#include <stdio.h>
#include <sprite.h>
#include <status.h>
#include <namehash.h>
#include <bstring.h>
#include <hash.h>
#include <stdlib.h>
#include <strings.h>
#include <sospRecord.h>
#include <kernel/fs.h>
#include <kernel/fsNameOps.h>
#include <fs.h>
extern long startSec;
extern long startUsec;

#define MULTI_OPS

#define EXTRA 2

extern char *ctime();
extern char *filetype();

extern int debug;
extern int thisLRU;

extern char *prefixList;

char bar[sizeof(nameRec)] = {120};

extern char *timeStr;

extern int version;

extern long sec;
extern long firstSecs;

float totalSecs;

char charBuf[500];

unsigned char masks[4] = {0x3,0xc,0x30,0xc0};

#define NLRU(x) 0

int remember_host, remember_home;
int remember_file;


#if 1
#define BYTE(n) ((n)>>2)
#define POS(n) (((n)+(n))&6)
#define GET_MACHINEINFO(mi,n) ((mi[BYTE(n)]>>POS(n))&0x3)
#define SET_MACHINEINFO(mi,n,v) mi[BYTE(n)] = (mi[BYTE(n)]&~(3<<POS(n)))|\
	(v<<POS(n))
#else
#define GET_MACHINEINFO(mi,n) mi[n]
#define SET_MACHINEINFO(mi,n,v) mi[n]=v
#endif

void abort() { printf("%s",1); }


#if 0
GET_MACHINEINFO(mi,n)
unsigned char *mi;
int n;
{
    int v,v1;
    if (n<0 || n>=NUM_MACHINES) {
	fprintf(stderr,"Out of bounds\n");
    }
    v = GET_MACHINEINFO2(mi,n);
    v1 = GET_MACHINEINFO2(mi,n+NUM_MACHINES);
    if (v != v1) {
	fprintf(stderr,"Get error: %x %d\n", mi,n);
    }
    return v;
}

SET_MACHINEINFO(mi,n,v)
unsigned char *mi;
int n;
unsigned char v;
{
    if (n<0 || n>=NUM_MACHINES) {
	fprintf(stderr,"Out of bounds\n");
    }
    SET_MACHINEINFO2(mi,n,v);
    SET_MACHINEINFO2(mi,n+NUM_MACHINES,v);
}
#endif

int gcMax = 1000;
#define GC_INC 1000
#define GC_REMOVE_COUNT 20
#define GC_REMOVE_TIME 600

static LRUentry LRUlist[NUM_LRU_LISTS][NUM_MACHINES] = {NULL};

/*
 * Data structures:
 * An ID is a File_ID (aka 4 integers)
 * We also have trace_index, which is an int.  This is an index into the
 * list of ID's in the trace.
 * ID_NUM converts a trace_index to an ID.
 *
 * Invariants:
 * numActiveUsers is the number of nonremoved pointers to the name entry.
 * This should equal the number of machineInfos that are 1 or 2, plus the
 * number of opens.
 */


#define error(x) fprintf(stderr,"%19.19s: %s", ctime(&sec), x)
#define CHECKLRU(x) if ((x)<0 || (x)>=NUM_LRU_LISTS) printf("LRU=%d at %d\n",x,__LINE__)

#define mypanic() printf("%s",1)

#define MAKENEW 1
#define IFNEW 0

#define NUM_OPS 20
#define NUM_PREFIXES 50
#define LEVEL_TABLE_SIZE	103
#define AGE_TABLE_SIZE		303
#define IMT_SIZE 50

/*
 * NAME = normal name cache, ATTR = attr cache, NAME_ENTRY = entry-based
 * name cache, NAME_MIG, ATTR_MIG = caches with mig. back at home.
 */
#define NAME 0
#define ATTR 1
#define NAME_ENTRY 2
#define NAME_MIG 3
#define ATTR_MIG 4
#define MIG_MEAS (NUM_LRU_LISTS)
#define TMP_MEAS (NUM_LRU_LISTS+1)

char *LRUWord[] = {"name", "attribute", "name (entry)", "name (mig)",
	"attr (mig)"};

#define HARDLINK_2 11

/*
 * Functions for a directory lookup.
 */
char *microNames[] = {"?","name lookup","attr lookup", "name modify",
	"attr modify", "?", "remove", "create", "read"};
#define NAME_LOOKUP 1
#define ATTR_LOOKUP 2
#define NAME_MODIFY 3
#define ATTR_MODIFY 4
#define NAMEATTR_REMOVE 6
#define NAMEATTR_CREATE 7
#define DIR_READ 8

typedef int trace_index;
typedef int trace_data;
typedef int file_id;

#define DIFF_FD(x,y) (((x)[0]!=(y)[0])||((x)[1]!=(y)[1])||\
		      ((x)[2]!=(y)[2])||((x)[3]!=(y)[3]))

#define RETURN_ID (&data[3])

#define ID_NUM(n) ((n<0)?(int *)mypanic():&data[10+4*(n)])

int NULL_ID[4] = {0};
int prefixParts[50][40] = {0};

int tmpDir[4] = {1,14,10,125304};
nameRec *tmpName;

/*
 ****************************************************************************
 *	Counters
 ****************************************************************************
 */

int ops[NUM_OPS]={0};	/* Counter of operations */
int typeUsage[NUM_OPS]={0}; /* Number of accesses to each type. */
int nlookups = 0;
int nHitLookups = 0;	/* Number of lookups that hit every component. */
int nMigLookups = 0;	/* Number of migrated lookups. */
int ncomps = 0;
int nCompLookups = 0; /* Number of component lookups. */
int nInvalidates[NUM_LRU_LISTS] = {0}; /* Number of directory invalidates. */
int nMInvalidates[NUM_LRU_LISTS] = {0}; /* Number of directory-machine invalidates. */
int nRHits[NUM_LRU_LISTS+EXTRA] = {0}; /* Number of remote directory validates. */
int forcedMiss[NUM_LRU_LISTS+EXTRA]={0};
int nRValidates[NUM_LRU_LISTS+EXTRA] = {0}; /* Number of remote directory validates. */
int nRRevalidates[NUM_LRU_LISTS+EXTRA] = {0}; /* Number of remote directory revalidates. */
int nAccesses[NUM_LRU_LISTS+EXTRA] = {0}; /* Number of cache accesses. */
int nRRemoves[NUM_LRU_LISTS] = {0}; /* Number of remote directory removes. */
int prefixRef = 0;	/* Number of prefix directories referenced. */
int prefixRefNum[50] = {0}; /* Number of references, by prefix. */
int modifies[NUM_OPS] = {0}; /* Operations modifying directories. */
int entryCnt = 0;	/* Count of hash table entries. */
int realEntryCnt = 0;	/* Count of hash table entries (not reset). */
int reusedID = 0;	/* Count of hash entries that are reused. */
int badOp = 0;		/* Count of bad operations. */
int machineTable[NUM_MACHINES]={0};	/* Accesses by machine. */
int numDirectoryOpens = 0;	/* Number of directory opens. */
int unmatchedOpens = 0;	/* Files opened but not closed. */
int unmatchedCloses = 0;/* Files closed but not opened. */
int attributesWhileReading = 0; /* Accesses to attributes while reading file */
int attributesWhileWriting = 0; /* Accesses to attributes while writing file */
int attributesWhileExecuting = 0; /* Accesses to attributes while file execd */
int attributesWhileSafe = 0; /* Accesses to attributes while file closed */
int numLinksEncountered = 0; /* Count of symbolic links. */
int numMicroFuncs[NUM_OPS] = {0}; /* Count of low-level functions. */
int numGetAttrID = 0;	/* Number of get attributes by ID calls. */
int numSetAttrID = 0;	/* Number of set attributes by ID calls. */
int fileSharing = 0; /* Count of opens on shared file. */
int overflow = 0;	/* Number of records which may have overflowed. */
int numWrites = 0;	/* Number of file opens for writes. */
int numReads = 0;	/* Number of file opens for reads. */
int tmpAccess = 0;	/* Number of accesses to /tmp */
int migCount = 0;	/* Count of migrated lookups. */
int gcError = 0;	/* Count of trace errors due to removed entries. */
int numMoveOps = 0;	/* Number of moves. */
int mallocOpen=0, mallocName=0, mallocLRU=0; /* Memory usage. */
int mallocHash=0, mallocTable=0;
int hashOpen=0, hashName=0;
int nAttrPermReads=0;	/* Attribute operations from permission accesses.*/

/* Histogram tables */

int statusTable[NUM_OPS][20] = {0};	/* Return status by op. */
int ValidhistTable[NUM_LRU_LISTS+EXTRA][LEVEL_TABLE_SIZE] = {0}; /* LRU position when valid */
int foundInvalidTable[NUM_LRU_LISTS+EXTRA][LEVEL_TABLE_SIZE] = {0}; /* LRU when invalid. */
int InvalidhistTable[NUM_LRU_LISTS][LEVEL_TABLE_SIZE] = {0}; /* LRU when invalidated.  */
int invalidatedMachinesTable[NUM_LRU_LISTS][IMT_SIZE] = {0}; /* # of machines invalidated. */
int ageTable[NUM_LRU_LISTS][AGE_TABLE_SIZE] = {0}; /* used entry age. */

Hash_Table *streamTablePtr = NULL;

/*
 ****************************************************************************
 *	Procedures
 ****************************************************************************
 */

char *opname(), *status();

/* Print traces. */
void printLookupStats _ARGS_((void));
void dumpHash _ARGS_((void));

/* Low-level operations */
int accessDir _ARGS_((int function, nameRec *dirIDName, nameRec *entryIDName,
	int currHost, int homeHost, int type));
void modifyblock _ARGS_((file_id *fileID, trace_data *data, int machine,
	int LRU));

/* Hash table operations */
nameRec *newID _ARGS_((file_id *fileID, int force, char *name, int type));
char *getIDname _ARGS_((file_id *fileID));
void doneID _ARGS_((nameRec *namePtr));
void freeIDIfDone _ARGS_((nameRec *namePtr));

/* LRU list operations */
void invalidateLRU _ARGS_((nameRec *namePtr, int machine, int LRU));
void removeLRU _ARGS_((file_id *fileID));
int accessLRU _ARGS_((nameRec *namePtr, int machine, int LRU, int new, int
	migrated));
void dumpLRU _ARGS_((int LRU));
LRUentry * searchLRU _ARGS_((nameRec *namePtr, int machine, int *pos, int LRU));
void collectHashStatistics _ARGS_((nameRec *namePtr));
void garbageCollect _ARGS_((void));
void validateLRU _ARGS_((char *str));

/* Histogram operations */
void addHist _ARGS_((int val,int table[],int max));
void dumpHist _ARGS_((int *table, int max, int total));
void dumpHistGraph _ARGS_((int *table, int max, int total, int cumul, float
	xoff));
void sumHist _ARGS_((int *h1, int *h2, int *sum, int max));
 
/*
 ****************************************************************************
 *	Handle getattr and figure out the directories accessed
 ****************************************************************************
 */
void
dogetattr(data, machine)
    int *data;
    int machine;
{
/*
 * Getattr: 1:currHost, 2:homeHost, 3:fileID
 */
    int currHost = data[1];
    int homeHost = data[2];
    nameRec *namePtr;

    numGetAttrID++;
    if (homeHost == -1) homeHost = currHost;
    namePtr = newID(&data[3],IFNEW,(char *)NULL, UNKNOWN);
    (void)accessDir(ATTR_LOOKUP,namePtr,(char *)NULL,currHost, homeHost,
	    UNKNOWN);
    doneID(namePtr);
}

/*
 ****************************************************************************
 *	Handle setattr and figure out the directories accessed
 ****************************************************************************
 */
void
dosetattr(data, machine)
    int *data;
    int machine;
{
/*
 * Setattr: 1:currHost, 2:homeHost, 3:fileID
 */
    int currHost = data[1];
    int homeHost = data[2];
    nameRec *namePtr;

    namePtr = newID(&data[3],IFNEW,(char *)NULL, UNKNOWN);
    if (namePtr->fileID[3] == remember_file) {
	/*
	 * We did a SetAttrPath, and this operation followed.
	 * So we just return ignore this record.
	 */
    } else {
	/*
	 * No setAttrPath, so this must be a setAttrID.
	 * Unfortunately, we lost the host numbers, so we'll have to
	 * ignore the operation.  Since setAttrID is about 1% of the sets,
	 * this shouldn't matter too much.
	 */
	numSetAttrID++;
	if (currHost>0 && homeHost>0) {
	    (void)accessDir(ATTR_MODIFY,namePtr,(char *)NULL,currHost, homeHost, UNKNOWN);
	}
    }
    doneID(namePtr);
}

/*
 ****************************************************************************
 *	Handle opens and figure out the directories accessed
 ****************************************************************************
 */
void
doopen(data, machine)
    int *data;
    int machine;
{
/*
 * Open: 1:currHost, 2:homeHost, 3:fileID, 7:streamID, 11:effID, 12:realID,
 * 13:mode, 14:#reading, 15:#writing, 16:create, 17:fileSize, 18:modify,
 * 19:type, 20:consist.
 */
    int type = data[19];
    int mode = data[13];
    int currHost = data[1];
    int homeHost = data[2];
    nameRec *namePtr;
    Hash_Entry *entryPtr;
    int created;

    namePtr = newID(&data[3],IFNEW,(char *)NULL, type);

    if (type == FS_DIRECTORY) {
	numDirectoryOpens++;
	(void)accessDir(DIR_READ,namePtr,(char *)NULL,currHost, homeHost ,FS_DIRECTORY);
	if (mode & FS_WRITE) {
	    error("Write to directory\n");
	}
    }
    if (mode & FS_WRITE) {
	(void)accessDir(ATTR_MODIFY,namePtr,(char *)NULL,currHost, homeHost, UNKNOWN);
	numWrites++;
    } else {
	numReads++;
    }
    entryPtr = Hash_CreateEntry(streamTablePtr, (Address)&data[7], &created);
#if 0
    printf("Opening %x,%x,%x,%x by stream %x,%x,%x,%x\n", data[3], data[4],
	    data[5], data[6], data[7], data[8], data[9], data[10]);
#endif

    if (created) {
	hashOpen++;
	entryPtr->clientData = (ClientData) malloc(4*sizeof(int));
	mallocOpen += 4*sizeof(int);
	bcopy(&data[3], entryPtr->clientData, 4*sizeof(int));
    } else {
	error("Stream in use\n");
    }
    namePtr->numberOfOpens++;
    namePtr->numUsers++;
    namePtr->numActiveUsers++;
    if (namePtr->numberOfOpens > 1) {
	fileSharing++;
    }
    namePtr->openMode |= (mode & (FS_READ | FS_WRITE | FS_EXECUTE));
    doneID(namePtr);
}

/*
 ****************************************************************************
 *	Handle lookups and figure out the directories accessed
 ****************************************************************************
 */
void
doclose(data, machine)
    int *data;
    int machine;
{
/*
 * Close: 1:streamID, 5:offset, 6:size, 7:flags, 8:rwflags, 9:refCount,
 * 10:consist
 */

    int flags = data[7];
    int refCount = data[9];

    Hash_Entry *entryPtr;
    nameRec *namePtr;

    entryPtr = Hash_FindEntry(streamTablePtr, (Address)&data[1]);
    if (entryPtr == NULL) {
	unmatchedCloses++;
    } else {
	if ((flags & FS_RMT_SHARED) && refCount != 0) {
	} else {

	    namePtr = hashID(entryPtr->clientData);
	    if (namePtr==NULL) {
		error("****BAD CLOSE");
		return;
	    }
	    if (namePtr->numberOfOpens <= 0) {
		error("Too many closes\n");
	    } else {
		namePtr->numberOfOpens--;
		namePtr->numUsers--;
		namePtr->numActiveUsers--;
		if (namePtr->numberOfOpens == 0) {
		    namePtr->openMode = 0;
		    freeIDIfDone(namePtr);
		}
	    }
	    free(entryPtr->clientData);
	    mallocOpen -= 4*sizeof(int);
	    Hash_DeleteEntry(streamTablePtr, entryPtr);
	    hashOpen--;
	}
    }
}

int lastOp = -1;
int hitLen[20]={0};
int hitCount=0;
#define MAX(a,b) ((a)>(b)?(a):(b))
#define DOLEVEL if (level==-1) {comp++;hit=9999;} else if (level>0) {comp++;hit=MAX(hit,level);}
int pathHitTable[LEVEL_TABLE_SIZE] = {0}; /* LRU position when valid */
/*
 ****************************************************************************
 *	Handle lookups and figure out the directories accessed
 ****************************************************************************
 */
/*
 * Lookup: 1:hostID, 2:homeHostID, 3:streamID, 7:status, 8:numIds, 9:op
 */
void
dolookup(data, machine)
    int *data;
    int machine;
{
    int i;
    int op,returnStatus, numIds;
    int returnType;
    int inTmp;
    int comp=0,hit=0;
    int currHost = data[1];
    int homeHost = data[2];
    nameRec *returnPath = NULL;
    nameRec *path[20];
    int level;

    bzero(path,sizeof(path));
    if (version != VERSION2) {
	fprintf(stderr,"Warning: old version: %d\n", version);
    }

    op = data[9]&0xff;
    returnStatus = data[7];
    returnType = data[9]>>8;
    numIds = data[8];
    if (homeHost < -1) homeHost = currHost; /* Bug? */
    if (debug) {
	fprintf(stderr,"LOOKUP: hostID: %d, home: %d, %s, numIDs %d, op %s, type %d\n", 
		currHost, homeHost, status(returnStatus), numIds,
		opname(op), returnType);
    }
    if (homeHost>=NUM_MACHINES || homeHost<0 || currHost<0) {
	error(sprintf(charBuf,"***Invalid machine number: %d,%d\n",
		homeHost, currHost));
	homeHost = NUM_MACHINES-1;
	currHost = NUM_MACHINES-1;
    }
    if (homeHost != currHost) migCount++;
    if (numIds>=10) {
	overflow++;
    }
    if (lastOp == 0x8a && op == FS_DOMAIN_REMOVE) {
	numMoveOps++;
    }
    if (returnStatus == SUCCESS) {
	lastOp = op;
    } else {
	lastOp = -1;
    }
    machineTable[currHost]++;
    inTmp = 0;
    for (i=0;i<numIds-1;i++) {
	if (!inTmp && !DIFF_FD(ID_NUM(i),tmpDir)) {
	    tmpAccess++;
	    {
	fprintf(stderr,"tmp LOOKUP: hostID: %d, home: %d, %s, numIDs %d, op %s, type %d %s", 
		currHost, homeHost, status(returnStatus), numIds,
		opname(op), returnType, ctime(&sec));
	    }
	fprintf(stderr,"nRValidates: %d, nRHits: %d, nAccesses: %d nforced %d\n",
	nRValidates[TMP_MEAS], nRHits[TMP_MEAS], nAccesses[TMP_MEAS],
		forcedMiss[TMP_MEAS]);
	    inTmp = 1;
	}
	if (ID_NUM(i+1)[0]==-1) {
	    path[i] = newID(ID_NUM(i),IFNEW,(char *)NULL, FS_SYMBOLIC_LINK);
	} else {
	    path[i] = newID(ID_NUM(i),IFNEW,(char *)NULL, FS_DIRECTORY);
	}
    }
    path[numIds-1] = newID(ID_NUM(numIds-1),IFNEW,(char *)NULL,UNKNOWN);
    returnPath = newID(RETURN_ID,IFNEW,(char *)NULL, returnType);
    if (op==0x8a) op = HARDLINK_2;
    if (op==-1) op = 4; /* To fix a bug in the tracing. */
    if ((op>=0 && op<11) || op==HARDLINK_2) {
	ops[op]++;
	if ((path[0]->flags)&PREFIX) {
	    int prefNum;
	    int partCnt;
	    prefNum = path[0]->flags&~PREFIX;
	    prefixRef++;
	    prefixRefNum[prefNum]++;
	    if (prefixParts[prefNum][0] != -1) {
		for (partCnt=0;;partCnt += 4) {
		    if (prefixParts[prefNum][partCnt+4]==-1) break;
		    level = accessDir(NAME_LOOKUP,hashID(&prefixParts[prefNum]
			    [partCnt]),
			    hashID(&prefixParts[prefNum][partCnt+4]),
			    currHost, homeHost, FS_DIRECTORY);
		    DOLEVEL;
		}
		if (prefixParts[prefNum][partCnt]!=-1) {
		    level =accessDir(NAME_LOOKUP,hashID(&prefixParts[prefNum]
			    [partCnt]), path[0], currHost, homeHost,
			    FS_DIRECTORY);
		    DOLEVEL;
		}
	    }
	}

	if (numIds <=0) {
	    error("NumIds <= 0 !\n");
	}
	/*
	 * We have entries: 0,1,2,3,...,nIds-1,return.
	 * Here we lookup 0:1, 1:2, ..., nIds-2:nIds-1
	 */
	for (i=0;i<=numIds-2;i++) {
	    level =accessDir(NAME_LOOKUP,path[i],path[i+1],currHost,
		    homeHost, i==numIds-2?FS_DIRECTORY:UNKNOWN);
	    DOLEVEL;
	}
	if (!DIFF_FD(RETURN_ID,ID_NUM(numIds-1))) {
	    /* RETURN_ID == last lookup */
	} else if (DIFF_FD(RETURN_ID, NULL_ID)) {
	    /* Unique RETURN_ID */
	    if (returnStatus == SUCCESS &&
		    (op==FS_DOMAIN_OPEN || op==HARDLINK_2 ||
		    op==FS_DOMAIN_MAKE_DIR)) {
		/* This is normal */
	    } else {
		error(sprintf(charBuf,"Different returnID (%x,%x,%x,%x) for op %s, status %s\n",
			RETURN_ID[0], RETURN_ID[1], RETURN_ID[2], RETURN_ID[3],
			opname(op), status(returnStatus)));
	    }
	} else {
	    /* Null RETURN_ID */
	}
	if (returnStatus==SUCCESS) {
	    statusTable[op][0]++;
	    typeUsage[returnType]++;
	} else {
	    statusTable[op][(returnStatus&0xf)+1]++;
	}
	if (returnStatus == FS_FILE_NOT_FOUND) {
		/*
		returnStatus==FS_NO_ACCESS ||
		returnStatus==FS_IS_DIRECTORY ||
		returnStatus==FS_NOT_DIRECTORY ||
		returnStatus==FS_DIR_NOT_EMPTY) {}
		*/
	    level =accessDir(NAME_LOOKUP,path[numIds-1],(char *)NULL,currHost,
		    homeHost, UNKNOWN);
	    DOLEVEL;
	}
	if (returnStatus == SUCCESS) {
	    switch (op) {
		default:
		case FS_DOMAIN_IMPORT:
		case FS_DOMAIN_EXPORT:
		case FS_DOMAIN_MAKE_DEVICE:
		    error(sprintf(charBuf,"Wasn't expecting op %d\n", op));
		    break;
		case FS_DOMAIN_GET_ATTR:
		    if (DIFF_FD(ID_NUM(numIds-1), RETURN_ID)) {
			error("Expected same returnID (3)\n");
		    }
		    level =accessDir(ATTR_LOOKUP,returnPath,(char *)NULL,currHost,
			    homeHost, returnType);
		    DOLEVEL;
		    break;
		case FS_DOMAIN_SET_ATTR:
		    if (DIFF_FD(ID_NUM(numIds-1), RETURN_ID)) {
			error("Expected same returnID (4)\n");
		    }
		    remember_host = currHost;
		    remember_home = homeHost;
		    remember_file = returnPath->fileID[3];
		    level =accessDir(ATTR_MODIFY,returnPath,(char *)NULL, currHost,
			    homeHost, returnType);
		    DOLEVEL;
		    modifies[4]++;
		    break;
		case FS_DOMAIN_REMOVE_DIR:
		    if (DIFF_FD(RETURN_ID, NULL_ID)) {
			error("Expected 0 returnID (8)\n");
		    }
		    level =accessDir(NAMEATTR_REMOVE,path[numIds-2],
			    path[numIds-1],currHost, homeHost, returnType);
		    DOLEVEL;
		    modifies[8]++;
		    break;
		case FS_DOMAIN_OPEN:
		    if (DIFF_FD(ID_NUM(numIds-1), RETURN_ID)) {
			doneID(returnPath);
			returnPath = newID(RETURN_ID,MAKENEW,(char *)NULL, returnType);
			level =accessDir(NAMEATTR_CREATE,path[numIds-1],
				returnPath, currHost, homeHost, returnType);
			DOLEVEL;
			modifies[2]++;
		    }
		    break;
		case FS_DOMAIN_REMOVE:
		    if (DIFF_FD(RETURN_ID, NULL_ID)) {
			error("Expected 0 return (7)\n");
		    }
		    level =accessDir(NAMEATTR_REMOVE,path[numIds-2],
			    path[numIds-1], currHost, homeHost, returnType);
		    DOLEVEL;
		    modifies[7]++;
		    break;
		case FS_DOMAIN_RENAME:
		    printf("Rename?");
		    break;
		case FS_DOMAIN_HARD_LINK: /* hardlink */
		    if (DIFF_FD(ID_NUM(numIds-1), RETURN_ID)) {
			error("Expected same returnID (10)\n");
		    }
		    level =accessDir(NAME_MODIFY,path[numIds-2],
			    returnPath, currHost, homeHost, returnType);
		    DOLEVEL;
		    modifies[10]++;
		    break;
		case HARDLINK_2: /* hardlink, part 2 */
		    if (!DIFF_FD(ID_NUM(numIds-1), RETURN_ID)) {
			error("Expected different returnID (10)\n");
		    }
		    level =accessDir(NAME_MODIFY,path[numIds-1],
			    returnPath, currHost, homeHost, returnType);
		    DOLEVEL;
		    modifies[10]++;
		    break;
		case FS_DOMAIN_MAKE_DIR:
		    if (!DIFF_FD(ID_NUM(numIds-1), RETURN_ID)) {
			error("Expected different returnID (6)\n");
		    }
		    doneID(returnPath);
		    returnPath = newID(RETURN_ID,MAKENEW,(char *)NULL, returnType);
		    level =accessDir(NAMEATTR_CREATE,path[numIds-1],
			    returnPath, currHost, homeHost, returnType);
		    DOLEVEL;
		    modifies[6]++;
		    break;
	    }
	}
    } else {
	error(sprintf(charBuf,"*** Bad op %x!\n", op));
	badOp++;
    }
    for (i=0;i<numIds;i++) {
	doneID(path[i]);
    }
    doneID(returnPath);
    nlookups++;
    if (currHost != homeHost) {
	nMigLookups++;
    }
    ncomps += numIds;
    if (entryCnt >= gcMax) {
	 garbageCollect();
    }
    hitLen[comp]++;
    hitCount++;
    addHist(hit,pathHitTable,LEVEL_TABLE_SIZE);
}
/*
 ****************************************************************************
 *	Print the results of the traces
 ****************************************************************************
 */

char *graphNames[] = {"GNamecache.x", "GAttrcache.x", "GEntrycache.x",
    "GNamecacheMig.x", "GAttrcacheMig.x", "GMigMeas.x"};
char *graphTitles[] = { "Name cache performance",
    "Attribute cache performance", "Name entry cache performance",
    "Name cache performance (no mig)", "Attribute cache performance (no mig)",
    "Name cache for migrated procs."};

void
donerecs()
{
    int i;

    validateLRU("end");
    dumpHash();

    totalSecs = sec - firstSecs;
    printf("%d sec start, %d sec end : %s", (int)firstSecs, (int)sec,
	    ctime(&firstSecs));
    printf("Total # of seconds = %.3f (%.3f hours), ends %s", totalSecs,
	    totalSecs/3600, ctime(&sec));

    printLookupStats();

#if 0
    for (i=0;i<NUM_LRU_LISTS;i++) {
	type = LRUWord[thisLRU];
	printf("-------------------%s---------------\n",type);
	printf("\n%s's depth in LRU list when found:\n", type);
	dumpHist(ValidhistTable[i], LEVEL_TABLE_SIZE,
	    nRValidates[i]+nRRevalidates[i]+nRHits[i]);

	printf("\n%s's depth in LRU list when found invalid (consistency miss):\n",
	        type);
	dumpHist(foundInvalidTable[i], LEVEL_TABLE_SIZE,
	    nRValidates[i]+nRRevalidates[i]+nRHits[i]);

	printf("\nConsistency above hits:\n");
	sumHist(ValidhistTable[i], foundInvalidTable[i],
		foundInvalidTable[i], LEVEL_TABLE_SIZE);
	dumpHist(foundInvalidTable[i], LEVEL_TABLE_SIZE,
	    nRValidates[i]+nRRevalidates[i]+nRHits[i]);

	printf("\n%s's depth in LRU list when invalidated:\n", type);
	dumpHist(InvalidhistTable[i], LEVEL_TABLE_SIZE, -1);

	printf("\n%s's idle age when used:\n", type);
	dumpHist(ageTable[i], AGE_TABLE_SIZE, -1);


	dumpLRU(i);
    }
    printf("---------------------------------------------\n");


    printf("\nNumber of machines used directory:\n");
#endif

    if (thisLRU==0) {
	printf("##file GNumInvalid.x\n");
	printf("BarGraph: 1\nBarWidth: .1\nNoLines: 1\n");
	printf("TitleText: Number of invalidations\n");
	printf("XUnitText: Number of invalidations\n");
	printf("YUnitText: Percent\n");
	printf("YStep: 10\nXStep: 1\nXLowLimit: 0\nXHighLimit: 5\n");
	puts("ZeroWidth: 2\nXFormat: %2.0f\nYFormat: %3.0f\n");
	printf("Device: Gremlin\nDisposition: To File\n");
    } else {
	printf("##append GNumInvalid.x\n");
    }
    for (i=0;i<NUM_LRU_LISTS;i++) {
	printf("\n\"%s\n", graphTitles[thisLRU]);
	dumpHistGraph(invalidatedMachinesTable[i],5, -1, TRUE, thisLRU/10.);
    }
    printf("##end\n");


#define GGRAPHSTR "Markers: 0\nYUnitText: Percent\nYLowLimit: 0\n\
YHighLimit: 100\nYStep: 10\nXFormat: %2.0f\nYFormat: %3.0f\n\
XUnitText: Number cached\n\
Geometry: =600x300\n\
Device: Gremlin\nDisposition: To File\nGremlin.OutputAxisSize: 12\n"


    if (thisLRU==NAME) dumpLRU(NAME);


    for (i=0;i<NUM_LRU_LISTS+EXTRA;i++) {
	if (nRValidates[i]+nRRevalidates[i]+nRHits[i] != nAccesses[i]) {
	    fprintf(stderr,"Access count error\n");
	    fprintf(stderr,"%d,%d,%d,%d %d\n", nRValidates[i],
		    nRRevalidates[i], nRHits[i], nAccesses[i],i);
	}
	if (i==0) {
	    printf("##file %s\n%s",graphNames[thisLRU], GGRAPHSTR);
	    printf("TitleText: %s\n", graphTitles[thisLRU]);
	} else if (i==1) {
	    printf("##file GMig%s\n%s",graphNames[thisLRU], GGRAPHSTR);
	    printf("TitleText: %s (only mig)\n", graphTitles[thisLRU]);
	} else {
	    printf("##file GTmp%s\n%s",graphNames[thisLRU], GGRAPHSTR);
	    printf("TitleText: %s (/tmp)\n", graphTitles[thisLRU]);
	}
	printf("XLowLimit: 0\nXHighLimit: %d\n", LEVEL_TABLE_SIZE);
	printf("\n\"Compulsory misses\n0 100\n100 100\n");
	if (forcedMiss[i] != 0) {
	printf("\n\"Entry misses\n0 %f\n100 %f\n",
		100-(nRValidates[i]-forcedMiss[i])*100./nAccesses[i],
		100-(nRValidates[i]-forcedMiss[i])*100./nAccesses[i]);
	}
	printf("\n\"Capacity misses\n0 %f\n100 %f\n",
		100-nRValidates[i]*100./nAccesses[i],
		100-nRValidates[i]*100./nAccesses[i]);
	printf("\n\"Consistency misses\n");
	sumHist(ValidhistTable[i], foundInvalidTable[i],
		foundInvalidTable[i], LEVEL_TABLE_SIZE);
	dumpHistGraph(foundInvalidTable[i], LEVEL_TABLE_SIZE,
	    nAccesses[i], TRUE, 0.0);
	printf("\n\"Hits\n");
	dumpHistGraph(ValidhistTable[i], LEVEL_TABLE_SIZE,
	    nAccesses[i], TRUE, 0.0);
	printf("##end\n");
    }

    if (thisLRU==0) {
	printf("##file GPathHit.x\n");
	printf("%s",GGRAPHSTR);
	printf("TitleText: Hits over entire path\n");
	printf("XLowLimit: 0\nXHighLimit: %d\n", LEVEL_TABLE_SIZE);
	printf("\n\"Component hit rate\n");
	dumpHistGraph(ValidhistTable[0], LEVEL_TABLE_SIZE,
	    nAccesses[0], TRUE, 0.0);
	printf("\n\"Path hit rate\n");
	dumpHistGraph(pathHitTable, LEVEL_TABLE_SIZE, hitCount, TRUE, 0.0);
	printf("\n\"Predicted path hit rate\n");
	{
	    int cumul=0,j;
	    float prod;
	    float t;
	    for (i=0;i<LEVEL_TABLE_SIZE-3;i++) {
		cumul += ValidhistTable[0][i];
		prod = 1;
		t = 0;
		for (j=0;j<20;j++) {
		    t += hitLen[j]*prod;
		    prod *= cumul/(float)nAccesses[0];
		}
		printf("%d %5.2f\n", i, t/hitCount*100);
	    }
	}
	printf("##end\n");
    }

    if (thisLRU==0) {
	printf("##file GInvSize.x\n");
	printf("TitleText: Invalidations vs. size\n");
	puts(GGRAPHSTR);
	printf("YHighLimit: 7\n");
	printf("YStep: 1\n");
	printf("XLowLimit: 0\nXHighLimit: 50\nXStep: 10\n");
    } else {
	printf("##append GInvSize.x\n");
    }
    for (i=0;i<NUM_LRU_LISTS;i++) {
	printf("\n\"%s\n", graphTitles[thisLRU]);
	dumpHistGraph(InvalidhistTable[i], LEVEL_TABLE_SIZE,
	    nAccesses[i], TRUE, 0.0);
    }
    printf("##end\n");

    if (thisLRU==0) {
	printf("##file GIdleAge.x\n");
	printf("TitleText: Idle age when used\n");
	puts(GGRAPHSTR);
	printf("XUnitText: seconds\n");
	printf("YHighLimit: 100\n");
	printf("LogX: 1\n");
    } else {
	printf("##append GIdleAge.x\n");
    }
    for (i=0;i<NUM_LRU_LISTS;i++) {
	printf("\n\"%s\n", graphTitles[thisLRU]);
	ageTable[i][1] += ageTable[i][0];
	ageTable[i][0] = 0;
	dumpHistGraph(ageTable[i], AGE_TABLE_SIZE, -1, TRUE, 0.0);
    }
    printf("##end\n");

}

void
printLookupStats()
{
    int op;
    int i;
    int nOps=0;
    int statusTotals[20];
    FILE *prefixFile;

    fprintf(stderr,"mallocOpen: %d, mallocName: %d, mallocLRU: %d\n",
	    mallocOpen, mallocName, mallocLRU);
    fprintf(stderr,"mallocHash: %d, mallocTable: %d\n", mallocHash,
	    mallocTable);
    fprintf(stderr,"hashOpen: %d, hashName: %d\n", hashOpen, hashName);

    for (op=0;op<NUM_OPS;op++) {
	nOps += ops[op];
    }
    printf("--Results--\n");

    printf("\n");
    printf("Number of lookup calls: %d\n", nlookups);
    printf("%d of these had a bad operation field.\n", badOp);

    printf("\nOperations:\n");
    for (op=0;op<NUM_OPS;op++) {
	if (ops[op]>0) {
	    printf("  %s: %d %5.2f%%, %d mods\n", opname(op),
		ops[op], ops[op]*100./nOps, modifies[op]);
	    printf("      %s: %d %5.2f%% %5.2f%%\n", status(0),
		statusTable[op][0], statusTable[op][0]*100./ops[op]);
	    for (i=0;i<19;i++) {
		if (statusTable[op][i+1]>0) {
		    printf("      %s: %d %5.2f%% %5.2f%%\n",
			    status((int)0x40000+i),
			    statusTable[op][i+1],
			    statusTable[op][i+1]*100./nlookups,
			    statusTable[op][i+1]*100./ops[op]);
		}
	    }
	}
    }

    if (thisLRU==0) {
	printf("##file TMiscMeasure.d\n");
	printf("Lookups\t%d\n", nlookups);
	printf("Migrated lookups\t%d\n", nMigLookups);
	/*
	printf("Accesses to directories\t%d\n", nCompLookups);
	printf("Prefix references\t%d\n", prefixRef);
	*/
	printf("Symbolic links encountered\t%d\n", numLinksEncountered);
	printf("/tmp accesses\t%d\n", tmpAccess);
	printf("Directory opens as a file\t%d\n", numDirectoryOpens);
	printf("Duration of trace (secs)\t%5.0f\n", totalSecs);
	printf("##end\n");
    }
    if (thisLRU==NAME) {
	printf("##append TMiscMeasure.d\n");
	printf("_\nName accesses\t%d\n", nAccesses[0]);
	printf("  Invalidates\t%d\n", nInvalidates[0]);
	printf("##end\n");
    } else if (thisLRU==ATTR) {
	printf("##append TMiscMeasure.d\n");
	printf("_\nAttribute accesses\t%d\n", nAccesses[0]);
	printf("  For permissions\t%d\n", nAttrPermReads);
	printf("  Invalidates\t%d\n", nInvalidates[0]);
	printf("##end\n");
    }

    printf("##file TMiscAttr.d\n");
    printf("Accesses to open write file attributes\t%d\n",
	    attributesWhileReading);
    printf("Accesses to open read file attributes\t%d\n",
	    attributesWhileWriting);
    printf("_\n");
    printf("Fstat\t%d\n", numGetAttrID); 
    printf("FsetStat\t%d\n", numSetAttrID); 
    printf("_\n");
    printf("Stat\t%d\n", ops[3]); 
    printf("SetStat\t%d\n", ops[4]); 
    printf("_\n");
    printf("Permission accesses\t%d\n", nAttrPermReads);
    printf("_\n");
    printf("Total accesses\t(elsewhere)\n");
    printf("##end\n");

    printf("##file TOpBreakdown.d\n");
    for (op=0;op<NUM_OPS;op++) {
	if (ops[op]>0) {
	    printf("%s\t%d\t%5.2f\t%5.2f\t%5.2f\t%5.2f\n", opname(op),
		ops[op], ops[op]*100./nOps,
		statusTable[op][0]*100./ops[op],
		statusTable[op][13]*100./ops[op],
		statusTable[op][10]*100./ops[op]);
	}
    }
    printf("_\n(Rename)\t%d\n", numMoveOps);
    for (i=0;i<20;i++) {
	statusTotals[i]=0;
	for (op=0;op<NUM_OPS;op++) {
	    statusTotals[i] += statusTable[op][i];
	}
    }
    printf("_\nTotals\t%d\t%5.2f\t%5.2f\t%5.2f\t%5.2f\n", nlookups, 100.,
	    statusTotals[0]*100./nlookups, statusTotals[13]*100./nlookups,
	    statusTotals[10]*100./nlookups);
    printf("##end\n");

    printf("\nStatus totals:\n");
    printf("      %s: %d (%5.2f%%)\n", status(0), statusTotals[0],
	statusTotals[0]*100./nlookups);
    for (i=0;i<19;i++) {
	if (statusTotals[i+1]>0) {
	    printf("      %s: %d (%5.2f%%)\n", status((int)0x40000+i),
	    statusTotals[i+1], statusTotals[i+1]*100./nlookups);
	}
    }

    printf("\nAvg # components returned: %5.2f\n", ncomps/(float)nlookups);

    for (i=0;i<NUM_LRU_LISTS;i++) {
	char *type;
	type = LRUWord[thisLRU];
	printf("\n%s lookup statistics\n", type);
	printf("%d %s invalidated (removed from LRU lists for invalidation)\n",
		nInvalidates[i], type);
	printf("%d remote %s removes (#machines with entry removed for destruction)\n",
		nRRemoves[i], type);
	printf("%d machine-%s invalidated (#machines with entry removed for invalidation)\n", nMInvalidates[i],type);
	printf("%d remote %s validates (%5.2f)(compulsory)(0)\n",
		nRValidates[i], type, nRValidates[i]*100./nAccesses[i]);
	printf("%d remote %s revalidates (#invalidated entries reused (1))\n", nRRevalidates[i], type);
	printf("%d remote %s hits (#valid entries used(2))\n", nRHits[i],
	    type);
	printf("%d total %s accesses (sum of the above)\n", nAccesses[i],
		type);
    }
    
    printf("\n%d accesses to directories\n", nCompLookups);
    printf("\n%d prefix references\n", prefixRef);
    printf("\n%d hash table entries created\n", entryCnt);
    if (prefixList != NULL) {
	prefixFile = fopen(prefixList,"r");
	if (prefixFile == NULL) {
	    perror(prefixList);
	    exit(-1);
	}
	for (i=1;i<NUM_PREFIXES;i++) {
	    char buf[100],buf2[100];
	    fgets(buf2,99,prefixFile);
	    sscanf(buf2,"%s",buf);
	    fgets(buf2,99,prefixFile);
	    if (prefixRefNum[i]>0) {
		printf("Prefix %s: %d refs\n", buf, prefixRefNum[i]);
	    }
	}
	fclose(prefixFile);
    }
#if 0
    printf("\nMachine statistics:\n");
    for (i=0;i<NUM_MACHINES;i++) {
	if (machineTable[i]>0) {
	    printf("Machine %d: %d ops\n", i,machineTable[i]);
	}
    }
#endif

    printf("\nReturn type on success:\n");
    for (i=0;i<NUM_OPS;i++) { if (typeUsage[i]>0) {
	    printf("\t%s: %d\n", filetype(i), typeUsage[i]);
	}
    }

    printf("\nReused hash table entries: %d\n", reusedID);

    printf("\nNumber of directory opens: %d\n", numDirectoryOpens);

    printf("\nNumber of unmatched opens: %d\n", unmatchedOpens);
    printf("Number of unmatched closes: %d\n", unmatchedCloses);

    printf("\nAccesses to attributes while file being read: %d\n", 
	    attributesWhileReading);
    printf("Accesses to attributes while file being written: %d\n",
	    attributesWhileWriting);
    printf("Accesses to attributes while file executing: %d\n",
	    attributesWhileExecuting);
    printf("Accesses to attributes while file closed: %d\n",
	    attributesWhileSafe);
    printf("Number of write opens: %d, read opens: %d\n", numWrites,
	    numReads);

    printf("\nSymbolic links: %d\n", numLinksEncountered);

    printf("\nMicro ops:\n");
    for (i=0;i<NUM_OPS;i++) {
	if (numMicroFuncs[i]>0) {
	    printf("    %s: %d\n", microNames[i], numMicroFuncs[i]);
	}
    }

    printf("\nGetAttrID: %d, SetAttrID: %d\n", numGetAttrID, numSetAttrID);

    printf("\nFile sharing: %d\n", fileSharing);

    printf("\nOverflow: %d\n", overflow);

    printf("\n/tmp accesses: %d\n", tmpAccess);

    printf("\nMigrated lookups: %d\n", migCount);

    printf("\nGC errors: %d\n", gcError);

    printf("\nNumber of Move operations (link,delete) = %d\n", numMoveOps);

}

/*
 * Results2: end of the results.
 */

/*
 * Dump hash table entries.
 */
void
dumpHash()
{
    Hash_Search search;
    Hash_Entry *entryPtr;

    entryPtr = Hash_EnumFirst(tablePtr, &search);
    while (entryPtr != NULL) {
	collectHashStatistics((nameRec *)(entryPtr->clientData));
        entryPtr = Hash_EnumNext(&search);
    }
}

/*
 * Dump LRU results.
 */
void dumpLRU(LRU)
    int LRU;
{
    LRUentry *entry, *oldentry;
    int count;
    int machine;
    int time;
#define NUM_TO_DUMP 100
    int min[NUM_TO_DUMP], max[NUM_TO_DUMP], avg[NUM_TO_DUMP],
	    cnt[NUM_TO_DUMP];
    printf("##file GEntryAge.x\n");
    printf("Markers: 0\nTitleText: Directory age at completion\n");
    printf("XUnitText: Position\nYUnitText: Age(sec)\n");
    printf("YLowLimit: 0\nYHighLimit: 1200\n");
    printf("XLowLimit: 0\nXHighLimit: 50\n");
    printf("XStep: 20\nYStep: 120\n");
    puts("XFormat: %2.0f\nYFormat: %3.0f\n");
    printf("Device: Gremlin\nDisposition: To File\n");

    CHECKLRU(LRU);
    for (count=1;count<NUM_TO_DUMP;count++) {
	min[count] = 99999999;
	max[count] = 0;
	avg[count] = 0;
	cnt[count] = 0;
    }
    for (machine=1;machine<NUM_MACHINES;machine++) {
	oldentry = &LRUlist[LRU][machine];
	for (count=1;count<100;count++) {
	    entry = oldentry->down;
	    if (entry==NULL) {
		break;
	    }
	    if (entry->nameInfo != NULL) {
		if (GET_MACHINEINFO(entry->nameInfo->machineInfo[LRU],machine)
			!=1) {
		    count--;
		} else {
		    time = sec - entry->lastAccess;
		    if (time<min[count]) min[count] = time;
		    if (time>max[count]) max[count] = time;
		    cnt[count]++;
		    avg[count] += time;
		}
	    }
	    oldentry = entry;
	}
    }
    printf("\n\"Max\n");
    for (count=1;count<100;count++) {
	if (cnt[count]>0) printf("%d %d\n", count, max[count]);
    }
    printf("\n\"Avg\n");
    for (count=1;count<100;count++) {
	if (cnt[count]>0) printf("%d %5.2f\n", count, avg[count]/
		(float)cnt[count]);
    }
    printf("\n\"Min\n");
    for (count=1;count<100;count++) {
	if (cnt[count]>0) printf("%d %d\n", count, min[count]);
    }
    printf("##end\n");
}

/*
 ****************************************************************************
 *	Handle low-level operations (access directory, modify directory)
 ****************************************************************************
 */
#define MAXHIT(h,l) ((l)<0?9999:((h)>(l)?(h):(l)))

static char *accessDirNames[] = {"???", "NAME_LOOKUP", "ATTR_LOOKUP",
	"NAME_MODIFY", "ATTR_MODIFY", "???", "NAMEATTR_REMOVE",
	"NAMEATTR_CREATE", "DIR_READ"};

/*
 * Called when we access a directory block.
 * We access an entry in the parent directory and do some function on it.
 * We may read the entry, its attributes, remove it, modify it, or delete it.
 *
 * Return the maximum name cache size required for success.
 *
 * We assume that (except for NAME_LOOKUP) we have already done any
 * necessary name resolution on the entry.
 *
 * Ops:
 *  NAME_LOOKUP(parent, entry)
 *  ATTR_LOOKUP(file,NULL)
 *  ATTR_MODIFY(file,NULL)
 *  NAME_MODIFY(parent,entry)
 *  NAMEATTR_REMOVE(parent,entry)
 *  NAMEATTR_CREATE(parent,entry)
 *  DIR_READ(directory, NULL)
 */
int
accessDir(function, dirIDName, entryIDName, currHost, homeHost, type)
    int function;
    nameRec *dirIDName;
    nameRec *entryIDName;
    int currHost;
    int homeHost;
    int type;
{
    int migrated;
    int level=0;
    if (function<0 || function >= sizeof(accessDirNames)/sizeof(char*)) {
	error("Bad function in accessDir\n");
    }
    if (debug) {
	printf("accessDir: %s on ", accessDirNames[function]);
	if (dirIDName != NULL && dirIDName != (nameRec *)-1) {
	    printf("(%x,%x,%x,%x)", dirIDName->fileID[0], dirIDName->fileID[1],
		    dirIDName->fileID[2], dirIDName->fileID[3]);
	} else {
	    printf("(NULL)\n");
	}
	if (entryIDName != NULL && entryIDName != (nameRec *)-1) {
	    printf("/(%x,%x,%x,%x)\n", entryIDName->fileID[0],
		    entryIDName->fileID[1], entryIDName->fileID[2],
		    entryIDName->fileID[3]);
	} else {
	    printf("/(NULL)\n");
	}
    }
    if (dirIDName==NULL) {
	error("Warning: NULL dirIDName to accessDir\n");
	return 0;
    }
    if (dirIDName==(nameRec *)-1 || entryIDName==(nameRec *)-1) {
	return 0;
    }
    if (currHost<0 || homeHost<0) {
	error("accessDir: negative machine\n");
	return 0;
    }
    if (currHost >= NUM_MACHINES || homeHost >= NUM_MACHINES) {
	error("accessDir: machine number too big!!!*****\n");
	return 0;
    }
    nCompLookups++;
    migrated = ((currHost != homeHost)?1:0)|((dirIDName==tmpName)?2:0);
    numMicroFuncs[function]++;
    switch (function) {
        case NAME_LOOKUP:
	    nAttrPermReads++;
#ifdef MULTI_OPS
	    (void) accessLRU(dirIDName,currHost,ATTR, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,ATTR_MIG, FALSE, migrated);
#endif
	    level = accessLRU(dirIDName,currHost,NAME, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,NAME_MIG, FALSE, migrated);
	    (void) accessLRU(entryIDName,currHost,NAME_ENTRY, FALSE,
		    migrated);
	    break;
        case ATTR_LOOKUP:
	    (void) accessLRU(dirIDName,currHost,ATTR, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,ATTR_MIG, FALSE, migrated);
	    if (dirIDName->openMode & FS_WRITE) {
		attributesWhileWriting++;
	    } else if (dirIDName->openMode & FS_READ) {
		attributesWhileReading++;
	    } else if (dirIDName->openMode & FS_EXECUTE) {
		attributesWhileExecuting++;
	    } else {
		if (dirIDName->numberOfOpens>0) {
		    error("File open but not R/W ?!\n");
		}
		attributesWhileSafe++;
	    }
	    break;
        case NAME_MODIFY:
#ifdef MULTI_OPS
	    level = accessLRU(dirIDName,currHost,NAME, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,NAME_MIG, FALSE, migrated);
	    (void) accessLRU(dirIDName,currHost,ATTR, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,ATTR_MIG, FALSE, migrated);
#endif
	    invalidateLRU(dirIDName,currHost,NAME); /* Dir no longer valid. */
	    invalidateLRU(dirIDName,homeHost,NAME_MIG);
	    invalidateLRU(entryIDName,currHost,NAME_ENTRY); /* Entry invalid. */
	    break;
        case ATTR_MODIFY:
#ifdef MULTI_OPS
	    (void) accessLRU(dirIDName,currHost,ATTR, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,ATTR_MIG, FALSE, migrated);
#endif
	    invalidateLRU(dirIDName,currHost,ATTR); /* Attrs no lonver valid. */
	    invalidateLRU(dirIDName,homeHost,ATTR_MIG);
	    break;
        case NAMEATTR_REMOVE:
#ifdef MULTI_OPS
	    (void) accessLRU(dirIDName,currHost,ATTR, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,ATTR_MIG, FALSE, migrated);
#endif
	    invalidateLRU(dirIDName,currHost,NAME); /* Dir no longer valid. */
	    invalidateLRU(dirIDName,homeHost,NAME_MIG);
	    invalidateLRU(entryIDName,currHost,NAME_ENTRY); /* Entry invalid. */
	    removeLRU(entryIDName);
	    break;
        case NAMEATTR_CREATE:
#ifdef MULTI_OPS
	    (void) accessLRU(dirIDName,currHost,ATTR, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,ATTR_MIG, FALSE, migrated);
	    level = accessLRU(dirIDName,currHost,NAME, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,NAME_MIG, FALSE,
		    migrated);
#endif
	    invalidateLRU(dirIDName,currHost,NAME); /* Directory no longer valid */
	    invalidateLRU(dirIDName,homeHost,NAME_MIG);
	    (void) accessLRU(entryIDName,currHost,ATTR, TRUE, migrated);
	    (void) accessLRU(entryIDName,homeHost,ATTR_MIG, TRUE, migrated);
	    (void) accessLRU(entryIDName,homeHost,NAME_ENTRY, TRUE, migrated);
	    if (type == FS_DIRECTORY) {
		level = accessLRU(entryIDName,currHost,NAME, TRUE, migrated);
		(void) accessLRU(entryIDName,homeHost,NAME_MIG, TRUE,
			migrated);
	    }
	    break;
	case DIR_READ:
#if 0
	    nAttrPermReads++;
#ifdef MULTI_OPS
	    (void) accessLRU(dirIDName,currHost,ATTR, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,ATTR_MIG, FALSE, migrated);
#endif
	    level = accessLRU(dirIDName,currHost,NAME, FALSE, migrated);
	    (void) accessLRU(dirIDName,homeHost,NAME_MIG, FALSE, migrated);
	    (void) accessLRU(-1,homeHost,NAME_ENTRY, FALSE, migrated);
#endif
	    break;
	default:
	    error(sprintf(charBuf,"Bad function in accessDir (%d)\n",
		    function));
	    exit(-1);
    }
    return level;
}

/*
 * Collect statistics for a hash record.
 */
void
collectHashStatistics(namePtr)
    nameRec *namePtr;
{
}

/*
 * Clean up some unused stuff so we don't run out of memory.
 */
void
garbageCollect()
{
    int machine;
    int lru;
    int lruCount=0;
    int count;
    int removePos;
    int nullRemove;
    LRUentry *entry, *newentry;
    int numLRUfreed = 0;

    /*
     * Remove entries more than 20 down on the list and more than 1 hour old.
     */

    for (lru=0;lru<NUM_LRU_LISTS;lru++) {
	for (machine=0;machine<NUM_MACHINES;machine++) {
	    /*
	     * First go through and decide what we want to discard.
	     */
	    newentry = LRUlist[lru][machine].down;
	    removePos = -1;
	    for (count=1;;count++) {
		lruCount++;
		entry = newentry;
		if (entry==NULL) {
		    break;
		}
		newentry = entry->down;
		/*
		 * At this point, entry points to something, and newentry
		 * points to the next thing.
		 */
		if (entry->nameInfo == NULL || (count>GC_REMOVE_COUNT &&
			sec-entry->lastAccess>GC_REMOVE_TIME)) {
		    if (removePos == -1) {
			removePos = count;
		    }
		} else {
		    removePos = -1;
		}
	    }
	    /*
	     * Now go through and discard those entries.
	     */
	    nullRemove = 0;
	    if (removePos != -1) {
		newentry = LRUlist[lru][machine].down;
		for (count=1;count<removePos;count++) {
		    entry = newentry;
		    newentry = entry->down;
		}
		entry->down = NULL;
		while (1) {
		    entry = newentry;
		    if (entry==NULL) {
			break;
		    }
		    newentry = entry->down;
		    if (entry->nameInfo != NULL) {
			entry->nameInfo->numUsers--;
			entry->nameInfo->numActiveUsers--;
			SET_MACHINEINFO(entry->nameInfo->machineInfo[lru],
				machine, 0);
			freeIDIfDone(entry->nameInfo);
		    } else {
			nullRemove++;
		    }
		    numLRUfreed++;
		    free(entry);
		    mallocLRU -= sizeof(LRUentry);
		}
	    }
	}
    }
    if (entryCnt>=gcMax) {
	gcMax = entryCnt+GC_INC;
    } else {
	gcMax += GC_INC;
    }
    fprintf(stderr,"%19.19s: GC removed %d LRU (%d null), max=%d, cnt=%d, numLRU=%d, %d lookups\n",
	    ctime(&sec), numLRUfreed, nullRemove, gcMax, entryCnt, lruCount,
	    nlookups);
    fprintf(stderr,"mallocOpen: %d, mallocName: %d, mallocLRU: %d\n",
	    mallocOpen, mallocName, mallocLRU);
    fprintf(stderr,"mallocHash: %d, mallocTable: %d\n", mallocHash,
	    mallocTable);
    fprintf(stderr,"hashOpen: %d, hashName: %d\n", hashOpen, hashName);
}

/*
 ****************************************************************************
 *	Hash table operations
 ****************************************************************************
 */

Hash_Table *tablePtr = NULL;

/*
 * Hash on an id
 * This routine converts an id into a hash entry (nameRec *).
 */
nameRec *
hashID(id)
int id[];
{
    Hash_Entry *entryPtr;
    if (id==NULL) {
	error("NULL hash key!\n");
	mypanic();
    }
    entryPtr = Hash_FindEntry(tablePtr, (Address)id);
    if (entryPtr==NULL) { 
	error(sprintf(charBuf,"Id: %d %d %d %d missing from table\n", id[0],
		id[1], id[2], id[3]));
	return NULL;
    } else {
	return (nameRec *)entryPtr->clientData;
    }
}

/*
 * This creates a nameRecPtr entry for the ID if it does not exist.
 * If force is MAKENEW, the old record will be destroyed.
 * If force is IFNEW, an old record will be preserved.
 * If name != NULL, it will be used as the new name for the entry.
 */
nameRec *
newID(fileID, force, name, type)
file_id *fileID;
int force;
char *name;
int type;
{
    Hash_Entry *entryPtr;
    nameRec *nameRecPtr=NULL;
    int created;
    if (fileID[0]==-1) {
	numLinksEncountered++;
	return (nameRec *)-1;
    }
    if (fileID==NULL) {
	error("Warning: null lookupID\n");
	return NULL;
    }
    if (tablePtr != NULL) {
	entryPtr = Hash_CreateEntry(tablePtr, (Address)fileID, &created);
	nameRecPtr = (nameRec *)entryPtr->clientData;
	if (created) {
	    hashName++;
	}
	if (created==FALSE &&
		(force==MAKENEW || nameRecPtr->type == INVALID)) {
	    if (nameRecPtr->numUsers != 0) {
		fprintf(stderr,"Reuse of %x\n", fileID[3]);
		if (nameRecPtr->numActiveUsers != 0) {
		    fprintf(stderr,"And its still active\n");
		    removeLRU(nameRecPtr);
		    entryPtr = Hash_CreateEntry(tablePtr, (Address)fileID, &created);
		    if (created) {
			hashName++;
		    }
		}
	    }
	    reusedID++;
	    created = TRUE;
	}
	if (created==TRUE) {
	    nameRecPtr = (nameRec *)malloc(sizeof(nameRec));
	    mallocName += sizeof(nameRec);
	    bzero((char *)nameRecPtr, sizeof(nameRec));
	    nameRecPtr->fileID[0] = fileID[0];
	    nameRecPtr->fileID[1] = fileID[1];
	    nameRecPtr->fileID[2] = fileID[2];
	    nameRecPtr->fileID[3] = fileID[3];
	    nameRecPtr->type = type;
#if 0
	    if (name == NULL) {
		sprintf(nameRecPtr->name, "NEW%d", realEntryCnt);
	    } else {
		sprintf(nameRecPtr->name, name);
	    }
#endif
	    entryPtr->clientData = (ClientData) nameRecPtr;
	    entryCnt++;
	    realEntryCnt++;
	} else if (((nameRec *)entryPtr->clientData)->type == UNKNOWN) {
	    ((nameRec *)entryPtr->clientData)->type = type;
	} else if (type != UNKNOWN &&
		((nameRec *)entryPtr->clientData)->type != type) {
	    error(sprintf(charBuf,"Warning: type change: %s to %s on %x %x %x %x\n",
		filetype(((nameRec *)entryPtr->clientData)->type),
		filetype(type), fileID[0], fileID[1], fileID[2], fileID[3]));
	}
    }
     nameRecPtr->numUsers++;
#if 0
    printf("New; %x,%x,%x,%x %x,%x,%x,%x -> %x\n", fileID[0], fileID[1],
    fileID[2], fileID[3], nameRecPtr->fileID[0], nameRecPtr->fileID[1],
    nameRecPtr->fileID[2], nameRecPtr->fileID[3], nameRecPtr);
#endif
     return nameRecPtr;
}

/*
 * Decrement the counter associated with newID, and discard if necessary.
 */
void
doneID(namePtr)
nameRec *namePtr;
{
    if (namePtr==(nameRec *)-1) return;
    namePtr->numUsers--;
#if 0
    printf("Done %x,%x,%x,%x (%x)\n", namePtr->fileID[0], namePtr->fileID[1],
	    namePtr->fileID[2], namePtr->fileID[3], namePtr);
#endif
    freeIDIfDone(namePtr);
}

/*
 * If we're done with the entry, free it.
 */
void
freeIDIfDone(namePtr)
nameRec *namePtr;
{
    Hash_Entry *entryPtr;
    if (namePtr->numUsers==0) {
	if (namePtr->numberOfOpens>0) {
	    fprintf(stderr,"******numberOfOpens>0 on %x,%x,%x,%x\n",
		    namePtr->fileID[0], namePtr->fileID[1],
		    namePtr->fileID[2], namePtr->fileID[3]);
	    return;
	}
	collectHashStatistics(namePtr);
	entryPtr = Hash_FindEntry(tablePtr, (Address)namePtr->fileID);
	Hash_DeleteEntry(tablePtr, entryPtr);
	hashName--;
#if 0
	printf("Freeing %x,%x,%x,%x\n", namePtr->fileID[0],
	namePtr->fileID[1], namePtr->fileID[2], namePtr->fileID[3]);
#endif
	free((char *)namePtr);
	mallocName -= sizeof(nameRec);
	entryCnt--;
    }
}

static char lookupBuf[100]; 
/*
 * Print the name associated with an id.
 */
char *
getIDname(fileID)
file_id *fileID;
{
    if (fileID==NULL) {
	error("Warning: null lookupID\n");
	return "<NULL>";
    }
#if 0
    if (tablePtr != NULL) {
	entryPtr = Hash_FindEntry(tablePtr, (Address)fileID);
	if (entryPtr == NULL) {
	    error("Warning: undefined lookupID\n");
	    return "<UNDEFINED>";
	} else {
	    return ((nameRec *)(entryPtr->clientData))->name;
	}
    } else {
#endif
	sprintf(lookupBuf,"(%x %x %x %x)", fileID[0], fileID[1], fileID[2],
		fileID[3]);
	return lookupBuf;
#if 0
    }
#endif
}

/*
 * Initialize the hash table.
 */
void
initHash()
{
    int id[4];

    tablePtr = (Hash_Table *)malloc(sizeof(Hash_Table));
    Hash_InitTable(tablePtr, 0, 4);
    id[0] = id[1] = id[2] = id[3] = 0;
    newID(id, MAKENEW, "NULL", FS_FILE);
    tmpName = newID(tmpDir, MAKENEW, "TMP", FS_DIRECTORY);

    streamTablePtr = (Hash_Table *)malloc(sizeof(Hash_Table));
    Hash_InitTable(streamTablePtr, 0, 4);
}

int prefixCount = 1;

/*
 * Initialize the hash table.
 */
void
loadHash(name, type)
char *name;
int type;
{
    FILE *inFile;
    char buf[1000];
    int id[4];
    char *bufp;
    int i;

    inFile = fopen(name,"r");
    if (inFile==NULL) {
       perror("open");
       exit(-1);
    }
    while (1) {
	if (fscanf(inFile,"%s %d %d %d %d", buf, &id[0], &id[1], &id[2],
		&id[3]) != 5) break;
	fgets(buf,499,inFile);
	newID(id, MAKENEW, buf, FS_DIRECTORY);
	if (type==1) {
	    hashID(id)->flags = PREFIX | prefixCount;
	    fgets(buf,499,inFile);
	    bufp = buf;
	    for (i=0;i<30;i++) {
		if (bufp==NULL || *bufp == '\n') break;
		prefixParts[prefixCount][i] = atoi(bufp);
		bufp = strchr(bufp,' ');
		if (bufp!=NULL) {
		    bufp++;
		}
	    }
	    if (i%4 != 0) {
		fprintf(stderr,"Bad # components for prefix %d\n",
			prefixCount);
		i &= 3;
	    }
	    prefixParts[prefixCount][i] = -1;
	    prefixCount++;
	}
    }
    fclose(inFile);
}

/*
 ****************************************************************************
 *	LRU list operations
 ****************************************************************************
 */

/*
 * Search the LRU list for an entry.  Return the parent, or NULL.
 * Also return the position on the list (first = 1).
 */
LRUentry *
searchLRU(namePtr, machine, pos, LRU)
nameRec *namePtr;
int machine;
int *pos;
int LRU;
{

    register LRUentry *entry, *oldentry;
    register int count;
    CHECKLRU(LRU);
    if (namePtr == NULL) {
	error("NULL searchLRU\n");
    }
    oldentry = &LRUlist[LRU][machine];
    for (count=1;;count++) {
	entry = oldentry->down;
#if 0
	if (entry==NULL) {
	    *pos = -1;
	    fprintf(stderr,"***Wasted searchLRU\n");
	    return NULL;
	} else if (entry->nameInfo == namePtr) {
#else
	if (entry->nameInfo == namePtr) {
#endif
	    *pos = count;
	    return oldentry;
	}
	oldentry = entry;
    }
}

/*
 * Invalidate file from all LRU lists, except the specified.
 */
void
invalidateLRU(namePtr,machine,LRU)
    nameRec *namePtr;
    int machine;
    int LRU;
{
    int i;
    int pos;
    LRUentry *entry;
    unsigned char *machineInfo;
    int numMachinesInvalidated = 0;
    if (LRU != thisLRU) return;
    LRU = NLRU(LRU);
    CHECKLRU(LRU);
    if (debug) {
	printf("invalidateLRU(%x,%x,%x,%x: m: %d, l: %d\n", namePtr->fileID[0],
		namePtr->fileID[1], namePtr->fileID[2], namePtr->fileID[3],
		machine, LRU);
    }
    machineInfo = namePtr->machineInfo[LRU];
    for (i=0;i<NUM_MACHINES;i++) {
	if (machine==i) {
	} else {
	    if (GET_MACHINEINFO(machineInfo,i)==1) {
		entry = searchLRU(namePtr, i, &pos,LRU);
		if (entry==NULL) {
		    error("***(a)Removing file not in LRU\n");
		}
		addHist(pos,InvalidhistTable[LRU],LEVEL_TABLE_SIZE);
		SET_MACHINEINFO(machineInfo,i,2);
		numMachinesInvalidated++;
	    } else if (GET_MACHINEINFO(machineInfo,i)==3) {
		gcError++;
	    }
	}
    }
    nInvalidates[LRU]++;
    nMInvalidates[LRU] += numMachinesInvalidated;
    addHist(numMachinesInvalidated,invalidatedMachinesTable[LRU],IMT_SIZE);
}

/*
 * Remove file from all LRU lists.
 * This sets the machineInfo to 0 and counts up nRRemoves.
 */
void
removeLRU(namePtr)
    nameRec *namePtr;
{
    int i;
    int pos;
    LRUentry *entry;
    int LRU;
    unsigned char *machineInfo;
    if (namePtr->type == INVALID) {
	error("Double invalidation\n");
	return;
    }
    if (debug) {
	printf("removeLRU: %x,%x,%x,%x\n", namePtr->fileID[0],
		namePtr->fileID[1], namePtr->fileID[2], namePtr->fileID[3]);
    }
    namePtr->type = INVALID;
    for (LRU=0;LRU<NUM_LRU_LISTS;LRU++) {
	machineInfo = namePtr->machineInfo[LRU];
	for (i=0;i<NUM_MACHINES;i++) {
	    if (GET_MACHINEINFO(machineInfo,i)!= 0) {
		entry = searchLRU(namePtr, i, &pos,LRU);
		if (entry==NULL) {
		    error("***(b)Removing file not in LRU\n");
		}
		SET_MACHINEINFO(machineInfo,i,0);
		entry->down->nameInfo = (nameRec *) NULL;
		namePtr->numUsers--;
		namePtr->numActiveUsers--;
		nRRemoves[LRU]++;
	    }
	}
    }
    if (namePtr->numActiveUsers != namePtr->numberOfOpens) {
	error("Num active users mismatch\n");
    }
    freeIDIfDone(namePtr);
}

/*
 * Access a file on the machine's LRU list.
 * This will move the file to the top of the list (creating it if necessary).
 * The position on the list will be returned (-1 if not present).
 *
 * Side effects: entry moved to top of LRU list (may be created)
 */
int
accessLRU(namePtr,machine,LRU, new, migrated)
    nameRec *namePtr;
    int machine;
    int LRU;
    int new;
    int migrated;
{
    int pos;
    int machineInfo;
    int created=0;
    register LRUentry *entry, *currEntry;

    if (LRU != thisLRU) return 0;
    LRU = NLRU(LRU);
    nAccesses[LRU]++;
    if (migrated&1) {
	nAccesses[MIG_MEAS]++;
    }
    if (migrated&2) {
	nAccesses[TMP_MEAS]++;
    }
    if (namePtr==NULL) {
	/*
	 * Forced miss.
	 */
	forcedMiss[LRU]++;
	nRValidates[LRU]++;
	if (migrated&1) {
	    forcedMiss[MIG_MEAS]++;
	    nRValidates[MIG_MEAS]++;
	}
	if (migrated&2) {
	    forcedMiss[TMP_MEAS]++;
	    nRValidates[TMP_MEAS]++;
	    fprintf(stderr,"forcedMiss\n");
	}
	return -1;
    }
    if (debug) {
	printf("accessLRU: (%x,%x,%x,%x): m: %d, LRU: %d\n",
		namePtr->fileID[0], namePtr->fileID[1], namePtr->fileID[2],
		namePtr->fileID[3], machine, LRU);
    }
    machineInfo = GET_MACHINEINFO(namePtr->machineInfo[LRU],machine);
    if (machineInfo != 0) {
	entry = searchLRU(namePtr, machine, &pos,LRU);
	if (new) {
	    error("accessLRU: new entry exists\n"); 
	}
    } else {
	entry = NULL;
    }
    if (entry==NULL) {
	created = 1;
	entry = (LRUentry *)malloc(sizeof(LRUentry));
	mallocLRU += sizeof(LRUentry);
	bzero((char *)entry,sizeof(LRUentry));
	entry->down = LRUlist[LRU][machine].down;
	currEntry = entry;
	LRUlist[LRU][machine].down = entry;
	entry->nameInfo = namePtr;
	namePtr->numUsers++;
	namePtr->numActiveUsers++;
	pos = -1;
    } else if (entry != &LRUlist[LRU][machine]) {
	/* Only move if not first on the list */
	LRUentry *oldFirst, *newFirst;
	newFirst = entry->down;
	oldFirst = LRUlist[LRU][machine].down; /* First entry on the list */
	LRUlist[LRU][machine].down = newFirst;
	entry->down = newFirst->down;
	newFirst->down = oldFirst;
	currEntry = newFirst;
    } else {
	currEntry = entry->down;
    }
    if (namePtr->type==INVALID) {
	error("Accessed invalid entry\n");
    }
    if (machineInfo==0) {
	pos = -1;
	if (debug) {
	    printf("accessLRU: nRValidates++: used invalid name\n");
	}
	nRValidates[LRU]++;
	if (migrated&1) {
	    nRValidates[MIG_MEAS]++;
	}
	if (migrated&2) {
	    nRValidates[TMP_MEAS]++;
	    fprintf(stderr,"nRValidate: (%x,%x,%x,%x): m: %d, LRU: %d\n",
		namePtr->fileID[0], namePtr->fileID[1], namePtr->fileID[2],
		namePtr->fileID[3], machine, LRU);
	}
    } else if (machineInfo==1) {
	if (currEntry->lastAccess==0) {
	    addHist(-1, ageTable[LRU], AGE_TABLE_SIZE);
	} else {
	    addHist((int)(sec-currEntry->lastAccess),
		    ageTable[LRU], AGE_TABLE_SIZE);
	}
	nRHits[LRU]++;
	addHist(pos,ValidhistTable[LRU],LEVEL_TABLE_SIZE);
	if (migrated&1) {
	    nRHits[MIG_MEAS]++;
	    addHist(pos,ValidhistTable[MIG_MEAS],LEVEL_TABLE_SIZE);
	}
	if (migrated&2) {
	    nRHits[TMP_MEAS]++;
	    addHist(pos,ValidhistTable[TMP_MEAS],LEVEL_TABLE_SIZE);
	    fprintf(stderr,"nRHits: at %d (%x,%x,%x,%x): m: %d, LRU: %d\n",
		pos, namePtr->fileID[0], namePtr->fileID[1], namePtr->fileID[2],
		namePtr->fileID[3], machine, LRU);
	}
    } else if (machineInfo==3) {
	pos = -1;
	nRValidates[LRU]++;
	if (migrated&1) {
	    nRValidates[MIG_MEAS]++;
	}
	if (migrated&2) {
	    nRValidates[TMP_MEAS]++;
	    fprintf(stderr,"nRValidates: (%x,%x,%x,%x): m: %d, LRU: %d\n",
		namePtr->fileID[0], namePtr->fileID[1], namePtr->fileID[2],
		namePtr->fileID[3], machine, LRU);
	}
	gcError++;
    } else {
	nRRevalidates[LRU]++;
	addHist(pos,foundInvalidTable[LRU],LEVEL_TABLE_SIZE);
	if (migrated&1) {
	    nRRevalidates[MIG_MEAS]++;
	    addHist(pos,foundInvalidTable[MIG_MEAS],LEVEL_TABLE_SIZE);
	}
	if (migrated&2) {
	    nRRevalidates[TMP_MEAS]++;
	    addHist(pos,foundInvalidTable[TMP_MEAS],LEVEL_TABLE_SIZE);
	    fprintf(stderr,"nRRealidates: (%x,%x,%x,%x): m: %d, LRU: %d\n",
		namePtr->fileID[0], namePtr->fileID[1], namePtr->fileID[2],
		namePtr->fileID[3], machine, LRU);
	}
    }
    currEntry->lastAccess = sec;
    if (created != (machineInfo==0?1:0)) {
	error("LRU creation error\n");
	fprintf(stderr,"Created = %d, machineInfo=%d\n", created, machineInfo);
    }
    SET_MACHINEINFO(namePtr->machineInfo[LRU],machine,1);
    return pos;
}

/*
 * Do some validations. */
void
validateLRU(str)
char *str;
{
    int lru, m, mi;
    LRUentry *entry, *oldentry;
    int c;

    for (lru=0;lru<NUM_LRU_LISTS;lru++) {
	for (m=0;m<NUM_MACHINES;m++) {
	    c = 0;
	    oldentry = &LRUlist[lru][m];
	    while (1) {
		entry = oldentry->down;
		if (entry==NULL) {
		    break;
		}
		if (entry->nameInfo != NULL) {
		    mi = GET_MACHINEINFO(entry->nameInfo->machineInfo[lru], m);
		    if (mi != 1 && mi != 2) {
			fprintf(stderr,"%s: Bad mi: %d on list [%d][%d][%d]\n",
				str,mi, lru,m,c);
			mypanic();
		    }
		}
		c++;
		oldentry = entry;
	    }
	}
    }
}

/*
 ****************************************************************************
 *	Histogram operations
 ****************************************************************************
 */

/*
 * Histogram tables work as follows:
 * 0...max-3 hold the number with that value.
 * max-2 holds the number >= max-2
 * max-1 holds the number < 0.
 */

/*
 * Add to histogram
 */
void
addHist(val,table,max)
int val;
int table[];
int max;
{
    if (val>=max-2) {
	table[max-2]++;
    } else if (val<0) {
	table[max-1]++;
    } else{
	table[val]++;
    }
}

/*
 * Sum two histograms, yielding a third.
 */
void
sumHist(h1,h2,sum,max)
int *h1, *h2, *sum, max;
{
    int i;
    for (i=0;i<max;i++) {
	sum[i] = h1[i]+h2[i];
    }
}

/*
 * Dump histogram.
 * total is the requested total for dividing percentages.
 * If -1, the real total will be used.
 */
void
dumpHist(table,max,total)
int *table;
int max;
int total;
{
    int maxUsed,i;
    int cumul=0;
    int realTotal;
    for (maxUsed=max-2;maxUsed>0;maxUsed--) {
	if (table[maxUsed]!=0) break;
    }
    realTotal = table[max-1];
    for (i=0;i<=maxUsed;i++) {
	realTotal += table[i];
    }
    if (total<0) {
	total = realTotal;
    }

    printf("    #       amount         cumul\n");
    for (i=0;i<=maxUsed;i++) {
	cumul += table[i];
	if (i==max-2) {
	    printf(">=");
	} else {
	    printf("  ");
	}
	printf("%3d %5d (%5.2f) %5d (%5.2f)\n", i, table[i],
		table[i]*100./total, cumul, cumul*100./total);
    }
    if (table[max-1]>0) {
	printf("Neg: %d (%5.2f)\n", table[max-1], table[max-1]*100./total);
    }
    if (total == realTotal) {
	printf("Total: %d\n", total);
    } else {
	printf("Sum: %d of %d = %5.2f%%\n", realTotal, total,
		realTotal*100./total);
    }
}

/*
 * Dump histogram in xgraph format.
 * total is the requested total for dividing percentages.
 * If -1, the real total will be used.
 */
void
dumpHistGraph(table,max,total,cumul,xoff)
int *table;
int max;
int total;
int cumul;
float xoff;
{
    int maxUsed,i;
    int cumulTotal=0;
    int realTotal;

    for (maxUsed=max-2;maxUsed>0;maxUsed--) {
	if (table[maxUsed]!=0) break;
    }
    realTotal = table[max-1];
    for (i=0;i<=maxUsed;i++) {
	realTotal += table[i];
    }
    if (total<0) {
	total = realTotal;
    }

    if (maxUsed==max-2) maxUsed--;
    for (i=0;i<=maxUsed;i++) {
	if (cumul) {
	    cumulTotal += table[i];
	} else {
	    cumulTotal = table[i];
	}
	printf("%f %7.4f\n", i+xoff, cumulTotal*100./total);
    }
    printf("%f %7.4f\n", max+xoff, cumulTotal*100./total);
}
