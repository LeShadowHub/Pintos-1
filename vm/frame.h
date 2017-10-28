/*************************************
*             frame.h               *
************************************/
#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/palloc.h"

void frame_table_init(void);
void * frame_allocate(enum palloc_flags flag, void *page);
void frame_free(void * frame);

#endif
