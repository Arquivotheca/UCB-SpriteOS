/*
 * scsi.h --
 *
 *	Common declarations for SCSI command formaters. This file only covers
 *	definitions pertaining to the SCSI common command set that are
 *	common to all SCSI device types (ie disk, tapes, WORM, printers, etc).
 *	Device sepecific command format can be found in scsi{Disk,Tape,WORM}.h
 *	SCSI protocol releated declarations can be found in scsiHBA.h.
 *	Definitions for the SCSI device sub-system. Some of the following
 *      references from the proceedings of the 1984 Mini/Micro Northeast
 *	Conference might be useful in understanding SCSI. 
 *
 *
 * Copyright 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: /sprite/src/kernel/dev/RCS/scsi.h,v 9.3 91/04/12 17:54:22 jhh Exp $ SPRITE (Berkeley)
 */

#ifndef _SCSI_H
#define _SCSI_H

#include "machparam.h"
#include "scsiDevice.h"

/*
 * "Standard" SCSI Commands. SCSI command are divided into 8 group as
 * follows:
 *	Group0	(0x00 - 0x1f).  Basic commands. 6 bytes long
 *	Group1	(0x20 - 0x3f).  Extended command. 10 bytes.
 *	Group2	(0x40 - 0x5f).	Reserved.
 *	Group2	(0x60 - 0x7f).	Reserved.
 *	Group2	(0x80 - 0x9f).	Reserved.
 *	Group2	(0xa0 - 0xbf).	Reserved.
 *	Group6	(0xc0 - 0xdf).	Vendor Unique
 *	Group7	(0xe0 - 0xff).  Vendor Unique
 *	
 *
 */

/*
 * Scsi Group0 commands all are 6 bytes and have a format according to 
 * struct ScsiGroup0Cmd.
 */

#define SCSI_TEST_UNIT_READY	0x00
#define SCSI_REZERO_UNIT	0x01
#define SCSI_REQUEST_SENSE	0x03
#define	SCSI_FORMAT_UNIT	0x04
#define SCSI_REASSIGN_BLOCKS	0x07
#define SCSI_READ		0x08
#define SCSI_WRITE		0x0a
#define SCSI_SEEK		0x0b
#define SCSI_INQUIRY		0x12
#define SCSI_MODE_SELECT	0x15
#define	SCSI_RESERVE_UNIT	0x16
#define	SCSI_RELEASE_UNIT	0x17
#define SCSI_COPY		0x18
#define SCSI_MODE_SENSE		0x1A
#define SCSI_START_STOP		0x1b
#define	SCSI_RECV_DIAG_RESULTS	0x1c
#define SCSI_SEND_DIAGNOSTIC	0x1d
#define SCSI_PREVENT_ALLOW 	0x1e
/*
 * Group1 commands are all 10 bytes and have a format according to
 * struct ScsiGroup1Cmd.
 */
#define SCSI_READ_CAPACITY 	0x25	
#define	SCSI_READ_EXT		0x28
#define	SCSI_WRITE_EXT		0x2a
#define	SCSI_SEEK_EXT		0x2b
#define	SCSI_WRITE_VERIFY	0x2e
#define	SCSI_VERIFY_EXT		0x2f
#define	SCSI_SEARCH_HIGH	0x30
#define SCSI_SEARCH_EQUAL	0x31
#define	SCSI_SEARCH_LOW		0x32
#define	SCSI_SET_LIMITS		0x33
#define	SCSI_COMPARE		0x39
#define	SCSI_COPY_VERIFY	0x3a


/*
 * Group-0 commands for sequential access devices.
 */

#define SCSI_REWIND		0x01
#define SCSI_READ_BLOCK_LIMITS	0x05
#define	SCSI_TRACK_SELECT	0x0b
#define	SCSI_READ_REVERSE	0x0f
#define SCSI_WRITE_EOF		0x10
#define SCSI_SPACE		0x11
#define	SCSI_VERIFY		0x13
#define	SCSI_READ_BUFFER	0x14
#define SCSI_ERASE_TAPE		0x19
#define	SCSI_LOAD_UNLOAD	0x1b


/*
 * The standard group-0 6-byte SCSI control block.  Note that the 
 * fields between highAddr and blockCount inclusive are command dependent.
 * The definitions Addr and BlockCount cover most of the commands we will
 * use.
 */

