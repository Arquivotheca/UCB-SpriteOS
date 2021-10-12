// list.h -- routines to manage LISP-like lists
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef LIST_H
#define LIST_H

#include "utility.h"

typedef struct listElt {
   struct listElt *next;
   int key;
   void *item;
} ListElement;

class List {
  public:
    List();
    ~List();

    void Prepend(void *item); 	// Put item at the beginning of the list
    void Append(void *item); 	// Put item at the end of the list
    void *Remove(); 	 	// Take item off the front of the list

    void Mapcar(VoidFunctionPtr func);	// Lisp-like mapcar

    // Routines to keep list in order (sorted by key)
    void SortedInsert(void *item, int sortKey);	// Put item into list
    void *SortedRemove(int *keyPtr); 	  	// Take first item off list

    bool IsEmpty() { return (first == NULL) ? TRUE : FALSE; }

  private:
    ListElement *first;  	// Head of the list
    ListElement *last;
};

#endif
