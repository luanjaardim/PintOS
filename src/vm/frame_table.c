#include "frame_table.h"

static struct lock frame_lock;
static struct hash frame_table;

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED);
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

void frame_table_init() {
    hash_init(&frame_table, frame_hash_func, frame_less_func, NULL);
    lock_init(&frame_lock);
}

bool insert_page_on_table(void *page) {
    struct frame_table_entry *elem = malloc(sizeof(struct frame_table_entry));
    if(!elem) return false;

    lock_acquire(&frame_lock);
    elem->owner = thread_current();
    elem->frame = page;
    hash_insert(&frame_table, &elem->e);
    lock_release(&frame_lock);
    return true;
}

void free_page_on_table(void *page) {
    struct frame_table_entry tmp_;
    tmp_.frame = page;
    lock_acquire(&frame_lock);

    struct hash_elem *h = hash_find(&frame_table, &(tmp_.e));
    if(h == NULL) PANIC("Page not found on frame table");

    struct frame_table_entry *elem = hash_entry(h, struct frame_table_entry, e);
    hash_delete(&frame_table, &(elem->e));

    lock_release(&frame_lock);
    free(elem);
}

// Hash Functions required for [frame_map]. Uses 'kpage' as key.
static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, e);
  return hash_bytes( &entry->frame, sizeof entry->frame );
}
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, e);
  struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, e);
  return a_entry->frame < b_entry->frame;
}
