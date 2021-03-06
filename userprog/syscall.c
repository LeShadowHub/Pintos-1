/*
 * Modified by:
 * Matthew Tawil (mt33924)
 * Allen Pan (xp572)
 * Ze Lyu (zl5298)
 */

#include "userprog/syscall.h"
#include <stdint.h>
#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include <stdlib.h>
#include "threads/malloc.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "lib/user/syscall.h"

static void syscall_handler(struct intr_frame *);

/************************ System Calls ************************/
static void sys_halt(void);
static void sys_exit(int status);
static pid_t sys_exec(const char* cmdline);
static int sys_wait(pid_t);
static bool sys_create(const char* file, unsigned initial_size);
static bool sys_remove(const char* file);
static int sys_open(const char* file);
static int sys_filesize(int fd);
static int sys_read(int fd, void* buffer, unsigned size);
static int sys_write(int fd, const void* buffer, unsigned size);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_close(int fd);

/* Added Syscalls for Subdirectories*/
static bool sys_chdir(const char *file);
static bool sys_mkdir(const char *file);
static bool sys_readdir(int fd, const char *file);
static bool sys_isdir(int fd);
static int sys_inumber(int fd);

/************************ Memory Access Functions ************************/
static void user_mem_read(void* dest_addr, void* uaddr, size_t size);
static int user_mem_read_byte(const uint8_t *uaddr);
static bool user_mem_write_byte(uint8_t *dest, uint8_t byte);
static void invalid_user_access(void);
static void verify_string(const uint8_t *ptr);
static void verify_dest(void *dest, unsigned size);
/************************ File Table Helper Functions ************************/
static struct file_table_entry* get_file_table_entry_by_fd(int fd);
static int add_to_file_table (struct file_table_entry *fte);

static struct lock lock_filesys;

/*
 * void syscall_init (void)
 * Description: system call initialization.
 */
