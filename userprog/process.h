/*
 * Modified by:
 * Matthew Tawil (mt33924)
 * Allen Pan (xp572)
 * Ze Lyu (zl5298) 
 */

#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/synch.h"
#include "threads/thread.h"
#include "filesys/file.h"
/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)
/* Thread identifier type. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */


struct pcb_t {
   pid_t pid;						          // Process ID
   int exit_status;                    // exit status, valid when thread is in DYING state
   int already_wait;                   // 0 or 1; whether wait() or process_wait() is called on this thread already
   int killed;                         // 0 or 1; whether this thread is been killed by thread or exit itself
   int orphan;                         // 0 or 1; whether its parent has exited
   // orphan means it will never be called wait() upon, so when it exits, it needs to be freed
   // otherwise, the parent process needs to free it at the end of process_wait()
   struct file *executable;            // the current executable file
   struct list_elem elem;              // element in child_list
   struct semaphore process_exec_sema; // used in process_execute and start_process
   struct semaphore process_wait_sema;  // used in process_wait and process_exit
};

/*Info of current file*/
struct file_table_entry {
   int fd;
   struct file* file;
   struct list_elem elem;
   struct dir *dir;
};


tid_t process_execute (const char *cmdline);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */

