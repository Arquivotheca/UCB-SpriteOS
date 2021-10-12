#include <sprite.h>
#include <hash.h>

#define NUM_MACHINES 100
#define NUM_MACHINES_4 25

#define NUM_LRU_LISTS 1

#define NAMELEN 100
typedef struct nameRec {
    int fileID[4];	/* This is the FileID used to reference the file.  */
    short type;		/* FS_FILE, etc. */
    short numUsers;	/* Number of lists using this file. */
    short numActiveUsers; /* Number of lists actively using the file. */ 
    char numberOfOpens;	/* Count of number of people with this file open. */
    char openMode;	/* FS_READ or FS_WRITE */
    unsigned char machineInfo[NUM_LRU_LISTS][NUM_MACHINES_4];
		/* 0 = not present, 1 = valid, 2 = invalid. */
    int flags;		/* Holds # if this is a prefix. */
} nameRec;

#define PREFIX 0x8000

#define UNKNOWN -1
#define INVALID -2

/*
 * The LRU entry holds an entry in the LRU list associated with each
 * machine.  To determine if the entry is valid or not, one must check
 * the machineInfo vector.
 */
typedef struct LRUentry {
    nameRec *nameInfo;	/* Pointer to the associated nameRec structure. */
    struct LRUentry *down;
    long lastAccess;/* Time we last used the entry. */
} LRUentry;

extern Hash_Table *tablePtr;

extern nameRec *hashID _ARGS_((int id[]));
extern char *	lookupID _ARGS_((int id[]));

void dolookup _ARGS_((trace_data *data, int machine)); 
void doopen _ARGS_((trace_data *data, int machine)); 
void doclose _ARGS_((trace_data *data, int machine)); 
void dogetattr _ARGS_((trace_data *data, int machine)); 
void dosetattr _ARGS_((trace_data *data, int machine)); 

void initHash _ARGS_((void));
void loadHash _ARGS_((char *name, int type));
void dorec _ARGS_((Sys_TracelogRecord *tracePtr));
void donerecs _ARGS_((void));