void syscall_init(void) {
    lock_init(&lock_filesys);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/*
 * void syscall_handler (struct intr_frame* f UNUSED)
 *     - Parameters:
 *         - f: interrupt stack frame pointer.
 * Description:
 */
static void syscall_handler(struct intr_frame* f UNUSED) {
    // get syscall number
    uint32_t syscall_num;
    user_mem_read(&syscall_num, f->esp, sizeof (syscall_num));

    // excute syscall according to syscall number
    switch (syscall_num) {
            /* Halt the operating system. */
        case SYS_HALT:
        {
            sys_halt();
            break;
        }
            /* Terminate this process. */
        case SYS_EXIT:
        {
            // get status from stack
            int status;
            user_mem_read(&status, f->esp + 4, sizeof (status));
            sys_exit(status);
            break;
        }
            /* Start another process. */
        case SYS_EXEC:
        {
            char *cmdline;
            user_mem_read(&cmdline, f->esp + 4, sizeof (cmdline));
            // syscall, return pid_t
            f->eax = sys_exec(cmdline);
            break;
        }

            /* Wait for a child process to die. */
        case SYS_WAIT:
        {
            pid_t pid;
            user_mem_read(&pid, f->esp + 4, sizeof (pid));
            f->eax = sys_wait(pid);
            break;
        }

            /* Create a file. */
        case SYS_CREATE:
        {
            const char* file;
            unsigned initial_size;
            user_mem_read(&file, f->esp + 4, sizeof (file));
            user_mem_read(&initial_size, f->esp + 8, sizeof (initial_size));
            f->eax = sys_create(file, initial_size);
            break;
        }

            /* Delete a file. */
        case SYS_REMOVE:
        {
            const char* file;
            user_mem_read(&file, f->esp + 4, sizeof (file));
            f->eax = sys_remove(file);
            break;
        }

            /* Open a file. */
        case SYS_OPEN:
        {
            const char* file;
            user_mem_read(&file, f->esp + 4, sizeof (file));
            f->eax = sys_open(file);
            break;
        }
            /* Obtain a file's size. */
        case SYS_FILESIZE:
        {
            int fd;
            user_mem_read(&fd, f->esp + 4, sizeof (fd));
            // syscall, return int
            f->eax = sys_filesize(fd);
            break;
        }
            /* Read from a file. */
        case SYS_READ:
        {
            int fd;
            user_mem_read(&fd, f->esp + 4, sizeof (fd));
            // get buffer address
            void *buffer;
            user_mem_read(&buffer, f->esp + 8, sizeof (buffer));
            // get size
            unsigned size;
            user_mem_read(&size, f->esp + 12, sizeof (size));
            f->eax = sys_read(fd, buffer, size);
            break;
        }
            /* Write to a file. */
        case SYS_WRITE:
        {
            // get fd
            int fd;
            user_mem_read(&fd, f->esp + 4, sizeof (fd));
            // get buffer address
            void *buffer;
            user_mem_read(&buffer, f->esp + 8, sizeof (buffer));
            // get size
            unsigned size;
            user_mem_read(&size, f->esp + 12, sizeof (size));
            // syscall, return int
            f->eax = sys_write(fd, buffer, size);
            break;
        }

            /* Change position in a file. */
        case SYS_SEEK:
        {
            int fd;
            user_mem_read(&fd, f->esp + 4, sizeof (fd));
            unsigned position;
            user_mem_read(&position, f->esp + 8, sizeof (position));

            sys_seek(fd, position);
            break;
        }

            /* Report current position in a file. */
        case SYS_TELL:
        {

            int fd;
            user_mem_read(&fd, f->esp + 4, sizeof (fd));
            f->eax = sys_tell(fd);
            break;
        }

            /* Close a file. */
        case SYS_CLOSE:
        {
            int fd;
            user_mem_read(&fd, f->esp + 4, sizeof (fd));
            //syscall, no return
            sys_close(fd);
            break;
        }

        /* Change the current directory. */
        case SYS_CHDIR:
        {
           const char* dir;
           user_mem_read(&dir, f->esp + 4, sizeof (dir));
           f->eax = sys_chdir(dir);
           break;
        }
        /* Create a directory. */
        case SYS_MKDIR:
        {
           const char* dir;
           user_mem_read(&dir, f->esp + 4, sizeof (dir));
           f->eax = sys_mkdir(dir);
           break;
        }
        /* Reads a directory entry. */
        case SYS_READDIR:
        {
           int fd;
           char *name;
           user_mem_read(&fd, f->esp + 4, sizeof (fd));
           user_mem_read(&name, f->esp + 8, sizeof (name));
           f->eax = sys_readdir(fd,name);
           break;
        }
        /* Tests if a fd represents a directory. */
        case SYS_ISDIR:
        {
           int fd;
           user_mem_read(&fd, f->esp + 4, sizeof (fd));
           f->eax = sys_isdir(fd);
           break;
        }
        /* Returns the inode number for a fd. */
        case SYS_INUMBER:
        {
           int fd;
           user_mem_read(&fd, f->esp + 4, sizeof (fd));
           f->eax = sys_inumber(fd);
           break;
        }
     }

  }

/************************ System Call Implementation ************************/

/*
 * void sys_halt (void)
 * Description: terminates Pintos by calling shutdown_power_off()
 *     (declared in threads/init.h). This should be seldom used,
 *     because you lose some information about possible deadlock
 *     situations, etc.
 */
void sys_halt(void) {
    shutdown_power_off();
}

/*
 * void sys_exit (int status)
 * Discription: terminates the current user program, returning status
 *     to the kernel. If the process's parent waits for it (see below),
 *     this is the status that will be returned. Conventionally, a status
 *     of 0 indicates success and nonzero values indicate errors.
 */
void sys_exit(int status) {
    struct thread *cur = thread_current();
    printf("%s: exit(%d)\n", cur->name, status);
    // assign status
    cur->pcb->exit_status = status;
    thread_exit();
}

/*
 * pid_t sys_exec (const char* cmdline)
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
pid_t sys_exec(const char* cmdline) {
    /* check whether cmdline is a valid address
       since when we get cmdline, we check the pointer whose content is this
       cmdline pointer, need to check whether the actual cmdline ptr is valid
       will use user_mem_read here since it fullfills the need
     */
    verify_string(cmdline);

    lock_acquire(&lock_filesys); // in load(), file system is used
    pid_t pid = process_execute(cmdline);
    lock_release(&lock_filesys);

    return pid;
}

