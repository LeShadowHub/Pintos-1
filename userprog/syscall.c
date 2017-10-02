#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
   intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
   void * sp = f->esp; // a copy of stack pointer; do not want to modify the stack pointer
   // check if the pointer is valid; but wondering if this is necessary, because does the user set the pointer themselves?
   // check if it's a user space virtual address
   // check if the virtual memory is mapped
   // check if it's a null pointer
   if (sp == NULL || is_kernel_vaddr(sp))

   int syscall_num = *sp;
   ((uint32_t *)sp)++;
   switch (syscall_num) {
      case SYS_HALT:
         halt();
      break;
      case SYS_EXIT:
         int status = *sp;
         exit();
      break;
      case SYS_EXEC:
      break;
      case SYS_WAIT:
      break;
      case SYS_CREATE:
      break;
      case SYS_REMOVE:
      break;
      case SYS_OPEN:
      break;
      case SYS_FILESIZE:
      break;
      case SYS_READ:

      break;
      case SYS_WRITE:
      break;
      case SYS_SEEK:
      break;
      case SYS_TELL:
      break;
      case SYS_CLOSE:
      break;

   }

   printf ("system call!\n");

   thread_exit ();
}


void halt (void) {
   shutdown_power_off();
}

void exit (int status) {

}

pid_t exec (const char *file) {

}

int wait (pid_t) {

}

bool create (const char *file, unsigned initial_size) {

}

bool remove (const char *file) {

}

int open (const char *file) {

}

int filesize (int fd) {

}

int read (int fd, void *buffer, unsigned length) {

}

int write (int fd, const void *buffer, unsigned length) {

}

void seek (int fd, unsigned position) {

}

unsigned tell (int fd) {

}

void close (int fd) {

}

