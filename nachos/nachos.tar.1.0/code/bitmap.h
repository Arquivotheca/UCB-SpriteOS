// bitmap.h -- class for managing a bitmap.  Initialized with
//   the number of bits being managed -- must be a multiple of wordsize!

#ifndef BITMAP_H
#define BITMAP_H

#include "utility.h"
#include "openfile.h"

const bitsInByte = 8;
const bitsInWord = 32;

class BitMap {
  public:
    BitMap(int nitems);
    ~BitMap() { delete map; }
    
    void Mark(int which);   	// set bit
    void Clear(int which);  	// clear bit
    bool Test(int which);   	// is bit set?
    int Find();            	// return a clear bit, -1 if none
    int NumClear();		// return the number of clear bits

    void Print();		// print contents of bitmap
    
    void FetchFrom(OpenFile *file); 	// fetch contents from disk 
    void WriteBack(OpenFile *file); 	// write contents to disk

  private:
    int numBits;
    unsigned int *map;
};

#endif
