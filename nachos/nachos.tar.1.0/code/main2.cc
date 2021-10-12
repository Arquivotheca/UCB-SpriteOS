/* main.cc -- entry point into the operating system */

#include "utility.h"
#include "thread.h"
#include "synch.h"
#include "machine.h"
#include "scheduler.h"
#include "system.h"

#ifdef HW2
#include "filesys.h"
#endif

const int transferSize = 10; 	// make it small, just to be difficult
static char buf[transferSize];

static void
Copy(char *from, char *to)
{
    FILE *fp;
    OpenFile* openFile;
    int amountRead, fileLength;

    if ((fp = fopen(from, "r")) == NULL) {
	printf("Copy: couldn't open input file %s\n", from);
	return;
    }

    fseek(fp, 0, 2);
    fileLength = ftell(fp);
    fseek(fp, 0, 0);

    if (!fileSystem->Create(to, fileLength)) {
	printf("Copy: couldn't create output file %s\n", to);
	return;
    }
    
    openFile = fileSystem->Open(to);
    ASSERT(openFile != NULL);
    
    while ((amountRead = fread(buf, sizeof(char), transferSize, fp)) > 0)
	openFile->Write(buf, amountRead);

    delete openFile;
    return;
}

static void
Print(char *name)
{
    OpenFile *openFile;    
    int amountRead;

    if ((openFile = fileSystem->Open(name)) == NULL) {
	printf("Print: unable to open file %s\n", name);
	return;
    }
    
    while ((amountRead = openFile->Read(buf, transferSize)) > 0)
	for (int i = 0; i < amountRead; i++)
	    printf("%c", buf[i]);

    delete openFile;
    return;
}

/* The main routine. */
/* usage: nachos [-f -d debugflags] (-f causes the disk to be re-formated) 
 *
 * The test driver uses a bunch more flags.
 */
int
main (int argc, char **argv)
{
    (void) Initialize(argc, argv);
    
    /* Parse the arguments for the test driver. */
    argc--; argv++;
    while (argc > 0) {
	if (!strcmp(*argv, "-c")) { 		/* from UNIX to Nachos */
	    ASSERT(argc > 2);
	    Copy(*(argv + 1), *(argv + 2));
	    argc -= 2; argv -= 2;

	} else if (!strcmp(*argv, "-p")) {	/* read a file */
	    ASSERT(argc > 1);
	    Print(*(argv + 1));
	    argc--; argv++;
	
	} else if (!strcmp(*argv, "-r")) {	/* remove file */
	    ASSERT(argc > 1);
	    fileSystem->Remove(*(argv + 1));
	    argc--; argv++;
	
	} else if (!strcmp(*argv, "-l")) {	/* list files */
            fileSystem->List();
	
	} else if (!strcmp(*argv, "-q")) {	/* print filesystem */
            fileSystem->Print();
	
	} else {  /* ignore */
	}
	argc--; argv++;
    }
    
    currentThread->Finish();
    
    /* Not reached... */
    return(0);
}
