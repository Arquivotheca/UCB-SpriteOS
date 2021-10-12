// post.cc -- higher level network interface
//
// Deliver incoming messages to specific mailboxes (like the postal service),
// synchronizing incoming messages with receivers.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "post.h"
#include "synchlist.h"

static void
PrintHeader(PacketHeader pktHdr, MailHeader mailHdr)
{
    printf("From (%d, %d) to (%d, %d) bytes %d\n",
    	    pktHdr.from, mailHdr.from, pktHdr.to, mailHdr.to, mailHdr.length);
}

// Put a message into a mailbox, waking up anyone who is waiting
void 
MailBox::Put(PacketHeader pktHdr, MailHeader mailHdr, char *data)
{ 
    Mail *mail = new Mail; 

    mail->pktHdr = pktHdr;
    mail->mailHdr = mailHdr;
    bcopy(data, mail->data, mailHdr.length);
    messages->Append((void *)mail);	// will wake up any waiters
}

// Get a message from a mailbox, waiting if none is available
void 
MailBox::Get(PacketHeader *pktHdr, MailHeader *mailHdr, char *data) 
{ 
    DEBUG('n', "Waiting for mail in mailbox\n");
    Mail *mail = messages->Remove();	// will wait if list is empty

    *pktHdr = mail->pktHdr;
    *mailHdr = mail->mailHdr;
    if (DebugIsEnabled('n')) {
	printf("Got mail from mailbox: ");
	PrintHeader(*pktHdr, *mailHdr);
    }
    bcopy(mail->data, data, mail->mailHdr.length);
    delete mail;
}

// Dummy functions because C++ can't indirectly invoke member functions
static void MrPostman(int arg)
{ PostOffice* po = (PostOffice *) arg; po->MsPostMan(); }
static void ReadAvail(int arg)
{ PostOffice* po = (PostOffice *) arg; po->IncomingPacket(); }
static void WriteDone(int arg)
{ PostOffice* po = (PostOffice *) arg; po->PacketSent(); }

// Constructor -- initialize the post office, and the network
PostOffice::PostOffice(NetworkAddress addr, double reliability, int nBoxes)
{
    network = new Network(addr, reliability, ReadAvail, WriteDone, (int) this);
    netAddr = addr; 
    numBoxes = nBoxes;
    boxes = new MailBox[nBoxes];

    // for synchronization with the interrupt handlers
    messageAvailable = new Semaphore("message available", 0);
    messageSent = new Semaphore("message sent", 0);
    sendLock = new Lock("message send lock");
    
    // Create a thread whose sole job is to wait for incoming messages,
    // and put them in the right mailbox. 
    Thread *t = new Thread("postal worker");
    t->Fork(MrPostman, (int) this);
}

PostOffice::~PostOffice()
{
    delete network;
    delete boxes;
    delete messageAvailable;
    delete messageSent;
    delete sendLock;
}

// Wait for incoming messages, and put them in the right mailbox
// Incoming messages have the MailHeader tacked on the front
void
PostOffice::MsPostMan()
{
    PacketHeader pktHdr;
    MailHeader mailHdr;
    char *buffer = new char[MaxPacketSize];

    for (;;) {
        // first, wait for a message
        messageAvailable->P();	
        pktHdr = network->Receive(buffer);

        // then, put message in the correct mailbox
        mailHdr = *(MailHeader *)buffer;
        if (DebugIsEnabled('n')) {
	    printf("Putting mail into mailbox: ");
	    PrintHeader(pktHdr, mailHdr);
        }
        boxes[mailHdr.to].Put(pktHdr, mailHdr, buffer + sizeof(MailHeader));
    }
}

// Concatenate the MailHeader and the data, and pass to the Network
void
PostOffice::Send(PacketHeader pktHdr, MailHeader mailHdr, char* data)
{
    char* buffer = new char[MaxPacketSize];

    if (DebugIsEnabled('n')) {
	printf("Post send: ");
	PrintHeader(pktHdr, mailHdr);
    }
    ASSERT(mailHdr.length <= MaxMailSize);
    
    // fill in pktHdr, for the Network layer
    pktHdr.from = netAddr;
    pktHdr.length = mailHdr.length + sizeof(MailHeader);

    // concatenate MailHeader and data
    bcopy(&mailHdr, buffer, sizeof(MailHeader));
    bcopy(data, buffer + sizeof(MailHeader), mailHdr.length);

    sendLock->Acquire();   // need mutually exclusive access to the network
    network->Send(pktHdr, buffer);
    messageSent->P();
    sendLock->Release();

    delete buffer;
}

// Retrieve a message from a specific box if one is available, otherwise wait
void
PostOffice::Receive(int box, PacketHeader *pktHdr, 
				MailHeader *mailHdr, char* data)
{
    ASSERT((box >= 0) && (box < numBoxes));

    boxes[box].Get(pktHdr, mailHdr, data);
}
