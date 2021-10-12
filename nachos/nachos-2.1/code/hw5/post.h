// post.h -- higher level network interface
//
// The US Post Office delivers mail to the addressed mailbox. 
// By analogy, our post office delivers packets to a specific buffer 
// (mail box), based on the a mail box number stored in the packet header.
// Mail waits in the box until a thread asks for it; if the mail box
// is empty, threads can wait for mail to arrive in it. 
//
// Thus, the service the post office provides is to de-multiplex 
// incoming packets, delivering them to the appropriate thread.
//
// With each message, you get a return address, which consists of a "from
// address", which is the id of the machine that sent the message, and
// a "from box", which is the number of a mailbox on that machine that you
// can send an acknowledgement, if your protocol requires this.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef POST_H
#define POST_H

#include "network.h"
#include "synch.h"
#include "synchlist.h"

typedef int MailBoxAddress;

// prepended to message, before message is sent to Network
typedef struct {
    MailBoxAddress to;
    MailBoxAddress from;
    int length;		// bytes of message data (excluding the mail header)
} MailHeader;

#define MaxMailSize 	(MaxPacketSize - sizeof(MailHeader))

// storage for an incoming message
typedef struct {
     PacketHeader pktHdr;
     MailHeader mailHdr;
     char data[MaxMailSize];
} Mail;

class MailBox {
  public: 
    MailBox() { messages = new SynchList(); }

   // synchronized put and get
    void Put(PacketHeader pktHdr, MailHeader mailHdr, char *data);
    void Get(PacketHeader *pktHdr, MailHeader *mailHdr, char *data); 

  private:
    SynchList *messages;	// list of incoming messages
};

class PostOffice {
  public:
    PostOffice(NetworkAddress addr, double reliability, int nBoxes);
    ~PostOffice();
    
    // Send a message to a mailbox on a remote machine.
    // Use the fromBox in the MailHeader as the return box for ack's.
    void Send(PacketHeader pktHdr, MailHeader mailHdr, char *data);
    
    // Return a message if there is one in box; otherwise, wait.
    void Receive(int box, PacketHeader *pktHdr, 
				MailHeader *mailHdr, char *data);

    void MsPostMan();  // wait for messages, and put them in the mailbox
    void PacketSent() { messageSent->V(); }
    void IncomingPacket() { messageAvailable->V(); }
    
  private:
    Network *network;		 // physical network
    NetworkAddress netAddr;	 // our net address
    MailBox *boxes;		 // table of mail boxes to hold incoming mail
    int numBoxes;
    Semaphore *messageAvailable; // message has arrived
    Semaphore *messageSent;	 // message has been sent
    Lock *sendLock;		 // only one outgoing message at a time
};

#endif