/*
 * int sys_wait(pid_t pid)
 *     - Parameters:
 *         - pid: the pid of a child process to be waiting for.
 *     - Return: the status that pid passed to exit. If pid did not call exit(),
 *           but was terminated by the kernel (e.g. killed due to an exception), wait(pid)
 *           must return -1.
 * Description: Waits for a child process pid and retrieves the child's exit status.
 * Function Calls:
 *     - [int process_wait(tid_t child_tid)] in process.h
 */
int sys_wait(pid_t pid) {
    int status = process_wait(pid); // for now, hopefully exception returns -1
    return status;
}

/*
 * bool sys_create (const char* file, unsigned initial_size)
 *     - Parameters:
 *         - file: file name for the file to be created.
 *         - initial_size: size in bytes for the file to be created.
 *     - Return: true if successful, false otherwise.
 * Description: creates a new file called file initially initial_size bytes in size.
 */

bool sys_create(const char* file, unsigned initial_size) {
    verify_string(file);
    lock_acquire(&lock_filesys);
    bool result = filesys_create(file, initial_size);
    lock_release(&lock_filesys);
    return result;
}

/*
 * bool sys_remove (const char* file)
 *     - Parameters:
 *         - file: file name of the file to be deleted.
 *     - Return: true if successful, false otherwise.
 * Description: deletes the file called file. A file may be removed regardless of whether
 *     it is open or closed, and removing an open file does not close it.
 */
bool sys_remove(const char* file) {
    /*Check if filename is valid*/
    verify_string(file);
    lock_acquire(&lock_filesys);
    bool result = filesys_remove(file);
    lock_release(&lock_filesys);
    return result;
}

/*
 * int sys_open (const char *file)
 *     - Parameters:
 *         - file:
 *     - Return: a nonnegative integer handle called a "file descriptor" (fd),
 *           or -1 if the file could not be opened.
 * Description: opens the file called file.
 */
 int sys_open(const char* path) {
    verify_string(path);
    struct file_table_entry *fte = palloc_get_page(0);
    if (!fte) return -1;  // memory allocation failed
    fte->file = NULL;
    fte->dir = NULL;

    lock_acquire(&lock_filesys);
    struct file *file = filesys_open(path);
    if (file == NULL) {   // file not successfully opened
      lock_release(&lock_filesys);
      palloc_free_page(fte);
      return -1;
   }

   // check whether the opened file is a directory
   struct inode *inode = file_get_inode(file);  // in filesys_open, made sure inode exists
   ASSERT(inode != NULL);
   if (inode_is_directory(inode)) {
      struct dir * dir = dir_open(inode_reopen(inode));
      file_close(file); // not a regular file so destroy the structure and close inode; DO this after reopen to precent count reaching 0
      fte->dir = dir;
   }
   else fte->file = file; // only one of these 2 can be non-NULL

   lock_release(&lock_filesys);

   return add_to_file_table(fte); // returns the fd
}

/*
 * int sys_filesize (int fd)
 *     - Parameters:
 *         - fd: file descriptor of the file to be checked.
 *     - Return: size of file in bytes.
 * Description: returns the size, in bytes, of the file open as fd.
 */
int sys_filesize(int fd) {
    int size;
    lock_acquire(&lock_filesys);
    struct file_table_entry* fte = get_file_table_entry_by_fd(fd);
    if (fte == NULL) {
        lock_release(&lock_filesys);
        return -1;  // return -1 if no such entry? not sepcified
    }
    size = file_length(fte->file);
    lock_release(&lock_filesys);
    return size;
}

/*
 * int sys_read (int fd, void *buffer, unsigned size)
 *     - Parameters:
 *         - fd: file descriptor to the file to be read.
 *         - buffer: buffer for data read.
 *         - size: size, in bytes, to be read.
 *     - Return: the number of bytes actually read (0 at end of file), or -1 if
 *           the file could not be read (due to a condition other than end of file).
 * Description: reads size bytes from the file open as fd into buffer. Fd 0 reads from
 *     the keyboard using input_getc().
 */
