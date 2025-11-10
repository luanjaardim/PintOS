#include "supt_table.h"

struct sup_page_table_entry *sup_get_page(const struct hash *h, void *page) {
  struct sup_page_table_entry tmp;
  tmp.upage = page;
  struct hash_elem *e = hash_find(h, &tmp.e);
  if(e) return hash_entry(e, struct sup_page_table_entry, e);
  else return NULL;
}

void evict_frame(struct sup_page_table_entry *spt) {
    spt->status = EVICTED;
    spt->kpage = NULL;
    spt->swap_index = send_to_swap(spt->kpage);
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
void sup_destroy_func(struct hash_elem *elem, void *aux UNUSED)
{
  struct sup_page_table_entry *entry = hash_entry(elem, struct sup_page_table_entry, e);
  free(entry);
  return;
}