typedef struct ScsiGroup0Cmd {
#if BYTE_ORDER == BIG_ENDIAN
    unsigned char command;		/* command code, defined below.  The
					 * upper three bits of this are zero
					 * to indicate the control block is
					 * only 6 bytes long */
    unsigned char unitNumber	:3;	/* Logical Unit (LUN) to which to
					 * pass the command.  The device
					 * has already been selected using
					 * the "targetID" bit. */
    unsigned char highAddr	:5;	/* High bits of address */
    unsigned char midAddr;		/* Middle bits of address */
    unsigned char lowAddr;		/* Low bits of address */
    unsigned char blockCount;		/* Blocks to transfer */
    unsigned char vendor57	:1;	/* Vendor unique bit */
    unsigned char vendor56	:1;	/* Vendor unique bit */
    unsigned char pad1		:4;	/* Reserved */
    unsigned char linkIntr	:1;	/* Interrupt after linked command */
    unsigned char link		:1;	/* Another command follows */
#else
    unsigned char command;		/* command code, defined below.  The
					 * upper three bits of this are zero
					 * to indicate the control block is
					 * only 6 bytes long */
    unsigned char highAddr	:5;	/* High bits of address */
    unsigned char unitNumber	:3;	/* Logical Unit (LUN) to which to
					 * pass the command.  The device
					 * has already been selected using
					 * the "targetID" bit. */
    unsigned char midAddr;		/* Middle bits of address */
    unsigned char lowAddr;		/* Low bits of address */
    unsigned char blockCount;		/* Blocks to transfer */
    unsigned char link		:1;		/* Another command follows */
    unsigned char linkIntr	:1;	/* Interrupt after linked command */
    unsigned char pad1		:4;	/* Reserved */
    unsigned char vendor56	:1;	/* Vendor unique bit */
    unsigned char vendor57	:1;	/* Vendor unique bit */
#endif
} ScsiGroup0Cmd;

/*
 * SCSI status completion information.  This is returned by the device
 * when a command completes. 
 */

typedef struct ScsiStatus {
#if BYTE_ORDER == BIG_ENDIAN
    unsigned char reserved	:1;	/* Reserved. */
    unsigned char vendor06	:1;	/* Vendor unique bit */
    unsigned char vendor05	:1;	/* Vendor unique bit */
    unsigned char intStatus	:1;	/* Intermediate status */
    unsigned char busy		:1;	/* Device busy or reserved */
    unsigned char conditionMet	:1;	/* Condition met */
    unsigned char check		:1;	/* Check the sense data for more info */
    unsigned char vendor00	:1;	/* Vendor unique bit */
#else
    unsigned char vendor00	:1;	/* Vendor unique bit */
    unsigned char check		:1;	/* Check the sense data for more info */
    unsigned char conditionMet	:1;	/* Condition met */
    unsigned char busy		:1;	/* Device busy or reserved */
    unsigned char intStatus	:1;	/* Intermediate status */
    unsigned char vendor05	:1;	/* Vendor unique bit */
    unsigned char vendor06	:1;	/* Vendor unique bit */
    unsigned char reserved	:1;	/* Reserved. */
#endif
} ScsiStatus;

/*
 * SCSI_RESERVED_STATUS() - Return TRUE if the status byte has a reserved code.
 */

#define	SCSI_RESERVED_STATUS(byte) (byte&0x80)


/*
 * Sense information provided after some errors.  This is divided into
 * two kinds, classes 0-6, and class 7.  This is 30 bytes big to allow
 * for the drive specific sense bytes that follow the standard 4 byte header.
 *
 * For extended sense, this buffer may be cast into another type.  Also
 * The actual size of the sense data returned is used to detect what
 * kind of tape drive is out there.  Kludgy, but true.
 */
typedef struct ScsiClass0Sense {
#if BYTE_ORDER == BIG_ENDIAN
    unsigned char valid		:1;	/* Sense data is valid */
    unsigned char error		:7;	/* 3 bits class and 4 bits code */
    unsigned char highAddr;		/* High byte of block address */
    unsigned char midAddr;		/* Middle byte of block address */
    unsigned char lowAddr;		/* Low byte of block address */
    unsigned char sense[26];		/* Target specific sense data */
#else
    unsigned char error		:7;	/* 3 bits class and 4 bits code */
    unsigned char valid		:1;	/* Sense data is valid */
    unsigned char highAddr;		/* High byte of block address */
    unsigned char midAddr;		/* Middle byte of block address */
    unsigned char lowAddr;		/* Low byte of block address */
    unsigned char sense[26];		/* Target specific sense data */
#endif
} ScsiClass0Sense;

/*
 * Definitions for errors in the sense data.  The error field is specified
 * as a 3 bit class and 4 bit code, but it is easier to treat it as a
 * single 7 bit field.
 */
