// synchlist.cc -- make synchronized accesses to a list
//
// Implemented in "monitor"-style -- surround each procedure with a
// lock acquire and release pair, using condition signal and wait for
// synchronization.
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "synchlist.h"

SynchList::SynchList()
{
    list = new List(); 
    lock = new Lock("list lock"); 
    listEmpty = new Condition("list empty cond");
}

SynchList::~SynchList()
{ 
    delete list; 
    delete lock;
    delete listEmpty;
}

void
SynchList::Append(void *item)
{
    lock->Acquire();
    list->Append(item);
    listEmpty->Signal(lock);
    lock->Release();
}

// wait if nothing is on the list
void *
SynchList::Remove()
{
    void *item;

    lock->Acquire();
    while ((item = list->Remove()) == NULL)
	listEmpty->Wait(lock);
    lock->Release();
    ASSERT(item != NULL);
    return item;
}

void
SynchList::Mapcar(VoidFunctionPtr func)
{ 
    lock->Acquire(); 
    list->Mapcar(func);
    lock->Release(); 
}
