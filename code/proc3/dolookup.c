#include <stdio.h>
#include <sprite.h>
#include <status.h>
#include <namehash.h>
#include <bstring.h>
#include <hash.h>
#include <stdlib.h>
#include <strings.h>
extern long startSec;
extern long startUsec;

#define CHECKLRU(x) if ((x)!=0 && (x)!=1) printf("LRU=%d at %d\n",x,__LINE__)

int ops[20]={0};
int nlookups = 0;
int ncomps = 0;
int nCompLookups[2] = {0}; /* Number of component lookups. */
int nInvalidates[2] = {0}; /* Number of directory invalidates. */
int nMInvalidates[2] = {0}; /* Number of directory-machine invalidates. */
int nRevalidates[2] = {0}; /* Number of directory revalidates. */
int nRHits[2] = {0}; /* Number of remote directory validates. */
int nRValidates[2] = {0}; /* Number of remote directory validates. */
int nRRevalidates[2] = {0}; /* Number of remote directory revalidates. */
int nRRemoves[2] = {0}; /* Number of remote directory removes. */

int badOp = 0;
int statusTable[20][20] = {0};	/* Table for return status. */

#define NAME 0
#define ATTR 1

#define LEVEL_TABLE_SIZE	30
int ValidhistTable[2][LEVEL_TABLE_SIZE] = {0};
int InvalidhistTable[2][LEVEL_TABLE_SIZE] = {0};
int NumRemoteInvHistTable[2][LEVEL_TABLE_SIZE] = {0};

char *opname(), *status();

#define DIFF_FD(x,y) (((x)[0]!=(y)[0])||((x)[1]!=(y)[1])||\
		      ((x)[2]!=(y)[2])||((x)[3]!=(y)[3]))

void accessblocks _ARGS_((int first, int last, int *data, int machine, int
	LRU));
void accessblock _ARGS_((int *fileID, int *data, int machine, int LRU));
void modifyblock _ARGS_((int *fileID, int *data, int machine, int LRU));
void initRec _ARGS_((nameRec *nameRecPtr, int *id));
void invalidateLRU _ARGS_((int *fileID, int machine, int LRU));
void removeLRU _ARGS_((int *fileID, int LRU));
int accessLRU _ARGS_((int *fileID, int machine, int LRU));
void dumpLRU _ARGS_((int machine, int LRU));
void initHash _ARGS_((char *name));
void dumpHist _ARGS_((int *table, int max));
void addHist _ARGS_((int val,int table[],int max));
void recordInTable _ARGS_((nameRec *nameRecPtr));
void dumpHash _ARGS_((void));
void donerecs _ARGS_((void));
void dolookup _ARGS_((void));
void donelookup _ARGS_((void));

