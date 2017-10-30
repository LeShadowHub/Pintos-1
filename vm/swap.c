/*************************************
 *             swap.c                *
 ************************************/

#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

struct block *swap_slots;
static struct bitmap *swap_table;

static size_t SECTORS_PER_SLOT = PGSIZE / BLOCK_SECTOR_SIZE;
static size_t SWAP_TABLE_SIZE;    // number of slots (each slot is one page)
static void block_write_slot(struct block * block, size_t start_sector, void * buffer);
static void block_read_slot(struct block * block, size_t start_sector, void * buffer);


/* Initialize swap table and get swap slots*/
bool swap_init() {
   /* Initialize device (disk) */
   swap_slots = block_get_role (BLOCK_SWAP);
   if (swap_slots == NULL) PANIC ("Error: Cannot get block: BLOCK_SWAP");
   SWAP_TABLE_SIZE = block_size(swap_slots) / SECTORS_PER_SLOT;

   /* Intialize map */
   swap_table = bitmap_create (SWAP_TABLE_SIZE);
   if (swap_table == NULL) return false;  // memory allocation error
   /* initialize all bits to be true */
   bitmap_set_all (swap_table, true);

   return true;
}

/* not used
    all resources should be freed automatically when system shut down */
void swap_destroy(void) {
   bitmap_destroy(swap_slots);
}

/* Write one frame (page) to the swap slots (one slot)
   Return the index of the slot written'
   Return BITMAP_ERROR if failed */
size_t swap_out (void *frame){

    /* Search for available region */
    size_t slot_index = bitmap_scan(swap_table, 0, 1, true);  // start at 0, 1 consecutive page
    if (slot_index == BITMAP_ERROR) return BITMAP_ERROR;
    /* mark used in swap table for the slot */
    bitmap_set(swap_table, slot_index, false);
    // write one page to the swap slots
    block_write_slot(swap_slots, slot_index * SECTORS_PER_SLOT, frame);

    return slot_index;
}

/* Write the content from slot SLOT_INDEX to FRAME*/
void swap_in (size_t slot_index, void * frame){
   ASSERT(slot_index < SWAP_TABLE_SIZE);
   ASSERT(bitmap_test(swap_slots, slot_index) == false);

   block_read_slot(swap_slots, slot_index * SECTORS_PER_SLOT, frame);
   bitmap_set(swap_table, slot_index, true);
}


/*
   Write one slot (size of a page) in the block device (aka swap slots)
*/
static void block_write_slot(struct block * block, size_t start_sector, void * buffer) {
   for(size_t i = 0; i < SECTORS_PER_SLOT; i++){
      // block_write (struct block *block, block_sector_t sector, const void *buffer)
      block_write(block, start_sector + i, buffer + (BLOCK_SECTOR_SIZE * i));
   }
}

static void block_read_slot(struct block * block, size_t start_sector, void * buffer) {
   for(size_t i = 0; i < SECTORS_PER_SLOT; i++){
      block_read(block, start_sector + i, buffer + (i * BLOCK_SECTOR_SIZE));
   }
}

