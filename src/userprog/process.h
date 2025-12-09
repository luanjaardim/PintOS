#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/file.h"

typedef size_t mmapid_t;

struct mmap_desc {
    mmapid_t id;
    struct list_elem e;
    struct file *f;
    void *upage;
    size_t length;
};

struct file_desc {
    uint32_t id;
    struct list_elem e;
    struct file *f;
};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

bool install_page (void *upage, void *kpage, bool writable);

#endif /* userprog/process.h */
