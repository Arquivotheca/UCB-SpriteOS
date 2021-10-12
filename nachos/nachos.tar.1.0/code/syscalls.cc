
#ifdef HW3_SOL

/* This file contains the HandleSyscall function of Machine.
 *
 */

#include "machine.h"
#include "console.h"
#include "addrspace.h"
#include "scheduler.h"
#include "syscalls.h"

// Can't include the stupid system files.
#define O_RDWR          002             /* open for read & write */
#define O_CREAT         01000           /* open with file create */
#define O_FSYNC         0100000         /* syncronous write *0004*/
#define SEEK_SET	0

void
HandleSyscall(int arg)
{
    SyscallType type = machine->ReadRegister(2);
    int args[4];
    for (int i = 0; i < 4; i++) {
	args[i] = machine->ReadRegister(4 + i);
	// printf("arg %d is 0x%x\n", i, args[i]);
    }
    
    int retvalue = -1;
    switch (type) {
      case GetThreadIdSyscall:  // No args.
	{
	    retvalue = 0; // space->getThreadId();
	    break;
	}
	
      case OpenSyscall:         // r4: filename
	{
	    char name[64];
	    space->ReadString(args[0], name, 64);
	    retvalue = open(name, O_RDWR | O_CREAT | O_FSYNC, 0644);
	    if (retvalue == -1)
		perror(name);
	    break;
	}
	
      case ReadSyscall:		// r4: filedes, r5: buffer, r6: nbytes
	{
	    int nbytes = args[2];
	    if ((nbytes < 0) || (nbytes > 1024)) {
		fprintf(stderr, "Error: bad nbytes arg (%d) to read\n", nbytes);
		break;
	    }
	    char buffer[nbytes];
	    retvalue = read(args[0], buffer, nbytes);
	    if (retvalue == -1) {
		perror("read syscall");
		break;
	    }
	    space->WriteBuffer(args[1], buffer, nbytes);
	    break;
	}
	
      case WriteSyscall: 	// r4: filedes, r5: buffer, r6: nbytes
	{
	    int nbytes = args[2];
	    if ((nbytes < 0) || (nbytes > 1024)) {
		fprintf(stderr, "Error: bad nbytes arg (%d) to write\n",
			nbytes);
		break;
	    }
	    char buffer[nbytes];
	    space->ReadBuffer(args[1], buffer, nbytes);
	    retvalue = write(args[0], buffer, nbytes);
	    if (retvalue == -1)
		perror("write syscall");
	    break;
	}
	
      case SeekSyscall:		// r4: filedes, r5: position
	{
	    retvalue = lseek(args[0], args[1], SEEK_SET);
	    if (retvalue == -1)
		perror("seek syscall");
	    break;
	}
	
      case ConsoleReadSyscall:	  // r4: buffer, nbytes
	{
	    int nbytes = args[1];
	    if ((nbytes < 0) || (nbytes > 1024)) {
		fprintf(stderr, "Error: bad nbytes arg (%d) to console read\n",
			nbytes);
		break;
	    }
	    char buffer[nbytes];
	    console->ReadString(buffer, nbytes);
	    for (int j = 0; buffer[j]; j++)
		if (buffer[j] == '\n')
		    buffer[j] = '\0';
	    space->WriteString(args[0], buffer);
	    retvalue = strlen(buffer);
	    break;
	}
	
      case ConsoleWriteSyscall:	  // r4: buffer
	{
	    char buffer[100];
	    space->ReadBuffer(args[0], buffer, 100);
	    buffer[100] = 0;
	    console->WriteString(buffer);
	    retvalue = 1;
	    break;
	}
	
      case ExecSyscall:   // r4: pathname
	{
	    char buffer[100];
	    space->ReadString(args[0], buffer, 100);
	    char* s = new char[strlen(buffer)];
	    strcpy(s, buffer);
	    printf("Execing: %s\n", s);
	    (void) new Thread("foo", StartProcess, (int) s);
	    retvalue = 1;
	    break;
	}
	
      default:
	printf("bad syscall type %d\n", type);
	break;
    }
    
    machine->WriteRegister(2, retvalue);
}

#endif HW3_SOL