#define RETURN_ID (&data[3])
#define ID_NUM(n) ((n<0)?NULL:&data[10+4*(n)])
void
dolookup(data)
    int *data;
{
    int i;
    int op,returnStatus, numIds, hostID;
    op = data[9];
    returnStatus = data[7];
    numIds = data[8];
    hostID = data[1];
/*
    printf("LOOKUP: hostID: %d, home: %d, %s, numIDs %d, op %s\n", 
	    hostID, data[2], status(returnStatus), numIds,
	    opname(op));
*/
    if (hostID>=NUM_MACHINES) {
	fprintf(stderr,"***Invalid machine number: %d\n", hostID);
	hostID = NUM_MACHINES-1;
    }
    for (i=0;i<numIds;i++) {
	(void)lookupID(ID_NUM(i),1);
    }
    (void)lookupID(RETURN_ID,0);
    if (op==0x8a) op = 17;
    if (op==-1) op = 4; /* To fix a bug in the tracing. */
    if (op>=0 && op<20) {
	ops[op]++;
	accessblocks(0,numIds-2,data,hostID, NAME);
	switch (returnStatus) {
	    case FS_FILE_NOT_FOUND:
	    case FS_DIR_NOT_EMPTY:
	    case FS_FILE_EXISTS:
		if (numIds>0) {
		    accessblock(ID_NUM(numIds-1),data,hostID,NAME);
		}
		break;
	    case FS_LOOKUP_REDIRECT:
	    /* 
	     * Should do something about it.
	     */
	    break;
	    case FS_NO_ACCESS:
	    case SUCCESS:
		break;
	    default:
		printf("Don't know how to handle status: %x\n", returnStatus);
		break;
	}
	if (returnStatus==SUCCESS) {
	    statusTable[op][0]++;
	} else {
	    statusTable[op][(returnStatus&0xf)+1]++;
	}
	if (returnStatus == SUCCESS) {
	    switch (op) {
		default:
		case 0: /* import */
		case 1: /* export */
		case 5: /* makedevice */
		    printf("Wasn't expecting op %d\n", op);
		    break;
		case 3: /* getattrpath */
		    accessblock(RETURN_ID,data,hostID,ATTR);
		    break;
		case 4: /* setattrpath */
		    modifyblock(ID_NUM(numIds-1),data,hostID,NAME);
		    modifyblock(ID_NUM(numIds-1),data,hostID,ATTR);
		    break;
		case 8: /* removedir */
		    /*
		    printf("Rmdir: Entry %s removed from %s\n",
			lookupID(ID_NUM(numIds-1),0), lookupID(ID_NUM(numIds-2),0));
		    */
		    modifyblock(ID_NUM(numIds-2),data,hostID,NAME);
		    removeLRU(ID_NUM(numIds-1),NAME);
		    removeLRU(ID_NUM(numIds-1),ATTR);
		    break;
		case 2: /* open */
		    if (numIds==0||DIFF_FD(ID_NUM(numIds-1), RETURN_ID)) {
			/*
			printf("Open: Entry %s created in %s\n",
			    lookupID(RETURN_ID,0), lookupID(ID_NUM(numIds-1),0));
			*/
			initRec(hashID(RETURN_ID),RETURN_ID);
			if (numIds>0) {
			    accessblock(ID_NUM(numIds-1),data,hostID,NAME);
			    modifyblock(ID_NUM(numIds-1),data,hostID,NAME);
			}
		    }
		    accessblock(RETURN_ID,data,hostID,ATTR);
		    break;
		case 7: /* remove */
		    /*
		    printf("Remove: Entry %s removed from %s\n",
			lookupID(ID_NUM(numIds-1),0), lookupID(ID_NUM(numIds-2),0));
		    */
		    modifyblock(ID_NUM(numIds-2),data,hostID,NAME);
		    removeLRU(RETURN_ID,ATTR);
		    break;
		case 9: /* rename */
		    printf("Rename?");
		    break;
		case 10: /* hardlink */
		    /*
		    printf("Hardlink: Entry %s linked in %s\n",
			lookupID(ID_NUM(numIds-1),0), lookupID(ID_NUM(numIds-2),0));
		    */
		    modifyblock(ID_NUM(numIds-2),data,hostID,NAME);
		    break;
		case 17: /* hardlink, part 2 */
		    break;
		case 6: /* makedir */
		    /*
		    printf("Directory: %s created in %s\n",
			lookupID(RETURN_ID,0), lookupID(ID_NUM(numIds-1),0));
		    */
		    initRec(hashID(RETURN_ID),RETURN_ID);
		    accessblock(ID_NUM(numIds-1),data,hostID,NAME);
		    modifyblock(ID_NUM(numIds-1),data,hostID,NAME);
		    modifyblock(ID_NUM(numIds-1),data,hostID,ATTR);
		    break;
	    }
	}
    } else {
	fprintf(stderr,"*** Bad op %x!\n", op);
	badOp++;
    }
    nlookups++;
    ncomps += numIds;
}

void
donerecs()
{
    donelookup();
    /*
    dumpHash();
    */
    printf("\nDirectory's depth in LRU list when found:\n");
    dumpHist(ValidhistTable[NAME], LEVEL_TABLE_SIZE);
    printf("\nDirectory's depth in LRU list when invalidated:\n");
    dumpHist(InvalidhistTable[NAME], LEVEL_TABLE_SIZE);
    printf("\nAttribute's depth in LRU list when found:\n");
    dumpHist(ValidhistTable[ATTR], LEVEL_TABLE_SIZE);
    printf("\nAttributes's depth in LRU list when invalidated:\n");
    dumpHist(InvalidhistTable[ATTR], LEVEL_TABLE_SIZE);
    printf("\nNumber of remote machines invalidated per invalidation:\n");
    dumpHist(NumRemoteInvHistTable[NAME], LEVEL_TABLE_SIZE);
}