int sys_read(int fd, void* buffer, unsigned size) {
   verify_dest(buffer, size);

   unsigned bytes_read;
   lock_acquire(&lock_filesys);
   if (fd == 0) {   // read from keyboard
      for (unsigned i=0; i<size; i++) {
         bool retval = user_mem_write_byte(buffer, input_getc());
         if (!retval) {
            lock_release(&lock_filesys);
            invalid_user_access();
         }
      }
      bytes_read = size;
   }
   else {      // read from opened file
      struct file_table_entry *fte = get_file_table_entry_by_fd(fd);
      if (fte == NULL) {
         lock_release(&lock_filesys);
         return -1;  // fd is not in the current thread's file table
      }
      bytes_read = file_read (fte->file, buffer, size);
   }
   lock_release(&lock_filesys);
   return bytes_read;
}

/*
 * int sys_write (int fd, const void* buffer, unsigned size)
 *     - Parameters:
 *         - fd: file descriptor for the file to be written.
 *         - buffer: buffer for data to be written.
 *         - size: size of data to be written.
 *     - Return: the number of bytes actually written, which may be less than size
 *           if some bytes could not be written.
 * Description: writes size bytes from buffer to the open file fd. Fd 1 writes to the console.
 *     Your code to write to the console should write all of buffer in one call to putbuf(),
 *     at least as long as size is not bigger than a few hundred bytes.
 */
int sys_write(int fd, const void* buffer, unsigned size) {
    verify_dest(buffer, size);

    unsigned bytes_written;
    lock_acquire(&lock_filesys);
    if (fd == 1) {  // write to console
        putbuf(buffer, size);
        lock_release(&lock_filesys);
        return size;
    }
    else {  // write to a file
      struct file_table_entry *fte = get_file_table_entry_by_fd(fd);
      if (fte == NULL) {
         lock_release(&lock_filesys);
         return -1;  // fd is not in the current thread's file table
      }
      ASSERT (fte->file == NULL || fte->dir == NULL);
      if (fte->file == NULL) {  // directory is not allowed to be written
         lock_release(&lock_filesys);
         return -1;  // fd is not in the current thread's file table
      }
      bytes_written = file_write (fte->file, buffer, size);
   }
   lock_release(&lock_filesys);
   return bytes_written;
}

/*
 * void sys_seek (int fd, unsigned position)
 *     - Parameters:
 *         - fd: file descriptor of file to be read or written.
 *         - position:
 * Description: changes the next byte to be read or written in open file fd to position, expressed
 *     in bytes from the beginning of the file. (Thus, a position of 0 is the file's start.)
 */
void sys_seek(int fd, unsigned position) {
    lock_acquire(&lock_filesys);
    struct file_table_entry* fte = get_file_table_entry_by_fd(fd);
    if (fte == NULL) {
        lock_release(&lock_filesys);
        return;
    }
    file_seek(fte->file, position);
    lock_release(&lock_filesys);
}

/*
 * unsigned sys_tell (int fd)
 *     - Parameters:
 *         - fd: file descriptor of file to be read or written.
 *     - Return: position of the next byte to be read or written.
 * Description: returns the position of the next byte to be read or written in open file fd, expressed
 *     in bytes from the beginning of the file.
 */
unsigned sys_tell(int fd) {
    unsigned  tell;
    lock_acquire(&lock_filesys);
    struct file_table_entry* fte = get_file_table_entry_by_fd(fd);
    if (fte == NULL) {
        lock_release(&lock_filesys);
        return -1;
    }
    tell = file_tell(fte->file);
    lock_release(&lock_filesys);
    return tell;
}

/*
* void sys_close (int fd)
*     - Parameters: file descriptor to be closed.
* Description: closes file descriptor fd.
**/
void sys_close(int fd) {
   lock_acquire(&lock_filesys);
   struct file_table_entry* fte = get_file_table_entry_by_fd(fd);
   if (fte == NULL) {
      lock_release(&lock_filesys);
      return;
   }
   ASSERT (fte->file == NULL || fte->dir == NULL);
   if (fte->dir == NULL) {  // regular file
      file_close(fte->file);
   } else { // directory
      dir_close(fte->dir);
   }
   list_remove(&fte->elem);
   palloc_free_page(fte);

   lock_release(&lock_filesys);
}

