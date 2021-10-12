// post.h -- higher level network interface

#ifndef POST_H
#define POST_H

#include "network.h"
#include "synch.h"

// The Post Office is where you send messages from and where you wait for
// messages to arrive for you.  There are a certain number of mailboxes.
// Each mailbox can hold one message.  The message waits in the box until
// somebody removes it, using PostOffice::GetFromBox.  If another message
// comes in the meantime it is thrown away.
//
// With each message, you get a return address, which consists of a "from
// address", which is the id of the machine that sent the message, and
// a "from box", which is the number of a mailbox on that machine that you
// can send an acknowledgement, if your protocol requires this.

class PostOffice {
  public:
    PostOffice(int size);
    ~PostOffice();
    
    // Send the message to the machine toAddr, to mailbox toBox.  Pass the
    // fromBox as the return box for acknowledgements.
    void SendMessage(NetworkAddress toAddr, char* data, int length, int toBox,
		    int fromBox);
    
    // If there is a message, then return the data by filling in the
    // variables pointed to by the arguments.  If there isn't one,
    // wait for it.  There may be multiple waiters at once.
    void ReceiveMessage(int num, char* data, int maxLength, int* length,
			NetworkAddress* fromAddr, int* fromBox);

    // This method will tell you if there is data waiting for you in the
    // mailbox.
    bool CheckBox(int num);
    
    // This is called by the interrupt handler.
    void HandleMessage();
    
    // This is used by HandleMessage.
    void PutInBox(int num, char* data, int length, NetworkAddress fromAddr,
		  int fromBox);
    
    void WakeupPostman() { messageAvailable->V(); }
    
    // The "friend" declaration says that the following functions can look
    // inside this class and access its private members.
    
    friend void MsPostman(int arg);
    friend void NetworkInterruptHandler(int arg);
    
  private:
    int numBoxes;
    class Mailbox* boxes;
    Semaphore* messageAvailable;
};

#endif
