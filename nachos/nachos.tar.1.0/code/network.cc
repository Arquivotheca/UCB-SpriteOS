/* network.cc -- emulate a network interface. */

#include "network.h"
#include "scheduler.h"
#include "system.h"

extern "C" {
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
extern int recvfrom(int s, char* buf, int len, int flags,
		    struct sockaddr *from, int *fromlen);
extern int sendto(int s, char* msg, int len, int flags,
		  struct sockaddr* to, int tolen);
}

// This has a delay of 1/10 of a second, so it's not quite busy waiting.

static bool
pollInput(int fd)
{
    int rfd = (1 << fd), wfd = 0, xfd = 0;
    struct timeval shorttime;
    shorttime.tv_sec = 0;
    shorttime.tv_usec = 100000;
    int i = select(32, &rfd, &wfd, &xfd, &shorttime);
    if (i == -1) {
	perror("select");
	exit(1);
    }
    return (i ? TRUE : FALSE);
}

// -------------------------------------

inline void
checkReturn(int val, int expected, char* name)
{
    if (val == -1) {
	perror(name);
	exit(1);
    } else if (val < expected) {
	fprintf(stderr, "%s: expected return value %d, got %d\n", name,
		expected, val);
	exit(1);
    }
}

// O sibili si emgo.
// Fortibusis e naro.
// O nobili demis trux.
// Vadis enim?  Caus en dux.
//		- Dubious Cogitus the Elder

struct PacketHeader {
    NetworkAddress who;
    int length;
};

Network::Network(NetworkAddress addr, double reliability)
{
    ident = addr;
    if (reliability < 0) reliability = 0;
    if (reliability > 1) reliability = 1;
    chanceToWork = reliability;
    
    // Create the socket.
    if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }

    // Bind it to a filename in the current directory.
    char name[64];
    sprintf(name, "SOCKET_%d", (int) addr);
    (void) unlink(name);
    
    struct sockaddr_un uname;
    uname.sun_family = AF_UNIX;
    strcpy(uname.sun_path, name);
    if (bind(sock, (struct sockaddr *) &uname, sizeof (uname)) < 0) {
	perror("bind");
	exit(1);
    }
    
    DEBUG('n', "* Created socket %s\n", name);
}

Network::~Network()
{
    char buf[32];
    sprintf(buf, "SOCKET_%d", ident);
    unlink(buf);
}

// This is an ugly hack but I don't know how else to do it.

void
Network::CheckActive()
{
    if (DataAvailable())
	machine->ScheduleInterrupt(NetworkInterrupt, 0, 1);
}

bool
Network::DataAvailable()
{
    return (pollInput(sock));
}

void
Network::Send(NetworkAddress toAddr, char* data, int length)
{
    if (length > MAX_PACKETSIZE) {
	fprintf(stderr, "Error: length too large for send: %d, max is %d\n",
		length, MAX_PACKETSIZE);
	exit(1);
    }
    DEBUG('n', "* Sending to addr %d, %d bytes... ", (int) toAddr, length);

    if (random() % 100 >= chanceToWork * 100) {
	DEBUG('n', "oops, lost it!\n");
	return;
    }
    
    struct sockaddr_un uname;
    uname.sun_family = AF_UNIX;
    sprintf(uname.sun_path, "SOCKET_%d", toAddr);
    PacketHeader ph;
    ph.who = ident;
    ph.length = length;
    checkReturn(sendto(sock, (char *) &ph, sizeof (ph), 0,
		       (struct sockaddr *) &uname, sizeof (uname)),
		sizeof (ph), "sendto");
    checkReturn(sendto(sock, data, length, 0, (struct sockaddr *) &uname,
		       sizeof (uname)),
		length, "sendto");
    DEBUG('n', "done!\n");
}

int
Network::Receive(NetworkAddress* fromAddr, char* data, int length)
{
    PacketHeader ph;
    struct sockaddr_un uname;
    
    int flen = sizeof (uname);
    checkReturn(recvfrom(sock, (char *) &ph, sizeof (ph), 0,
			 (struct sockaddr *) &uname, &flen),
		sizeof (ph), "recvfrom");
    
    DEBUG('n', "* Network recieved packet from %d, length %d... ",
	  (int) ph.who, ph.length);
    
    if (ph.length > MAX_PACKETSIZE) {
	fprintf(stderr, "Error: packet from sender %d: size %d, max is %d\n",
		ph.who, ph.length, MAX_PACKETSIZE);
	exit(1);
    }
    char* buffer = new char[ph.length];
    
    flen = sizeof (uname);
    checkReturn(recvfrom(sock, buffer, ph.length, 0,
			 (struct sockaddr *) &uname, &flen),
		ph.length, "recvfrom");
    
    DEBUG('n', "got the data!\n");
        
    int cc = (length < ph.length) ? length : ph.length;
    bcopy(buffer, data, cc);
    delete buffer;
    *fromAddr = ph.who;
    return (cc);
}
