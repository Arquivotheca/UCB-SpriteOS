#include <stdio.h>
#include <hash.h>
#include <namehash.h>

Hash_Table *tablePtr = NULL;

int entryCnt = 0;

/*
 * Process a file ID.
 */
lookupID(id)
int id[4];
{
    Hash_Entry *entryPtr;
    int new;
    if (tablePtr != NULL) {
	entryPtr = Hash_FindEntry(tablePtr, id);
	if (entryPtr == NULL) {
	    nameRec *nameRecPtr;
	    nameRecPtr = (nameRec *)malloc(sizeof(nameRec));
	    nameRecPtr->fileID[0] = id[0];
	    nameRecPtr->fileID[1] = id[1];
	    nameRecPtr->fileID[2] = id[2];
	    nameRecPtr->fileID[3] = id[3];
	    sprintf(nameRecPtr->name, "NEW%d", entryCnt);
	    entryCnt++;
	    recordInTable(nameRecPtr);
	    printf("  File: %s\n", nameRecPtr->name);
	} else {
	    printf("  File: %s\n", ((nameRec *)(entryPtr->clientData))->name);
	}
    } else {
	printf("  FileID: (%x %x %x %x)\n", id[0], id[1], id[2], id[3]);
    }
}

/*
 * Initialize the hash table.
 */
initHash(name)
char *name;
{
    FILE *inFile;
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
	if (fscanf(inFile,"%s %d %d %d %d", nameRecPtr->name,
	    &nameRecPtr->fileID[0], &nameRecPtr->fileID[1],
	    &nameRecPtr->fileID[2], &nameRecPtr->fileID[3]) != 5) break;
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
