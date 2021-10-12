/* list.cc -- Routines to manage a list of "things".
 *
 *  NOTE: Mutual exclusion must be provided by the caller.
 */

#include "list.h"

List::List()
{ 
    first = last = NULL; 
}

List::~List()
{ 
    while (Remove() != NULL);	 // delete all the elements on the list
}

// Append something
void
List::Append(void *item)
{
    ListElement *element = new ListElement(item);

    if (first == NULL) {
	first = element;
	last = element;
    } else {
	last->next = element;
	last = element;
    }
}

// Prepend something
void
List::Prepend(void *item)
{
    ListElement *element = new ListElement(item);

    if (first == NULL) {
	first = element;
	last = element;
    } else {
	element->next = first;
	first = element;
    }
}

// Remove from the front of the list, if anything is on the list.
void *
List::Remove()
{
    if (first == NULL) 
	return NULL;

    ListElement *element = first;
    void *thing = first->item;

    if (first == last) {
        first = NULL;
	last = NULL;
    } else {
        first = element->next;
    }
    delete element;
    return thing;
}

// Remove a specific item from the list
void *
List::RemoveThis(ListElement *element)
{
    ListElement *ptr;
    void *thing = element->item;

    if (first == element)
      return Remove();
    for (ptr = first; ptr != NULL; ptr = ptr->next)
       if (ptr->next == element) {
	  ptr->next = ptr->next->next;
	  if (last == element) 
	     last = ptr;
	  delete element;
	  return thing;
	  }
    return NULL;
}

// Apply func to every element in the list.  
void
List::Mapcar(VoidFunctionPtr func)
{
    for (ListElement *ptr = first; ptr != NULL; ptr = ptr->next)
       (*func)((int)ptr->item);
}

// Apply func to find if "thing" is on the list 
ListElement *
List::Find(BoolFunctionPtr *func, void *thing)
{
    for (ListElement *ptr = first; ptr != NULL; ptr = ptr->next)
       if ((*func)(ptr->item, thing)) 
	   return ptr;
    return NULL;
}

// Count up the number of elements
int
List::NumElements()
{
    int count = 0;

    for (ListElement *ptr = first; ptr != NULL; ptr = ptr->next)
	count++;		
    return count;
}

