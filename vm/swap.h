/*************************************
 *             swap.h                *
 * Created by Matthew T on 10/23/17. *
 ************************************/

#ifndef VM_SWAP_H
#define VM_SWAP_H

/* Swap Space Functions */


/* Initialize Table */
void vm_swap_init (void);

/* Write contents of page to swap disk */
swap_index_t vm_swap_out (void*);

/* Write contents into swap index */
void vm_swap_in (swap_index_t, void*);

/* Free up unused swap region */
void vm_swap_free (swap_index_t);




#endif /* swap_h */
