
#include "system.h"
#include "utility.h"
#include "thread.h"
#include "synch.h"
#include "machine.h"
#include "scheduler.h"
#include <signal.h>

#ifdef HW2
#include "synchdisk.h"
#include "filesys.h"
#endif

#ifdef HW3
#include "console.h"
#include "memorymgr.h"
#include "memory.h"
#endif
#ifdef HW3_SOL
#include "syscalls.h"
#endif
#ifdef HW5
#include "network.h"
#include "post.h"
#endif


Machine     *machine;
Scheduler   *scheduler;

#ifdef HW2
SynchDisk   *synchDisk;
FileSystem  *fileSystem;
#endif

#ifdef HW3
SynchConsole* console;
Memory* memory;
MemoryManager* memoryManager;
AddrSpace* space;
#endif

#ifdef HW3_SOL
#define TimesliceTicks	100     /* Amount of time between context switches. */

void
timerHandler(int arg)
{
    machine->YieldAfter();
    machine->ScheduleInterrupt(TimerInterrupt, 0, TimesliceTicks);
}
#endif

#ifdef HW5
Network* network;
PostOffice* postOffice;
int netname = 0;
#endif

void
Initialize(int argc, char **argv)
{
    char* debugArgs = "";
#ifdef HW2
    bool format = FALSE;
#endif
#ifdef HW5
    double rely = 1;
#endif
    
    /* Parse the arguments. */
    argc--; argv++;
    while (argc > 0) {
	if (!strcmp(*argv, "-d")) {
	    argc--; argv++;
	    debugArgs = *argv;
#ifdef HW2
	} else if (!strcmp(*argv, "-f")) {
	    format = TRUE;
#endif
#ifdef HW5
	} else if (!strcmp(*argv, "-n")) {
	    netname = atoi(*++argv);
	    argc--;
	} else if (!strcmp(*argv, "-nr")) {
	    rely = atof(*++argv);
	    argc--;
#endif
	} else {  /* ignore */
	}
	argc--; argv++;
    }

    DebugInit(debugArgs);
    machine = new Machine();
    scheduler = new Scheduler();
    ThreadSystemInit();

    machine->InterruptEnable();
    signal(SIGINT, Cleanup);
    
#ifdef HW2
    synchDisk = new SynchDisk("DISK");
    fileSystem = new FileSystem(format);
#endif

#ifdef HW3
    console = new SynchConsole();
    memory = new Memory(MemorySize);
    memoryManager = new MemoryManager();
#endif
    
#ifdef HW3_SOL
    machine->setExceptionHandler(SyscallException, HandleSyscall, 0);
    
    // Now, it's time to do preemptive scheduling.
    machine->setInterruptHandler(TimerInterrupt, timerHandler);
    machine->ScheduleInterrupt(TimerInterrupt, 0, TimesliceTicks);
#endif
    
#if HW5
    if (netname >= 0) {
	network = new Network(netname, rely);
	postOffice = new PostOffice(10);
    } else
	network = NULL;
#endif
}

#ifdef HW3
void
StartProcess(int name)
{
    char* filename = (char *) name;
    AddrSpace* as = new AddrSpace(filename, NULL);    
    as->PrintInfo();
    currentThread->Yield();  // Just to make things a bit more interesting.
    machine->Run();
    delete as;
}
#endif

void
Cleanup()
{
    printf("leaning up...\n");  // If you type ^C then the C is already there.
#ifdef HW5
    if (network) {
	delete postOffice;
	delete network;
    }
#endif
    
#ifdef HW3
    delete memoryManager;
    delete memory;
    delete console;
#endif

#ifdef HW2
    delete fileSystem;
    delete synchDisk;
#endif
    
    delete scheduler;
    delete machine;
    
    exit(0);
}

void
Yield()
{
    currentThread->Yield();
}
