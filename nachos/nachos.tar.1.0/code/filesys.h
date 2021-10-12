/* filesys.h -- Class representing the file system.
 *
 * Defines routines to manage naming in the file system --
 *   creating, opening, and deleting named files.
 *
 * File read and file write are defined in the OpenFile class.
 */

#ifndef FS_H
#define FS_H

#include "openfile.h"

class FileSystem {
    
  public:
    /* Initialize the file system -- assumes "synchDisk" has been initialized.
     * If format is set, the disk is garbage, so initialize the directory
     * and the map of free blocks.
     */
    FileSystem(bool format);

    bool Create(char *name, int initialSize);  	// create a file

    OpenFile* Open(char *name); 	// open a file

    bool Remove(char *name);  		// delete a file

    void List();

    void Print();

  private:
   OpenFile* freeMapFile;
   OpenFile* directoryFile;
};

#endif
