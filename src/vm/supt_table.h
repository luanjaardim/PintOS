#ifndef __SUPT_LIB__
#define __SWAP_LIB__

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
#include "threads/palloc.h"

enum pg_stats {
    OWNED,
    EVICTED, // on swap
};

struct sup_page_table_entry {
    uint32_t* upage;
    uint32_t* kpage;
    uint64_t access_time;
    enum pg_stats status;
    int swap_index; // index where the page was stored on swap
    bool dirty;
    bool accessed;

    struct hash_elem e;
};

struct sup_page_table_entry *sup_get_entry(const struct hash *h, void *page);
void *remove_from_supt_table(const struct hash *h, void *upage, bool remove_from_pagedir);
void evict_frame(struct sup_page_table_entry *spt);

unsigned sup_hash_func(const struct hash_elem *elem, void *aux UNUSED);
bool sup_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
void sup_destroy_func(struct hash_elem *elem, void *aux UNUSED);

#endif