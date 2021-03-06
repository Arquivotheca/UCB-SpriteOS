/*
 * fsioFile.h --
 *
 *	Declarations for regular file access, local and remote.
 *
 * Copyright 1987 Regents of the University of California
 * All rights reserved.
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 *
 * $Header: /sprite/src/kernel/fsio/RCS/fsioFile.h,v 9.5 91/09/10 18:38:13 rab Exp $ SPRITE (Berkeley)
 */

#ifndef _FSFILE
#define _FSFILE

#ifdef KERNEL
#include <fsio.h>
#include <fsutil.h>
#include <fsconsist.h>
#include <fscache.h>
#include <fsioLock.h>
#include <fsNameOps.h>
#else
#include <kernel/fsio.h>
#include <kernel/fsutil.h>
#include <kernel/fsconsist.h>
#include <kernel/fscache.h>
#include <kernel/fsioLock.h>
#include <kernel/fsNameOps.h>
#endif
/*
 * When a regular file is opened state is packaged up on the server
 * and used on the client to set up the I/O handle for the file.
 * This is the 'streamData' generated by Fsio_FileNameOpen and passed to
 * Fsio_FileIoOpen
 */
typedef struct Fsio_FileState {
    Boolean	cacheable;	/* TRUE if the client can cache data blocks */
    int		version;	/* Version number for data block cache */
    int		openTimeStamp;	/* Time stamp used to catch races between
				 * open replies and cache consistency msgs */
    Fscache_Attributes attr;	/* A copy of some file attributes */
    int		newUseFlags;	/* The server may modify the stream use flags.
				 * In particular, the execute bit is stripped
				 * off when directories are opened. */
} Fsio_FileState;

/*
 * When a client re-opens a file it sends the following state to the server.
 */
typedef struct Fsio_FileReopenParams {
    Fs_FileID	fileID;		/* File ID of file to reopen. MUST BE FIRST */
    Fs_FileID	prefixFileID;	/* File ID for the prefix of this file. */
    Fsio_UseCounts	use;		/* Reference counts */
    Boolean	flags;		/* FSIO_HAVE_BLOCKS | FS_SWAP */
    int		version;	/* Expected version number for the file. */
} Fsio_FileReopenParams;

/*
 * File reopen flags
 *	FSIO_HAVE_BLOCKS	Set when the client has dirty blocks in its cache.
 *		This implies that it ought to be able to continue caching.
 *		A race exists in that another client could open for writing
 *		first, and thus invalidate the first client's data, or another
 *		client could open for reading and possibly see stale data.
 *	FS_SWAP	This stream flag is passed along so the server doesn't
 *		erroneously grant cacheability to swap files.
 *			
 */
#define FSIO_HAVE_BLOCKS		0x1
/*resrv FS_SWAP			0x4000 */

/*
 * The I/O descriptor for a local file.  Used with FSIO_LCL_FILE_STREAM.
 */

typedef struct Fsio_FileIOHandle {
    Fs_HandleHeader	hdr;		/* Standard handle header.  The
					 * 'major' field of the fileID
					 * is the domain number.  The
					 * 'minor' field is the file num. */
    Fsio_UseCounts		use;	/* Open, writer, and exec counts.
					 * Used for consistency checks. This
					 * is a summary of all uses of a file */
    int			flags;		/* FSIO_FILE_NAME_DELETED and
					 * FSIO_FILE_DESC_DELETED */
    struct Fsdm_FileDescriptor *descPtr;/* Reference to disk info, this
					 * has attritutes, plus disk map. */
    Fscache_FileInfo	cacheInfo;	/* Used to access block cache. */
    Fsconsist_Info	consist;	/* Client use info needed to enforce
					 * network cache consistency */
    Fsio_LockState		lock;		/* User level locking state. */
    Fscache_ReadAheadInfo	readAhead;/* Read ahead info used to synchronize
					 * with other I/O and closes/deletes. */
    struct Vm_Segment	*segPtr;	/* Reference to code segment needed
					 * to flush VM cache. */
} Fsio_FileIOHandle;			/* 268 BYTES (316 with traced locks) */

/*
 * Flags for local I/O handles.
 *	FSIO_FILE_NAME_DELETED		Set when all names of a file have been
 *				removed.  This marks the handle for removal.
 *	FSIO_FILE_DESC_DELETED		Set when the disk descriptor is in
 *				the process of being removed.  This guards
 *				against a close/remove race where two parties
 *				try to do the disk deletion phase.
 */
#define FSIO_FILE_NAME_DELETED		0x1
#define FSIO_FILE_DESC_DELETED		0x2


/*
 * OPEN SWITCH
 * The nameOpen procedure is used on the file server when opening streams or
 * setting up an I/O fileID for a file or device.  It is keyed on
 * disk file descriptor types( i.e. FS_FILE, FS_DIRECTORY, FS_DEVICE,
 * FS_PSEUDO_DEVICE).  The nameOpen procedure returns an ioFileID
 * used for I/O on the file, plus other data needed for the client's
 * stream.  The streamIDPtr is NIL during set/get attributes, which
 * indicates that the extra stream information isn't needed.
 */

