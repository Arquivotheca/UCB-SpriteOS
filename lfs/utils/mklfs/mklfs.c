/* 
 * mklfs.c --
 *
 *	The mklfs program - Make a LFS file system on a disk.
 *
 * Copyright 1989 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/admin/mklfs/RCS/mklfs.c,v 1.1 91/05/31 11:09:07 mendel Exp $ SPRITE (Berkeley)";
#endif /* not lint */

#ifdef __STDC__
/*
 * If we are compiling on a machine that has a ASCII C compiler, set the
 * define _HAS_PROTOTYPES which causes the Sprite header files to 
 * expand function definitions to included prototypes.
 */
#define	_HAS_PROTOTYPES 
#endif /* __STDC__ */

#include <sprite.h>
#include <varargs.h>
#include <stdio.h>
#include <cfuncproto.h>

#include <option.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <bstring.h>

#include <fs.h>
#include <kernel/fs.h>
#include <kernel/dev.h>
#include <kernel/fsdm.h>
#include <kernel/fslcl.h>
#include <kernel/devDiskLabel.h>

#include <kernel/lfsDesc.h>
#include <kernel/lfsDescMap.h>
#include <kernel/lfsFileLayout.h>
#include <kernel/lfsSegLayout.h>
#include <kernel/lfsStableMem.h>
#include <kernel/lfsSuperBlock.h>
#include <kernel/lfsUsageArray.h>
#include <kernel/lfsStats.h>

#include "getMachineInfo.h"

/*
 * Mklfs works by building parts of the file system in memory and
 * then writing the parts to disk.  The following data structures
 * describe the in memory format on a segment and checkpoint region.
 */

/*
 * SegMem - Description of a segment being initialized for a new file system.
 */
typedef struct SegMem {
    char	*startPtr; 	  /* Start of segment memory. */
    unsigned int diskAddress;	  /* Disk address of start of segment. */
    char	*dataPtr;	  /* Next data block to be allocated. */
    int		activeBytes;	  /* Number of active bytes in segment. */
    char	*summaryPtr;	  /* Next summary bytes to be allocated. */
} SegMem;

/*
 * CheckPointMem - Desciption of checkpoint region being initialized for
 *		   a new file system. 
 */
typedef struct CheckPointMem {
    char	*startPtr;	/* Start of checkpoint memory. */
    char	*memPtr; 	/* Next checkpoint byte to be allocated. */
} CheckPointMem;

/*
 * In memory version of a Lfs Stable Memory data structure.
 */

typedef struct StableMem {
    LfsStableMemParams *paramsPtr;
    char		**blockPtrs;
} StableMem;

/*
 * Attributes of the root directory of the new file system.
 */
typedef struct RootDir {
    int truncVersion;	/* Truncate version number of root directory. */
    int	accessTime;	/* Time of last access of root directory. */
    int	numBlocks;	/* Number of blocks in root direcotry. */
    int descBlockAddress; /* Disk address of root directory block. */
} RootDir;

static RootDir root;

/*
 * DEFAULT_SEG_SIZE - Default size of an LFS segment in bytes.
 */
#define	DEFAULT_SEG_SIZE (DEV_BYTES_PER_SECTOR*1024)

int	blockSize = DEV_BYTES_PER_SECTOR; /* Minumin unit of allocation for
					   * the file system.  Must be at
					   * least a sector size. 
					   */
int	segmentSize = DEFAULT_SEG_SIZE;	  /* Size of LFS segment in bytes. */
int	numberSegments = -1;		  /* Number of sections occupied by
					   * by the file system. -1 means 
					   * compute based on disk size. 
					   */
int	descBlockSize = FS_BLOCK_SIZE;	  /* Size of the descriptor blocks. */
int	maxDesc	  = -1;			  /* Maximum number of descriptors to
					   * support.  -1 means compute based
					   * on disk size. 
					   */
int	usageArrayBlockSize = FS_BLOCK_SIZE;   /* Block size of usage array
						* Data structure. */
int	descMapBlockSize = FS_BLOCK_SIZE;	/* Block size of desc map
						 * data structure. */
int	segAlignment = DEV_BYTES_PER_SECTOR; /* Address multiple to 
						  * start segments. */
int	maxNumCacheBlocks = -1;		/* Maximum number of file cache
					 * blocks on the machine 
					 * being used. */
int	maxCacheBlocksCleaned = -1;	/* Maximum number of file cache
					 * blocks used during cleaning.
					 */
int	serverID = -1;	/* Server ID to put in file system. */
char	*deviceName;	/* Device to write file system to. */
double  maxUtilization = .85; /* Maximum utilization allowed for file system. */
double  segFullLevel = .95;   /* Minumum utilization of a full segment. */
int	checkpointInterval = 60; /* Checkpoint every 60 seconds. */

Option optionArray[] = {
    {OPT_DOC, (char *) NULL,  (char *) NULL,
"This program generates an empty LFS file system. WARNING: This command\n will overwrite any existing data or file system on the specified device.\n Synopsis: \"mklfs [switches] deviceName\"\n Command-line switches are:"},
    {OPT_INT, "maxNumCacheBlocks", (Address) &maxNumCacheBlocks,
		"Maximum number of cache blocks to avail on machine."},
    {OPT_INT, "maxCacheBlocksCleaned", (Address) &maxCacheBlocksCleaned,
		"Maximum number of cache blocks to clean at a time."},
    {OPT_INT, "spriteID", (Address) &serverID,
		"Sprite ID for superblock."},
    {OPT_FLOAT, "maxUtilization", (Address) &maxUtilization,
		"Maximum disk utilization allowed."},
    {OPT_FLOAT, "segFullLevel", (Address) &segFullLevel,
		"Minimum segment utilization for a full segment."},
    {OPT_INT, "maxDesc", (Address) &maxDesc, 
	"Maximum number of descriptors."},
    {OPT_INT, "checkpointInterval", (Address) &checkpointInterval,
	"Frequent of checkpoint in seconds."},
    {OPT_INT, "blockSize", (Address) &blockSize, 
	"Block size of file system in bytes."},
    {OPT_INT, "segmentSize", (Address) &segmentSize, 
	"Segment size of file system in bytes."},
    {OPT_INT, "numSegments", (Address) &numberSegments, 
	"Number of segment in file system."},
    {OPT_INT, "segAlignment", (Address) &segAlignment,
		"Insure that the first segment starts at this multiple."},
    {OPT_INT, "descBlockSize", (Address) &descBlockSize, 
	"Descriptor block size."},
    {OPT_INT, "descMapBlockSize", (Address) &descMapBlockSize,
		"Descriptor map block size"},
    {OPT_INT, "usageArrayBlockSize", (Address) &usageArrayBlockSize,
		"Segment usage array block size"},
};
/*
 * Forward routine declartions. 
 */
