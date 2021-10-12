
/* $Revision: 1.2 $ on $Date: 1991/12/20 07:50:34 $
 * $Source: /home/ygdrasil/a/faustus/cs162/hw1+/RCS/machine.h,v $
 *
 * The memory manager class.
 */

#ifndef MEMORYMGR_H
#define MEMORYMGR_H

#include "addrspace.h"

class MemoryManager {
  public:
    MemoryManager();
    
    // This routine allocates space in the Machine and fills in the PTE's
    // handed to it with the appropriate stuff.
    void AllocateSpace(PTE* table, int tableSize);
    
    // This frees all the pages given and marks the PTE's invalid.  Should be
    // done when a process terminates.
    void DeallocateSpace(PTE* table, int tableSize);

  private:
    int nextFree;
};

#endif
