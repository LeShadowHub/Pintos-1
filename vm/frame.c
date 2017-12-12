/*************************************
 *             frame.c               *
 ************************************/

#include "vm/frame.h"
#include <list.h>
#include "threads/synch.h"
#include "userprog/process.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "threads/malloc.h"

struct list frame_table;

static struct lock lock_frame;
static struct list_elem * cur_e;     // used for replacement clock algorithm
static struct frame_table_entry * get_FTE_by_frame(void *frame);
static struct frame_table_entry * get_evict_FTE (void);
static void _frame_free (void * frame, bool free_frame);


// frame table entry
struct frame_table_entry {
   void * frame;      // pointer to the base addr of the physical frame that's being occupied
   void * page;      // virtual address (which should be at the beginning of a page) that associated with this frame
   struct thread * thread;    // pointer to the associated process
   struct list_elem elem;
};

void frame_table_init(void) {
   list_init(&frame_table);
   lock_init(&lock_frame);
   cur_e = list_head(&frame_table);
}

/*
The actual mapping process does not happen here.
input: page is a pointer to the base virtual address of the page to be allocated
NOTES: My understanding is, palloc is used to get a FRAME (or page, since they
are same in size) from the PHYSICAL memory, despite the naming. And in physical
memory, there are regions dedicated for the kernel.
*/
void * frame_allocate(enum palloc_flags flag, void *page) {
   // create critical section
   lock_acquire(&lock_frame);
   // get a frame in the physical memory allocated
   void * frame = palloc_get_page(flag);
   if (frame == NULL) {     // frame allocation failed, swap out a frame
      struct frame_table_entry *fte_evicted = get_evict_FTE();
      // clears Present bit, page itself not freed. other bits are preserved (such as dirty bit)
      pagedir_clear_page(fte_evicted->thread->pagedir, fte_evicted->page);
      struct sup_page_table_entry * spte = get_spte (&fte_evicted->thread->sup_page_table, fte_evicted->page);
      ASSERT(spte != NULL);
      // In Pintos, every user virtual page is aliased to its kernel virtual page. You must manage these aliases somehow.


      if (!(pagedir_is_dirty(fte_evicted->thread->pagedir, fte_evicted->page) || pagedir_is_dirty(fte_evicted->thread->pagedir, fte_evicted->frame))
            && spte->page_type == FROM_FILESYS)
      {  // not dirty and it's from filesys
         spte->frame = NULL;
         spte_to_filesys (spte);
      }
      else { // otherwise, write the frame to swap slot
            size_t swap_index = swap_out(fte_evicted->frame);
            if (swap_index == SWAP_ERROR) PANIC("Error: No free swap slot");
            spte_swap_out(spte,swap_index);
      }

      _frame_free(fte_evicted->frame, true);  // free this frame, then bring in a new frame
      frame = palloc_get_page(flag);
      ASSERT(frame != NULL);
   }

   // create a frame table entry
   struct frame_table_entry *fte = (struct frame_table_entry *) malloc(sizeof(struct frame_table_entry));
   if (fte == NULL) {
      lock_release(&lock_frame);
      return NULL;
   }
   fte->thread = thread_current();
   fte->frame = frame;
   fte->page = page;

   // insert entry to frame table
   list_push_back(&frame_table, &fte->elem);
   lock_release(&lock_frame);
   return frame;
}

/* Free the frame and delete it from frame table */
void frame_free(void * frame) {
   lock_acquire(&lock_frame);
   _frame_free(frame, true);
   lock_release(&lock_frame);
}

/* Remove frame table entry but not free the frame*/
void frame_table_entry_delete(void * frame) {
   lock_acquire(&lock_frame);
   _frame_free(frame, false);
   lock_release(&lock_frame);
}

static void _frame_free (void * frame, bool free_frame) {
   ASSERT (lock_held_by_current_thread(&lock_frame) == true);
   struct frame_table_entry *fte = get_FTE_by_frame(frame);
   list_remove(&fte->elem);
   if (free_frame) palloc_free_page(frame);
   free(fte);
}

/* does a linear search to find the FTE associated with this frame*/
static struct frame_table_entry * get_FTE_by_frame(void *frame) {
   ASSERT (lock_held_by_current_thread(&lock_frame) == true);

   struct list_elem *e;
   struct frame_table_entry *fte;
   enum intr_level old_level;

   old_level = intr_disable ();

   for (e = list_begin (&frame_table); e != list_end (&frame_table);
        e = list_next (e))
     {
      fte = list_entry (e, struct frame_table_entry, elem);
      if (fte->frame == frame) break;  // comparing the value of pointer (aka physical address), should be fine
     }
   intr_set_level (old_level);
   if (e == list_end(&frame_table)) return NULL;
   return fte;
}

/*
   determine which page to replace with clock algorithm
   did not concern dirty bit
   return the frame table entry to be replaced
*/
static struct frame_table_entry * get_evict_FTE (void) {
   ASSERT(!list_empty(&frame_table));
   if (cur_e == list_head (&frame_table))  cur_e = list_next(cur_e);  // a workaround; since cur_e can only be initialized to head
   struct frame_table_entry *cur = list_entry (cur_e, struct frame_table_entry, elem);
   struct thread * t = thread_current();

   while (pagedir_is_accessed(t->pagedir, cur->page)) {
      pagedir_set_accessed(t->pagedir, cur->page, false);
      cur_e = list_next(cur_e);
      if (cur_e == list_end(&frame_table))  cur_e = list_begin(&frame_table);
      cur = list_entry (cur_e, struct frame_table_entry, elem);
   }
   // this frame_table_entry is going to be evicted, so change cur_e (the clock ptr) to the next entry
   cur_e = list_next(cur_e);
   return cur;
}
