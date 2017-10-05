#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static void user_mem_read(void *dest, void *uaddr, size_t size);


static void sys_halt (void);
static void sys_exit (int status);

// static pid_t sys_exec (const char *file);
// static int sys_wait (pid_t);
// static bool sys_create (const char *file, unsigned initial_size);
// static bool sys_remove (const char *file);
// static int sys_open (const char *file);
// static int sys_filesize (int fd);
// static int sys_read (int fd, void *buffer, unsigned length);
static int sys_write (int fd, const void *buffer, unsigned size);
// static void sys_seek (int fd, unsigned position);
// static unsigned sys_tell (int fd);
// static void sys_close (int fd);

void syscall_init (void) {
   intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f UNUSED) {
   void *sp = f->esp; // copy of stack pointer, don't want to modify the pointer
   // extract syscall number
   int syscall_num;
   user_mem_read(&syscall_num, sp, sizeof(syscall_num));
   sp = (int *) sp + 1;

   switch (syscall_num) {
      case SYS_HALT: {
         sys_halt();
         break;
      }
      case SYS_EXIT: {
         int status;
         user_mem_read(&status, sp, sizeof(status));
         sys_exit(status);
         break;
      }

      case SYS_EXEC: {

      }
      case SYS_WAIT: {

      }
      case SYS_CREATE: {

      }
      case SYS_REMOVE: {

      }
      case SYS_OPEN: {

      }
      case SYS_FILESIZE: {

      }
      case SYS_READ: {

      }

      case SYS_WRITE: {
         int fd;
         user_mem_read(&fd, sp, sizeof(fd));
         sp = (int *) sp + 1;
         void *buffer;
         user_mem_read(&buffer, sp, sizeof(buffer));
         sp = (void **) sp + 1;
         unsigned size;
         user_mem_read(&size, sp, sizeof(size));
         sp = (unsigned *) sp + 1;
         sys_write(fd, buffer, size);
      }
      case SYS_SEEK: {

      }
      case SYS_TELL: {

      }
      case SYS_CLOSE: {

      }

   }

}

static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *dest, uint8_t byte);
static void invalid_user_access(void);


/*
As part of a system call, the kernel must often access memory through pointers provided by a user program. The kernel must be very careful about doing so, because the user can pass a null pointer, a pointer to unmapped virtual memory, or a pointer to kernel virtual address space (above PHYS_BASE).
*/
static void user_mem_read(void *dest, void *uaddr, size_t size) {
   // uaddr must be below PHYS_BASE and must not be NULL pointer
   if (uaddr == NULL || !is_user_vaddr(uaddr))
      invalid_user_access();
   for (unsigned int i=0; i<size; i++) {
      int val = get_user(uaddr + i);
      if (val == -1)
         invalid_user_access();
      *(uint8_t *)(dest + i) = val & 0xFF;
   }
}


/*
Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
UADDR points to a byte of memory
Returns the byte value if successful, -1 if a segfault
occurred.
*/
static int get_user (const uint8_t *uaddr) {
   int result;
   asm ("movl $1f, %0; movzbl %1, %0; 1:"
   : "=&a" (result) : "m" (*uaddr));
   return result;
}

/*
Writes BYTE to user address DEST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred.
*/
static bool put_user (uint8_t *dest, uint8_t byte) {
   int error_code;
   asm ("movl $1f, %0; movb %b2, %1; 1:"
   : "=&a" (error_code), "=m" (*dest) : "q" (byte));
   return error_code != -1;
}

/*
for now just exits with status -1
how to free memory and release lock?
*/
static void invalid_user_access() {
   sys_exit(-1);
}

/************************ System Call Implementation *************************/
/*
 * System Call: void halt (void)
 * Description: terminates Pintos by calling shutdown_power_off() 
 *     (declared in threads/init.h). This should be seldom used, 
 *     because you lose some information about possible deadlock 
 *     situations, etc.
 */
static void sys_halt (void) {
   shutdown_power_off();
}

/*
 * System Call: void exit (int status)
 * Discription: terminates the current user program, returning status 
 *     to the kernel. If the process's parent waits for it (see below), 
 *     this is the status that will be returned. Conventionally, a status 
 *     of 0 indicates success and nonzero values indicate errors.
 */
static void sys_exit (int status) {
   struct thread *cur = thread_current();
   printf("%s: exit(%d)\n", cur->name, status);
   cur->pcb->exit_status = status;
   thread_exit();
}