extern void BuildInitialFileLayout _ARGS_((LfsFileLayoutParams *fileLayoutPtr,
			SegMem *segPtr, CheckPointMem	*checkpointPtr));
extern void BuildInitialDescMap _ARGS_((LfsDescMapParams *descParamsPtr, 
			SegMem *segPtr, CheckPointMem	*checkpointPtr));
extern void BuildInitialUsageArray _ARGS_((LfsSegUsageParams *usagePtr,
			SegMem *segPtr, CheckPointMem	*checkpointPtr));
static StableMem *BuildStableMem _ARGS_((int memType, int memBlockSize,
			int entrySize, int maxNumEntries, 
			LfsStableMemParams *smemParamsPtr));
static void UpdateStableMem _ARGS_((StableMem *stableMemPtr, 
			int entryNumber, char *entryPtr));
static void LayoutStableMem _ARGS_((StableMem *stableMemPtr, 
			SegMem *segPtr, CheckPointMem	*checkpointPtr));

static unsigned int GetDiskAddressOf _ARGS_((SegMem *segPtr));
static void MakeRootDir _ARGS_((LfsFileDescriptor *rootDescPtr, 
			LfsFileDescriptor *lostDescPtr,
			SegMem *segPtr, RootDir *rootPtr));
static void FileDescInit _ARGS_((LfsFileDescriptor *fileDescPtr, 
			int fileNumber, int fileType));
static void SetDescriptorMap _ARGS_((StableMem *stableMemPtr, 
			int fileNumber, unsigned int diskAddress, 
			int truncVersion, int accessTime));
extern void WriteDisk _ARGS_((int diskFd, int blockOffset, char *bufferPtr, 
			int bufferSize));

static Boolean IsPowerOfTwo _ARGS_((int ival));
static Boolean IsMultipleOf _ARGS_((int imultiple, int ival));
static int BlockCount _ARGS_((int objectSize, int blockSize));
static void EraseOldFileSystem _ARGS_((int diskFd));
static int ComputeDiskSize _ARGS_((int diskFd));