#define SCSI_NO_SENSE_DATA		0x00
#define SCSI_NOT_READY			0x04
#define SCSI_NOT_LOADED			0x09
#define SCSI_INSUF_CAPACITY		0x0a
#define SCSI_HARD_DATA_ERROR		0x11
#define SCSI_WRITE_PROTECT		0x17
#define SCSI_CORRECTABLE_ERROR		0x18
#define SCSI_FILE_MARK			0x1c
#define SCSI_INVALID_COMMAND		0x20
#define SCSI_UNIT_ATTENTION		0x30
#define SCSI_END_OF_MEDIA		0x34

/*
 * The standard "extended" sense data returned by SCSI devices.  This
 * has an error field of 0x70, for a "class 7" error.
 */
typedef struct ScsiClass7Sense {
#if BYTE_ORDER == BIG_ENDIAN
    unsigned char valid		:1;	/* Sense data is valid */
    unsigned char error7	:7;	/* == 0x70 */
    unsigned char pad1;			/* Also "segment number" for copy */
    unsigned char fileMark	:1;	/* File mark on device */
    unsigned char endOfMedia	:1;	/* End of media reached */
    unsigned char badBlockLen	:1;	/* Block length mis-match (Exabyte) */
    unsigned char pad2		:1;
    unsigned char key		:4;	/* Sense keys defined below */
    unsigned char info1;		/* Information byte 1 */
    unsigned char info2;		/* Information byte 2 */
    unsigned char info3;		/* Information byte 3 */
    unsigned char info4;		/* Information byte 4 */
    unsigned char length;		/* Number of additional info bytes */
#else
    unsigned char error7	:7;	/* == 0x70 */
    unsigned char valid		:1;	/* Sense data is valid */
    unsigned char pad1;			/* Also "segment number" for copy */
    unsigned char key		:4;	/* Sense keys defined below */
    unsigned char pad2		:1;
    unsigned char badBlockLen	:1;	/* Block length mis-match (Exabyte) */
    unsigned char endOfMedia	:1;	/* End of media reached */
    unsigned char fileMark	:1;	/* File mark on device */
    unsigned char info1;		/* Information byte 1 */
    unsigned char info2;		/* Information byte 2 */
    unsigned char info3;		/* Information byte 3 */
    unsigned char info4;		/* Information byte 4 */
    unsigned char length;		/* Number of additional info bytes */
#endif
} ScsiClass7Sense;			/* 8 Bytes */

/*
 * Key values for standardized sense class 7. 
 */
#define SCSI_CLASS7_NO_SENSE		0
#define SCSI_CLASS7_RECOVERABLE	1
#define SCSI_CLASS7_NOT_READY		2
#define SCSI_CLASS7_MEDIA_ERROR	3
#define SCSI_CLASS7_HARDWARE_ERROR	4
#define SCSI_CLASS7_ILLEGAL_REQUEST	5

/*
 * These seem to have different meanings to different vendors....
 */
#define SCSI_CLASS7_MEDIA_CHANGE	6
#define SCSI_CLASS7_UNIT_ATTN		6

#define SCSI_CLASS7_WRITE_PROTECT	7
#define SCSI_CLASS7_BLANK_CHECK		8
#define SCSI_CLASS7_VENDOR		9
#define SCSI_CLASS7_POWER_UP_FAILURE	10
#define SCSI_CLASS7_ABORT		11
#define SCSI_CLASS7_EQUAL		12
#define SCSI_CLASS7_OVERFLOW		13
#define SCSI_CLASS7_RESERVED_14		14
#define SCSI_CLASS7_RESERVED_15		15

/*
 * Maximum size of sense data that a device can return. XXX - fix this.
 */
#define	SCSI_MAX_SENSE_LEN	64

/*
 * Data return by the SCSI inquiry command. 
 */

