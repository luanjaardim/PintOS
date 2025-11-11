#include "swap.h"

static struct block *global_swap_block;
static struct bitmap *used_positions;
static struct lock swap_lock;

#define FREE true
#define OCCUPIED false
static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
static size_t swap_size;

void swap_init() {
    global_swap_block = block_get_role(BLOCK_SWAP);
    ASSERT(global_swap_block != NULL)

    swap_size = block_size(global_swap_block) / SECTORS_PER_PAGE; // number of pages
    used_positions = bitmap_create(swap_size);
    bitmap_set_all(used_positions, FREE);
    lock_init(&swap_lock);
}

int send_to_swap(void *page) {
    lock_acquire(&swap_lock);
    int index = bitmap_scan(used_positions, 0, 1, FREE);
    ASSERT(index != BITMAP_ERROR);
    write_to_block(page, index);
    bitmap_set(used_positions, index, OCCUPIED);
    lock_release(&swap_lock);
    return index;
}

void take_from_swap(void *page, unsigned index) {
    ASSERT(index < swap_size);
    lock_acquire(&swap_lock);
    if(bitmap_test(used_positions, index) == FREE) PANIC("Invalid swap index");
    read_from_block(page, index);
    bitmap_set(used_positions, index, FREE);
    lock_release(&swap_lock);
}

void read_from_block(uint8_t* frame, int index)
{
    for(int i = 0; i < 8; ++i)
    {
        block_read(global_swap_block, index * SECTORS_PER_PAGE + i, frame + (i * BLOCK_SECTOR_SIZE));
    }
}

void write_to_block(uint8_t* frame, int index)
{
    for(int i = 0; i < 8; ++i)
    {
        block_write(global_swap_block, index * SECTORS_PER_PAGE + i, frame + (i * BLOCK_SECTOR_SIZE));
    }
}