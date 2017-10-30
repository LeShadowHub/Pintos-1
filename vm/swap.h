/*************************************
 *             swap.h                *
 ************************************/

#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <bitmap.h>

#define SWAP_ERROR  BITMAP_ERROR
bool swap_init();
void swap_destroy(void);
size_t swap_out (void *frame);
void swap_in (size_t slot_index, void * frame);
void swap_free (size_t slot_index);



#endif /* swap_h */
