/* 
 * tape.h --
 *
 *	Definitions and macros for tape devices.
 *
 * Copyright 1991 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that this copyright
 * notice appears in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef _TAPE
#define _TAPE

#include <sys/types.h>

/*   
 * Tape-drive specific commands:
 *
 *   IOC_TAPE_COMMAND		Issue a tape drive specific command
 *   IOC_TAPE_STATUS		Return status info from a tape drive
 */
#define IOC_TAPE			(3 << 16)
#define IOC_TAPE_COMMAND		(IOC_TAPE | 0x1)
#define IOC_TAPE_STATUS			(IOC_TAPE | 0x2)

/*
 * Mag tape control, IOC_TAPE_COMMAND
 * The one IN parameter specifies a specific
 * tape command and a repetition count.
 */
typedef struct Dev_TapeCommand {
    int command;
    int count;
} Dev_TapeCommand;

#define IOC_TAPE_WEOF			0
#define IOC_TAPE_REWIND			1
#define IOC_TAPE_SKIP_BLOCKS		2
#define IOC_TAPE_SKIP_FILES		3
#define IOC_TAPE_BACKUP_BLOCKS		4
#define IOC_TAPE_BACKUP_FILES		5
#define IOC_TAPE_OFFLINE		6
#define IOC_TAPE_RETENSION		7
#define IOC_TAPE_ERASE			8
#define IOC_TAPE_NO_OP			9
#define IOC_TAPE_DONT_RETENSION		10
#define IOC_TAPE_SKIP_EOD		11
#define IOC_TAPE_GOTO_BLOCK		12
#define IOC_TAPE_LOAD			13
#define IOC_TAPE_UNLOAD			14
#define IOC_TAPE_PREVENT_REMOVAL	15
#define IOC_TAPE_ALLOW_REMOVAL		16


/*
 * Mag tape status, IOC_TAPE_STATUS
 * This returns status info from drives.
 * Any fields that are not valid will be set to -1.
 * Legal values for drive-specific fields can be found in the header files
 * in /sprite/lib/include/dev.
 *
 * NOTE: error counters may be reset by the device.  For example,
 * the Exabyte will reset the counters when a new tape is loaded,
 * the tape is rewound, or when you switch from reading to writing or
 * vice versa.
 */

typedef struct Dev_TapeStatus {
    int		type;		/* Type of tape drive, see below. */
    int		blockSize;	/* Size of physical block. */
    int		position;	/* Current block number. */
    int		remaining;	/* Number of blocks remaining on the tape. */
    int		dataError;	/* Number of data errors -- bad read after
				 * write or bad read. */
    int		readWriteRetry;	/* Number of reads/writes that had to be
				 * retried. */
    int		trackingRetry;	/* Number of tracking retries. */
    Boolean	writeProtect;	/* TRUE if tape is write-protected. */
    int		bufferedMode;	/* Buffered mode.  Value is drive specific. */
    int		speed;		/* Tape speed. Value is drive specific. */
    int		density;	/* Tape density. Value is drive specific. */
} Dev_TapeStatus;

/*
 * Stubs to interface to Fs_IOControl
 */
extern ReturnStatus Ioc_TapeStatus();
extern ReturnStatus Ioc_TapeCommand();

/*
 * Types for tape drive controllers.
 */

#define DEV_TAPE_UNKNOWN	0
#define DEV_TAPE_SYSGEN		1
#define DEV_TAPE_EMULEX		2

#define DEV_TAPE_8MM		0x100
#define DEV_TAPE_EXB8200	(DEV_TAPE_8MM | 1)
#define DEV_TAPE_EXB8500	(DEV_TAPE_8MM | 2)

#define DEV_TAPE_4MM		0x200
#define DEV_TAPE_TLZ04		(DEV_TAPE_4MM | 1)

/*
 * Values for the typeData field.
 */

/*
 * For the exb8500 the typeData is a bunch of flag bits.
 */
#define DEV_TAPE_STATUS_8200_MODE	0x1  /* Tape will be read/written
					      * in exb8200 mode. */
#endif /* _TAPE */
