#include <stdio.h>
#include <signal.h>
#include <sprite.h>
#include <sysStats.h>
#include <sospRecord.h>
#include <time.h>
#include <status.h>
#include <namehash.h>

extern long startSec;
extern long startUsec;

char *opname(), *status();

char *timeStr="";

long sec, usec;

void
dorec(tracePtr, machine)
    Sys_TracelogRecord *tracePtr;
    int machine;
{
    int *data;
    int type;
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
#if 0
    timeStr = ctime((time_t *)&sec);
    timeStr[19] = '\0';
    sprintf(timeStr+19,".%3.3d",usec/1000);
#endif
#if 0
    printf("%s.%3.3d: ", timeStr+4, usec/1000);
#endif
    switch (type) {
	case SOSP_GET_ATTR:
	    dogetattr(data, machine);
	    break;
	case SOSP_SET_ATTR:
	    dosetattr(data, machine);
	    break;
	case SOSP_LOOKUP:
	    dolookup(data, machine);
	    break;
	case SOSP_OPEN:
	    doopen(data, machine);
	    break;
	case SOSP_CLOSE:
	    doclose(data, machine);
	    break;
    }
}

char buf[10];

char *opnames[] = {"Import", "Export", "Open", "Stat", "SetStat",
     "MakeDevice", "Mkdir", "Unlink", "Rmdir", "Rename", "Link",
     "Link(2)"};

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
     if (x==FS_LOCAL_OP_INVALID) return "local op invalid";
     if (x==FS_IS_DIRECTORY) return "is directory";
     if (x==FS_NOT_DIRECTORY) return "not directory";
     if (x==FS_NOT_OWNER) return "not owner";
     if (x==FS_FILE_EXISTS) return "file exists";
     if (x==FS_DIR_NOT_EMPTY) return "dir not empty";
     sprintf(buf,"%x",x);
     return buf;
}

char *types[] = {"File", "Directory", "Symbolic link", "Remote link",
    "Device", "Remote device", "Local pipe", "Named pipe", "Pseudo dev",
    "Pseudo FS", "Extra file"};

char *filetype(x)
int x;
{
    if (x>=0 && x<sizeof(types)/sizeof(char *)) {
	return types[x];
    } else {
	sprintf(buf,"%x",x);
	return buf;
    }
}