/*
Changes the current working directory of the process to dir, which may be
relative or absolute. Returns true if successful, false on failure.
*/
bool sys_chdir(const char *path){
    /* Check for invalid access*/
    verify_string(path);
    lock_acquire(&lock_filesys);

    struct dir *dir = dir_open_path (path);
    if(dir == NULL) {
      lock_release(&lock_filesys);
      return false;
   }
    dir_close(thread_current()->cwd);
    thread_current()->cwd = dir;

    lock_release(&lock_filesys);
    return true;
}

/*
Creates the directory named dir, which may be relative or absolute.
Returns true if successful, false on failure. Fails if dir already
exists or if any directory name in dir, besides the last, does not
already exist. That is, mkdir("/a/b/c") succeeds only if /a/b already
exists and /a/b/c does not.
*/
bool sys_mkdir(const char *dir){
    /* Check for invalid access*/
    verify_string(dir);
    bool result;
    lock_acquire(&lock_filesys);
    result = filesys_mkdir(dir);
    lock_release(&lock_filesys);
    return result;

}

/*
Reads a directory entry from file descriptor fd, which must represent a directory. If successful, stores the null-terminated file name in name, which must have room for READDIR_MAX_LEN + 1 bytes, and returns true. If no entries are left in the directory, returns false.
. and .. should not be returned by readdir.

If the directory changes while it is open, then it is acceptable for some entries not to be read at all or to be read multiple times. Otherwise, each directory entry should be read once, in any order.

READDIR_MAX_LEN is defined in lib/user/syscall.h. If your file system supports longer file names than the basic file system, you should increase this value from the default of 14.
*/
bool sys_readdir(int fd, const char *name){
    bool result;
    struct file_table_entry* fte = get_file_table_entry_by_fd(fd);

    lock_acquire(&lock_filesys);
    if(fte == NULL || fte->dir == NULL){  // must be a directory
	     lock_release(&lock_filesys);
        return false;
    }

    result = dir_readdir(fte->dir, name);

    lock_release(&lock_filesys);
    return result;
}

/* Returns true if fd represents a directory, false if it represents an ordinary file.
*/
bool sys_isdir(int fd){
    bool result;
    struct file_table_entry* fte = get_file_table_entry_by_fd(fd);
    //check for inode dir
    ASSERT (fte->file == NULL || fte->dir == NULL);
    return fte->file == NULL;

    return result;
}

/*
Returns the inode number of the inode associated with fd, which may represent an ordinary file or a directory.
An inode number persistently identifies a file or directory. It is unique during the file's existence. In Pintos, the sector number of the inode is suitable for use as an inode number.
*/
int sys_inumber(int fd) {
    int result;
    lock_acquire(&lock_filesys);
    struct file_table_entry* fte = get_file_table_entry_by_fd(fd);
    // get inode number
    if (fte->file != NULL) result = (int) inode_get_inumber(file_get_inode(fte->file));
    else if (fte->dir != NULL) result = (int) inode_get_inumber(dir_get_inode(fte->dir));

    lock_release(&lock_filesys);
    return result;
}


/************************ Memory Access Functions Implementation ************************/

/*
 * void user_mem_read(void* dest_addr, void* uaddr, size_t size)
 *     - Parameters:
 *         - dest_addr: destination address to save the result of memory read.
 *         - uaddr: starting memory location to be read from.
 *         - size: number of bytes to be read.
 * Description: As part of a system call, the kernel must often access memory through
 *     pointers provided by a user program. The kernel must be very careful about doing so,
 *     because the user can pass a null pointer, a pointer to unmapped virtual memory, or a
 *     pointer to kernel virtual address space (above PHYS_BASE).
 */
