/* openfile.h -- class for managing a table of open file descriptors.
 *
 * Defines UNIX-like file operations of open, lseek, read, write, and close.
 *   (cf. type 'man open' to UNIX prompt).
 */

#ifndef OPENFILE_H
#define OPENFILE_H

class FileHeader;

class OpenFile {
  public:
    OpenFile(int sector);		// open the file
    ~OpenFile();			// close the file

    // set the position to start reading/writing -- like UNIX lseek
    void Seek(int position) {seekPosition = position;}	

    // Read/write bytes from the file; return the # actually read/written.
    int Read(char *into, int numBytes);
    int Write(char *from, int numBytes);

    // Bypass call to Seek
    int ReadAt(char *into, int numBytes, int position);
    int WriteAt(char *into, int numBytes, int position);

  private:
    FileHeader *hdr;
    int seekPosition;
};

#endif
