/*************************************
 *             frame.c               *
 ************************************/

#include "vm/frame.h"
#include <list.h>
#include "threads/synch.h"
#include "userprog/process.h"
#include "threads/thread.h"
#include "threads/interrupt.h"


struct list frame_table;

static struct lock lock_frame;
static struct frame_table_entry * get_FTE_by_frame(void *frame);

// frame table entry
struct frame_table_entry {
   void * frame;      // pointer to the base addr of the physical frame that's being occupied
   void * page;      // virtual address (which should be at the beginning of a page) that associated with this frame
   struct thread * proc;    // pointer to the associated process
   struct list_elem elem;
};

void frame_table_init(void) {
   list_init(&frame_table);
   lock_init(&lock_frame);
}

/*
The actual mapping process does not happen here.
input: page is a pointer to the base virtual address of the page to be allocated
notes: My understanding is, palloc is used to get a FRAME (or page, since they are same in size) from the PHYSICAL memory, despite the naming. And in physical memory, there are regions dedicated for the kernel.
*/
void * frame_allocate(enum palloc_flags flag, void *page) {
   // create critical section
   lock_acquire(&lock_frame);
   // get a frame in the physical memory allocated
   void * frame = palloc_get_page(flag);
   if (frame == NULL) {     // page allocation failed
      lock_release(&lock_frame);
      return frame;
   }
   // create a frame table entry
   struct frame_table_entry *fte = (struct frame_table_entry *) malloc(sizeof(struct frame_table_entry));
   fte->proc = thread_current();
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
   struct frame_table_entry *fte = get_FTE_by_frame(frame);
   list_remove(&fte->elem);
   palloc_free_page(frame);
   free(fte);
   lock_release(&lock_frame);
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
