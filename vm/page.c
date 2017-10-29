/*************************************
 *             page.c                *
 ************************************/

#include "vm/page.h"
#include <stdlib.h>
#include <string.h>
#include <debug.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"



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
*/
bool spte_create_by_type(struct hash * spt, void * page, void * frame, enum page_type_t page_type, void * aux) {
   struct sup_page_table_entry * spte = (struct sup_page_table_entry *) malloc(sizeof(struct sup_page_table_entry));
   if (spte == NULL) return false;

   spte->page = page;
   spte->frame = frame;
   spte->page_type = page_type;

   switch (page_type) {
      case ON_FRAME:
         spte->present = true;
         break;
      case ALL_ZERO:
         break;
      case SWAP_SLOT:
         break;
      case FROM_FILESYS: {
         struct sup_pte_data_filesys * data = (struct sup_pte_data_filesys *) aux;
         spte->page_read_bytes = data->page_read_bytes;
         spte->page_zero_bytes = data->page_zero_bytes;
         spte->file = data->file;
         spte->file_ofs = data->file_ofs;
         spte->writable = data->writable;
         spte->present = false;  // overwrite the present flag
      break;
      }

   }

   struct hash_elem * retval = hash_insert(spt, &spte->elem);
   if (retval != NULL) {  // spt is already in the hash table
      free(spte);
      return false;
   }

   return true;
}

bool load_page(struct sup_page_table_entry * spte) {
   bool success;
   switch (spte->page_type) {
      case ON_FRAME:

         break;
      case ALL_ZERO:
         break;
      case SWAP_SLOT:
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
  struct sup_page_table_entry spte;
  struct hash_elem *e;

  spte.page = page;
  e = hash_find (spt, &spte.elem);
  return e != NULL ? hash_entry (e, struct sup_page_table_entry, elem) : NULL;
}




static bool load_page_from_filesys (struct sup_page_table_entry * spte) {
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
      spte->page_type = ON_FRAME;
   }
   else {
      frame_free(frame);
      return false;
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
   if (spte->frame != NULL) {
      ASSERT(spte->page_type == ON_FRAME);
      frame_table_entry_delete(spte->frame);   /* Remove fte but not free the frame, since it's going to be freed by pagedir_destroy*/
   }
   free(spte);
}
