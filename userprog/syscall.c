#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static void halt (void);
static void exit (int status);
static pid_t exec (const char *file);
static int wait (pid_t);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned length);
static int write (int fd, const void *buffer, unsigned length);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);

void
syscall_init (void)
{
   intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
   void * sp = f->esp; // a copy of stack pointer; do not want to modify the stack pointer
   // this is not the best way to check memory access, but will do this for now.
   // check if the pointer is valid; but wondering if this is necessary, because does the user set the pointer themselves for syscalls?
   // check if it's a user space virtual address
   // check if the virtual memory is mapped
   // check if it's a null pointer
   if (sp == NULL || is_kernel_vaddr(sp))     // how to check if it's mapped?
      exit(1);  // don't how exactly we should terminate the process; will do this for now
   // But do we need to check the validity each time we increment the pointer?

   int syscall_num = *(int *)sp;  // my understanding is, this assignment triggers syscall read automatically. And when run in Pintos, the syscall we implemented is used, not the one from C library
   sp = (uint32_t *)sp + 1;

   switch (syscall_num) {
      case SYS_HALT: {
         halt();
      break;
      }

      case SYS_EXIT: {
         int status = *(int *)sp;
         exit(status);
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

   printf ("system call!\n");

   thread_exit ();
}


void halt (void) {
   shutdown_power_off();
}

void exit (int status) {

}

// pid_t exec (const char *file) {
//
// }
//
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