typedef struct Fsio_OpenOps {
    int		type;			/* One of the file descriptor types */
    /*
     * The calling sequence for the nameOpen routine is:
     *	FooNameOpen(handlePtr, openArgsPtr, openResultsPtr);
     */
    ReturnStatus (*nameOpen) _ARGS_((Fsio_FileIOHandle *handlePtr, 
				     Fs_OpenArgs *openArgsPtr,
				     Fs_OpenResults *openResultsPtr));
} Fsio_OpenOps;

extern Fsio_OpenOps fsio_OpenOpTable[];

/*
 * Open operations.
 */
extern ReturnStatus Fsio_FileNameOpen _ARGS_((Fsio_FileIOHandle *handlePtr, 
			Fs_OpenArgs *openArgsPtr, 
			Fs_OpenResults *openResultsPtr));

/*
 * Stream operations.
 */
extern ReturnStatus Fsio_FileIoOpen _ARGS_((Fs_FileID *ioFileIDPtr,
		int *flagsPtr, int clientID, ClientData streamData, char *name,
		Fs_HandleHeader **ioHandlePtrPtr));
extern ReturnStatus Fsio_FileRead _ARGS_((Fs_Stream *streamPtr,
		Fs_IOParam *readPtr, Sync_RemoteWaiter *remoteWaitPtr, 
		Fs_IOReply *replyPtr));
extern ReturnStatus Fsio_FileWrite _ARGS_((Fs_Stream *streamPtr, 
		Fs_IOParam *writePtr, Sync_RemoteWaiter *remoteWaitPtr, 
		Fs_IOReply *replyPtr));
extern ReturnStatus Fsio_FileIOControl _ARGS_((Fs_Stream *streamPtr, 
		Fs_IOCParam *ioctlPtr, Fs_IOReply *replyPtr));
extern ReturnStatus Fsio_FileSelect _ARGS_((Fs_HandleHeader *hdrPtr, 
		Sync_RemoteWaiter *waitPtr, int *readPtr, int *writePtr, 
		int *exceptPtr));
extern ReturnStatus Fsio_FileMigClose _ARGS_((Fs_HandleHeader *hdrPtr, 
		int flags));
extern ReturnStatus Fsio_FileMigOpen _ARGS_((Fsio_MigInfo *migInfoPtr, int size,
		ClientData data, Fs_HandleHeader **hdrPtrPtr));
extern ReturnStatus Fsio_FileMigrate _ARGS_((Fsio_MigInfo *migInfoPtr, 
		int dstClientID, int *flagsPtr, int *offsetPtr, int *sizePtr, 
		Address *dataPtr));
extern ReturnStatus Fsio_FileReopen _ARGS_((Fs_HandleHeader *hdrPtr, 
		int clientID, ClientData inData, int *outSizePtr, 
		ClientData *outDataPtr));
extern ReturnStatus Fsio_FileBlockCopy _ARGS_((Fs_HandleHeader *srcHdrPtr, 
		Fs_HandleHeader *dstHdrPtr, int blockNum));
extern Boolean Fsio_FileScavenge _ARGS_((Fs_HandleHeader *hdrPtr));
extern ReturnStatus Fsio_FileClose _ARGS_((Fs_Stream *streamPtr, int clientID,
		Proc_PID procID, int flags, int dataSize,
		ClientData closeData));
extern ReturnStatus Fsio_FileCloseInt _ARGS_((Fsio_FileIOHandle *handlePtr,
		int ref, int write, int exec, int clientID, Boolean callback));
extern void Fsio_FileClientKill _ARGS_((Fs_HandleHeader *hdrPtr, int clientID));

extern void Fsio_FileSyncLockCleanup _ARGS_((Fsio_FileIOHandle *handlePtr));

extern void Fsio_InstallSrvOpenOp _ARGS_((int fileType, 
			Fsio_OpenOps *openOpsPtr));
extern ReturnStatus Fsio_LocalFileHandleInit _ARGS_((Fs_FileID *fileIDPtr,
		char *name, struct Fsdm_FileDescriptor *descPtr,
		Boolean cantBlock, Fsio_FileIOHandle **newHandlePtrPtr));

extern ReturnStatus Fsio_DeviceNameOpen _ARGS_((Fsio_FileIOHandle *handlePtr, 
				Fs_OpenArgs *openArgsPtr, 
				Fs_OpenResults *openResultsPtr));
/*
 * ftrunc() support
 */
extern ReturnStatus Fsio_FileTrunc _ARGS_((Fsio_FileIOHandle *handlePtr, 
			int size, int flags));

#endif /* _FSFILE */