typedef struct ScsiInquiryData {
#if BYTE_ORDER == BIG_ENDIAN
    unsigned char 	type;		/* Peripheral Device type. See below. */
    unsigned char 	rmb:1;		/* Removable Medium bit. */
    unsigned char	qualifier:7; 	/* Device type qualifier. */
    unsigned char	version;	/* Version info. */
    unsigned char	reserved:4;	/* reserved. */
    unsigned char	format:4;	/* Response format. */
    unsigned char	length;		/* length of data returned. */
#ifdef notdef
    unsigned char	vendor;		/* Vendor unqiue parameter. */
    unsigned char	reserved2[2];	/* More reserved. */
    char		vendorInfo[8];	/* Vector identification. */
    char		productInfo[8]; /* Product identification. */
    char		firmwareInfo[4]; /* Firmware identification. */
#endif

    unsigned char	reserved2[3];	/* Reserved			      */
    unsigned char	vendorID[8];	/* Vendor ID (ASCII)		      */
    unsigned char	productID[16];	/* Product ID (ASCII)		      */
    unsigned char	revLevel[4];	/* Revision level (ASCII)	      */
    unsigned char	revData[8];	/* Revision data (ASCII)	      */
#else
    unsigned char 	type;		/* Peripheral Device type. See below. */
    unsigned char	qualifier:7; 	/* Device type qualifier. */
    unsigned char 	rmb:1;		/* Removable Medium bit. */
    unsigned char	version;	/* Version info. */
    unsigned char	format:4;	/* Response format. */
    unsigned char	reserved:4;	/* reserved. */
    unsigned char	length;		/* length of data returned. */
    unsigned char	reserved2[3];	/* Reserved			      */
    unsigned char	vendorID[8];	/* Vendor ID (ASCII)		      */
    unsigned char	productID[16];	/* Product ID (ASCII)		      */
    unsigned char	revLevel[4];	/* Revision level (ASCII)	      */
    unsigned char	revData[8];	/* Revision data (ASCII)	      */
#endif
}  ScsiInquiryData;


/*
 * The SCSI Peripheral type ID codes as return by the SCSI_INQUIRY command.
 *
 * SCSI_DISK_TYPE - Direct Access Device.
 * SCSI_TAPE_TYPE - Sequential Access Device.
 * SCSI_PRINTER_TYPE - Printer Device.
 * SCSI_HOST_TYPE - Processor Device.
 * SCSI_WORM_TYPE - Write-Once Read-Multiple Device.
 * SCSI_ROM_TYPE  - Read-Only Direct Access Device.
 * SCSI_SCANNER_TYPE - Scanner device.
 * SCSI_OPTICAL_MEM_TYPE - Optical memory device.
 * SCSI_MEDIUM_CHANGER_TYPE - Medium changer device.
 * SCSI_COMMUNICATIONS_TYPE - Communications device.
 * SCSI_NODEVICE_TYPE - Logical Unit not present or implemented.
 *
 * Note that codes 0xa-0x7e are reserved and 0x80-0xff are vendor unique.
 */
#define	SCSI_DISK_TYPE		0
#define	SCSI_TAPE_TYPE		1
#define	SCSI_PRINTER_TYPE	2
#define	SCSI_HOST_TYPE		3
#define	SCSI_WORM_TYPE		4
#define	SCSI_ROM_TYPE		5
#define	SCSI_SCANNER_TYPE	6
#define	SCSI_OPTICAL_MEM_TYPE	7
#define	SCSI_MEDIUM_CHANGER_TYPE	8
#define	SCSI_COMMUNICATIONS_TYPE	9
#define	SCSI_NODEVICE_TYPE	0x7f

/*
 * Standard header for SCSI_MODE_SELECT commands for tapes.
 */

typedef struct ScsiTapeModeSelectHdr {
#if BYTE_ORDER == BIG_ENDIAN
    unsigned char	reserved[2];	/* Reserved. */
    unsigned char	reserved2:1;	/*  ""	     */
    unsigned char	bufferedMode:3;	/* Type of buffer to be done. */
    unsigned char	speed:4;	/* Drive speed. */
    unsigned char	length;		/* Block descriptor length. */
#else
    unsigned char	reserved[2];	/* Reserved. */
    unsigned char	speed:4;	/* Drive speed. */
    unsigned char	bufferedMode:3;	/* Type of buffer to be done. */
    unsigned char	reserved2:1;	/*  ""	     */
    unsigned char	length;		/* Block descriptor length. */
#endif
} ScsiTapeModeSelectHdr;
/*
 * Format of a SCSI_START_STOP command. This is a group 0 command, but
 * the command contents are different.
 */
