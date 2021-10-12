#define NAMELEN 100
typedef struct nameRec {
    char name[NAMELEN];
    int fileID[4];
    int count;
} nameRec;

extern Hash_Table *tablePtr;
