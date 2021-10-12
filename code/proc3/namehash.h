#include <sprite.h>
#include <hash.h>

#define NUM_MACHINES 90

#define NAMELEN 100
typedef struct nameRec {
    char name[NAMELEN];
    int fileID[4];	/* This is the FileID used to reference the file.  */
    int valid[2];	/* 1 if this entry is valid. 0 if it has been
			    invalidated */
    unsigned char machineInfo[2][NUM_MACHINES];
		/* 0 = not present, 1 = valid, 2 = invalid. */
    int flags;
} nameRec;

#define PREFIX 0x8000

/*
 * The LRU entry holds an entry in the LRU list associated with each
 * machine.  To determine if the entry is valid or not, one must check
 * the machineInfo vector.
 */
typedef struct LRUentry {
    nameRec *nameInfo;	/* Pointer to the associated nameRec structure. */
    struct LRUentry *down;
} LRUentry;

extern Hash_Table *tablePtr;

extern nameRec *hashID _ARGS_((int id[]));
extern char *	lookupID _ARGS_((int id[]));