extern void panic();
extern int  open _ARGS_((char *path, int flags, int mode));

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	Main routine of mklfs - parse arguments and do the work.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
main(argc,argv)
    int	argc;
    char *argv[];
{
    int	   diskFd, maxCheckPointSize, maxCheckPointBlocks, moduleType;
    int		blocksPerSeg, segNum, startOffset;
    LfsSuperBlock	*superBlockPtr;
    char		*segMemPtr, *checkPointPtr, *summaryPtr;
    SegMem	seg;
    CheckPointMem checkpoint;
    LfsSegSummaryHdr	*sumHdrPtr;
    LfsCheckPointRegion *regionPtr;
    LfsSegSummary 	*segSumPtr;
    LfsCheckPointHdr    *cpHdrPtr, *oldCheckPointPtr;
    int		sizeInSectors;
    extern ReturnStatus Proc_GetHostIDs 
			_ARGS_((int *virtualHostPtr, int *physicalHostPtr));


    GetMachineInfo(&serverID, &maxNumCacheBlocks);
    /*
     * Set the default number of file cache blocks to use while cleaning
     * segments. We choose 8 megabytes or one third the number of cache
     * blocks, which ever is less.
     */

    if (maxCacheBlocksCleaned < 0) {
	maxCacheBlocksCleaned = (8*1024*1024)/FS_BLOCK_SIZE;
	if (maxCacheBlocksCleaned < maxNumCacheBlocks/3) {
	    maxCacheBlocksCleaned = maxNumCacheBlocks/3;
	}
    }

    argc = Opt_Parse(argc, argv, optionArray, Opt_Number(optionArray), 0);
    if (argc != 2) { 
         Opt_PrintUsage(argv[0], optionArray, Opt_Number(optionArray));
	 exit(1);
    } else {
	deviceName = argv[1];
    }
    /*
     * Validate the parameters.
     */
    if (!IsPowerOfTwo(blockSize) || (blockSize % DEV_BYTES_PER_SECTOR)) {
	fprintf(stderr,"Blocksize (%d) not power of two or not multiple of sector size %d.\n",
			blockSize, DEV_BYTES_PER_SECTOR);
	exit(1);
    }
    if (!IsMultipleOf(segmentSize, blockSize)) {
	fprintf(stderr,"Segment size (%d) is not a multiple of blockSize.\n",
		segmentSize);
	exit(1);
    }
    if (!IsMultipleOf(descBlockSize, blockSize)) {
	fprintf(stderr,
		"Descriptor block size (%d) is not a multiple of blockSize.\n",
		descBlockSize);
	exit(1);
    }
    if (!IsMultipleOf(descMapBlockSize, blockSize)) {
	fprintf(stderr,
		"Descriptor map block size (%d) is not a multiple of the blockSize.\n",
		descMapBlockSize);
	exit(1);
    }
    if (!IsMultipleOf(usageArrayBlockSize, blockSize)) {
	fprintf(stderr,
		"Usage array block size (%d) is not a multiple of the blockSize.\n",
		usageArrayBlockSize);
	exit(1);
    }
    if (!IsMultipleOf(segAlignment, DEV_BYTES_PER_SECTOR)) {
	fprintf(stderr,
		"segAlignment (%d) is not a multiple of the DEV_BYTES_PER_SECTOR.\n",
		segAlignment);
	exit(1);
    }
    segAlignment = segAlignment/DEV_BYTES_PER_SECTOR;

    if (maxCacheBlocksCleaned > maxNumCacheBlocks) {
	fprintf(stderr, "maxCacheBlocksCleaned large than maxNumCacheBlocks\n");
	exit(1);
    } 

    /*
     * Open of the device and write the file system.
     */
    diskFd = open(deviceName, O_RDWR, 0);
    if (diskFd < 0) {
	fprintf(stderr,"%s: ", argv[0]);
	perror(deviceName);
	exit(1);
    }
    sizeInSectors = ComputeDiskSize(diskFd);
    if (numberSegments == -1) {
	if (sizeInSectors < 0) {
	    fprintf(stderr,
		"Can't compute disk size, must specified -numSegments\n");
	    exit(1);
	}
	/*
	 * Pack as many segments on to disk that will fit. Leave
	 * 128K at beginning for label, superblock, and checkpoint
	 * stuff.
	 */
	numberSegments = (sizeInSectors*blockSize)/segmentSize - 
				((128*1024)/segmentSize);
    }

    if (maxDesc == -1) {
	/*
	 * Unless otherwise told, generate one descriptor for
	 * every 8K of disk space.
	 */
	maxDesc = (numberSegments * segmentSize) / 8192;
    }

    /*
     * Compute an upper bound of the checkpoint size. Checkpoint contains:
     *		a checkpoint header.
     *		as many as LFS_MAX_NUM_MODS check point regions.
     *		a checkpoint trailer.
     *		the desc map checkpoint info complete with block index.
     *		the seg usage checkpoint info complete with block index.
     *		some stats data.
     */
    maxCheckPointSize = sizeof(LfsCheckPointHdr) + 
	sizeof(LfsCheckPointRegion) * LFS_MAX_NUM_MODS + 
			sizeof(LfsCheckPointTrailer) +
	sizeof(LfsDescMapCheckPoint) + sizeof(LfsStableMemCheckPoint) +
	sizeof(int) * 
		BlockCount(maxDesc*sizeof(LfsDescMapEntry),descMapBlockSize) +
	sizeof(LfsSegUsageCheckPoint) +  sizeof(LfsStableMemCheckPoint) +
        sizeof(int) * 
        BlockCount(numberSegments*sizeof(LfsSegUsageEntry),usageArrayBlockSize)
	+ LFS_STATS_MAX_SIZE;

    /*
     * Round to a multiple of the block size.
     */
    maxCheckPointBlocks = BlockCount(maxCheckPointSize,blockSize);
    maxCheckPointSize = maxCheckPointBlocks * blockSize;
    /*
     * Fill in the super block header.
     */
    startOffset = LFS_SUPER_BLOCK_OFFSET;
    superBlockPtr = (LfsSuperBlock *) malloc(LFS_SUPER_BLOCK_SIZE);
    bzero((char *)superBlockPtr, LFS_SUPER_BLOCK_SIZE);
    superBlockPtr->hdr.magic = LFS_SUPER_BLOCK_MAGIC;
    superBlockPtr->hdr.version = LFS_SUPER_BLOCK_VERSION;
    superBlockPtr->hdr.blockSize = blockSize;
    superBlockPtr->hdr.maxCheckPointBlocks = maxCheckPointBlocks;
    superBlockPtr->hdr.checkpointInterval = checkpointInterval;
    superBlockPtr->hdr.checkPointOffset[0] = startOffset + 
		    BlockCount(LFS_SUPER_BLOCK_SIZE, blockSize);
    superBlockPtr->hdr.checkPointOffset[1]  = 
	superBlockPtr->hdr.checkPointOffset[0] + maxCheckPointBlocks;
    superBlockPtr->hdr.logStartOffset = 
	superBlockPtr->hdr.checkPointOffset[1] +  maxCheckPointBlocks;
    if ((superBlockPtr->hdr.logStartOffset % segAlignment) != 0) {
	superBlockPtr->hdr.logStartOffset += 
		(segAlignment - 
			superBlockPtr->hdr.logStartOffset % segAlignment);
    }
    superBlockPtr->hdr.maxNumCacheBlocks = maxCacheBlocksCleaned;

    /*
     * Reduce the number of segments until the file system fits on the disk.
     */
    if (sizeInSectors > 0) {
	while ((superBlockPtr->hdr.logStartOffset + 
		numberSegments * (segmentSize/blockSize)) *
			blockSize/DEV_BYTES_PER_SECTOR >= sizeInSectors) {
	    numberSegments--;
	}

    }
    segMemPtr = calloc(1, segmentSize);

    summaryPtr = segMemPtr;


    seg.startPtr = segMemPtr;
    summaryPtr = segMemPtr + segmentSize - blockSize;
    seg.dataPtr = summaryPtr;
    seg.activeBytes = 0;
    seg.diskAddress = superBlockPtr->hdr.logStartOffset;
    seg.summaryPtr = summaryPtr + sizeof(LfsSegSummary) + 
				sizeof(LfsSegSummaryHdr);
    checkPointPtr = calloc(1, maxCheckPointSize);
    checkpoint.startPtr = checkPointPtr;
    checkpoint.memPtr = checkPointPtr + sizeof(LfsCheckPointHdr) + 
					 sizeof(LfsCheckPointRegion);
    segSumPtr = (LfsSegSummary *) summaryPtr;
    sumHdrPtr = (LfsSegSummaryHdr *) (summaryPtr + sizeof(LfsSegSummary));
    regionPtr = (LfsCheckPointRegion *) 
			(checkPointPtr + sizeof(LfsCheckPointHdr));
    for (moduleType = LFS_FILE_LAYOUT_MOD; (moduleType < LFS_MAX_NUM_MODS);
		moduleType++) { 
	segMemPtr = seg.dataPtr;
	if (moduleType == LFS_DESC_MAP_MOD) { 
	    BuildInitialDescMap(&superBlockPtr->descMap, &seg, &checkpoint);
	} else if (moduleType == LFS_SEG_USAGE_MOD) {
	    BuildInitialUsageArray(&superBlockPtr->usageArray, &seg,
					&checkpoint);
	} else if (moduleType == LFS_FILE_LAYOUT_MOD) {
	    BuildInitialFileLayout(&superBlockPtr->fileLayout, &seg, 
				&checkpoint);
	} else {
	    panic("Unknown moduleType %d\n", moduleType);
	}
	sumHdrPtr->moduleType = moduleType;
	sumHdrPtr->lengthInBytes = (seg.summaryPtr - (char *) sumHdrPtr); 
	sumHdrPtr->numDataBlocks = (segMemPtr - seg.dataPtr)/blockSize;
	sumHdrPtr = (LfsSegSummaryHdr *) seg.summaryPtr;
	seg.summaryPtr += sizeof(LfsSegSummaryHdr);
	regionPtr->type = moduleType;
	regionPtr->size = (checkpoint.memPtr - (char *) regionPtr);

	regionPtr = (LfsCheckPointRegion *) checkpoint.memPtr;
	checkpoint.memPtr += sizeof(LfsCheckPointRegion);
    }
    checkpoint.memPtr -= sizeof(LfsCheckPointRegion);
    seg.summaryPtr -= sizeof(LfsSegSummaryHdr);

    ((LfsCheckPointTrailer *) checkpoint.memPtr)->timestamp = 1;
    checkpoint.memPtr += sizeof(LfsCheckPointTrailer);
    segSumPtr->magic = LFS_SEG_SUMMARY_MAGIC;
    segSumPtr->timestamp = 1;
    segSumPtr->prevSeg = 0;
    segSumPtr->nextSeg = 1;
    segSumPtr->size = (seg.summaryPtr - (char *) segSumPtr);
    segSumPtr->nextSummaryBlock = -1;

    cpHdrPtr = (LfsCheckPointHdr *) checkpoint.startPtr;
    cpHdrPtr->timestamp = 1;
    cpHdrPtr->size = (checkpoint.memPtr - checkpoint.startPtr);
    cpHdrPtr->version = 1;
    cpHdrPtr->domainNumber = -1;
    cpHdrPtr->attachSeconds = time(0);
    cpHdrPtr->detachSeconds = time(0);
    cpHdrPtr->serverID = serverID;

    ((Lfs_StatsVersion1 *) checkpoint.memPtr)->size = sizeof(Lfs_StatsVersion1);
    ((Lfs_StatsVersion1 *) checkpoint.memPtr)->version = 1;

    oldCheckPointPtr = (LfsCheckPointHdr *) calloc(1, blockSize);
    oldCheckPointPtr->timestamp = 0;
    oldCheckPointPtr->size = sizeof(LfsCheckPointHdr);
    oldCheckPointPtr->version = 1;

    EraseOldFileSystem(diskFd);

    WriteDisk(diskFd, startOffset,(char *) superBlockPtr, LFS_SUPER_BLOCK_SIZE);
    WriteDisk(diskFd, superBlockPtr->hdr.checkPointOffset[0],
			checkPointPtr, maxCheckPointSize);
    WriteDisk(diskFd, superBlockPtr->hdr.checkPointOffset[1], 
			(char *) oldCheckPointPtr, sizeof(*oldCheckPointPtr));

    WriteDisk(diskFd, GetDiskAddressOf(&seg), seg.dataPtr, 
			segmentSize - (seg.dataPtr - seg.startPtr));
    bzero(segMemPtr, blockSize);
    blocksPerSeg = segmentSize/blockSize;
    for (segNum = 1; segNum < numberSegments; segNum++) {
	WriteDisk(diskFd, superBlockPtr->hdr.logStartOffset +
		blocksPerSeg * segNum + (blocksPerSeg-1), segMemPtr, 
		blockSize);
    }

    exit(0);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * BuildInitialFileLayout --
 *
 *	Build the file structure for the file system. This consists of
 *	initializing the root directory
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Directory is added to the segments.
 *
 *----------------------------------------------------------------------
 */

static void
BuildInitialFileLayout(fileLayoutPtr, segPtr, checkpointPtr)
    LfsFileLayoutParams	*fileLayoutPtr;	/* File system params in super block to
					 * be filled in. */
    SegMem		*segPtr;	/* Segment to add data blocks to. */
    CheckPointMem	*checkpointPtr; /* Checkpoint info to be added. */
{
    LfsFileDescriptor	*descPtr;
    LfsFileLayoutSummary *layoutSumPtr;
    LfsFileLayoutDesc    *descLayoutPtr;

    /*
     * Initialize the file layout parameters. 
     */
    fileLayoutPtr->descPerBlock = descBlockSize/sizeof(LfsFileDescriptor);
    /*
     * Allocate a descriptor block for the root directory's descriptor.
     */
    segPtr->dataPtr -= descBlockSize;
    descPtr = (LfsFileDescriptor *) segPtr->dataPtr;

    segPtr->summaryPtr += sizeof(LfsFileLayoutDesc);
    descLayoutPtr = (LfsFileLayoutDesc *) segPtr->summaryPtr;
    descLayoutPtr->blockType = LFS_FILE_LAYOUT_DESC;
    descLayoutPtr->numBlocks = descBlockSize/blockSize;
    segPtr->activeBytes += 2*sizeof(LfsFileDescriptor);
    root.descBlockAddress = GetDiskAddressOf(segPtr);

    MakeRootDir(descPtr+0, descPtr+1, segPtr, &root);
    layoutSumPtr = (LfsFileLayoutSummary *) segPtr->summaryPtr;
    segPtr->summaryPtr += sizeof(LfsFileLayoutSummary);
    layoutSumPtr->blockType = LFS_FILE_LAYOUT_DATA;
    layoutSumPtr->numBlocks = root.numBlocks;
    layoutSumPtr->fileNumber = descPtr[0].fileNumber;
    layoutSumPtr->truncVersion = root.truncVersion = 1;
    layoutSumPtr->numDataBlocks = 1;
    (*(int *) segPtr->summaryPtr) = 0;
    segPtr->summaryPtr += sizeof(int);

    layoutSumPtr = (LfsFileLayoutSummary *) segPtr->summaryPtr;
    segPtr->summaryPtr += sizeof(LfsFileLayoutSummary);
    layoutSumPtr->blockType = LFS_FILE_LAYOUT_DATA;
    layoutSumPtr->numBlocks = root.numBlocks;
    layoutSumPtr->fileNumber = descPtr[1].fileNumber;
    layoutSumPtr->truncVersion = root.truncVersion;
    layoutSumPtr->numDataBlocks = 1;

    (*(int *) segPtr->summaryPtr) = 0;
    segPtr->summaryPtr += sizeof(int);

}


/*
 *----------------------------------------------------------------------
 *
 * BuildInitialDescMap --
 *
 *	Build the initial descriptor structure for a file system.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
BuildInitialDescMap(descParamsPtr, segPtr, checkpointPtr)
    LfsDescMapParams	*descParamsPtr; /* Parameters in super block to be 
					 * filled in. */
    SegMem		*segPtr;	/* Segment to add data blocks to. */
    CheckPointMem	*checkpointPtr; /* Checkpoint info to be added. */
{
    LfsDescMapCheckPoint	*cp;
    StableMem			*stableMemPtr;

    descParamsPtr->version = LFS_DESC_MAP_VERSION;
    descParamsPtr->maxDesc = maxDesc;

    stableMemPtr = BuildStableMem(LFS_DESC_MAP_MOD, descMapBlockSize, 
		   sizeof(LfsDescMapEntry),  maxDesc,
		   &(descParamsPtr->stableMem));

    /*
     * Reserve descriptors 0 and 1.
     */
    SetDescriptorMap(stableMemPtr, 0, 0, 0, 0);
    SetDescriptorMap(stableMemPtr, 1, 0, 0, 0); 
    SetDescriptorMap(stableMemPtr, FSDM_ROOT_FILE_NUMBER, 
		    root.descBlockAddress, 
		    root.truncVersion, root.accessTime);
    SetDescriptorMap(stableMemPtr, FSDM_LOST_FOUND_FILE_NUMBER, 
		    root.descBlockAddress, 
		    root.truncVersion, root.accessTime);

    cp = (LfsDescMapCheckPoint *) checkpointPtr->memPtr;
    cp->numAllocDesc = 4;
    checkpointPtr->memPtr += sizeof(LfsDescMapCheckPoint);
    LayoutStableMem(stableMemPtr, segPtr, checkpointPtr);
}


/*
 *----------------------------------------------------------------------
 *
 * BuildInitialUsageArray --
 *
 *	Build the initial usage array structure for the file system.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
BuildInitialUsageArray(usagePtr, segPtr, checkpointPtr)
    LfsSegUsageParams	*usagePtr;      /* Parameters in super block to be 
					 * filled in. */
    SegMem		*segPtr;	/* Segment to add data blocks to. */
    CheckPointMem	*checkpointPtr; /* Checkpoint info to be added to. */
{
    LfsSegUsageCheckPoint	*cp;
    LfsSegUsageEntry		entry;
    int				i, numBlocks;
    StableMem		*stableMemPtr;

    usagePtr->segmentSize = segmentSize;
    usagePtr->numberSegments = numberSegments;
    /*
     * Set the minimum number of clean segments that we allow the system
     * to get to before starting cleaning. We shouldn't let this 
     * number get any smaller than the size of the data we clean.
     * We make it two times this number to add a margin of safety.
     * 
     */
    usagePtr->minNumClean = 2*(FS_BLOCK_SIZE * (maxCacheBlocksCleaned))
				/  segmentSize;
    if (usagePtr->minNumClean < 10) {
	usagePtr->minNumClean = 10;
    }
    numBlocks = numberSegments * (segmentSize/blockSize);
    /*
     * Set the minimum number of free blocks allowed. This should be
     * based on the max disk space utilization required of the disk.
     * For very small disk this number may be dominated by the minNumClean
     * value.
     */
    usagePtr->minFreeBlocks = (int) (numBlocks - (numBlocks * maxUtilization));
    if (usagePtr->minFreeBlocks < 
		(usagePtr->minNumClean+10)*(segmentSize/blockSize)) {
	usagePtr->minFreeBlocks = 
		(usagePtr->minNumClean+10)*(segmentSize/blockSize);
    }

    usagePtr->wasteBlocks = (FS_BLOCK_SIZE/blockSize) + 2;

    /* 
     * Number of segments to clean before stoping because there is
     * until already clean. We clean enough to write back the
     * entire file cache. 
     */
    usagePtr->numSegsToClean = 1+(maxNumCacheBlocks*FS_BLOCK_SIZE)/segmentSize;
    if (usagePtr->numSegsToClean + usagePtr->minNumClean >= 
		(int) (numberSegments*maxUtilization)) {
	usagePtr->numSegsToClean = (int) (numberSegments*maxUtilization) - 
				        usagePtr->minNumClean - 10;
    }


    stableMemPtr = BuildStableMem(LFS_SEG_USAGE_MOD, usageArrayBlockSize, 
		   sizeof(LfsSegUsageEntry),  numberSegments,
		   &(usagePtr->stableMem));

    /*
     * Link all clean segments togther. 
     */
    entry.activeBytes = 0;
    entry.flags = LFS_SEG_USAGE_DIRTY;
    UpdateStableMem(stableMemPtr, 0, (char *) &entry);

    for (i = 1; i < numberSegments; i++) {
	entry.activeBytes = i+1;
	entry.flags = LFS_SEG_USAGE_CLEAN;
	UpdateStableMem(stableMemPtr, i, (char *) &entry);
    }
    entry.activeBytes = NIL;
    entry.flags = LFS_SEG_USAGE_CLEAN;
    UpdateStableMem(stableMemPtr, numberSegments-1, (char *) &entry);

    cp = (LfsSegUsageCheckPoint *) checkpointPtr->memPtr;
    cp->numClean = numberSegments-1;
    cp->numDirty = 1;
    cp->dirtyActiveBytes = (int) (segmentSize * segFullLevel);
    cp->currentSegment = 0;
    cp->currentBlockOffset = -1;
    checkpointPtr->memPtr += sizeof(LfsSegUsageCheckPoint);
    LayoutStableMem(stableMemPtr, segPtr, checkpointPtr);
    cp->curSegActiveBytes = segPtr->activeBytes;
    cp->previousSegment = -1;
    cp->cleanSegList = 1;
    cp->freeBlocks = numberSegments*(segmentSize/blockSize) - 
		     (segPtr->activeBytes + blockSize - 1)/blockSize;
}

/*
 *----------------------------------------------------------------------
 *
 * BuildStableMem --
 *
 *	Build the stable memory map.
 *
 * Results:
 *	A StableMem structure that can be used to calls to UpdateStableMem
 *	and LayoutStableMem.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static StableMem *
BuildStableMem(memType, memBlockSize, entrySize, maxNumEntries, smemParamsPtr)
    int	 memType;	/* Type number of stable memory map. */
    int	 memBlockSize;	/* Block size of the memory. */
    int	 entrySize;	/* Size of each entry in bytes. */
    int	 maxNumEntries; /* Maximum number of entries supported. */
    LfsStableMemParams *smemParamsPtr; /* Stable mem params to fill in. */
{
    StableMem			*stableMemPtr;

    /*
     * Fill in the params for this map.
     */
    smemParamsPtr->memType = memType;
    smemParamsPtr->blockSize = memBlockSize;
    smemParamsPtr->entrySize = entrySize;
    smemParamsPtr->maxNumEntries = maxNumEntries;

    smemParamsPtr->entriesPerBlock = 
		(memBlockSize - sizeof(LfsStableMemBlockHdr)) / entrySize;
    smemParamsPtr->maxNumBlocks = BlockCount(maxNumEntries, 
					smemParamsPtr->entriesPerBlock);

    stableMemPtr = (StableMem *) calloc(1, sizeof(StableMem));
    stableMemPtr->paramsPtr = smemParamsPtr;
    stableMemPtr->blockPtrs = (char **) malloc(sizeof(char *) * 
						smemParamsPtr->maxNumBlocks);

    bzero((char *) stableMemPtr->blockPtrs, sizeof(char *) * 
						smemParamsPtr->maxNumBlocks);

    return stableMemPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStableMem --
 *
 *	Update an entry in a stable memory data structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
UpdateStableMem(stableMemPtr, entryNumber, entryPtr)
    StableMem	*stableMemPtr;		/* Stable memory data to update. */
    int		entryNumber;	/* Entry number to upate. */
    char	*entryPtr;	/* New value for entry entryNumber. */
{
    int 		blockNum, offset;
    LfsStableMemBlockHdr *hdrPtr;

    if ((entryNumber < 0) || 
	(entryNumber >= stableMemPtr->paramsPtr->maxNumEntries)) {
	panic("Entry number out of range\n");
    }

    blockNum = entryNumber / stableMemPtr->paramsPtr->entriesPerBlock;
    offset = (entryNumber % stableMemPtr->paramsPtr->entriesPerBlock) * 
		stableMemPtr->paramsPtr->entrySize + 
				sizeof(LfsStableMemBlockHdr);

    if (stableMemPtr->blockPtrs[blockNum] == (char *) NULL) {
	stableMemPtr->blockPtrs[blockNum] = 
				calloc(1, stableMemPtr->paramsPtr->blockSize);
	hdrPtr = (LfsStableMemBlockHdr *) stableMemPtr->blockPtrs[blockNum];
	hdrPtr->magic = LFS_STABLE_MEM_BLOCK_MAGIC;
	hdrPtr->memType = stableMemPtr->paramsPtr->memType;
	hdrPtr->blockNum = blockNum;
    }
    bcopy(entryPtr, stableMemPtr->blockPtrs[blockNum] + offset, 
	  stableMemPtr->paramsPtr->entrySize);

}

/*
 *----------------------------------------------------------------------
 *
 * LayoutStableMem --
 *
 *	Add to a segment the contents of a stable memory data structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Data, summary, and checkpoint region added to.
 *
 *----------------------------------------------------------------------
 */

static void
LayoutStableMem(stableMemPtr, segPtr, checkpointPtr)
    StableMem	*stableMemPtr;		/* Stable memory data to layout. */
    SegMem		*segPtr;	/* Segment to add data blocks to. */
    CheckPointMem	*checkpointPtr; /* Checkpoint info to be added to. */
{
    LfsStableMemCheckPoint	*cp;
    int	block;

    cp = (LfsStableMemCheckPoint *) checkpointPtr->memPtr;
    cp->numBlocks = 0;
    checkpointPtr->memPtr += sizeof(LfsStableMemCheckPoint);
    for (block = 0; block < stableMemPtr->paramsPtr->maxNumBlocks; block++) { 
	if (stableMemPtr->blockPtrs[block]) {
	    segPtr->dataPtr -= stableMemPtr->paramsPtr->blockSize;
	    segPtr->activeBytes += stableMemPtr->paramsPtr->blockSize;
	    bcopy(stableMemPtr->blockPtrs[block],segPtr->dataPtr, 
			stableMemPtr->paramsPtr->blockSize);
	    *((int *) (segPtr->summaryPtr)) = block;
	    segPtr->summaryPtr += sizeof(int);
	    cp->numBlocks++;
	    *((int *) (checkpointPtr->memPtr)) = 
			GetDiskAddressOf(segPtr);
	    checkpointPtr->memPtr += sizeof(int);
	 } else {
	    *((int *) (checkpointPtr->memPtr)) = FSDM_NIL_INDEX;
	    checkpointPtr->memPtr += sizeof(int);
	 }
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MakeRootDir --
 *
 *	Make the initial root directory of a LFS file system..
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Data and summary blocks added to the segment.
 *
 *----------------------------------------------------------------------
 */
static void
MakeRootDir(rootDescPtr, lostDescPtr, segPtr, rootPtr)
    LfsFileDescriptor *rootDescPtr;  /* Root file descriptor to fill in. */
    LfsFileDescriptor *lostDescPtr; /* Lost an found descriptor to fill 
					   * in. */
    SegMem		*segPtr;     /* Segment to add directory to. */
    RootDir		*rootPtr;    /* Root directory info. */
{
    Fslcl_DirEntry *dirEntryPtr;
    char *fileName, *dirPtr;
    int length,  offset;

    FileDescInit(rootDescPtr, FSDM_ROOT_FILE_NUMBER, FS_DIRECTORY);
    FileDescInit(lostDescPtr, FSDM_LOST_FOUND_FILE_NUMBER, FS_DIRECTORY);
    segPtr->dataPtr -= FSLCL_DIR_BLOCK_SIZE;
    segPtr->activeBytes += FSLCL_DIR_BLOCK_SIZE;
    dirPtr = segPtr->dataPtr;
    rootDescPtr->common.lastByte = FSLCL_DIR_BLOCK_SIZE-1;
    rootDescPtr->common.direct[0] = GetDiskAddressOf(segPtr);
    rootDescPtr->common.numKbytes = 1;
    /*
     * Place the data in the first filesystem block.
     */
    dirEntryPtr = (Fslcl_DirEntry *) dirPtr;

    fileName = ".";
    length = strlen(fileName);
    dirEntryPtr->fileNumber = FSDM_ROOT_FILE_NUMBER;
    dirEntryPtr->recordLength = Fslcl_DirRecLength(length);
    dirEntryPtr->nameLength = length;
    strcpy(dirEntryPtr->fileName, fileName);
    offset = dirEntryPtr->recordLength;
    rootDescPtr->common.numLinks++;

    dirEntryPtr = (Fslcl_DirEntry *)(dirPtr + offset);
    fileName = "..";
    length = strlen(fileName);
    dirEntryPtr->fileNumber = FSDM_ROOT_FILE_NUMBER;
    dirEntryPtr->recordLength = Fslcl_DirRecLength(length);
    dirEntryPtr->nameLength = length;
    strcpy(dirEntryPtr->fileName, fileName);
    offset += dirEntryPtr->recordLength;
    rootDescPtr->common.numLinks++;

    dirEntryPtr = (Fslcl_DirEntry *)(dirPtr + offset);
    fileName = "lost+found";
    length = strlen(fileName);
    dirEntryPtr->fileNumber = FSDM_LOST_FOUND_FILE_NUMBER;
    dirEntryPtr->recordLength = FSLCL_DIR_BLOCK_SIZE - offset;
    dirEntryPtr->nameLength = length;
    strcpy(dirEntryPtr->fileName, fileName);
    lostDescPtr->common.numLinks++;



    /*
     * Initialized and add to the segment the lost+found directory. 
     */
    segPtr->dataPtr -= FSLCL_DIR_BLOCK_SIZE;
    segPtr->activeBytes += FSLCL_DIR_BLOCK_SIZE;
    dirPtr = segPtr->dataPtr;
    lostDescPtr->common.lastByte = FSLCL_DIR_BLOCK_SIZE-1;
    lostDescPtr->common.direct[0] = GetDiskAddressOf(segPtr);
    lostDescPtr->common.numKbytes = 1;
    /*
     * Fill in the directory.
     */
    dirEntryPtr = (Fslcl_DirEntry *) dirPtr;
    fileName = ".";
    length = strlen(fileName);
    dirEntryPtr->fileNumber = FSDM_LOST_FOUND_FILE_NUMBER;
    dirEntryPtr->recordLength = Fslcl_DirRecLength(length);
    dirEntryPtr->nameLength = length;
    strcpy(dirEntryPtr->fileName, fileName);
    offset = dirEntryPtr->recordLength;
    lostDescPtr->common.numLinks++;

    dirEntryPtr = (Fslcl_DirEntry *)(dirPtr + offset);
    fileName = "..";
    length = strlen(fileName);
    dirEntryPtr->fileNumber = FSDM_ROOT_FILE_NUMBER;
    dirEntryPtr->recordLength = FSLCL_DIR_BLOCK_SIZE - offset;
    dirEntryPtr->nameLength = length;
    strcpy(dirEntryPtr->fileName, fileName);
    rootDescPtr->common.numLinks++;

    rootPtr->accessTime = rootDescPtr->common.accessTime;
    rootPtr->numBlocks = FSLCL_DIR_BLOCK_SIZE/blockSize;
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * FileDescInit --
 *
 *	Initialized a LFS file descriptor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
FileDescInit(fileDescPtr, fileNumber, fileType)
    LfsFileDescriptor *fileDescPtr; 	/* File descriptor to fill in. */
    int		      fileNumber;	/* File number of descriptor. */
    int		      fileType;		/* File type of the desc. */
{
    int  timeVal, index;

    fileDescPtr->fileNumber = fileNumber;
    fileDescPtr->common.magic = FSDM_FD_MAGIC;
    fileDescPtr->common.flags = FSDM_FD_ALLOC;
    fileDescPtr->common.fileType = fileType;
    fileDescPtr->common.permissions = 0755;
    fileDescPtr->common.uid = 0;
    fileDescPtr->common.gid = 0;
    fileDescPtr->common.lastByte = -1;
    fileDescPtr->common.firstByte = -1;
    fileDescPtr->common.numLinks = 0;
    /*
     * Can't know device information because that depends on
     * the way the system is configured.
     */
    fileDescPtr->common.devServerID = -1;
    fileDescPtr->common.devType = -1;
    fileDescPtr->common.devUnit = -1;

    /*
     * Set the time stamps.  This assumes that universal time, not local
     * time, is used for time stamps.
     */
    timeVal = time(0);
    fileDescPtr->common.createTime = timeVal;
    fileDescPtr->common.accessTime = timeVal;
    fileDescPtr->common.descModifyTime = timeVal;
    fileDescPtr->common.dataModifyTime = timeVal;

    /*
     * Initialize map to NILs.
     */
    for (index = 0; index < FSDM_NUM_DIRECT_BLOCKS ; index++) {
	fileDescPtr->common.direct[index] = FSDM_NIL_INDEX;
    }
    for (index = 0; index < FSDM_NUM_INDIRECT_BLOCKS ; index++) {
	fileDescPtr->common.indirect[index] = FSDM_NIL_INDEX;
    }
    fileDescPtr->common.numKbytes = 0;
    fileDescPtr->common.version = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * SetDescriptorMap --
 *
 *	Set an entry in the descriptor map of the file system. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
SetDescriptorMap(stableMemPtr, fileNumber, diskAddress, truncVersion, 
			accessTime)
    StableMem	*stableMemPtr;   /* Stable memory of descriptor map. */
    int	fileNumber;		 /* File number to set. */
    unsigned int diskAddress;    /* Disk address of file number. */
    int truncVersion;		 /* Truncate version number of file. */
    int	accessTime;		 /* Time of last file access. */
{
    LfsDescMapEntry entry;

    entry.blockAddress = diskAddress;
    entry.truncVersion = truncVersion;
    entry.flags = LFS_DESC_MAP_ALLOCED;
    entry.accessTime = accessTime;
    UpdateStableMem(stableMemPtr, fileNumber, (char *) &entry);
}

/*
 *----------------------------------------------------------------------
 *
 * GetDiskAddressOf --
 *
 *	Return the disk address of the current data pointer of the segment.
 *
 * Results:
 *	The disk address of last data block allocated.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static unsigned int 
GetDiskAddressOf(segPtr)
    SegMem	*segPtr;	/* Return the disk address of the current
				 * dataPtr. */
{
    int blockOffset;
    blockOffset = (segPtr->dataPtr - segPtr->startPtr) / blockSize;
    return segPtr->diskAddress + blockOffset;
}

/*
 *----------------------------------------------------------------------
 *
 * WriteDisk --
 *
 *	Write the the sepcified buffer to disk.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This disk gets written or the program exits.
 *
 *----------------------------------------------------------------------
 */

static void
WriteDisk(diskFd, blockOffset, bufferPtr, bufferSize)
    int	diskFd;		/* Open file descriptor of disk. */
    int	blockOffset;	/* Block offset into disk to write. */
    char *bufferPtr;	/* Buffer to write. */
    int	 bufferSize;	/* Number of bytes to write. */
{
    int	numBytes, status;

    numBytes = BlockCount(bufferSize, blockSize) * blockSize;

    status = lseek(diskFd, blockOffset*blockSize, L_SET);
    if (status < 0) {
	fprintf(stderr,"Writing device: ");
	perror("lseek");
	exit(1);
    }
    status = write(diskFd, bufferPtr, numBytes);
    if (status != numBytes) {
	if (status < 0) {
	    fprintf(stderr,"Writing device: ");
	    perror("write");
	    exit(1);
	}
	fprintf(stderr,"Short write on device %d != %d\n", status, numBytes);
	exit(1);
    }

}

/*
 *----------------------------------------------------------------------
 *
 * IsPowerOfTwo --
 *
 *	Check to see if the integer is a postive power of two.
 *
 * Results:
 *	TRUE if the argument is a power of two. FALSE otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Boolean
IsPowerOfTwo(ival)
    int	 ival;	/* Value to check. */
{
    if (ival <= 0) {
	return FALSE;
    }
    while (!(ival & 1)) {
	ival >>= 1;
    }
    return ((ival >> 1) == 0);
}

/*
 *----------------------------------------------------------------------
 *
 * IsMultipleOf --
 *
 *	Check to see if one integer is a multiple of another..
 *
 * Results:
 *	TRUE if they are multiples. FALSE otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Boolean
IsMultipleOf(imultiple, ival)
    int	imultiple;	/* Value to check against ival. */
    int ival;		/* Base value. */
{
    int	itemp;

    itemp = imultiple/ival;

    return (itemp*ival == imultiple);
}


/*
 *----------------------------------------------------------------------
 *
 * BlockCount  --
 *
 *	Return how many blocks of a specified size it will take to 
 *	the specified object.
 *
 * Results:
 *	Number of blocks.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
BlockCount(objectSize, blockSize)
    int	objectSize;	/* Size of object in bytes. */
    int blockSize;	/* Block size in bytes. */
{

    return (objectSize + (blockSize-1))/blockSize;
}

#include <disk.h>

/*
 *----------------------------------------------------------------------
 *
 * EraseOldFileSystem --
 *
 *	Erase the old OFS file system from the disk so that the kernel
 *	attach code wont file it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
EraseOldFileSystem(diskFd)
    int	diskFd;	/* Open file descriptor of disk. */
{
    Disk_Label  *labelPtr;
    int		status, i;
    static	char bufferOfZeros[DEV_BYTES_PER_SECTOR];

    labelPtr = Disk_ReadLabel(diskFd);
    if (labelPtr == (Disk_Label *) NULL) {
	return;
    }
    if (labelPtr->summarySector > 0) {
	status = lseek(diskFd, 
		    DEV_BYTES_PER_SECTOR*labelPtr->summarySector, L_SET);
	if (status > 0) {
	    status = write(diskFd, bufferOfZeros, DEV_BYTES_PER_SECTOR);
	}
	if (status < 0) {
	    fprintf(stderr,"Erasing old file system: ");
	    perror("write");
	}
    }
    if (labelPtr->domainSector > 0) {
	for (i = 0; i < labelPtr->numDomainSectors; i++) {
	    status = lseek(diskFd, 
			DEV_BYTES_PER_SECTOR*(labelPtr->domainSector+i), L_SET);
	    if (status > 0) {
		status = write(diskFd, bufferOfZeros, DEV_BYTES_PER_SECTOR);
	    }
	    if (status < 0) {
		fprintf(stderr,"Erasing old file system: ");
		perror("write");
	    }
	 }

    }
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeDiskSize --
 *
 *	Compute the size of the disk by reading it from the disk label.
 *
 * Results:
 *	-1 if can't compute size.  The size.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
ComputeDiskSize(diskFd)
    int	diskFd;	/* Open file descriptor of disk. */
{
    struct stat sbuf;
    Disk_Label  *labelPtr;
    int		partition, sizeInSectors;

    labelPtr = Disk_ReadLabel(diskFd);
    if (labelPtr == (Disk_Label *) NULL) {
	return -1;
    }
    if (fstat(diskFd, &sbuf) < 0) {
	return -1;
    }
#ifdef ds3100
    /*
     * Handle some bogusness in the ds3100 port.
     */
    partition = unix_minor(sbuf.st_rdev) & 0x7;
#else
    partition = minor(sbuf.st_rdev) & 0x7;
#endif
    sizeInSectors = labelPtr->partitions[partition].numCylinders * 
			labelPtr->numHeads * labelPtr->numSectors;
    if (sizeInSectors <= 0) {
	sizeInSectors = -1;
    }
    return sizeInSectors;
}