static void user_mem_read(void* dest_addr, void* uaddr, size_t size) {
    // uaddr must be below PHYS_BASE and must not be NULL pointer
    if (uaddr == NULL || !is_user_vaddr(uaddr)) invalid_user_access();
    // read
    for (unsigned int i = 0; i < size; i++) {
        // read a byte from memory
        int byte_data = user_mem_read_byte(uaddr + i);
        // if byte_data = -1, the last memory read was a segment fault
        if (byte_data == -1) invalid_user_access();
        // save this byte of data to destination address
        *(uint8_t*) (dest_addr + i) = byte_data & 0xFF;
    }
}

/**********************************************************
The main reason to have both verify_string and verify_dest
is that one applies to KNOWN number of consecutive addresses
the other does not, so have to look for the null terminator
**********************************************************/

/*
check if the provided char pointer is a valid
every char in string must be in user space, and must be in a page thats mapped
 */
static void verify_string(const uint8_t *ptr) {
    if (ptr == NULL || !is_user_vaddr(ptr)) invalid_user_access();

    while (true) {
        char byte = user_mem_read_byte(ptr);
        if (byte == -1) invalid_user_access();
        else if (byte == '\0') return;
        ptr++;
    }
}

/*
   helper function for syscalls that writes to a user address
   check if the SIZE addresses starting at DEST are all valid to write
*/
static void verify_dest(void *dest, unsigned size) {
   for (unsigned i=0; i<size; i++) {
      if (dest == NULL || !is_user_vaddr(dest)) invalid_user_access();
      int byte_data = user_mem_read_byte(dest + i);
      if (byte_data == -1) invalid_user_access();
   }
}

/*
 * int user_mem_read_byte (const uint8_t* uaddr)
 *     - Parameters:
 *         - uaddr: address in user space to be read.
 *     - Return: the byte value if successful, -1 if a segfault occurred.
 * Description: Reads a byte at user virtual address uaddr.
 *     uaddr must be below PHYS_BASE.
 *     uaddr points to a byte of memory.
 */
static int user_mem_read_byte(const uint8_t* uaddr) {
    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
                : "=&a" (result) : "m" (*uaddr));
    return result;
}

/*
 * bool user_mem_write_byte (uint8_t* dest, uint8_t byte)
 *     - Parameters:
 *         - dest_addr: destination address for writing.
 *         - byte: byte of data to be written.
 *     - Return: true if successful, false if a segfault occurred.
 * Description: writes a byte to user address dest_addr.
 *     dest_addr must be below PHYS_BASE.
 */
static bool user_mem_write_byte(uint8_t* dest_addr, uint8_t byte) {
    int error_code;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
                : "=&a" (error_code), "=m" (*dest_addr) : "q" (byte));
    return error_code != -1;
}

/*
 * void invalid_user_access()
 * Description: for now just exits with status -1
 *     how to free memory and release lock?
 */
 static void invalid_user_access() {
    if (lock_held_by_current_thread(&lock_filesys))
    lock_release (&lock_filesys);
    sys_exit(-1);
    NOT_REACHED();
}

/************************ File Table Helper Functions ************************/

/*
   add the given file table entry to current thread's file table
   This implementation means directories have fd as well
   return: fd of the entry
*/
static int add_to_file_table (struct file_table_entry *fte) {
   struct thread *cur = thread_current();
   struct list *file_table = &cur->file_table;

   if (list_empty(file_table)) fte->fd = 3;       // 0, 1, 2 reserved
   else {
      struct file_table_entry *back = list_entry(list_back(file_table), struct file_table_entry, elem); // the last opened file
      fte->fd = back->fd + 1;
   }
   list_push_back(file_table, &fte->elem);
   return fte->fd;
}

/*Iterate through current file table to find the file_table_entry pointer*/
static struct file_table_entry* get_file_table_entry_by_fd(int fd) {
   struct thread *cur = thread_current();
   struct list *file_table = &cur->file_table;
   struct file_table_entry *fte;
   struct list_elem *e;
   for (e = list_begin(file_table); e != list_end(file_table); e = list_next(e)) {
      fte = list_entry(e, struct file_table_entry, elem);
      if (fte->fd == fd) {
         return fte;
      }
   }
   return NULL;
}

