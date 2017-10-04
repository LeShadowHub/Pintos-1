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
/*
static pid_t sys_exec (const char *file);
static int sys_wait (pid_t);
static bool sys_create (const char *file, unsigned initial_size);
static bool sys_remove (const char *file);
static int sys_open (const char *file);
static int sys_filesize (int fd);
static int sys_read (int fd, void *buffer, unsigned length);
static int sys_write (int fd, const void *buffer, unsigned length);
static void sys_seek (int fd, unsigned position);
static unsigned sys_tell (int fd);
static void sys_close (int fd);
*/
void syscall_init (void) {
   intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f UNUSED) {
   void *sp = f->esp; // copy of stack pointer, don't want to modify the pointer
   int syscall_num;
   user_mem_read(&syscall_num, sp, sizeof(syscall_num));


   switch (syscall_num) {
      case SYS_HALT: {
         sys_halt();
         break;
      }
      case SYS_EXIT: {
         sp = (uint32_t *) sp + 1;
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

      }
      case SYS_SEEK: {

      }
      case SYS_TELL: {

      }
      case SYS_CLOSE: {

      }

   }

   //printf ("system call!\n");

   thread_exit ();
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
      *(uint8_t *)(dest + i) = val && 0xFF;
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
Terminates Pintos by calling shutdown_power_off() (declared in threads/init.h). This should be seldom used, because you lose some information about possible deadlock situations, etc.
*/
static void sys_halt (void) {
   shutdown_power_off();
}

/*
Terminates the current user program, returning status to the kernel. If the process's parent waits for it (see below), this is the status that will be returned. Conventionally, a status of 0 indicates success and nonzero values indicate errors.
*/
static void sys_exit (int status) {
   struct thread *cur = thread_current();
   printf("%s: exit(%d)\n", cur->name, status);
   cur->pcb->exit_status = status;
   thread_exit();
}

/*
Runs the executable whose name is given in cmd_line, passing any given arguments, and returns the new process's program id (pid). Must return pid -1, which otherwise should not be a valid pid, if the program cannot load or run for any reason. Thus, the parent process cannot return from the exec until it knows whether the child process successfully loaded its executable. You must use appropriate synchronization to ensure this.
*/
// static pid_t sys_exec (const char *file) {
//
// }

// int wait (pid_t) {
//
// }
//
// bool create (const char *file, unsigned initial_size) {
//
// }
//
// bool remove (const char *file) {
//
// }
//
// int open (const char *file) {
//
// }
//
// int filesize (int fd) {
//
// }
//
// int read (int fd, void *buffer, unsigned length) {
//
// }
//
// int write (int fd, const void *buffer, unsigned length) {
//
// }
//
// void seek (int fd, unsigned position) {
//
// }
//
// unsigned tell (int fd) {
//
// }
//
// void close (int fd) {
//
// }

