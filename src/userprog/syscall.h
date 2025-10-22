#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdint.h>

void syscall_init (void);
void exit_syscall(uint32_t code);

#endif /* userprog/syscall.h */