/*
 * System Call: pid_t exec (const char* command)
 *     - Parameters:
 *         - command: command to be excuted, followed by its arguments.
 *     - Return: the new process's program id (pid). Must return pid -1,
 *           which otherwise should not be a valid pid, if the program
 *           cannot load or run for any reason.
 * Discription: runs the executable whose name is given in command, passing
 *     any given arguments. The parent process cannot return from the exec
 *     until it knows whether the child process successfully loaded its executable.
 *     You must use appropriate synchronization to ensure this.
 */
static pid_t sys_exec (const char *file) {

}

/*
 * System Call: int wait(pid_t pid)
 *     - Parameters:
 *         - pid: the pid of a child process to be waiting for.
 *     - Return: the status that pid passed to exit. If pid did not call exit(),
 *           but was terminated by the kernel (e.g. killed due to an exception), wait(pid)
 *           must return -1.
 * Description: Waits for a child process pid and retrieves the child's exit status.
 */
int wait (pid_t) {

}

/*
 * System Call: bool create (const char* file, unsigned initial_size)
 *     - Parameters:         
 *         - file: file name for the file to be created.
 *         - initial_size: size in bytes for the file to be created.
 *     - Return: true if successful, false otherwise.
 * Description: creates a new file called file initially initial_size bytes in size.
 */
bool create (const char *file, unsigned initial_size) {

}

/*
 * System Call: bool remove (const char* file)
 *     - Parameters:
 *         - file: file name of the file to be deleted.
 *     - Return: true if successful, false otherwise.
 * Description: deletes the file called file. A file may be removed regardless of whether
 *     it is open or closed, and removing an open file does not close it.
 */
bool remove (const char *file) {

}

/*
 * System Call: int open (const char *file)
 *     - Parameters:
 *         - file:
 *     - Return: a nonnegative integer handle called a "file descriptor" (fd),
 *           or -1 if the file could not be opened.
 * Description: opens the file called file.
 */
int open (const char *file) {

}

/*
 * System Call: int filesize (int fd)
 *     - Parameters:
 *         - fd: file descriptor of the file to be checked.
 *     - Return: size of file in bytes.
 * Description: returns the size, in bytes, of the file open as fd.
 */
int filesize (int fd) {

}

/*
 * System Call: int read (int fd, void *buffer, unsigned size)
 *     - Parameters:
 *         - fd: file descriptor to the file to be read.
 *         - buffer: buffer for data read.
 *         - size: size, in bytes, to be read.
 *     - Return: the number of bytes actually read (0 at end of file), or -1 if
 *           the file could not be read (due to a condition other than end of file).
 * Description: reads size bytes from the file open as fd into buffer. Fd 0 reads from
 *     the keyboard using input_getc().
 */
int read (int fd, void *buffer, unsigned length) {

}

/*
 * System Call: int write (int fd, const void* buffer, unsigned size)
 *     - Parameters:
 *         - fd: file descriptor for the file to be writen.
 *         - buffer: buffer for data to be writen.
 *         - size: size of data to be written.
 *     - Return: the number of bytes actually written, which may be less than size
 *           if some bytes could not be written.
 * Description: writes size bytes from buffer to the open file fd. Fd 1 writes to the console.
 *     Your code to write to the console should write all of buffer in one call to putbuf(),
 *     at least as long as size is not bigger than a few hundred bytes.
 */
int sys_write (int fd, const void *buffer, unsigned size) {
   // writing to the console
   if (fd == 1)
   {
      putbuf (buffer, size);
      return size;
   }
}

/*
 * System Call: void seek (int fd, unsigned position)
 *     - Parameters:
 *         - fd: file descriptor of file to be read or written.
 *         - position:
 * Description: changes the next byte to be read or written in open file fd to position, expressed
 *     in bytes from the beginning of the file. (Thus, a position of 0 is the file's start.)
 */
void seek (int fd, unsigned position) {

}

/*
 * System Call: unsigned tell (int fd)
 *     - Parameters:
 *         - fd: file descriptor of file to be read or written.
 *     - Return: position of the next byte to be read or written.
 * Description: returns the position of the next byte to be read or written in open file fd, expressed
 *     in bytes from the beginning of the file.
 */
unsigned tell (int fd) {
  
}

/*
 * System Call: void close (int fd)
 *     - Parameters: file descriptor to be closed.
 * Description: closes file descriptor fd.
 */
void close (int fd) {
   
}

