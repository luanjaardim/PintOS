#include "frame_table.h"

static struct lock frame_lock;
static struct hash frame_table;

void frame_table_init() {
    hash_init(&frame_table, frame_hash_func, frame_less_func, NULL);
    lock_init(&frame_lock);
}

bool insert_page_on_table(void *page) {
    struct frame_table_entry *elem = malloc(sizeof(struct frame_table_entry));
    struct sup_page_table_entry *elem_sup = malloc(sizeof(struct sup_page_table_entry));
    if(!elem || !elem_sup) {
        free(elem); 
        free(elem_sup);
        return false;
    }

    lock_acquire(&frame_lock);
    elem->owner = thread_current();
    elem->frame = page;
    elem_sup->user_vaddr = page;
    elem_sup->access_time = timer_ticks();
    elem_sup->dirty = false;

    // Insert on frame table map
    hash_insert(&frame_table, &elem->e);
    // Insert on sup frame table of the thread
    hash_insert(&elem->owner->sup_pg_t, &elem_sup->e);
    lock_release(&frame_lock);
    return true;
}

void free_page_on_table(void *page) {
    struct frame_table_entry tmp_;
    struct sup_page_table_entry tmp2_;
    tmp_.frame = page;
    tmp2_.user_vaddr = page;
    lock_acquire(&frame_lock);

    struct hash_elem *h = hash_find(&frame_table, &(tmp_.e));
    if(h == NULL) PANIC("Page not found on frame table");

    struct frame_table_entry *elem = hash_entry(h, struct frame_table_entry, e);
    hash_delete(&frame_table, &(elem->e));

    // If this process haven't already being terminated
    if(!hash_empty(&elem->owner->sup_pg_t)) {
        h = hash_find(&elem->owner->sup_pg_t, &(tmp2_.e));
        if(h == NULL) PANIC("Page not found on thread sup frame table");
        struct sup_page_table_entry *elem2 = hash_entry(h, struct sup_page_table_entry, e);
        hash_delete(&elem->owner->sup_pg_t, &(elem2->e));
        free(elem2);
    }

    lock_release(&frame_lock);
    free(elem);
}

void *find_oldest_table() {
    if(hash_empty(&frame_table)) PANIC("Hash should not be empty\n");

    struct hash_iterator i;

    hash_first (&i, &frame_table);
    while (hash_next (&i))
    {
        struct frame_table_entry *f = hash_entry (hash_cur (&i), struct frame_table_entry, e);
        struct thread *t = f->owner;
    }
    struct elem_list *oldest = list_front(&frame_table);
    struct elem_list *it = oldest;
}

unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, e);
  return hash_bytes( &entry->frame, sizeof entry->frame );
}
bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, e);
  struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, e);
  return a_entry->frame < b_entry->frame;
}

unsigned sup_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct sup_page_table_entry *entry = hash_entry(elem, struct sup_page_table_entry, e);
  
  return hash_int( (int)entry->user_vaddr );
}
bool sup_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct sup_page_table_entry *a_entry = hash_entry(a, struct sup_page_table_entry, e);
  struct sup_page_table_entry *b_entry = hash_entry(b, struct sup_page_table_entry, e);
  return a_entry->user_vaddr < b_entry->user_vaddr;
}
void sup_destroy_func(struct hash_elem *elem, void *aux UNUSED)
{
  struct sup_page_table_entry *entry = hash_entry(elem, struct sup_page_table_entry, e);
  free(entry);
  return;
}
