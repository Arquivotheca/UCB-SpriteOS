/* console.h -- emulate a terminal */

#ifndef CONSOLE_H
#define CONSOLE_H

#include "thread.h"
#include "synch.h"

class SynchConsole {
  public:
    SynchConsole();
    ~SynchConsole();
    
    void ReadString(char* data, int maxlength);
    void WriteString(char* data);

    bool CheckInput();
    
    // This should be called only by the machine code.
    void CheckActive();
    
    friend void ConsoleInputReady(int arg);
    
  private:
    Semaphore* inputReady;
    Lock *lock;
};

#endif
