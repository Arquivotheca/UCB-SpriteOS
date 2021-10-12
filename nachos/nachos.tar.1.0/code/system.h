
// Misc useful stuff.  All global variables are collected here.

#ifndef SYSTEM_H
#define SYSTEM_H

extern class Machine* machine;

extern class Scheduler* scheduler;

#ifdef HW2
extern class SynchDisk   *synchDisk;
extern class FileSystem  *fileSystem;
#endif

#ifdef HW3
extern class SynchConsole* console;
extern class Memory* memory;
extern class MemoryManager *memoryManager;
extern class AddrSpace* space;

#define NumPhysicalPages  	1024
#define MemorySize  		(NumPhysicalPages * PageSize)
#endif

// You must call Initialize before doing anything.
extern void Initialize(int argc, char **argv);

#ifdef HW3
// Start a process running.   This really takes a char*, but since we
// generally use it as an argument to new Thread, we make it an int instead.
extern void StartProcess(int name);
#endif

#ifdef HW5
extern class Network* network;
extern class PostOffice* postOffice;
extern int netname;
#endif

// Machine::Halt calls this.
extern void Cleanup();
extern void Yield();

#endif
