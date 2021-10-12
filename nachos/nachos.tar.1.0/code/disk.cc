/* disk.cc -- emulation of a physical disk.  */

#include "disk.h"
#include "machine.h"
#include "system.h"

/* We put this at the front of the UNIX file representing the
 * disk, to make it less likely we are over-writing useful data.
 */
const int MagicNumber = 0x456789ab;
const int MagicSize = sizeof(MagicNumber);

const int DiskSize = MagicSize + (NumSectors * SectorSize);

/* needed since we can't take a pointer of a member function */
static void
DiskInterruptHandler(int arg)
{
    Disk* disk = (Disk *)arg;

    disk->HandleInterrupt();
}

/* Open the UNIX file, making sure it's the right one.
 * Create it if it doesn't exist.
 */
Disk::Disk(char* name, VoidFunctionPtr callWhenDone, int callArg)
{
    DEBUG('d', "Initializing the disk, 0x%x 0x%x\n", callWhenDone, callArg);
    handler = callWhenDone;
    handlerArg = callArg;
    lastSector = 0;
    
    fileno = open(name, 2,  0644); /* O_RDWR */
    if (fileno >= 0) {
 	/* file exists, check magic number */
	int magicNum;
	int returnValue = read(fileno, (char *) &magicNum, MagicSize);
	ASSERT((returnValue >= 0) && (magicNum == MagicNumber));
    } else {
        fileno = open(name, 01002,  0644); /* O_RDWR | O_CREAT */
        ASSERT(fileno >= 0);	/* crash if we can't open the file */

	int magicNum = MagicNumber;  /* write out the magic number */
	int returnValue = write(fileno, (char *) &magicNum, MagicSize);
	ASSERT(returnValue >= 0);

        (void) lseek(fileno, DiskSize - 1, 0);	/* set size of disk */
	char tmp = '\0';
	returnValue = write(fileno, &tmp, 1);   /* write last byte */
	ASSERT(returnValue >= 0);
    }
    active = FALSE;
    
    // Make sure the interrupt handler in the machine is set up.
    machine->setInterruptHandler(DiskInterrupt, DiskInterruptHandler);
}

/* This destructor only needs to close the UNIX file. */
Disk::~Disk()
{
    int errorCode = close(fileno);
    ASSERT(errorCode >= 0);
}

/* Latency = seek time + rotational latency + 1
 *   Disk rotates at one sector per tick and seeks at one track per tick,
 *   The extra +1 is the time to read/write the sector.
 *
 *   To find the rotational latency, we first must figure out where the 
 *   disk head will be after the seek (if any).  We then figure out
 *   how long it will take to get to newSector after that point.
 */
int
Disk::ComputeLatency(int newSector)
{
    int newTrack = newSector / SectorsPerTrack;
    int oldTrack = lastSector / SectorsPerTrack;
    int seekTime = abs(newTrack - oldTrack);
    int afterSeek = (machine->getTimerTicks() + seekTime) % SectorsPerTrack;
    int rotationTime = ((newSector - afterSeek) + SectorsPerTrack) 
				% SectorsPerTrack;

    return (seekTime + rotationTime + 1);
}

static void
PrintSector (bool writing, int sector, char *data)
{
    int *p = (int *) data;

    if (writing)
        printf("Writing sector: %d\n", sector); 
    else
        printf("Reading sector: %d\n", sector); 
    for (int i = 0; i < (SectorSize/sizeof(int)); i++)
	printf("%x ", p[i]);
    printf("\n"); 
}

/* Emulate a disk read (or write) request -- do the read (write) immediately,
 * but don't tell caller until later some number of ticks later.
 */
void
Disk::ReadRequest(int sectorNumber, char* data)
{
    int ticks = ComputeLatency(sectorNumber);

    ASSERT(!active);
    ASSERT((sectorNumber >= 0) && (sectorNumber < NumSectors));
    
    DEBUG('d', "Reading from sector %d\n", sectorNumber);
    (void) lseek(fileno, SectorSize * sectorNumber + MagicSize, 0);/*SEEK_SET*/
    int bytesRead = read(fileno, data, SectorSize);
    ASSERT(bytesRead == SectorSize);
    if (DebugIsEnabled('d'))
	PrintSector(FALSE, sectorNumber, data);
    
    active = TRUE;
    
    lastSector = sectorNumber;
    
    machine->numDiskReads++;
    machine->ScheduleInterrupt(DiskInterrupt, (int) this, ticks);
}

void
Disk::WriteRequest(int sectorNumber, char* data)
{
    int ticks = ComputeLatency(sectorNumber);

    ASSERT(!active);
    ASSERT((sectorNumber >= 0) && (sectorNumber < NumSectors));
    
    DEBUG('d', "Writing to sector %d\n", sectorNumber);
    (void) lseek(fileno, SectorSize * sectorNumber + MagicSize, 0);
    int bytesWritten = write(fileno, data, SectorSize);
    ASSERT(bytesWritten == SectorSize);
    if (DebugIsEnabled('d'))
	PrintSector(TRUE, sectorNumber, data);
    
    active = TRUE;
    
    lastSector = sectorNumber;

    machine->numDiskWrites++;
    machine->ScheduleInterrupt(DiskInterrupt, (int) this, ticks);
}

/* Handle the interrupt, vectoring to whoever made the disk request */
void
Disk::HandleInterrupt ()
{ 
    active = FALSE;
    (*handler)(handlerArg);
}

