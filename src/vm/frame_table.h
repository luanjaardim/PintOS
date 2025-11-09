#define VIRT_MEM
#include "kernel/list.h"
#include "kernel/hash.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/timer.h"

struct frame_table_entry {
    uint32_t* frame;
    struct thread* owner;

    struct hash_elem e;
};

struct sup_page_table {
    struct hash map;
};

struct sup_page_table_entry {
    uint32_t* user_vaddr;
    uint64_t access_time;
    bool dirty;
    bool accessed;

    struct hash_elem e;
};

void frame_table_init();
bool insert_page_on_table(void *page);
void free_page_on_table(void *page);

unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED);
bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
unsigned sup_hash_func(const struct hash_elem *elem, void *aux UNUSED);
bool sup_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
void sup_destroy_func(struct hash_elem *elem, void *aux UNUSED);