void
donelookup()
{
    int i;
    int nOps=0;
    int op, opTotal;

    for (i=0;i<20;i++) {
	nOps += ops[i];
    }
    printf("--Results--\n");

    printf("\n");
    printf("Number of lookup calls: %d\n", nlookups);
    printf("%d of these had a bad operation field.\n", badOp);

    for (op=0;op<20;op++) {
	if (ops[op]>0) {
	    opTotal = 0;
	    for (i=0;i<19;i++) {
		opTotal += statusTable[op][i];
	    }
	    printf("\nReturn status for %s (%d (%5.2f%%)):\n",
		    opname(i), ops[i], ops[i]*100./nOps);
	    printf("  %s: %d (%5.2f%%)\n", status(0), statusTable[op][0],
	    statusTable[op][0]*100./opTotal);
	    for (i=0;i<19;i++) {
		if (statusTable[op][i+1]>0) {
		    printf("  %s: %d (%5.2f%%)\n", status(0x40000+i),
			    statusTable[op][i+1],
			    statusTable[op][i+1]*100./opTotal);
		}
	    }
	}
    }

    printf("\nOperations:\n");
    for (i=0;i<20;i++) {
	if (ops[i]>0) {
	    printf("  %s: %d (%5.2f%%)\n", opname(i), ops[i], ops[i]*100./nOps);
	}
    }

    printf("Avg # components returned: %5.2f\n", ncomps/(float)nlookups);
    for (i=0;i<1;i++) {
	if (i==NAME) {
	    printf("\nName lookup statistics:\n");
	} else {
	    printf("\nAttribute lookup statistics:\n");
	}
	printf("%d directories accessed\n", nCompLookups[i]);
	printf("%d directories invalidated\n", nInvalidates[i]);
	printf("%d revalidates\n", nRevalidates[i]);
	printf("\n%d machine-directories invalidated\n", nMInvalidates[i]);
	printf("%d remote directory validates\n", nRValidates[i]);
	printf("%d remote directory revalidates\n", nRRevalidates[i]);
	printf("%d remote directory hits\n", nRHits[i]);
	printf("%d remote directory removes\n", nRRemoves[i]);
    }
}

/*
 * Called when we access directory blocks.
 */
void
accessblocks(first,last,data,machine,LRU)
    int first,last;
    int *data;
    int machine;
    int LRU;
{
    int i;
    CHECKLRU(LRU);
    for (i=first;i<=last;i++) {
	accessblock(ID_NUM(i),data,machine,LRU);
    }
}

/*
 * Called when we access a directory block.
 */
void
accessblock(fileID,data,machine,LRU)
    int *fileID;
    int *data;
    int machine;
    int LRU;
{
    nameRec *namePtr;
    int level;
    CHECKLRU(LRU);
    if (fileID==NULL) {
	fprintf(stderr,"Warning: null accessblock\n");
	return;
    }
    namePtr = hashID(fileID);
    nCompLookups[LRU]++;
    if (namePtr->valid[LRU] == 0) {
	namePtr->revalidates[LRU]++;
	namePtr->valid[LRU] = 1;
	nRevalidates[LRU]++;
    }
    if (LRU==NAME) {
	namePtr->uses[LRU]++;
    }
    level = accessLRU(fileID,machine,LRU);
    if (namePtr->machineInfo[LRU][machine]==0) {
	namePtr->remoteRevalidates[LRU]++;
	nRValidates[LRU]++;
    } else if (namePtr->machineInfo[LRU][machine]==2) {
	namePtr->remoteRevalidates[LRU]++;
	nRRevalidates[LRU]++;
    } else {
	nRHits[LRU]++;
	addHist(level,ValidhistTable[LRU],LEVEL_TABLE_SIZE);
    }
}

