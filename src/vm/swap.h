#ifndef __SWAP_LIB__
#define __SWAP_LIB__

#include <string.h>
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
#include "filesys/file.h"

void swap_init();
int send_to_swap(void *page);
void take_from_swap(void *page, unsigned index);
void swap_free(unsigned index);

#endif