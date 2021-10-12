extern long startSec;
extern long startUsec;

int ops[20]={0};
int nlookups = 0;
int ncomps = 0;

char *opname(), *status();

dolookup(data)
    int *data;
{
    int i;
    printf("LOOKUP: hostID: %d, home: %d, %s, numIDs %d, op %s\n", 
	    data[1], data[2], status(data[7]), data[8],
	    opname(data[9]));
    if (data[9]>=0 && data[9]<20) ops[data[9]]++;
    for (i=0;i<data[8];i++) {
	lookupID(&data[10+i*4],1);
    }
    nlookups++;
    ncomps += data[8];
    printf("    Result:");
    lookupID(&data[3],0);
}

donelookup()
{
    int i;
    for (i=0;i<20;i++) {
	if (ops[i]>0) {
	    printf("%s: %d\n", opname(i), ops[i]);
	}
    }
    printf("# lookups: %d, avg components: %5.2f\n", nlookups,
	    ncomps/(float)nlookups);
}
