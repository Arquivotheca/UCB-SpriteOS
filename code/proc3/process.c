#include <stdio.h>
#include <signal.h>
#include <sprite.h>
#include <sysStats.h>
#include <kernel/sospRecord.h>
#include <time.h>
#include <status.h>

extern long startSec;
extern long startUsec;

char *opname(), *status();

dorec(tracePtr)
    Sys_TracelogRecord *tracePtr;
{
    int *data;
    int type;
    int i;
    long sec, usec;
    char *timeStr;
    int  len;
    data = (int *)&tracePtr->data;
    type = (tracePtr->recordLen&TRACELOG_TYPEMASK)>>16;
    len = (tracePtr->recordLen & TRACELOG_BYTEMASK);
    if (type != data[0]) {
	printf("Warning: type mismatch: %d vs %d\n", type, data[0]);
	type = data[0];
    }
    usec = tracePtr->time[1] + startUsec;
    sec = tracePtr->time[0] + startSec;
    if (usec>1000000) {
	sec++;
	usec -= 1000000;
    }
    timeStr = ctime((time_t *)&sec);
    timeStr[19] = '\0';
#if 0
    printf("%s.%3.3d: ", timeStr+4, usec/1000);
#endif
    switch (type) {
	case SOSP_LOOKUP:
	    dolookup(data);
	    break;
    }
}

char buf[10];

char *opnames[] = {"Import", "Export", "Open", "GetAttrPath", "SetAttrPath",
     "MakeDevice", "MakeDir", "Remove", "RemoveDir", "Rename", "HardLink"};

char *opname(x)
int x;
{
    if (x>=0 && x<sizeof(opnames)/sizeof(char *)) {
	return opnames[x];
    } else {
	sprintf(buf,"%x",x);
	return buf;
    }
}

char *status(x)
int x;
{
     if (x==0) return "success";
     if (x==FS_FILE_NOT_FOUND) return "not found";
     if (x==FS_NO_ACCESS) return "no access";
     if (x==FS_LOOKUP_REDIRECT) return "lookup redirect";
     if (x==FS_IS_DIRECTORY) return "is directory";
     if (x==FS_NOT_DIRECTORY) return "not directory";
     if (x==FS_NOT_OWNER) return "not owner";
     if (x==FS_FILE_EXISTS) return "file exists";
     if (x==FS_DIR_NOT_EMPTY) return "dir not empty";
     sprintf(buf,"%x",x);
     return buf;
}
