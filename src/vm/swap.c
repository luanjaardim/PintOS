#include "swap.h"

static struct block *global_swap_block;

void swap_init() {
    global_swap_block = block_get_role(BLOCK_SWAP);
}

void read_from_block(uint8_t* frame, int index)
{
    for(int i = 0; i < 8; ++i)
    {
        block_read(global_swap_block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
    }
}

void write_to_block(uint8_t* frame, int index)
{
    for(int i = 0; i < 8; ++i)
    {
        block_write(global_swap_block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
    }
}