
/* main.c -- entry point into the operating system */

#include "system.h"
#include "memorymgr.h"
#include "addrspace.h"
#include "scheduler.h"
#include "network.h"
#include "console.h"
#include "post.h"

/* Assignment 5: networking. */

static int othermachine = 0;

// To talk, send and recieve to box 0.  No replies.

static void
sendStuff(int who)
{
    for (;;) {
	char buffer[80];
	console->ReadString(buffer, 80);
	postOffice->SendMessage(othermachine, buffer, strlen(buffer) + 1, 0, 0);
    }
}

static void
receiveStuff(int who)
{
    for (;;) {
	char buffer[80], buffer2[100];
	int fa, fb, len;
	postOffice->ReceiveMessage(0, buffer, 80, &len, &fa, &fb);
	sprintf(buffer2, "From %d: %s", fa, buffer);
	console->WriteString(buffer2);
    }
}

/* The main routine. */
int
main(int argc, char **argv)
{
    bool ssmode = FALSE;
    
    Initialize(argc, argv);
    
    /* Parse the arguments. */
    argc--; argv++;
    while (argc > 0) {
	if (!strcmp(*argv, "-s")) {
	    ssmode = TRUE;
	} else if (!strcmp(*argv, "-o")) {
	    othermachine = atoi(*++argv);
	    argc--;
	} else if (!strcmp(*argv, "-rs")) {
	    srandom(atoi(*++argv));
	    argc--;
	}
	argc--; argv++;
    }
    
    // Now we have to wait a bit so that the user can start up two nachos
    // processes at one time.
    sleep(2);

    (void) new Thread("sender", sendStuff, 0);
    receiveStuff(0);
    
#if 0
    if (othermachine)
	for (int i = 0; i < 5; i++) {
	    char* foo = new char[32];
	    sprintf(foo, "number %d", i);
	    (void) new Thread(foo, talkTo, i);
	}
#endif
    
    currentThread->Stop(NULL);
    
    return(0);  /* Not reached. */
}

#if 0

// Send a message to the other guy at box who, and tell him to ack at
// box who + 5.  Then wait for his message, and when it comes, send
// the acknowledgement.  Then wait for the ack from him.

static void
mailboxTest(int who)
{
    char stuff[32];
    int len;
    NetworkAddress fromAddr;
    int fromBox;
    
    // This is the box that we expect our acks to come in.
    int rep = who + 5;
    
    // Send the first message.
    sprintf(stuff, "Hello world, I'm %d !", netname);
    postOffice->SendMessage(othermachine, stuff, strlen(stuff) + 1, who, rep);

    // Wait for the first message from the other machine.
    postOffice->ReceiveMessage(who, stuff, 32, &len, &fromAddr, &fromBox);
    printf("Got \"%s\" from %d at box %d, replying to box %d\n", stuff,
	   fromAddr, who, fromBox);

    // Send the ack to the box that the other machine requested.  We
    // don't expect an ack to our ack.
    sprintf(stuff, "Got it!");
    postOffice->SendMessage(othermachine, stuff, strlen(stuff) + 1, fromBox, 0);

    // Wait for the ack from the first message we sent.
    postOffice->ReceiveMessage(rep, stuff, 32, &len, &fromAddr, &fromBox);
    printf("Ack is: %s from %d at box %d\n", stuff, fromAddr, rep);
}
#endif
