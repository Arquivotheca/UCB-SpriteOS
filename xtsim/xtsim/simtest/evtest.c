/*
 *
 */

#include "event.h"
#include "schedule.h"

static int data;
static int numData;
static int printMsg=1;

int Source();
int Sink();

main(argc, argv)
    int         argc;
    char        *argv[];
{
    Event *rdy, *ack;
    int	pid;

    if (argc > 1) {
	numData = atoi(argv[1]);
    } else {
	numData = 1000;
    }
    if (argc > 2) {
	printMsg = 0;
    }
    InitSim();
    rdy = AllocEvent();
    ack = AllocEvent();
    if (Spawn("sink") == 0) {
	Sink(ack, rdy);
    } else {
	Source(rdy, ack);
    }
}

Source (rdy, ack)
    Event *rdy, *ack;
{
    int i;

    for (i = 0; i < numData; i++) {
    /*
	Delay(1);
    */
	data = i;
	if (printMsg) {
	    printf("Data= %5d Data Sent\n", data);
	}
	CauseEvent( rdy );
	WaitEvent(ack);
    }
    printf("numData = %d\n", i);
}

Sink (ack, rdy)
    Event *ack, *rdy;
{
    for (;;) {
	if (printMsg) {
	    printf("Data= %5d Data Received\n", data);
	}
	CauseEvent( ack );
	WaitEvent( rdy);
    /*
	Delay(1);
    */
    }
}
