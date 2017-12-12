#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();

  thread_current()->cwd = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool filesys_create (const char *path, off_t initial_size) {
   if (strlen(path) == 0) return false;
   block_sector_t inode_sector = 0;
   char * dirname = malloc(strlen(path)+1);   // strlen(dir) will be th maximum needed
   char * filename = malloc(strlen(path)+1);  // filename can be a directory's name
   memset(dirname, 0, strlen(path)+1);
   memset(filename, 0, strlen(path)+1);

   dir_extract_name(path, dirname, filename);
   struct dir *dir = dir_open_path (dirname);

   bool success = (dir != NULL
      && strlen(filename) != 0  // length == 0 means we are opening a directory, this is the wrong function
      && free_map_allocate (1, &inode_sector)
      && inode_create (inode_sector, initial_size, false)
      && dir_add (dir, filename, inode_sector));

   if (!success && inode_sector != 0)
      free_map_release (inode_sector, 1);
   if (dir != NULL) dir_close (dir);
   if (dirname != NULL) free(dirname);
   if (filename != NULL) free(filename);
   return success;
}

bool filesys_mkdir (const char *path) {
   if (strlen(path) == 0) return false;
   char dirname[strlen(path)+1];   // strlen(dir) will be th maximum needed
   char filename[strlen(path)+1];  // filename can be a directory's name
   // dirname might be empty, but filename must not be empty
   memset(dirname, 0, sizeof dirname);
   memset(filename, 0, sizeof filename);
   dir_extract_name(path, dirname, filename);
   struct dir *dir = dir_open_path (dirname);
   block_sector_t inode_sector = 0;
   bool success = (dir != NULL
                  && strlen(filename) != 0  // length == 0 should be forbidden. e.g. a/bc/ is not a directory name
                  && free_map_allocate (1, &inode_sector)
                  && dir_create(inode_sector, dir)
                  && dir_add(dir, filename, inode_sector));  // filename must not be in dir already
   if (!success && inode_sector != 0)
      free_map_release(inode_sector, 1);
   if (dir != NULL) dir_close (dir);
   return success;
}


/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails.
   Return NULL on fail
*/
struct file *
filesys_open (const char *path)
{
   if (strlen(path) == 0) return NULL;
   char dirname[strlen(path)+1];   // strlen(dir) will be th maximum needed
   char filename[strlen(path)+1];  // filename can be a directory's name
   // dirname might be empty, but filename must not be empty
   memset(dirname, 0, sizeof dirname);
   memset(filename, 0, sizeof filename);

   dir_extract_name(path, dirname, filename);

   struct dir *dir = dir_open_path (dirname);  // empty path should return cwd
   if (dir == NULL) return NULL;  // path not exist

   struct inode *inode = NULL;
   bool success = true;
   // if filename is empty,meaning we are opening a directory specified by dirname
   if (strlen(filename) == 0) {
      // already know this is a dir, but need to generate a dummy file. Will be resolved in sys_open
      inode = dir_get_inode (dir);
   } else {
      success = dir_lookup (dir, filename, &inode);  // filename doesnt exist under directory DIR
   }

  dir_close (dir);
  if (!success) return NULL;

  if (inode_is_removed(inode)) return NULL;  // might already be removed
  return file_open (inode);  // if inode is valid, open it
}

/* Deletes the file named NAME.
Returns true if successful, false on failure.
Fails if no file named NAME exists,
or if an internal memory allocation fails.
*/
bool filesys_remove (const char *path) {
   if (strlen(path) == 0) return false;
   char dirname[strlen(path)+1];   // strlen(dir) will be th maximum needed
   char filename[strlen(path)+1];  // filename can be a directory's name
   // dirname might be empty, but filename must not be empty
   memset(dirname, 0, sizeof dirname);
   memset(filename, 0, sizeof filename);
   dir_extract_name(path, dirname, filename);
   if (strlen(filename) == 0) return false;

   struct dir *dir = dir_open_path (dirname);
   bool success = dir != NULL && dir_remove (dir, filename);
   dir_close (dir);

   return success;
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, NULL))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

