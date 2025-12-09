#include "supt_table.h"
#include "vm/swap.h"

struct sup_page_table_entry *sup_get_entry(const struct hash *h, void *page) {
  struct sup_page_table_entry tmp;
  tmp.upage = page;
  struct hash_elem *e = hash_find(h, &tmp.e);
  if(e) return hash_entry(e, struct sup_page_table_entry, e);
  else return NULL;
}

bool filesys_add_to_supt_table(const struct hash *table, void *upage, struct file *f, size_t offset, size_t to_read, size_t padding) {
    struct sup_page_table_entry *elem_sup = malloc(sizeof(struct sup_page_table_entry));
    if(elem_sup == NULL) return false;

    elem_sup->upage = upage;
    elem_sup->kpage = NULL;
    elem_sup->f = f;
    elem_sup->offset = offset;
    elem_sup->to_read = to_read;
    elem_sup->padding = padding;
    elem_sup->status = MMAPED;

    struct hash_elem *prev_elem;
    prev_elem = hash_insert(table, &elem_sup->e);
    ASSERT(prev_elem == NULL);

    return true;
}

// remove a upage from supt_table and returns its kpage
void *remove_from_supt_table(const struct hash *table, void *upage, bool remove_from_pagedir) {

  struct thread *t = thread_current();
  struct sup_page_table_entry tmp;
  tmp.upage = upage;
  struct hash_elem *h = hash_find(table, &(tmp.e));
  if(h == NULL) PANIC("Page not found on thread sup frame table");

  struct sup_page_table_entry *elem = hash_entry(h, struct sup_page_table_entry, e);
  void *kpage = elem->kpage;
  if(remove_from_pagedir)
    pagedir_clear_page(t->pagedir, elem->upage);
  hash_delete(&t->sup_pg_t, &(elem->e));
  free(elem);
  return kpage;
}

void evict_frame(struct sup_page_table_entry *spt) {
    spt->swap_index = send_to_swap(spt->kpage);
    spt->kpage = NULL;
    spt->status = EVICTED;
}

bool take_from_filesys(void *kpage, struct sup_page_table_entry *spt) {
  file_seek(spt->f, spt->offset);

  if(spt->to_read == file_read(spt->f, kpage, spt->to_read)) return false;
  memset(kpage + spt->to_read, 0, spt->padding);
  return true;
}

unsigned sup_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct sup_page_table_entry *entry = hash_entry(elem, struct sup_page_table_entry, e);
  
  return hash_int( (int)entry->upage );
}
bool sup_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct sup_page_table_entry *a_entry = hash_entry(a, struct sup_page_table_entry, e);
  struct sup_page_table_entry *b_entry = hash_entry(b, struct sup_page_table_entry, e);
  return a_entry->upage < b_entry->upage;
}
void sup_destroy_func(struct hash_elem *elem, void *aux)
{
  struct thread *t = (struct thread *)aux;
  struct sup_page_table_entry *entry = hash_entry(elem, struct sup_page_table_entry, e);
  switch (entry->status)
  {
  case EVICTED:
    swap_free(entry->swap_index);
    break;
  case OWNED:
    pagedir_clear_page(t->pagedir, entry->upage);
    remove_from_frame_table(entry->kpage);
    break;
  default:
    NOT_REACHED()
    break;
  }
  palloc_free_multiple(entry->kpage, 1);
  free(entry);
  return;
}