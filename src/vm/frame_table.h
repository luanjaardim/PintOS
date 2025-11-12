#ifndef __FRAME_LIB__
#define __FRAME_LIB__

#ifndef VIRT_MEM
#define VIRT_MEM
#endif

#include "devices/block.h"
#include "kernel/list.h"
#include "kernel/hash.h"
#include "kernel/bitmap.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/timer.h"
#include "userprog/pagedir.h"

struct frame_table_entry {
    uint32_t* kpage;
    uint32_t* upage;
    struct thread* owner;
    // when reading or writing to a file we need all of it in the memory, because it is a blocking operation
    // so the page cannot be evicted
    bool evictable;

    struct hash_elem e;
};

void frame_table_init();
bool insert_page_on_table(void *upage, void *kpage, bool writable);
void *remove_from_frame_table(void *kpage);
void remove_kpage(void *kpage);
void *get_oldest_table();
void *remove_oldest_kpage();
void load_from_swap_again(struct hash *table, void *upage);
void load_and_set_not_evictable_buffer(const void *buffer, size_t size);
void unset_not_evictable_buffer(const void *buffer, size_t size);

unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED);
bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

#endif