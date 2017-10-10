// Created by:
// Matthew Tawil (mt33924)
// Allen Pan (xp572)
// Ze Lyu (zl5298)

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

void syscall_init (void);

#endif /* userprog/syscall.h */
