// bitmap.c -- routines to manage a bitmap 
//
// Copyright (c) 1992 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "bitmap.h"

// initialize bitmap to all bits clear
BitMap::BitMap(int nitems) 
{ 
    ASSERT((nitems % BitsInWord) == 0);
    numBits = nitems;
    map = new unsigned int[numBits / BitsInWord];
    for (int i = 0; i < numBits; i++) 
        Clear(i);
}

// Set specific bit
void
BitMap::Mark(int which) 
{ 
    ASSERT(which >= 0 && which < numBits);
    map[which / BitsInWord] |= 1 << (which % BitsInWord);
}
    
// Clear specific bit
void 
BitMap::Clear(int which) 
{
    ASSERT(which >= 0 && which < numBits);
    map[which / BitsInWord] &= ~(1 << (which % BitsInWord));
}

// Is specific bit set or clear?
bool 
BitMap::Test(int which)
{
    ASSERT(which >= 0 && which < numBits);
    
    if (map[which / BitsInWord] & (1 << (which % BitsInWord)))
	return TRUE;
    else
	return FALSE;
}

// Find a clear bit and mark it as in use.  Return -1 if none.
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

// Count up the number of clear bits
int 
BitMap::NumClear() 
{
    int count = 0;

    for (int i = 0; i < numBits; i++)
	if (!Test(i)) count++;
    return count;
}

// Print all the set bits
void
BitMap::Print() 
{
    printf("Bitmap set:\n"); 
    for (int i = 0; i < numBits; i++)
	if (Test(i))
	    printf("%d, ", i);
    printf("\n"); 
}

// fill in bit map from disk
void
BitMap::FetchFrom(OpenFile *file) 
{
    file->ReadAt((char *)map, numBits/BitsInByte, 0);
}

// write bitmap back to disk
void
BitMap::WriteBack(OpenFile *file)
{
   file->WriteAt((char *)map, numBits/BitsInByte, 0);
}