/*
 * Called when we modify a directory block.
 */
void
modifyblock(fileID,data,machine,LRU)
    int *fileID;
    int *data;
    int machine;
    int LRU;
{
    nameRec *namePtr;
    CHECKLRU(LRU);
    if (fileID==NULL) {
	fprintf(stderr,"Warning: null modifyblock\n");
	return;
    }
    namePtr = hashID(fileID);
    namePtr->valid[LRU] = 0;
    namePtr->uses[LRU]++;
    nInvalidates[LRU]++;
    accessLRU(fileID,machine,LRU);
    invalidateLRU(fileID,machine,LRU);
}

Hash_Table *tablePtr = NULL;

int entryCnt = 0;

/*
 * Hash on an id
 */
nameRec *
hashID(id)
int id[];
{
    Hash_Entry *entryPtr;
    entryPtr = Hash_FindEntry(tablePtr, id);
    if (entryPtr==NULL) { 
	fprintf(stderr,"Id: %d %d %d %d missing from table\n", id[0], id[1],
		id[2], id[3]);
	return NULL;
    } else {
	return (nameRec *)entryPtr->clientData;
    }
}

/*
 * Initialize a name record.
 */
void
initRec(nameRecPtr,id)
    nameRec *nameRecPtr;
    int *id;
{
    bzero((char *)nameRecPtr, sizeof(nameRec));
    nameRecPtr->fileID[0] = id[0];
    nameRecPtr->fileID[1] = id[1];
    nameRecPtr->fileID[2] = id[2];
    nameRecPtr->fileID[3] = id[3];
    nameRecPtr->count[NAME] = 0;
    nameRecPtr->valid[NAME] = 1;
    nameRecPtr->count[ATTR] = 0;
    nameRecPtr->valid[ATTR] = 1;
}

/*
 * Make a new record.
 */
nameRec *
newRec(id)
    int id[];
{
    nameRec *nameRecPtr;
    nameRecPtr = (nameRec *)malloc(sizeof(nameRec));
    initRec(nameRecPtr,id);
    return nameRecPtr;
}

static char lookupBuf[100]; 
/*
 * Process a file ID.
 * This creates the name if it does not exist.
 * It adds count to the counter for that file.
 */
char *
lookupID(fileID, count)
int *fileID;
int count;
{
    Hash_Entry *entryPtr;
    nameRec *nameRecPtr;
    if (fileID==NULL) {
	fprintf(stderr,"Warning: null lookupID\n");
	return "<NULL>";
    }
    if (tablePtr != NULL) {
	entryPtr = Hash_FindEntry(tablePtr, fileID);
	if (entryPtr == NULL) {
	    nameRecPtr = newRec(fileID);
	    sprintf(nameRecPtr->name, "NEW%d", entryCnt);
	    entryCnt++;
	    recordInTable(nameRecPtr);
	} else {
	    nameRecPtr = (nameRec *)(entryPtr->clientData);
	}
	nameRecPtr->count[NAME] += count;
	return nameRecPtr->name;
    } else {
	sprintf(lookupBuf,"(%x %x %x %x)", fileID[0], fileID[1], fileID[2],
		fileID[3]);
	return lookupBuf;
    }
}

/*
 * Dump results from the hash table.
 */
void
dumpHash()
{
    Hash_Search search;
    Hash_Entry *entryPtr;
    nameRec *nameRecPtr;
    int LRU = NAME;

    entryPtr = Hash_EnumFirst(tablePtr, &search);
    while (entryPtr != NULL) {
	nameRecPtr = (nameRec *)(entryPtr->clientData);
	if (nameRecPtr->count[LRU]>0) {
	    printf("%d %d %d %d", nameRecPtr->count[LRU],
		    nameRecPtr->revalidates[LRU],
		    nameRecPtr->remoteRevalidates[LRU],nameRecPtr->uses[LRU]);
	    if (nameRecPtr->name[0]=='N') {
		printf(" (%x %x %x %x)",
			nameRecPtr->fileID[0], nameRecPtr->fileID[1],
			nameRecPtr->fileID[2], nameRecPtr->fileID[3]);
	    }
	    printf(" %s\n", nameRecPtr->name);
	}
	entryPtr = Hash_EnumNext(&search);
    }
}

