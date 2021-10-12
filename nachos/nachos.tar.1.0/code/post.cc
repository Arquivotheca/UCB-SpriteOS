// post.cc -- higher level network interface

#include "system.h"
#include "post.h"
#include "scheduler.h"

// A mailbox contains one message at a time.

class Mailbox {
  public:
    Mailbox() {
	lock = new Lock("mailbox lock");
	cond = new Condition("mailbox condition", lock);
	curData = NULL;
    }

    ~Mailbox() {
	delete cond;
	delete lock;
    }
    
    int Put(char* data, int length, NetworkAddress fromAddr, int fromBox) {
	DEBUG('n', "Thread %s in mailbox put\n", currentThread->getName());
	lock->Acquire();
	DEBUG('n', "Thread %s got the lock\n", currentThread->getName());
	if (curData) {
	    DEBUG('n', "Data already in box, dropping packet...\n");
	    delete data;
	} else {
	    curData = data;
	    curLength = length;
	    curFromAddr = fromAddr;
	    curFromBox = fromBox;
	
	    DEBUG('n', "Thread %s signalling\n", currentThread->getName());
	    cond->Signal();
	}
	lock->Release();
    }
    
    int Get(char* data, int maxLength, int* length, NetworkAddress* fromAddr,
	    int* fromBox) {
	DEBUG('n', "Thread %s in mailbox::get\n", currentThread->getName());
	lock->Acquire();
	DEBUG('n', "Thread %s got the lock\n", currentThread->getName());
	while (!curData)
	    cond->Wait();
	DEBUG('n', "Thread %s got signalled\n", currentThread->getName());
	int len = curLength;
	if (len > maxLength)
	    len = maxLength;
	bcopy(curData, data, len);
	*length = len;
	*fromAddr = curFromAddr;
	*fromBox = curFromBox;
	delete curData;
	curData = NULL;
	DEBUG('n', "Thread %s: got %d bytes from %d, from box %d\n",
	      currentThread->getName(), len, curFromAddr, curFromBox);
	lock->Release();
    }
    
    bool Check() {
	// We don't need to get a lock, since this is only a read.
	bool result = (curData ? TRUE : FALSE);
	DEBUG('n', "Thread %s checking mailbox: %s\n", currentThread->getName(),
	      (result ? "something there!" : "empty..."));
	return (result);
    }
        
  private:
    Lock* lock;
    Condition* cond;
    char* curData;
    int curLength;
    NetworkAddress curFromAddr;
    int curFromBox;
};

static void
NetworkInterruptHandler(int arg)
{
    DEBUG('n', "INTERRUPT: Input is ready for the network\n");
    postOffice->messageAvailable->V();
}

// The reason we need a separate thread to do this is that if the interrupt
// handler took the data from the network and put it in the mailbox,
// it would have to acquire a lock, and that is forbidden to interrupt
// handlers.

static void
MsPostman(int arg)
{
  PostOffice* po = (PostOffice *) arg;
  for (;;) {
      while (!network->DataAvailable())
	  po->messageAvailable->P();
      po->HandleMessage();
  }
}

PostOffice::PostOffice(int size)
{
    numBoxes = size;
    boxes = new Mailbox[size];
    messageAvailable = new Semaphore("message available");
    
    (void) new Thread("post man", MsPostman, (int) this);
    machine->setInterruptHandler(NetworkInterrupt, NetworkInterruptHandler);
}

PostOffice::~PostOffice()
{
    delete messageAvailable;
    delete boxes;
}

void
PostOffice::SendMessage(NetworkAddress toAddr, char* data, int length,
			int toBox, int fromBox)
{
    DEBUG('n', "Post send: from box %d to addr %d box %d bytes %d\n",
	  fromBox, toAddr, toBox, length);
    
    char* buffer = new char[length + sizeof (int) * 2];
    ((int *) buffer)[0] = toBox;
    ((int *) buffer)[1] = fromBox;
    bcopy(data, buffer + sizeof (int) * 2, length);
    network->Send(toAddr, buffer, length + sizeof (int) * 2);
    delete buffer;
}

void
PostOffice::ReceiveMessage(int num, char* data, int maxLength, int* length,
			    NetworkAddress* fromAddr, int* fromBox)
{
    ASSERT((num >= 0) && (num < numBoxes));
    boxes[num].Get(data, maxLength, length, fromAddr, fromBox);
}

// Beware of mathematicians and all those who make empty prophecies.
// The danger already exists that the mathematicians have made covenant with
// the devil to darken the spirit and to confine man in the bonds of hell.
//        - St. Augustine

void
PostOffice::HandleMessage()
{
    NetworkAddress fromAddr;
    char* buffer = new char[MAX_PACKETSIZE];
    int length = network->Receive(&fromAddr, buffer, MAX_PACKETSIZE);
    int toBox = ((int *) buffer)[0];
    int fromBox = ((int *) buffer)[1];
    length -= sizeof (int) * 2;
    char* data = new char[length];
    bcopy(buffer + sizeof (int) * 2, data, length);
    DEBUG('n', "Post recieve: from box %d addr %d into box %d bytes %d\n",
	  fromBox, fromAddr, toBox, length);
    PutInBox(toBox, data, length, fromAddr, fromBox);
}

void
PostOffice::PutInBox(int num, char* data, int length, int fromAddr, int fromBox)
{
    ASSERT((num >= 0) && (num < numBoxes));
    boxes[num].Put(data, length, fromAddr, fromBox);
}

bool
PostOffice::CheckBox(int num)
{
    ASSERT((num >= 0) && (num < numBoxes));
    return (boxes[num].Check());
}
