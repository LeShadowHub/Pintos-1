// /*************************************
//  *             swap.c                *
//  ************************************/
// 
// #include "swap.h"
// #include <bitmap.h>
// #include "devices/block.h"
// #include "threads/vaddr.h"
// #include "threads/synch.h"
// #include <stdio.h>
// 
// struct block *swap_device;
// 
// static struct bitmap *swap_map;
// 
// static size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;
// static size_t swap_pages;
// 
// /* Initialize table */
// 
// void swap_init(){
// 
//     /* Initialize device(disk) */
//     swap_device = block_get_role (BLOCK_SWAP);
//     /* Error Check */
//     if (swap_device == NULL)
//         PANIC ("Error: Cannot initialize disk");
// 
//     /* Intialize map */
//     swap_map = bitmap_create (swap_pages);
//     /* Error Check */
//     if (swap_map == NULL)
//         PANIC ("Error: Cannot initialize map");
// 
//     /* initialize all bits to be true */
//     bitmap_set_all (swap_map, true);
// 
// }
// 
// /* Write to disk */
// swap_index_t vm_swap_out (void *page){
// 
//     /* Search for available region */
//     size_t swap_idx = bitmap_scan(swap_map, 0, 1, true);
// 
//     size_t i;
//     for(i = 0; i < SECTORS_PER_PAGE; i++){
//         /* Write page to swap slot */
//         block_write(swap_device,
//                     swap_idx * SECTORS_PER_PAGE + i,
//                     page + (BLOCK_SECTOR_SIZE * i));
//     }
// 
//     /* Set bitmap and initialize slot */
//     bitmap_set(swap_map, swap_idx, false);
// 
//     return swap_idx;
// }
// 
// /* Write to index */
// void vm_swap_in (swap_index_t swap_idx, void *page){
// 
//     size_t i;
//     for(i = 0; i < SECTORS_PER_PAGE; i++){
// 
//         block_read(swap_device, (swap_idx * SECTORS_PER_PAGE) + i,
//                    page + (i * BLOCK_SECTOR_SIZE));
//     }
// 
//     bitmap_set(swap_map, swap_idx, true);
// }
// 
// /* Free region */
// void vm_swap_free (swap_index_t swap_idx){
// 
//     if(bitmap_test(swap_device, swap_idx)) PANIC("Error: Unassigned free block request");
//     bitmap_flip(swap_device, swap_idx);
// 
// }