/*
 * Initialize the hash table.
 */
void
initHash(name)
char *name;
{
    FILE *inFile;
    char buf[1000];
    nameRec *nameRecPtr;
    int id[4];

    tablePtr = (Hash_Table *)malloc(sizeof(Hash_Table));
    Hash_InitTable(tablePtr, 0, 4);
    if (name != NULL) {
	inFile = fopen(name,"r");
	if (inFile==NULL) {
	   perror("open");
	   exit(-1);
	}
	while (1) {
	    if (fscanf(inFile,"%s %d %d %d %d", buf, &id[0], &id[1], &id[2],
		    &id[3]) != 5) break;
	    nameRecPtr = newRec(id);
	    strncpy(nameRecPtr->name,buf,NAMELEN-1);
	    nameRecPtr->name[NAMELEN-1] = '\0';
	    recordInTable(nameRecPtr);
	}
	fclose(inFile);
    }
    id[0] = id[1] = id[2] = id[3] = 0;
    nameRecPtr = newRec(id);
    sprintf(nameRecPtr->name,"%s","NULL");
    recordInTable(nameRecPtr);
}

void
recordInTable(nameRecPtr)
nameRec *nameRecPtr;
{
    int created;
    Hash_Entry *entryPtr;
    entryPtr = Hash_CreateEntry(tablePtr, nameRecPtr->fileID, &created);
    if (created) {
	entryPtr->clientData = (ClientData) nameRecPtr;
    } else {
	free(nameRecPtr);
#if 0
	fprintf(stderr,"%s (%d %d %d %d) already exists!\n",
		nameRecPtr->name, nameRecPtr->fileID[0],
		nameRecPtr->fileID[1], nameRecPtr->fileID[2],
		nameRecPtr->fileID[3]);
	fprintf(stderr,"The old name is %s (%d %d %d %d)\n",
		((nameRec *)(entryPtr->clientData))->name,
		((nameRec *)(entryPtr->clientData))->fileID[0],
		((nameRec *)(entryPtr->clientData))->fileID[1],
		((nameRec *)(entryPtr->clientData))->fileID[2],
		((nameRec *)(entryPtr->clientData))->fileID[3]);
#endif
    }
}

static LRUentry LRUlist[2][NUM_MACHINES] = {NULL};

/*
 * Search the LRU list for an entry.  Return the parent, or NULL.
 * Also return the position on the list (first = 0).
 */
