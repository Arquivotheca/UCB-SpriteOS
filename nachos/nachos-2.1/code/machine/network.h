// network.h -- emulate a network (with an interface similar to the console)
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef NETWORK_H
#define NETWORK_H

#include "utility.h"

typedef int NetworkAddress;	 // specified on the command line

// format on the wire is: PacketHeader, then the data
typedef struct {
    NetworkAddress to;
    NetworkAddress from;
    int length;	 // bytes of packet data, excluding the packet header
		 // (but including the MailHeader prepended by the post office)
} PacketHeader;

#define MaxWireSize 	64	 // largest packet that can go out on the wire
#define MaxPacketSize 	(MaxWireSize - sizeof(struct PacketHeader))	

class Network {
  public:
    // Reliability must be a number between 0 and 1.  This is the chance
    // that the network will lose a packet.  Note that you must
    // do srandom() in main() or Initialize() to make the random
    // number seed something you know about.
    
    Network(NetworkAddress addr, double reliability,
  	  VoidFunctionPtr readAvail, VoidFunctionPtr writeDone, int callArg);
    ~Network();
    
    // Send the specified data to to.who.  Returns immediately.
    // writeHandler is invoked once the next packet can be sent.
    // Note that writeHandler is called whether or not the packet is dropped.
    // Also, the from field of the PacketHeader is filled in automatically.
    void Send(PacketHeader hdr, char* data);

    // Poll network input.  If there is a packet waiting, copy the packet 
    // into "data" and return the header.  If no packet is waiting, 
    // return a header with length 0.  Call this when readHandler is invoked.
    PacketHeader Receive(char* data);

    void SendDone();		// internal emulation routines
    void CheckPktAvail();

  private:
    NetworkAddress ident;	// our network address
    double chanceToWork;	// likelihood packet will be dropped
    int sock;			// UNIX socket number for incoming packets
    char sockName[32];		// name of UNIX socket
    VoidFunctionPtr writeHandler; // next packet can be sent
    VoidFunctionPtr readHandler;  // packet has arrived
    int handlerArg;		  // pointer to post office
    bool sendBusy;		// packet is being sent
    bool packetAvail;		// packet arrived some time ago 
    PacketHeader inHdr;		// info about arrived packet
    char inbox[MaxPacketSize];  // data for arrived packet
};


#endif
