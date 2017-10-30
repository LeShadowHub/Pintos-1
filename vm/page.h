/*************************************
 *             page.h                *
 ************************************/
#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include "filesys/filesys.h"


enum page_type_t {
   ON_FRAME,
   ALL_ZERO,      // only used for file loading
   SWAP_SLOT,
   FROM_FILESYS
};

struct sup_pte_data_filesys{
   size_t page_read_bytes;
   size_t page_zero_bytes;
   struct file * file;
   off_t file_ofs;
   bool writable;
};

struct sup_pte_data_swapslot{
   size_t swap_index;
   bool writable;
};


struct sup_page_table_entry {
   void * page;
   void * frame;
   bool writable;
   bool present;                 // whether this page is in physical memory
   enum page_type_t page_type;

   // filesys
   struct file * file;
   off_t file_ofs;
   size_t page_read_bytes;
   size_t page_zero_bytes;

   // swap slot
   size_t swap_index;            // swap table index

   struct hash_elem elem;
};

bool sup_page_table_init(struct hash *);
void sup_page_table_destroy(struct hash *);
struct sup_page_table_entry * spte_create(struct hash * spt, void * page, void * frame);
struct sup_page_table_entry * spte_create_by_type(struct hash * spt, void * page, void * frame, enum page_type_t page_type, void * aux);
bool load_page(struct sup_page_table_entry * spte);
struct sup_page_table_entry * get_spte (struct hash * spt, const void * page);
void spte_to_filesys (struct sup_page_table_entry * spte);
void spte_swap_out (struct sup_page_table_entry * spte, size_t swap_index);
bool grow_stack (void * start_page);


#endif /* page_h */
