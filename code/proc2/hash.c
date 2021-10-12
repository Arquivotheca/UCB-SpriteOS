#include <stdio.h>
#include <hash.h>
#include <namehash.h>

Hash_Table *tablePtr = NULL;

int entryCnt = 0;

/*
 * Process a file ID.
 */
lookupID(id, count)
int id[4];
int count;
{
    Hash_Entry *entryPtr;
    nameRec *nameRecPtr;
    int new;
    if (tablePtr != NULL) {
	entryPtr = Hash_FindEntry(tablePtr, id);
	if (entryPtr == NULL) {
	    nameRecPtr = (nameRec *)malloc(sizeof(nameRec));
	    nameRecPtr->fileID[0] = id[0];
	    nameRecPtr->fileID[1] = id[1];
	    nameRecPtr->fileID[2] = id[2];
	    nameRecPtr->fileID[3] = id[3];
	    sprintf(nameRecPtr->name, "NEW%d", entryCnt);
	    entryCnt++;
	    recordInTable(nameRecPtr);
	} else {
	    nameRecPtr = (nameRec *)(entryPtr->clientData);
	}
	printf("  File: %s\n", nameRecPtr->name);
	nameRecPtr->count += count;
    } else {
	printf("  FileID: (%x %x %x %x)\n", id[0], id[1], id[2], id[3]);
    }
}

/*
 * Dump results from the hash table.
 */
dumpHash()
{
    Hash_Search search;
    Hash_Entry *entryPtr;
    nameRec *nameRecPtr;

    entryPtr = Hash_EnumFirst(tablePtr, &search);
    while (entryPtr != NULL) {
	nameRecPtr = (nameRec *)(entryPtr->clientData);
	if (nameRecPtr->count>0) {
	    if (nameRecPtr->name[0]=='N') {
		printf("%d %x %x %x %x\n", nameRecPtr->count,
			nameRecPtr->fileID[0], nameRecPtr->fileID[1],
			nameRecPtr->fileID[2], nameRecPtr->fileID[3]);
	    } else {
		printf("%d %s\n", nameRecPtr->count, nameRecPtr->name);
	    }
	}
	entryPtr = Hash_EnumNext(&search);
    }
}

/*
 * Initialize the hash table.
 */
initHash(name)
char *name;
{
    FILE *inFile;
    char buf[1000];
    nameRec *nameRecPtr;

    inFile = fopen(name,"r");
    if (inFile==NULL) {
       perror("open");
       exit(-1);
    }
    tablePtr = (Hash_Table *)malloc(sizeof(Hash_Table));
    Hash_InitTable(tablePtr, 0, 4);
    while (1) {
	nameRecPtr = (nameRec *)malloc(sizeof(nameRec));
	if (fscanf(inFile,"%s %d %d %d %d", buf,
	    &nameRecPtr->fileID[0], &nameRecPtr->fileID[1],
	    &nameRecPtr->fileID[2], &nameRecPtr->fileID[3]) != 5) break;
	strncpy(nameRecPtr->name,buf,NAMELEN-1);
	nameRecPtr->name[NAMELEN-1] = '\0';
	recordInTable(nameRecPtr);
    }
    nameRecPtr = (nameRec *)malloc(sizeof(nameRec));
    nameRecPtr->fileID[0] = nameRecPtr->fileID[1] = nameRecPtr->fileID[2] =
	    nameRecPtr->fileID[3] = 0;
    sprintf(nameRecPtr->name,"%s","NULL");
    recordInTable(nameRecPtr);
    fclose(inFile);
}

recordInTable(nameRecPtr)
nameRec *nameRecPtr;
{
    int created;
    Hash_Entry *entryPtr;
    entryPtr = Hash_CreateEntry(tablePtr, nameRecPtr->fileID, &created);
    nameRecPtr->count = 0;
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
