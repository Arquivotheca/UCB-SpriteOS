
/* This is the file that defines the system calls that are available.
 * Here are a few things you must note.
 * (1) All of the arguments MUST be variables.  Rather than exec("foo", ret),
 *     write char* foo = "foo"; exec(foo, ret);.  Failure to do this will cause
 *     the simulator to choke.
 * (2) The last argument is always the return code.  It is 0 if things go
 *     well, -1 if they don't, or a useful number like the file descriptor
 *     in open.
 * (3) The strings passed to open and exec are NULL-terminated, but the
 *     buffers passed to read and write are not.  ConsoleWrite is also
 *     NULL-terminated.
 */

#define getpid(ret) \
    asm("addiu $2, $0, 0x1"); \
    asm("syscall"); \
    asm("sw $2, %0" : "=g" (ret));

#define open(str, ret) \
    asm("lw $4, %0" : /* */ : "g" (str)); \
    asm("addiu $2, $0, 0x2"); \
    asm("syscall"); \
    asm("sw $2, %0" : "=g" (ret));

#define read(fd, buf, len, ret) \
    asm("lw $4, %0" : /* */ : "g" (fd)); \
    asm("lw $5, %0" : /* */ : "g" (buf)); \
    asm("lw $6, %0" : /* */ : "g" (len)); \
    asm("addiu $2, $0, 0x3"); \
    asm("syscall"); \
    asm("sw $2, %0" : "=g" (ret));

#define write(fd, buf, len, ret) \
    asm("lw $4, %0" : /* */ : "g" (fd)); \
    asm("lw $5, %0" : /* */ : "g" (buf)); \
    asm("lw $6, %0" : /* */ : "g" (len)); \
    asm("addiu $2, $0, 0x4"); \
    asm("syscall"); \
    asm("sw $2, %0" : "=g" (ret));

#define seek(fd, pos, ret) \
    asm("lw $4, %0" : /* */ : "g" (fd)); \
    asm("lw $5, %0" : /* */ : "g" (pos)); \
    asm("addiu $2, $0, 0x5"); \
    asm("syscall"); \
    asm("sw $2, %0" : "=g" (ret));

#define consoleRead(str, len, ret) \
    asm("lw $4, %0" : /* */ : "g" (str)); \
    asm("lw $5, %0" : /* */ : "g" (len)); \
    asm("addiu $2, $0, 0x6"); \
    asm("syscall"); \
    asm("sw $2, %0" : "=g" (ret));

#define consoleWrite(str, ret) \
    asm("lw $4, %0" : /* */ : "g" (str)); \
    asm("addiu $2, $0, 0x7"); \
    asm("syscall"); \
    asm("sw $2, %0" : "=g" (ret));

#define exec(str, ret) \
    asm("lw $4, %0" : /* */ : "g" (str)); \
    asm("addiu $2, $0, 0x8"); \
    asm("syscall"); \
    asm("sw $2, %0" : "=g" (ret));
