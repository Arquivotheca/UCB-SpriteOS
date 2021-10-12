/* bitmap.c -- routines to manage a bitmap */

#include "bitmap.h"

BitMap::BitMap(int nitems) 
{ 
    ASSERT((nitems % bitsInWord) == 0);
    numBits = nitems;
    map = new unsigned int[numBits / bitsInWord];
    for (int i = 0; i < numBits; i++) 
        Clear(i);
}

void
BitMap::Mark(int which) 
{ 
    ASSERT(which >= 0 && which < numBits);
    map[which / bitsInWord] |= 1 << (which % bitsInWord);
}
    
void 
BitMap::Clear(int which) 
{
    ASSERT(which >= 0 && which < numBits);
    map[which / bitsInWord] &= ~(1 << (which % bitsInWord));
}

bool 
BitMap::Test(int which)
{
    ASSERT(which >= 0 && which < numBits);
    
    if (map[which / bitsInWord] & (1 << (which % bitsInWord)))
	return TRUE;
    else
	return FALSE;
}

/* Find a clear bit and mark it as in use.  Return -1 if none. */
int 
BitMap::Find() 
{
    for (int i = 0; i < numBits; i++)
	if (!Test(i)) {
	    Mark(i);
	    return i;
	}
    return -1;
}

/* Count up the number of clear bits */
int 
BitMap::NumClear() 
{
    int count = 0;

    for (int i = 0; i < numBits; i++)
	if (!Test(i)) count++;
    return count;
}

void
BitMap::Print() 
{
    printf("Bitmap set:\n"); 
    for (int i = 0; i < numBits; i++)
	if (Test(i))
	    printf("%d, ", i);
    printf("\n"); 
}

/* fetch contents of bit map from disk */
void
BitMap::FetchFrom(OpenFile *file) 
{
    file->ReadAt((char *)map, numBits/bitsInByte, 0);
}

/* write bitmap back to disk */
void
BitMap::WriteBack(OpenFile *file)
{
   file->WriteAt((char *)map, numBits/bitsInByte, 0);
}