LRUentry *
searchLRU(recPtr, machine, pos, LRU)
nameRec *recPtr;
int machine;
int *pos;
int LRU;
{

    LRUentry *entry, *oldentry;
    int count;
    CHECKLRU(LRU);
    oldentry = &LRUlist[LRU][machine];
    for (count=0;;count++) {
	entry = oldentry->down;
	if (entry==NULL) {
	    *pos = -1;
	    return NULL;
	} else if (entry->nameInfo == recPtr) {
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
invalidateLRU(fileID,machine,LRU)
    int *fileID;
    int machine;
    int LRU;
{
    int i;
    int pos;
    LRUentry *entry;
    unsigned char *machineInfo;
    nameRec *recPtr;
    int numInvs = 0;
    CHECKLRU(LRU);
    recPtr = hashID(fileID);
    machineInfo = recPtr->machineInfo[LRU];
    for (i=0;i<NUM_MACHINES;i++) {
	if (machine==i) {
	} else {
	    if (machineInfo[i]==1) {
		entry = searchLRU(recPtr, i, &pos,LRU);
		if (entry==NULL) {
		    fprintf(stderr,"***(a)Removing file %s not in LRU\n",
			    recPtr->name);
		    fprintf(stderr,"machine %d (%d %d %d %d) %d\n", i,
			    fileID[0], fileID[1], fileID[2], fileID[3], LRU);
		    fprintf(stderr,"m76 = %d (%x)\n", machineInfo[76],
			    &machineInfo[76]);
		}
		addHist(pos,InvalidhistTable[LRU],LEVEL_TABLE_SIZE);
		machineInfo[i]=2;
		nMInvalidates[LRU] ++;
		numInvs++;
	    }
	}
    }
    addHist(numInvs,NumRemoteInvHistTable[LRU],LEVEL_TABLE_SIZE);
}

/*
 * Remove file from all LRU lists.
 */
void
removeLRU(fileID,LRU)
    int *fileID;
    int LRU;
{
    int i;
    int pos;
    LRUentry *entry;
    unsigned char *machineInfo;
    nameRec *recPtr;
    recPtr = hashID(fileID);
    machineInfo = recPtr->machineInfo[LRU];
    for (i=0;i<NUM_MACHINES;i++) {
	if (machineInfo[i]!= 0) {
	    entry = searchLRU(recPtr, i, &pos,LRU);
	    if (entry==NULL) {
		fprintf(stderr,"***(b)Removing file %s not in LRU\n",
			recPtr->name);
	    }
	    machineInfo[i]=0;
	    nRRemoves[LRU]++;
	}
    }
}

/*
 * Access a file on the machine's LRU list.
 * This will move the file to the top of the list (creating it if necessary).
 * The position on the list will be returned (-1 if not present).
 *
 * Side effects: entry moved to top of LRU list (may be created)
 */
int
accessLRU(fileID,machine,LRU)
    int *fileID;
    int machine;
    int LRU;
{
    nameRec *recPtr;
    int pos;
    LRUentry *entry;
    recPtr = hashID(fileID);
    entry = searchLRU(recPtr, machine, &pos,LRU);
    if (entry==NULL) {
	entry = (LRUentry *)malloc(sizeof(LRUentry));
	bzero((char *)entry,sizeof(LRUentry));
	entry->down = LRUlist[LRU][machine].down;
	LRUlist[LRU][machine].down = entry;
	entry->nameInfo = recPtr;
	pos = -1;
    } else if (entry != &LRUlist[LRU][machine]) {
	/* Only move if not first on the list */
	LRUentry *oldFirst, *newFirst;
	newFirst = entry->down;
	oldFirst = LRUlist[LRU][machine].down; /* First entry on the list */
	LRUlist[LRU][machine].down = newFirst;
	entry->down = newFirst->down;
	newFirst->down = oldFirst;
    }
    recPtr->machineInfo[LRU][machine]=1;
    return pos;
}

/*
 * Dump the LRU list.
 */
void
dumpLRU(machine,LRU)
    int machine;
    int LRU;
{
    LRUentry *entry;
    entry = &LRUlist[LRU][machine];
    if (entry != NULL) {
	printf("LRU list for %d:\n", machine);
	for (entry=LRUlist[LRU][machine].down ;entry != NULL;
		entry = entry->down) {
	    printf("  %s\n", entry->nameInfo->name);
	}
    }
}

/*
 * Add to histogram
 */
void
addHist(val,table,max)
int val;
int table[];
int max;
{
    if (val>=max) {
	table[max-2]++;
    } else if (val<0) {
	table[max-1]++;
    } else{
	table[val]++;
    }
}

/*
 * Dump histogram.
 */
void
dumpHist(table,max)
int *table;
int max;
{
    int maxUsed,i;
    int cumm=0;
    int total;
    for (maxUsed=max-2;maxUsed>0;maxUsed--) {
	if (table[maxUsed]!=0) break;
    }
    total = table[max-1];
    for (i=0;i<=maxUsed;i++) {
	total += table[i];
    }

    printf("  #       amount         cumm\n");
    for (i=0;i<=maxUsed;i++) {
	cumm += table[i];
	printf("%3d %5d (%5.2f) %5d (%5.2f)\n", i, table[i],
		table[i]*100./total, cumm, cumm*100./total);
    }
    if (table[max-1]>0) {
	printf("Neg: %d (%5.2f)\n", table[max-1], table[max-1]*100./total);
    }
    printf("Total: %d\n", total);
}
