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
    data = (int *)&tracePtr->data;
    type = (tracePtr->recordLen&TRACELOG_TYPEMASK)>>16;
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
    printf("%s.%3.3d: ", timeStr+11, usec/1000);
    switch (type) {
	case SOSP_OPEN:
	    printf("OPEN: HostID: %d, homeID: %d, fileID: (%x,%x,%x,%x),\n",
		    data[1], data[2], data[3], data[4], data[5], data[6]);
	    printf("\t\tstreamID: (%x, %x, %x, %x), effID: %d, realID %d,\n",
		    data[7], data[8], data[9], data[10], data[11], data[12]);
	    printf("\t\tmode %x, numNowReading: %d, numNowWriting: %d,\n",
		    data[13], data[14], data[15]);
	    printf("\t\tfileAge: %d, fileSize: %d\n", data[16], data[17]);
	    break;
	case SOSP_DELETE:
	    printf("DELETE: HostID: %d, homeID: %d, fileID: (%x,%x,%x,%x),\n",
		    data[1], data[2], data[3], data[4], data[5], data[6]);
	    printf("\t\tfileAge: %d, fileSize: %d\n", data[7], data[8]);
	case SOSP_CREATE:
	    printf("CREATE: HostID: %d, homeID: %d, fileID: (%x,%x,%x,%x)\n",
		    data[1], data[2], data[3], data[4], data[5], data[6]);
	    break;
	case SOSP_MKLINK:
	    printf("MKLINK: HostID: %d, homeID: %d, fileID: (%x,%x,%x,%x)\n",
		    data[1], data[2], data[3], data[4], data[5], data[6]);
	    break;
	case SOSP_SET_ATTR:
	    printf("SETATTR: HostID: %d, homeID: %d, fileID: (%x,%x,%x,%x)\n",
		    data[1], data[2], data[3], data[4], data[5], data[6]);
	    break;
	case SOSP_GET_ATTR:
	    printf("GETATTR: HostID: %d, homeID: %d, fileID: (%x,%x,%x,%x)\n", 
		    data[1], data[2], data[3], data[4], data[5], data[6]);
	    break;
	case SOSP_LSEEK:
	    printf("LSEEK: streamID: (%x,%x,%x,%x), old offset: %d, new: %d\n",
		    data[1], data[2], data[3], data[4], data[5], data[6]);
	    break;
	case SOSP_CLOSE:
	    printf("CLOSE: streamID: (%x,%x,%x,%x), offset: %d, fileSize: %d\n",
		    data[1], data[2], data[3], data[4], data[5], data[6]);
	    printf("\t\tflags: 0x%x\n", data[7]);
	    break;
	case SOSP_MIGRATE:
	    printf("MIGRATE: fromHostID: %d, toHostID: %d,\n",
		    data[1], data[2]);
	    printf("\t\tstreamID: (%x,%x,%x,%x), offset: %d\n",
		    data[3], data[4], data[5], data[6], data[7]);
	    break;
	case SOSP_TRUNCATE:
	    printf("TRUNCATE: streamID: (%x,%x,%x,%x),\n",
		data[1], data[2], data[3], data[4]);
	    printf("\t\toldLength: %d, newLength: %d\n", data[5], data[6]);
	    break;
	case SOSP_CONSIST_CHANGE:
	    printf("CONSIST_CHANGE: HostID: %d, fileID: (%x,%x,%x,%x),\n",
		    data[1], data[2], data[3], data[4], data[5]);
	    printf("\t\toperation: %d, forWrite?: %d\n", data[6], data[7]);
	    break;
	case SOSP_READ:
	    printf("READ: hostID: %d, streamID: (%x,%x,%x,%x),\n",
		    data[1], data[2], data[3], data[4], data[5]);
	    printf("\t\treadIt: %d, offset: %d, numbytes: %d\n",
		    data[6], data[7], data[8]);
	    break;
	case SOSP_LOOKUP:
	    dolookup(data);
	    break;
	case SOSP_CONSIST_ACTION:
	    printf("CONSIST_ACTION: causingHostID: %d, affectedHostID: %d,\n",
		    data[1], data[2]);
	    printf("\t\tfileID: (%x,%x,%x,%x), action: %x\n",
		    data[3], data[4], data[5], data[6], data[7]);
	    break;
	case SOSP_PREFIX:
	    printf("PREFIX: clientID: %d, rpcID: 0x%x\n", data[1], data[2]);
	    break;
	default:
	    printf("Unknown type: %d\n", type);
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
	sprintf(buf,"%x",status);
	return buf;
    }
}

char *status(x)
int x;
{
     if (x==0) return "success";
     if (x==FS_FILE_NOT_FOUND) return "not found";
     sprintf(buf,"%x",status);
     return buf;
}
