#include "frame_table.h"
#include "vm/supt_table.h"
#include "vm/swap.h"

static struct lock frame_lock;
static struct hash frame_table;

void frame_table_init() {
    hash_init(&frame_table, frame_hash_func, frame_less_func, NULL);
    lock_init(&frame_lock);
}

struct frame_table_entry *get_entry(void *page) {
  struct frame_table_entry tmp;
  tmp.kpage = page;
  struct hash_elem *e = hash_find(&frame_table, &tmp.e);
  if(e) return hash_entry (e, struct frame_table_entry, e);
  else return NULL;
}

bool insert_page_on_table(void *upage, void *kpage, bool writable) {
    if(upage == NULL || kpage == NULL) return false;
    struct frame_table_entry *elem = malloc(sizeof(struct frame_table_entry));
    struct sup_page_table_entry *elem_sup = malloc(sizeof(struct sup_page_table_entry));
    if(!elem || !elem_sup) {
        free(elem); 
        free(elem_sup);
        return false;
    }

    lock_acquire(&frame_lock);
    elem->owner = thread_current();
    elem->kpage = elem_sup->kpage = kpage;
    elem->upage = elem_sup->upage = upage;
    elem_sup->access_time = timer_ticks();
    elem_sup->dirty = false;
    elem_sup->status = OWNED;
    elem_sup->swap_index = -1;

    // Insert on frame table map
    hash_insert(&frame_table, &elem->e);
    // Insert on sup frame table of the thread
    hash_insert(&elem->owner->sup_pg_t, &elem_sup->e);
    // Install page to pagedir
    if(!install_page(upage, kpage, writable)) {
      lock_release(&frame_lock);
      PANIC("Failed to install page.");
    }
    lock_release(&frame_lock);
    return true;
}

// removes a kpage from frame table and returns its upage
void *remove_from_frame_table(void *kpage) {
    struct frame_table_entry tmp_;
    tmp_.kpage = kpage;
    lock_acquire(&frame_lock);

    struct hash_elem *h = hash_find(&frame_table, &(tmp_.e));
    if(h == NULL) PANIC("Page not found on frame table");
    struct frame_table_entry *elem = hash_entry(h, struct frame_table_entry, e);
    void *upage = elem->upage;
    hash_delete(&frame_table, &(elem->e));

    lock_release(&frame_lock);
    free(elem);
    return upage;
}

// remove kpage from both frame table and sup table
void remove_kpage(void *kpage) {
// TODO REMOVE FROM SWAP IF THERE
  struct frame_table_entry *ft = get_entry(kpage);
  struct thread *t = ft->owner;
  struct sup_page_table_entry *sp = sup_get_entry(&t->sup_pg_t, ft->upage);
  void *upage = sp->upage;
  remove_from_frame_table(kpage);
  remove_from_supt_table(&t->sup_pg_t, upage, true);
}

void *get_oldest_table() {
    if(hash_empty(&frame_table)) PANIC("Hash should not be empty\n");

    struct hash_iterator i;
    struct sup_page_table_entry *oldest = NULL;

    lock_acquire(&frame_lock);
    hash_first (&i, &frame_table);
    while (hash_next (&i))
    {
        struct frame_table_entry *f = hash_entry (hash_cur (&i), struct frame_table_entry, e);
        struct thread *t = f->owner;
        struct sup_page_table_entry *sp = sup_get_entry(&t->sup_pg_t, f->upage);
        if(oldest == NULL || sp->access_time < oldest->access_time) 
          oldest = sp;
    }
    lock_release(&frame_lock);
    if(oldest) return oldest->kpage;
    else return NULL;
}

void *remove_oldest_kpage() {
    void *page = get_oldest_table(); // removed the oldest page, it can now be used by the next process
    struct frame_table_entry *ft = get_entry(page);
    struct thread *t = ft->owner;
    struct sup_page_table_entry *sp = sup_get_entry(&t->sup_pg_t, ft->upage);
    void *upage = remove_from_frame_table(page);
    pagedir_clear_page(t->pagedir, upage);
    evict_frame(sp);
    return page;
}

unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, e);
  return hash_bytes( &entry->kpage, sizeof entry->kpage);
}
bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, e);
  struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, e);
  return a_entry->kpage < b_entry->kpage;
}
