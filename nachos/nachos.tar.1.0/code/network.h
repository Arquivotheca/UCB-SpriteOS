/* network.h -- emulate a network interface.
 *
 * Here is how this works.  We start a new thread that acts as the
 * network hardware -- it basically loops, semi-busy waiting (I don't
 * think the UNIX SIGIO stuff works so this is the best we can do),
 * and whenever a packet comes in, it puts it in the network packet queue
 * and fires off a network interrupt for the next tick.  The interrupt
 * handler then calls the inputReady function, which ought to take the stuff
 * off the queue.  The network thread has a loop of (1) wait for input,
 * timing out after 0.1 seconds, (2) if there was input, deal with it,
 * (3) yield.
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "thread.h"
#include "synch.h"

// This is the address that the user specifies on the command line.
typedef int NetworkAddress;

// This is the largest size that the network will send.
#define MAX_PACKETSIZE 256

// I haven't lost my mind, it's backed up on tape somewhere.
//        - Peter da Silva

class Network {
  public:
    // Reliability must be a number between 0 and 1.  This is the chance
    // that the network will lose a packet.  Note that you must
    // do srandom() in main() or Initialize() to make the random
    // number seed something you know about.
    
    Network(NetworkAddress addr, double reliability);
    ~Network();
    
    // Send the specified data to toAddr.
    
    void Send(NetworkAddress toAddr, char* data, int length);

    // If there is a packet waiting, return it.  If there isn't,
    // wait until one comes.  Note that this blocks all the threads,
    // so you shouldn't call it unless you have recieved a NetworkInterrupt.
        
    int Receive(NetworkAddress* fromAddr, char* data, int length);
    
    // Is there data to be read?  If so, generate a NetworkInterrupt.
    // This routine should be called every so often -- at the top of
    // Machine::CheckInterrupts() is a good place.
    
    void CheckActive();
    
    // This should always be called before calling Receive.
    bool DataAvailable();
    
  private:
    NetworkAddress ident;
    double chanceToWork;
    int sock;
};

#endif
