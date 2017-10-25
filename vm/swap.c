/*************************************
 *             swap.c                *
 * Created by Matthew T on 10/23/17. *
 ************************************/

#include "swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include <stdio.h>

struct block *swap_device;

static struct bitmap *swap_map;

static size_t SECTORS_PERPAGE = PGSIZE / BLOCK_SECTOR_SIZE;
static size_t swap_pages;

/* Initialize table */

void swap_init(){
    
    /* Initialize device(disk) */
    swap_device = block_get_role (BLOCK_SWAP);
    /* Error Check */
    if (swap_device == NULL)
        PANIC ("Error: Cannot initialize disk");
    
    /* Intialize map */
    swap_map = bitmap_create (swap_pages);
    /* Error Check */
    if (swap_map == NULL)
        PANIC ("Error: Cannot initialize map");
    
    /* initialize all bits to be true */
    bitmap_set_all (swap_map, true);
    
}

/* Write to disk */
swap_index_t vm_swap_out (void*){
    
    
    
}

/* Write to index */
void vm_swap_in (swap_index_t, void*){
    
    
    
}

/* Free region */
void vm_swap_free (swap_index_t){
    
    
    
}

