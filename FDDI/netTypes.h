/*
 * netTypes.h --
 *
 *	This defines the types and contants for the networking software.
 *
 * Copyright 1985, 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 *
 * $Header: /sprite/src/kernel/Cvsroot/kernel/net/netTypes.h,v 1.7 91/09/18 22:19:35 jhh Exp $ SPRITE (Berkeley)
 */

#ifndef _NETTYPES
#define _NETTYPES

#ifdef KERNEL
#include <netEther.h>
#include <user/net.h>
#include <syncTypes.h>
#include <fs.h>
#else
#include <netEther.h>
#include <net.h>
#include <kernel/syncTypes.h>
#include <kernel/fs.h>
#endif

/*
 * This define is used by the net module in maintaining routing
 * information.  It is needed by other modules that maintain state
 * about the various Sprite hosts.
 */

#define NET_NUM_SPRITE_HOSTS	200

/*
 * Maximum number of network interfaces a host may have.
 */

#define NET_MAX_INTERFACES	3

/*
 * Constants defining the different types of packets.
 */

#define NET_PACKET_UNKNOWN	0x0
#define NET_PACKET_SPRITE	0x1
#define NET_PACKET_ARP		0x2
#define NET_PACKET_RARP		0x3
#define NET_PACKET_DEBUG	0x4
#define NET_PACKET_IP		0x5

/*
 * Scatter/gather vector element.  The network output routines take
 * an array of these elements as a specifier for the packet.  This
 * format lets clients of the network module save extra copies because
 * they can leave data objects where they lie.  The done and mutexPtr
 * parts are used to wait for the packet to be truely output.  The
 * mutex is released while the packet is output.
 */

typedef struct {
    Address		bufAddr;	/* In - Buffer address */
    int			length;		/* In - Size of the buffer */
    Sync_Semaphore	*mutexPtr;	/* Private to net module.
					 * Used to wait for output. */
    Boolean		done;		/* Out - set when I/O completes */
    void		((*callBackFunc)());	/* Call-back to say when
						 * we're done sending packet. */
    ClientData		clientData;	/* Client data to pass to call-back. */
} Net_ScatterGather;

/*
 * Statistics - the ethernet drivers record the number of occurences
 *	of various events.
 */
typedef struct {
    int	packetsRecvd;		/* # packets received of any type */
    int	packetsSent;		/* # packets sent of any type */
    int	packetsOutput;		/* # packets output of any type */
    int broadRecvd;		/* # broadcast packets received */
    int broadSent;		/* # broadcast packets sent */
    int others;			/* # packets between two other machines */
    int overrunErrors;		/* # packets received with overrun errors. */
    int crcErrors;		/* # packets received with CRC errors. */
    int fcsErrors;		/* # packets received with FCS errors */
    int frameErrors;		/* # packets received with framing errors */
    int rangeErrors;		/* # packets received with range errors */
    int collisions;		/* # of collisions on transmissions */
    int xmitCollisionDrop;	/* # of packets dropped because of too many
				   collisions. */
    int	xmitPacketsDropped;	/* # transmitted packets that are dropped */
    int	recvPacketsDropped;	/* # transmitted packets that are dropped */
    int matches;		/* # of address match packets */
    int recvAvgPacketSize;	/* average size of packets received */
    int recvAvgLargeSize;	/*  ...  of more than 100 bytes */
    int recvAvgSmallSize;	/*  ...  of less than 100 bytes */
    int sentAvgPacketSize;	/* average size of packets sent */
    int sentAvgLargeSize;	/*  ...  of more than 100 bytes */
    int sentAvgSmallSize;	/*  ...  of less than 100 bytes */
    int	bytesSent;		/* Total number of bytes transmitted. */
    int	bytesReceived;		/* Total number of bytes received. */
} Net_EtherStats;

/*
 * Statistics for the UltraNet interface. 
 */
typedef struct Net_UltraStats {
    int		packetsSent;		/* Number of packets sent. */
    int		bytesSent;		/* Number of bytes sent. */
    int		sentHistogram[33];	/* Histogram of bytes sent 
					 * (1K buckets). */
    int		packetsReceived;	/* Number of packets received. */
    int		bytesReceived;		/* Number of bytes received. */
    int		receivedHistogram[33];	/* Histogram of bytes received
					 * (1K buckets). */
} Net_UltraStats;

/*
 * Statistics for the FDDI interface.
 */

/*
 * Granularity at which we keep track of the size of packets sent
 * and received.
 */
