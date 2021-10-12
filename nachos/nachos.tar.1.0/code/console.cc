/* console.cc -- emulation of a tty */

#include "console.h"
#include "scheduler.h"
#include "system.h"

extern "C" {
#include <sys/types.h>
#include <time.h>
#include <sys/socket.h>
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

// The human mind ordinarily operates at only ten percent of its capacity --
// the rest is overhead for the operating system.
//        - G. Weinberg

static void
ConsoleInputReady(int arg)
{
    DEBUG('c', "INTERRUPT: Input is ready for the console\n");
    ((SynchConsole *) arg)->inputReady->V();
}

SynchConsole::SynchConsole()
{
    lock = new Lock("console lock");
    inputReady = new Semaphore("input ready");
    machine->setInterruptHandler(ConsoleInterrupt, ConsoleInputReady);
}

SynchConsole::~SynchConsole()
{
    delete inputReady;
    delete lock;
}

void
SynchConsole::ReadString(char* data, int maxlength)
{
    lock->Acquire();
    DEBUG('c', "Thread %s got the console lock\n", currentThread->getName());
    while (!pollInput(0))
	inputReady->P();
    DEBUG('c', "Thread %s has input ready...\n", currentThread->getName());
    if (!fgets(data, maxlength, stdin)) {
	perror("input");
	exit(1);
    }
    DEBUG('c', "Thread %s read %d characters.\n", currentThread->getName(),
	  strlen(data));
    machine->numConsoleCharsRead += strlen(data);
    lock->Release();
}

void
SynchConsole::WriteString(char* data)
{
    DEBUG('c', "Thread %s wrote %d characters.\n", currentThread->getName(),
	  strlen(data));
    fputs(data, stdout); fflush(stdout);
    machine->numConsoleCharsWritten += strlen(data);
}

void
SynchConsole::CheckActive()
{
    if (pollInput(0))  // 0 is the standard input file descriptor
	machine->ScheduleInterrupt(ConsoleInterrupt, (int) this, 1);
}

bool
SynchConsole::CheckInput()
{
    return (pollInput(0));
}
