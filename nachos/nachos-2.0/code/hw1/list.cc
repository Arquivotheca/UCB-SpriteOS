// list.cc -- Routines to manage a list of "things".
// 
//  NOTE: Mutual exclusion must be provided by the caller (cf. synchlist.cc).
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "list.h"

// Initialize the list to empty
List::List()
{ 
    first = last = NULL; 
}

List::~List()
{ 
    while (Remove() != NULL);	 // delete all the elements on the list
}

// Append something to the list
void
List::Append(void *item)
{
    ListElement *element = new ListElement;

    element->item = item;
    element->next = NULL;
    if (first == NULL) {	// list is empty
	first = element;
	last = element;
    } else {			// else put it after last
	last->next = element;
	last = element;
    }
}

// Prepend something to the list
void
List::Prepend(void *item)
{
    ListElement *element = new ListElement;

    element->item = item;
    element->next = NULL;
    if (first == NULL) {	// list is empty
	first = element;
	last = element;
    } else {			// else put it before first
	element->next = first;
	first = element;
    }
}

// insert something at the appropriate place
void
List::SortedInsert(void *item, int sortKey)
{
    ListElement *element = new ListElement;
    ListElement *ptr;

    element->item = item;
    element->key = sortKey;
    element->next = NULL;
    if (first == NULL) {	// list is empty 
        first = element;
        last = element;
    } else if (sortKey < first->key) {	// item goes on front of list
	element->next = first;
	first = element;
    } else {		// look for first elt in list bigger than item
        for (ptr = first; ptr->next != NULL; ptr = ptr->next) {
            if (sortKey < ptr->next->key) {
		element->next = ptr->next;
	        ptr->next = element;
		return;
	    }
	}
	last->next = element;		// item goes at end of list
	last = element;
    }
}

// Remove from the front of the list, if anything is on the list.
void *
List::SortedRemove(int *keyPtr)
{
    ListElement *element = first;
    void *thing;

    if (first == NULL) 
	return NULL;

    thing = first->item;
    if (first == last) {	// list had one item, now has none 
        first = NULL;
	last = NULL;
    } else {
        first = element->next;
    }
    if (keyPtr != NULL)
        *keyPtr = element->key;
    delete element;
    return thing;
}

// Same as SortedRemove, but ignore the key
void *
List::Remove()
{
    return SortedRemove(NULL);
}

// Apply func to every element in the list.  
void
List::Mapcar(VoidFunctionPtr func)
{
    for (ListElement *ptr = first; ptr != NULL; ptr = ptr->next) {
       DEBUG('l', "In mapcar, about to invoke %x(%x)\n", func, ptr->item);
       (*func)((int)ptr->item);
    }
}