#define NET_FDDI_STATS_HISTO_SHIFT      7
#define NET_FDDI_STATS_HISTO_SIZE       128
/*
 * Number of buckets in the packet size histogram.
 */
#define NET_FDDI_STATS_HISTO_NUM  \
          (NET_FDDI_MAX_BYTES >> NET_FDDI_STATS_HISTO_SHIFT)
/*
 * Greatest number of packets that could be reaped in one receive
 * interrupt.  See netDFInt.h:NET_DF_NUM_XMIT_ELEMENTS.
 */
#define NET_FDDI_STATS_RCV_REAPED       32

typedef struct Net_FDDIStats {
    int		packetsSent;		/* Number of packets sent. */
    int		bytesSent;	        /* Number of bytes sent. */
    int         transmitHistogram[NET_FDDI_STATS_HISTO_NUM];
                                        /* Histogram of packet sizes sent */
    int		packetsReceived;	/* Number of packets received. */
    int		bytesReceived;		/* Number of bytes received. */
    int         receiveHistogram[NET_FDDI_STATS_HISTO_NUM];
                                        /* Histogram of rcved packet sizes */
    int         receiveReaped[NET_FDDI_STATS_RCV_REAPED];
                                        /* Number of packets reaped per
					 * receive interrupt */
    int         xmtPacketsDropped;      /* Packets dropped because 
					 * of lack of transmit buffer space. */
    int         packetsQueued;          /* Number of packets written
					 * to adapter transmit buffers. */
} Net_FDDIStats;

/*
 * Statistics in general.
 */

typedef struct Net_Stats {
    Net_EtherStats	ether;
    Net_UltraStats	ultra;
    Net_UltraStats	hppi;
    Net_FDDIStats       fddi;
} Net_Stats;

/*
 * Structure that defines a network interface.
 */

typedef struct Net_Interface {
    char		*name;		/* Name of the interface. */
    int		 	unit;		/* Unit number of device. */
    Address		ctrlAddr;	/* Address of control register. */
    Boolean		virtual;	/* Is ctrlAddr in kernel VM? */
    int			vector;		/* Interrupt vector generated by 
					 * device. */

			/* Initialization routine. */
    ReturnStatus	(*init) _ARGS_((struct Net_Interface *interPtr));

			/* Output a packet. */
    ReturnStatus	(*output) _ARGS_((struct Net_Interface *interPtr,
				Address packetHeader, 
				Net_ScatterGather *scatterGatherPtr,
				int scatterGatherLength, Boolean rpc,
				ReturnStatus *statusPtr));

			/* Handle an interrupt. */
    void 		(*intr) _ARGS_((struct Net_Interface *interPtr, 
				Boolean polling));	

			/* Reset the interface */
    void 		(*reset) _ARGS_ ((struct Net_Interface *interPtr));	

			/* Perform ioctls on interface. */
    ReturnStatus	(*ioctl) _ARGS_((struct Net_Interface *interPtr,
				Fs_IOCParam *ioctlPtr, Fs_IOReply *replyPtr));	

			/* Get performance statistics. */
    ReturnStatus	(*getStats) _ARGS_((struct Net_Interface *interPtr,
				Net_Stats *statPtr));	

    int			number;		/* Interface number. */
    Net_NetworkType	netType;	/* Type of interface. See below. */
    int			flags;		/* Status flags. See below. */
    Sync_Semaphore	syncOutputMutex;/* Used to wait for packets
					 * to be output. */
    Sync_Semaphore	mutex;		/* Protects network interface board
					 * and related data structures. */
    int			maxBytes;	/* Maximum transfer unit 
					 * (packet size) */
    int			minBytes;	/* Minimum transfer unit. */

			/* Packet handler for network device driver. */
    void		(*packetProc) _ARGS_((struct Net_Interface *interPtr,
				int packetLength, Address packetPtr));

    ClientData		interfaceData;	/* Place for the interface routines
					 * store store stuff. */
    ClientData		devNetData;	/* Place for the network device
					 * driver to store stuff. */
    Net_Address		netAddress[NET_MAX_PROTOCOLS];
    Net_Address		broadcastAddress; /* Broadcast address for this
					   * interface. */
} Net_Interface;

/*
 * Flag values for Net_Interface.
 */

#define NET_IFLAGS_RUNNING	0x1	/* The interface is active. */
#define NET_IFLAGS_BROADCAST	0x2	/* Interface supports broadcast. */


#endif /* _NETTYPES */
