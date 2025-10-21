#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/file.h"

struct file_desc {
    uint32_t id;
    struct list_elem e;
    struct file *f;
};

struct parent_and_cmd_line {
    const char *cmd_line;
    struct thread *parent;
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
