// list.h -- routines to manage LISP-like lists

#ifndef LIST_H
#define LIST_H

#include "utility.h"

class ListElement {
  public:
   ListElement(void *data) { next = NULL; item = data; }

   ListElement *next;
   void *item;
};

class List {
  public:
    List();
    ~List();

    void Prepend(void *item); 	// Put item at the beginning of the list
    void Append(void *item); 	// Put item at the end of the list

    void *Remove(); 		// Take item off the front of the list
    void *RemoveThis(ListElement *element); // Take specific item off list

    void Mapcar(VoidFunctionPtr func);	// Lisp-like mapcar

    				// look for "thing" on the list
    ListElement *Find(BoolFunctionPtr *func, void *thing);  

    int NumElements();		// return the number of things in the list

    ListElement *getFirst() { return first; }

  private:
    ListElement* first;  	// Head of the list
    ListElement* last;
};

#endif
