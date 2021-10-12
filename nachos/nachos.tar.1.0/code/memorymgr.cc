
/* $Revision: 1.2 $ on $Date: 1991/12/20 07:50:34 $
 * $Source: /home/ygdrasil/a/faustus/cs162/hw1+/RCS/machine.cc,v $
 *
 * Stuff for the default memory manager.
 */

#include "memorymgr.h"

MemoryManager::MemoryManager()
{
    nextFree = 0;
}

void
MemoryManager::AllocateSpace(PTE* table, int tableSize)
{
#ifndef HW3_SOL
    if (nextFree) {
	printf("Can't allocate memory: there is already an addr space\n");
	exit(1);
    }
#endif
    
    for (int i = 0; i < tableSize; i++) {
	ASSERT(nextFree < NumPhysicalPages);	
	table[i].physicalFrame = nextFree++;
	table[i].valid = TRUE;
	table[i].use = table[i].dirty = FALSE;
    }
}

void
MemoryManager::DeallocateSpace(PTE* table, int tableSize)
{
#ifndef HW3_SOL
    nextFree = 0;
#endif
}
