/*
 * Modified by:
 * Matthew Tawil (mt33924)
 * Allen Pan (xp572)
 * Ze Lyu (zl5298)
 */

#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"

#define LOGGING_LEVEL 6

#include "lib/log.h"

static thread_func start_process NO_RETURN;
static bool load (const char ** argv, int argc, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created.
   Input: the entire string token from command line input (or maybe exec)
*/
tid_t process_execute (const char *cmdline) {
  char *file_name;
  struct thread *cur = thread_current();
  tid_t tid;
  // NOTE:
  // To see this print, make sure LOGGING_LEVEL in this file is <= L_TRACE (6)
  // AND LOGGING_ENABLE = 1 in lib/log.h
  // Also, probably won't pass with logging enabled.
  log(L_TRACE, "Started process execute: %s", cmdline);


  /* Make a copy of cmdline argument.
     Otherwise there's a race between the caller and load(). */
  char *cmdline_cp = palloc_get_page(0);  // allocate from kernel physical memory; where does this pointer reside?
  if (cmdline_cp == NULL) {
     return TID_ERROR;
  }
  strlcpy (cmdline_cp, cmdline, PGSIZE);

  file_name = palloc_get_page(0);
  if (file_name == NULL)  {
     palloc_free_page(cmdline_cp);
     return TID_ERROR;
  }
  strlcpy (file_name, cmdline, PGSIZE);
  // extract file name
  char *saveptr;
  strtok_r(file_name, " ", &saveptr); // file_name will only contain the file name now

  // the following implementation cloud have been improved if we create a PCB here and pass it into thread_create as aux

  /* Create a new thread to execute the executable. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, cmdline_cp);
  struct pcb_t *pcb;
  if (tid == TID_ERROR) {
     palloc_free_page (cmdline_cp);
  }
  else {  // if child wasn't created, sema wouldn't have been inited
     struct thread *child = get_thread_by_tid(tid);  // get the newly created thread (this implementation is kinda bad)
     list_push_back(&cur->child_list, &child->pcb->elem);  // add the new child to parent's child list
     pcb = child->pcb; // if child is killed, sema still needs access properly, so have a separate pointer for pcb is necessary
     sema_init(&pcb->process_wait_sema, 0);  // think should init here, cuz only necessary to wait when process_wait is called
     sema_down(&pcb->process_exec_sema);  // sema inited in thread_create
 }
  palloc_free_page (file_name);
  if (pcb->exit_status == -1) return -1;  // child was killed by kernel during creation
  return tid;
}

/*
A thread function that loads a user process and starts it running.
Since already inside the thread, only need to pass the command (which is not contained in struct thread)
Note commend_ includes the command/file name
*/
static void start_process (void *command_) {
   char *command = command_;
   struct intr_frame if_;
   bool success = false;
   struct thread *cur = thread_current();

   /* Initialize interrupt frame and load executable. */
   memset (&if_, 0, sizeof if_);
   if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
   if_.cs = SEL_UCSEG;
   if_.eflags = FLAG_IF | FLAG_MBS;

   // extract the filename
   char **argv = (const char**) palloc_get_page(0); // allocate kernel space for temp usage, will later be freed
   if (argv == NULL) {
      cur->pcb->exit_status = -1;  // kernel terminate the process, so exit code is -1
      printf("%s: exit(%d)\n", cur->name, cur->pcb->exit_status);
      thread_exit();  // will destory this thread, while keeping its pcb
   }
   else {
      int argc = 0;
      char *saveptr;
      // argv[argc++] = strtok_r(command, " ", &saveptr);
      // while (argv[argc] = strtok_r(NULL, " ", &saveptr) != NULL)  argc++;  // this way, terminated with NULL
      for (char *token = strtok_r(command, " ", &saveptr); token != NULL;
      token = strtok_r(NULL, " ", &saveptr))
         argv[argc++] = token;
      if (argv[argc-1] != NULL) argv[argc] = NULL;  // must terminate by NULL; this is needed for exec-arg test case
      success = load (argv, argc, &if_.eip, &if_.esp);
   }

   // deny write to the current running executable
   cur->pcb->executable = filesys_open(argv[0]);
   if (cur->pcb->executable != NULL)
      file_deny_write(cur->pcb->executable);

   sema_up(&cur->pcb->process_exec_sema);

   palloc_free_page(argv);  // args already pushed to user stack, so can free them here
   palloc_free_page(command);
   /* If load failed, quit. */
   if (!success) {
      cur->pcb->exit_status = -1;  // kernel terminate the process, so exit code is -1
      printf("%s: exit(%d)\n", cur->name, cur->pcb->exit_status);
      thread_exit();  // will destory this thread, while keeping its pcb
   }

   /* Start the user process by simulating a return from an
   interrupt, implemented by intr_exit (in
   threads/intr-stubs.S).  Because intr_exit takes all of its
   arguments on the stack in the form of a `struct intr_frame',
   we just point the stack pointer (%esp) to our stack frame
   and jump to it. */
   asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
   NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int process_wait (tid_t child_tid) {
   struct thread *cur = thread_current();
   struct list *child_list = &cur->child_list;  // must use pointer !! otherwise memory optimize out cur for some reason (maybe not enough memory)
   struct pcb_t *child;
   struct list_elem *e;  // used to find child thread
   int child_exit_status;

   // return -1 if child_tid is TID_ERROR, because kernel kills it during its creation
   if (child_tid == TID_ERROR) return -1;

   // find the child thread specified by child_tid
   for (e = list_begin (child_list); e != list_end (child_list);
      e = list_next (e)) {
      // convert list_element into the pcb_t that contains it
      child = list_entry(e, struct pcb_t, elem);
      if (child->pid == child_tid) break; // found child
   }

   if (e == list_end(child_list)) return -1;  // child_tid is not a child of current process
   if (child->already_wait) return -1;    // wait() or process_wait() already called upon this child
   else child->already_wait = 1; // mark wait() already called

   if (!child->killed)
      sema_down(&child->process_wait_sema);
   ASSERT (child->killed == 1);
   child_exit_status = child->exit_status;
  // remove child from parent's child list
  list_remove(e);

  // finally, free child's pcb, since never will access it again
  // child thread already destroyed in thread_exit, kept pcb for exit_status
  palloc_free_page(child);

  return child_exit_status;
}

/* Free the current process's resources. */
void process_exit (void) {
   struct thread *cur = thread_current ();
   uint32_t *pd;

   #ifdef USERPROG
   /* clean up this thread's children, free the killed ones, mark the rest orphan */
   while (!list_empty(&cur->child_list)) {
      struct list_elem * e = list_pop_front(&cur->child_list);
      struct pcb_t *child = list_entry(e, struct pcb_t, elem);
      if (child->killed) palloc_free_page(child);
      else child->orphan = 1;
   }

   /* close all opened files in current thread */
   while (!list_empty(&cur->file_table)) {
      struct list_elem *e = list_pop_front(&cur->file_table);
      struct file_table_entry * fte = list_entry(e, struct file_table_entry, elem);
      file_close(fte->file);
      palloc_free_page(fte);
   }

   cur->pcb->killed = 1;   // mark this thread killed
   sema_up(&cur->pcb->process_wait_sema);  // hope this does not cause problem when sema_up is never reached

   // if this thread is an orphan, can free its pcb it right now; thread itself will be freed upon return
   if (cur->pcb->orphan) palloc_free_page(cur->pcb);

   // allow write to the current executable again if opened successfully
   if (cur->pcb->executable != NULL) {
      file_allow_write(cur->pcb->executable);
      file_close(cur->pcb->executable);
   }
   #endif

   #ifdef VM
   // desctroys all supplemental page table entries, also frees all frames allocated for this process
   // the table itself will be freed when thread is freed (since it's a struct, not a pointer)
   sup_page_table_destroy(&cur->sup_page_table);
   #endif

   /* Destroy the current process's page directory and switch back
   to the kernel-only page directory. */
   pd = cur->pagedir;
   if (pd != NULL)
   {
      /* Correct ordering here is crucial.  We must set
      cur->pagedir to NULL before switching page directories,
      so that a timer interrupt can't switch back to the
      process page directory.  We must activate the base page
      directory before destroying the process's page
      directory, or our active page directory will be one
      that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
   }




}


/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const char **argv, int argc);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from file name (argv[0]) into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char **argv, int argc, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (argv[0]);
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", argv[0]);
      goto done;
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", argv[0]);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;  // the virtual addr of where the program resides
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, argv, argc))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);  // together must be multiple of page size
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  struct thread *cur = thread_current ();
  struct sup_pte_data_filesys aux; // hope this is small enough for now...

  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Divide the filling process to muliple attempts
         In each attempt, we allocate one page of memory (also get one frame of PM for this page, and map them)
         We will read PAGE_READ_BYTES bytes from FILE and zero the final PAGE_ZERO_BYTES bytes
         depend on the size of READ_BYTES
      */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      #ifdef VM
      // Lazy load
      aux.page_read_bytes = page_read_bytes;
      aux.page_zero_bytes = page_zero_bytes;
      aux.file = file;
      aux.file_ofs = ofs;
      aux.writable = writable;

      if (!spte_create_by_type(&cur->sup_page_table, upage, NULL, FROM_FILESYS, &aux))
         return false;
      ofs += page_read_bytes;  // offset for each page will be different

      #else
      /* allocate a frame of memory, associate the virtual page upage to it */
      uint8_t *frame = frame_allocate (PAL_USER, upage); // allocate from user pool
      if (frame == NULL)
        return false;

      /* Load this page (frame). */
      if (file_read (file, frame, page_read_bytes) != (int) page_read_bytes)
        {
          frame_free (frame);
          return false;
        }
      memset (frame + page_read_bytes, 0, page_zero_bytes);

      /*
         Add the page to the process's address space.
         This is where the actual mapping b/w virtual page and physical frame happens
      */
      if (!install_page (upage, frame, writable))
        {
          frame_free (frame);
          return false;
        }
      #endif

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/*
   Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory.
*/
static bool setup_stack (void **esp, const char **argv, int argc) {
  uint8_t *frame;
  bool success = false;
  void * stack_bound_addr = (uint8_t *) PHYS_BASE - PGSIZE;  // one page for stack; virtual address
  frame = frame_allocate (PAL_USER | PAL_ZERO, stack_bound_addr);
  if (frame != NULL)
    {
      success = install_page (stack_bound_addr, frame, true);  // mapping

      if (success) {
         *esp = PHYS_BASE;

         // first, push the arguments character by character
         for (int i=argc-1; i>=0; i--) {
            int len = strlen(argv[i]) + 1; // includes the byte for terminator
            *esp -= len;
            memcpy(*esp, argv[i], len); // copy content pointed by argv[i] to memory pointed by *esp
            argv[i] = (char *)*esp; // the pointer to that argument now point to where the argument resides in the stack
         }

         // word align
         *esp = (void *)((uint32_t)(*esp) & 0xfffffffc);

         // now push the arguemnt pointers to the stack, including the NULL terminator
         for (int i=argc; i>=0; i--) {
            *esp -= sizeof(char *);
            *(char **)*esp = argv[i];
         }


         //argv = (char **) *esp;   // point the argv (char**) to where the pointer to first string resides in the stack
         // now push the argv itself
         *esp -= sizeof(char **);
         *(char ***)*esp = (char **)(*esp + sizeof(char **)); // this is the argv on stack, points to argv[0] on the stack
         //*(uint32_t *)esp = (uint32_t) argv; // this is not ideal, but how to cast so that the content of that location is char **?

         // now push argc
         *esp -= sizeof(int);  // a word
         *(int *)*esp = argc;

         // now push the dummy return address
         *esp -= sizeof (void (*) (void)); // should be 4
         *(int *)*esp = 0;
      }
      else  // mapping failed
        frame_free (frame);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to user
physical address FRAME to the page table.
If WRITABLE is true, the user process may modify the page;
otherwise, it is read-only.
UPAGE must not already be mapped.
FRAME should probably be a page(frame) obtained from the user pool
with palloc_get_page(frame_allocate).
Returns true on success, false if UPAGE is already mapped or
if memory allocation fails. */
static bool
install_page (void *upage, void *frame, bool writable)
{
   struct thread *t = thread_current ();

   /* Verify that there's not already a page at that virtual
   address, then map our page there.
   also fail when not able to allocate a page for page table
   short circuit evaluation so set_page not called if get_page != NULL
   */
   if (pagedir_get_page (t->pagedir, upage) == NULL && pagedir_set_page (t->pagedir, upage, frame, writable)) {
      return spte_create_by_type(&t->sup_page_table, upage, frame, ON_FRAME, NULL);  // create a sup page table entry for this mapping
   }
   else return false;
}