typedef struct ScsiStartStopCmd {
#if BYTE_ORDER == BIG_ENDIAN
    unsigned char command;		/* 0x1b */
    unsigned char unitNumber	:3;	/* Logical Unit (LUN) to which to
					 * pass the command.  The device
					 * has already been selected using
					 * the "targetID" bit. */
    unsigned char pad1		:4;	/* Reserved */
    unsigned char immed		:1;	/* Immediate status bit */
    unsigned char pad2;			/* Reserved */
    unsigned char pad3;			/* Reserved */
    unsigned char pad4		:6;	/* Reserved */
    unsigned char loadEject	:1;	/* Load or eject medium */
    unsigned char start		:1;	/* Start or stop medium */
    unsigned char vendor57	:1;	/* Vendor unique bit */
    unsigned char vendor56	:1;	/* Vendor unique bit */
    unsigned char pad5		:4;	/* Reserved */
    unsigned char linkIntr	:1;	/* Interrupt after linked command */
    unsigned char link		:1;	/* Another command follows */
#else
    unsigned char command;		/* command code, defined below.  The
					 * upper three bits of this are zero
					 * to indicate the control block is
					 * only 6 bytes long */
    unsigned char immed		:1;	/* Immediate status bit */
    unsigned char pad1		:4;	/* Reserved */
    unsigned char unitNumber	:3;	/* Logical Unit (LUN) to which to
					 * pass the command.  The device
					 * has already been selected using
					 * the "targetID" bit. */
    unsigned char pad2;			/* Reserved */
    unsigned char pad3;			/* Reserved */
    unsigned char start		:1;	/* Start or stop medium */
    unsigned char loadEject	:1;	/* Load or eject medium */
    unsigned char pad4		:6;	/* Reserved */
    unsigned char link		:1;	/* Another command follows */
    unsigned char linkIntr	:1;	/* Interrupt after linked command */
    unsigned char pad5		:4;	/* Reserved */
    unsigned char vendor56	:1;	/* Vendor unique bit */
    unsigned char vendor57	:1;	/* Vendor unique bit */
#endif
} ScsiStartStopCmd;


/*
 * Format of a SCSI_READ_EXT command.
 */

typedef struct ScsiReadExtCmd {
#if BYTE_ORDER == BIG_ENDIAN
    unsigned char command;		/* command code. */
    unsigned char unitNumber	:3;	/* Logical Unit (LUN) to which to
					 * pass the command.  The device
					 * has already been selected using
					 * the "targetID" bit. */
    unsigned char dpo		:1;	/* Disable page out. */
    unsigned char fua		:1;	/* Force unit access. */
    unsigned char pad1		:2;	/* Reserved. */
    unsigned char relAddr	:1;	/* Who knows? */
    unsigned char highAddr;		/* High bits of address. */
    unsigned char highMidAddr;		/* High middle bits of address. */
    unsigned char lowMidAddr;		/* Low middle bits of address. */
    unsigned char lowAddr;		/* Low bits of address */
    unsigned char pad2;			/* Reserved. */
    unsigned char highCount;		/* High bits of number to transfer */
    unsigned char lowCount;		/* Low bits of number to transfer */
    unsigned char vendor	:2;	/* Vendor specific. */
    unsigned char pad		:4;	/* Reserved. */
    unsigned char flag		:1;	/* Flag bit. See SCSI doc. */
    unsigned char link		:1;	/* Link commands. */
#else
    unsigned char command;		/* command code. */
    unsigned char relAddr	:1;	/* Who knows? */
    unsigned char pad1		:2;	/* Reserved. */
    unsigned char fua		:1;	/* Force unit access. */
    unsigned char dpo		:1;	/* Disable page out. */
    unsigned char unitNumber	:3;	/* Logical Unit (LUN) to which to
					 * pass the command.  The device
					 * has already been selected using
					 * the "targetID" bit. */
    unsigned char highAddr;		/* High bits of address. */
    unsigned char highMidAddr;		/* High middle bits of address. */
    unsigned char lowMidAddr;		/* Low middle bits of address. */
    unsigned char lowAddr;		/* Low bits of address */
    unsigned char pad2;			/* Reserved. */
    unsigned char highCount;		/* High bits of number to transfer */
    unsigned char lowCount;		/* Low bits of number to transfer */
    unsigned char link		:1;	/* Link commands. */
    unsigned char flag		:1;	/* Flag bit. See SCSI doc. */
    unsigned char pad		:4;	/* Reserved. */
    unsigned char vendor	:2;	/* Vendor specific. */
#endif
} ScsiReadExtCmd;

/*
 * The SCSI_WRITE_EXT command had the same format as SCSI_READ_EXT.
 */

typedef struct ScsiReadExtCmd ScsiWriteExtCmd;

#define	MAX_SCSI_ERROR_STRING	128

extern int devScsiNumErrors[];
extern char **devScsiErrors[];
/*
 * Routines.
 */
extern Boolean DevScsiMapClass7Sense _ARGS_((int senseLength, char *senseDataPtr, ReturnStatus *statusPtr, char *errorString));
extern ReturnStatus DevScsiGroup0Cmd _ARGS_((ScsiDevice *devPtr, int cmd, unsigned int blockNumber, unsigned int countNumber, register ScsiCmd *scsiCmdPtr));

#endif /* _SCSI_H */
