// network.cc -- emulate a network interface, using UNIX sockets
//	operates in much the same way as the console device
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "network.h"
#include "scheduler.h"
#include "system.h"
#include "utility.h"
#include "interrupt.h"
#include "stats.h"

extern "C" {
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/file.h>
extern int recvfrom(int s, char* buf, int len, int flags,
		    struct sockaddr *from, int *fromlen);
extern int sendto(int s, char* msg, int len, int flags,
		  struct sockaddr* to, int tolen);
extern long random(void);
}

// Create a UNIX sockaddr 
static struct sockaddr_un
UnameFill(NetworkAddress addr)
{
    struct sockaddr_un uname;

    uname.sun_family = AF_UNIX;
    sprintf(uname.sun_path, "SOCKET_%d", (int) addr);
    return uname;
}

// Dummy functions because C++ can't call member functions indirectly 
static void NetworkReadPoll(int arg)
{ Network *network = (Network *)arg; network->CheckPktAvail(); }
static void NetworkSendDone(int arg)
{ Network *network = (Network *)arg; network->SendDone(); }

// Initialize the network emulation
//   addr is used to generate the socket name
//   reliability says whether we drop packets to emulate unreliable links
//   readAvail, writeDone, callArg -- analogous to console
Network::Network(NetworkAddress addr, double reliability,
	VoidFunctionPtr readAvail, VoidFunctionPtr writeDone, int callArg)
{
    ident = addr;
    if (reliability < 0) chanceToWork = 0;
    else if (reliability > 1) chanceToWork = 1;
    else chanceToWork = reliability;

    // set up the stuff to emulate asynchronous interrupts
    writeHandler = writeDone;
    readHandler = readAvail;
    handlerArg = callArg;
    sendBusy = FALSE;
    inHdr.length = 0;
    
    // Create the socket.
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    ASSERT(sock >= 0);

    // Bind it to a filename in the current directory.
    sprintf(sockName, "SOCKET_%d", (int)addr);
    (void) unlink(sockName);	// in case it's still around from last time
    
    struct sockaddr_un uname = UnameFill(addr);
    int retVal = bind(sock, (struct sockaddr *) &uname, sizeof(uname));
    ASSERT(retVal >= 0);
    DEBUG('n', "Created socket %s\n", sockName);

    // start polling for incoming packets
    interrupt->Schedule(NetworkReadPoll, (int)this, NetworkTime, NetworkRecvInt);
}

Network::~Network()
{
    unlink(sockName);
}

// if a packet is already buffered, we simply delay reading 
// the incoming packet.  In real life, the incoming 
// packet might be dropped if we can't read it in time.
void
Network::CheckPktAvail()
{
    // schedule the next time to poll for a packet
    interrupt->Schedule(NetworkReadPoll, (int)this, NetworkTime, NetworkRecvInt);

    // do nothing if packet is already buffered, or none to be read
    if ((inHdr.length != 0) || !PollFile(sock))
	return;		// packet already buffered, or none to read

    // otherwise, read packet in
    char *buffer = new char[MaxWireSize];
    int retVal = recvfrom(sock, buffer, MaxWireSize, 0,
			 	(struct sockaddr *) NULL, (int *)NULL);

    // divide packet into header and data
    inHdr = *(PacketHeader *)buffer;
    ASSERT((retVal == MaxWireSize) && (inHdr.to == ident) 
				&& (inHdr.length <= MaxPacketSize));
    bcopy(buffer + sizeof(PacketHeader), inbox, inHdr.length);

    DEBUG('n', "Network received packet from %d, length %d... ",
	  				(int) inHdr.from, inHdr.length);
    stats->numPacketsRecvd++;

    // and tell user packet has arrived
    (*readHandler)(handlerArg);		 
}

// notify user that another packet can be sent
void
Network::SendDone()
{
    sendBusy = FALSE;
    stats->numPacketsSent++;
    (*writeHandler)(handlerArg);
}

// send a packet by concatenating hdr and data, and schedule
// an interrupt to tell the user when the next packet can be sent 
//
// Note we always pad out a packet to MaxWireSize before putting it into
// the socket, because it's simpler at the receive end.
void
Network::Send(PacketHeader hdr, char* data)
{
    struct sockaddr_un uname = UnameFill(hdr.to);
    
    ASSERT((sendBusy == FALSE) && (hdr.length > 0) 
		&& (hdr.length <= MaxPacketSize) && (hdr.from == ident));
    DEBUG('n', "Sending to addr %d, %d bytes... ", hdr.to, hdr.length);

    interrupt->Schedule(NetworkSendDone, (int)this, NetworkTime, NetworkSendInt);

    if (random() % 100 >= chanceToWork * 100) { // emulate a lost packet
	DEBUG('n', "oops, lost it!\n");
	return;
    }

    // concatenate hdr and data into a single buffer, and send it out
    char *buffer = new char[MaxWireSize];
    *(PacketHeader *)buffer = hdr;
    bcopy(data, buffer + sizeof(PacketHeader), hdr.length);
    int retVal = sendto(sock, buffer, MaxWireSize, 0, 
			(struct sockaddr *) &uname, sizeof(uname));
    ASSERT(retVal == MaxWireSize);
    delete buffer;
}

// read a packet, if one is buffered
PacketHeader
Network::Receive(char* data)
{
    PacketHeader hdr = inHdr;

    inHdr.length = 0;
    if (hdr.length != 0)
    	bcopy(inbox, data, hdr.length);
    return hdr;
}
