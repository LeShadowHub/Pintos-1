#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/synch.h"

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

tid_t process_execute (const char *cmdline);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process_control_block {
   pid_t pid;						          // Process ID
   tid_t parent_tid;	                   // Parent process's tid, putting the entire parent thread struct here is too big
   int exit_status;                    // exit status, valid when thread is in DYING state
   char *cmdline;
   /* Synchronization */
   struct semaphore sema_init_process;   /* the semaphore used between start_process() and process_execute() */
};

#endif /* userprog/process.h */
