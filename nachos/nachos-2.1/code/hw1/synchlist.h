// synchlist.h -- synchronize accesses to a list
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef SYNCHLIST_H
#define SYNCHLIST_H

#include "utility.h"
#include "list.h"
#include "synch.h"

// these functions are the same as for List, except for synchronization
class SynchList {
  public:
    SynchList();
    ~SynchList();

    void Append(void *item);	// and wake up any one who is waiting
    void *Remove();		// and sleep if list is empty
    void Mapcar(VoidFunctionPtr func);

  private:
    List *list;
    Lock *lock;
    Condition *listEmpty;
};

#endif
