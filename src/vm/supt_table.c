#include "supt_table.h"

struct sup_page_table_entry *sup_get_entry(const struct hash *h, void *page) {
  struct sup_page_table_entry tmp;
  tmp.upage = page;
  struct hash_elem *e = hash_find(h, &tmp.e);
  if(e) return hash_entry(e, struct sup_page_table_entry, e);
  else return NULL;
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
    spt->status = EVICTED;
    spt->swap_index = send_to_swap(spt->kpage);
    spt->kpage = NULL;
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
  pagedir_clear_page(t->pagedir, entry->upage);
  free(entry);
  return;
}