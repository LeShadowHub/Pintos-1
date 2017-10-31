/*************************************
 *             page.c                *
 ************************************/

#include "vm/page.h"
#include <stdlib.h>
#include <string.h>
#include <debug.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"


static bool load_page_allzero (struct sup_page_table_entry * spte);
static bool load_page_from_swapslot (struct sup_page_table_entry * spte);
static bool load_page_from_filesys (struct sup_page_table_entry * spte);
static unsigned spt_hash_func (const struct hash_elem *pte_, void *aux UNUSED);
static bool spt_less_func (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
static void spt_destroy_func (struct hash_elem *spte_, void *aux UNUSED);

/*
   spt is a pre-process hash table already allocated during thread creation
*/
bool sup_page_table_init(struct hash * spt) {
   return hash_init(spt, spt_hash_func, spt_less_func, NULL);
}

void sup_page_table_destroy(struct hash * spt) {
   ASSERT(spt != NULL);

   hash_destroy(spt, spt_destroy_func);
}

/*
   supplmental page table entry create
   virtual PAGE to physical FRAME are mapped (or FRAME not specified)
   Fail on either memory allocation failure or SPTE already in table
   Can create 2 types of page: ON_FRAME, FROM_FILESYS, ALL_ZERO
   SWAP_SLOT is not created, instead swapped out
*/
struct sup_page_table_entry * spte_create_by_type(struct hash * spt, void * page, void * frame, enum page_type_t page_type, void * aux) {
   struct sup_page_table_entry * spte = (struct sup_page_table_entry *) malloc(sizeof(struct sup_page_table_entry));
   if (spte == NULL) return NULL;

   spte->page = page;
   spte->frame = frame;

   spte->page_type = page_type;
   switch (page_type) {
      case ON_FRAME:
         spte->writable = true;
         spte->present = true;
         break;
      case ALL_ZERO:
         spte->writable = true;
         spte->present = false;
         break;
      case SWAP_SLOT:
         PANIC ("Error: Cannot directly create a page in swap slot");
      //    struct sup_pte_data_swapslot * data = (struct sup_pte_data_swapslot *) aux;
      //    spte->swap_index = data->swap_index;
      //    spte->writable = data->writable;
      //    spte->present = false;
         break;
      case FROM_FILESYS: {
         struct sup_pte_data_filesys * data = (struct sup_pte_data_filesys *) aux;
         if (data->page_read_bytes == 0) {
            // all zero page, nothing to read from executable
            // I would have directly loaded it to a frame, but requirement says "creating a new page consisting of all zeroes at the first page fault."
            // Also considered to have a condition in load_page_from_filesys to work this out, but page_type needs to become on_frame for all_zero after load, the code would be messy
            spte->page_type = ALL_ZERO;
            spte->writable = data->writable; // or maybe change this to always write
            spte->present = false;
         } else {
            spte->page_read_bytes = data->page_read_bytes;
            spte->page_zero_bytes = data->page_zero_bytes;
            spte->file = data->file;
            spte->file_ofs = data->file_ofs;
            spte->writable = data->writable;
            spte->present = false;
         }
      break;
      }
   }
   struct hash_elem * retval = hash_insert(spt, &spte->elem);
   if (retval != NULL) {  // spt is already in the hash table
      free(spte);
      return NULL;
   }
   return spte;
}


void spte_swap_out (struct sup_page_table_entry * spte, size_t swap_index) {
   spte->page_type = SWAP_SLOT;
   spte->present = false;
   spte->frame = NULL;
   spte->swap_index = swap_index;
}

void spte_to_filesys (struct sup_page_table_entry * spte) {
   spte->present = false;
   spte->frame = NULL;
}

/* need to allocate the pages between START_PAGE and current stack bound
   If this function failed, then stack memory allocation is messed up, but process is going to exit anyways so don't care*/
bool grow_stack (void * start_page) {
   struct thread * cur = thread_current();
   void * cur_stack_bound_addr = cur->cur_stack_bound_addr;
   for (int i=0; start_page + i * PGSIZE < cur_stack_bound_addr; i++) {
      // grow stack by creating an all zero frame (need to load page right after due to how the function was written)
      struct sup_page_table_entry * spte = spte_create_by_type(&cur->sup_page_table, start_page + i * PGSIZE, NULL, ALL_ZERO, NULL);
      if (spte == NULL) return false;
      if (!load_page(spte)) return false;
      cur->cur_stack_bound_addr = start_page;
   }
   return true;
}


bool load_page(struct sup_page_table_entry * spte) {
   bool success;
   switch (spte->page_type) {
      case ON_FRAME:
         PANIC ("Error: Trying to load a page that's already on frame");
         break;
      case ALL_ZERO:
         success = load_page_allzero(spte);
         break;
      case SWAP_SLOT:
         success = load_page_from_swapslot(spte);
         break;
      case FROM_FILESYS:
         success = load_page_from_filesys (spte);
         break;
   }
   return success;
}


/* Returns the SPTE containing the given virtual page,
or a null pointer if no such SPTE exists. */
struct sup_page_table_entry * get_spte (struct hash * spt, const void * page) {
   ASSERT ((uint32_t) page % PGSIZE == 0);
   struct sup_page_table_entry spte;
   struct hash_elem *e;

   spte.page = page;
   e = hash_find (spt, &spte.elem);
   return e != NULL ? hash_entry (e, struct sup_page_table_entry, elem) : NULL;
}


static bool load_page_allzero (struct sup_page_table_entry * spte) {
   uint8_t *frame = frame_allocate (PAL_USER | PAL_ZERO, spte->page); // allocate from user pool and set to zero
   if (frame == NULL) return false;

   struct thread *cur = thread_current ();
   if (pagedir_set_page (cur->pagedir, spte->page, frame, spte->writable)){
      spte->present = true;
      spte->frame = frame;
      spte->page_type = ON_FRAME;
   }
   else {
      frame_free(frame);
      return false;
   }
   return true;
}

static bool load_page_from_swapslot (struct sup_page_table_entry * spte) {
   uint8_t *frame = frame_allocate (PAL_USER, spte->page); // allocate from user pool
   if (frame == NULL) return false;

   struct thread *cur = thread_current ();
   // ASSERT(!pagedir_is_present(cur->pagedir, spte->page));
   if (pagedir_set_page (cur->pagedir, spte->page, frame, spte->writable)){
      spte->present = true;
      spte->frame = frame;
      spte->page_type = ON_FRAME;
   }
   else {
      frame_free(frame);
      return false;
   }
   swap_in(spte->swap_index, frame);  // swap in after assuring all operations succeed (since swap_in wont fail)

   return true;
}

static bool load_page_from_filesys (struct sup_page_table_entry * spte) {
   ASSERT(spte->frame == NULL);

   /* allocate a frame of memory, associate the virtual page upage to it */
   uint8_t *frame = frame_allocate (PAL_USER, spte->page); // allocate from user pool
   if (frame == NULL) return false;

   file_seek(spte->file, spte->file_ofs);
   /* Load this page (frame). */
   if (file_read (spte->file, frame, spte->page_read_bytes) != (int) spte->page_read_bytes) {
      frame_free (frame);
      return false;
   }
   memset (frame + spte->page_read_bytes, 0, spte->page_zero_bytes);

   /*
   Add the page to the process's address space.
   This is where the actual mapping b/w virtual page and physical frame happens
   */
   struct thread *cur = thread_current ();

   if (pagedir_set_page (cur->pagedir, spte->page, frame, spte->writable)){
      spte->present = true;
      spte->frame = frame;
      // NOT changing page_type, only change present flag, so we still get page from FILESYS after it's evicted
   }
   else {
      frame_free(frame);
      return false;
      // since file read always start from the specified offset, nothing to restore
   }
   return true;
}


static unsigned spt_hash_func (const struct hash_elem *spte_, void *aux UNUSED)
{
  const struct sup_page_table_entry *spte = hash_entry (spte_, struct sup_page_table_entry, elem);

  return hash_bytes (&spte->page, sizeof spte->page);
}

static bool spt_less_func (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct sup_page_table_entry *a = hash_entry (a_, struct sup_page_table_entry, elem);
  const struct sup_page_table_entry *b = hash_entry (b_, struct sup_page_table_entry, elem);

  return a->page < b->page;
}

static void spt_destroy_func (struct hash_elem *spte_, void *aux UNUSED) {
   const struct sup_page_table_entry *spte = hash_entry (spte_, struct sup_page_table_entry, elem);
   if (spte->present) { // present and frame might be redundant
      ASSERT(spte->page_type == ON_FRAME || spte->page_type == FROM_FILESYS);
      frame_table_entry_delete(spte->frame);   /* Remove fte but not free the frame, since it's going to be freed by pagedir_destroy*/
   }
   else if (spte->page_type == SWAP_SLOT) {
      swap_free(spte->swap_index);
   }


   free(spte);
}

