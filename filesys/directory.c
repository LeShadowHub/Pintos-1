#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* A directory. */
struct dir
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

 static bool dir_is_empty(struct dir * dir);

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool dir_create(block_sector_t new_sector, struct dir * parent) {

   block_sector_t parent_sector;
   if (parent != NULL) parent_sector = inode_get_inumber(parent->inode);
   else parent_sector = new_sector; // root's parent is itself // dont know if this causes issue
   // two entries: . and ..
   bool ret = inode_create (new_sector, 2 * sizeof (struct dir_entry), true);
   if (!ret) return ret;

   struct inode *inode = inode_open (new_sector);  // adding this directory to list of inode
                                                   // opened but will be closed in dir_close
   struct dir * dir = dir_open (inode);

   dir_add (dir, ".", new_sector);
   dir_add (dir, "..", parent_sector);  // root's parent is itself
   dir_close (dir);
   return true;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/*
   path can be absolute or relative
   no trailing "/"
   return NULL on error
*/
struct dir * dir_open_path (const char *path_) {
   char *path = malloc(strlen(path_)+1);
   strlcpy(path, path_, strlen(path_)+1);
   char *ptr = path;
   struct dir * cur;   // the current directory. Will traverse from it
   if (ptr[0] == '/') {
      cur = dir_open_root();
      ptr++;
   } else {  // relative, empty path_ should return cwd
      cur = dir_reopen(thread_current()->cwd);
   }
   char *token, *saveptr;
   token = strtok_r(ptr, "/", &saveptr);
   // traverse one at a time to get the wanted dir
   while (token != NULL) {
      struct inode *inode;
      if(! dir_lookup(cur, token, &inode)) {
         dir_close(cur);
         free(path);
         return NULL; // such directory not exist
      }
      ASSERT(inode_is_directory(inode));
      struct dir *temp = dir_open(inode); // dir_open wouldn't again open the inode
      // inode_close(inode);
      dir_close(cur);
      if (temp == NULL)  {
         free(path);
         return NULL;  // dir_open failed
      }
      cur = temp;
      token = strtok_r(NULL, "/", &saveptr);
   }
   free(path);

   // if the directory is already removed
   if (inode_is_removed(cur->inode)) {
      dir_close(cur);
      return NULL;
   }
   return cur;
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode)
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

   /* Prevent removing non-empty directory. */
  if (inode_is_directory (inode)) {
     // have to open a new inode, otherwise the current one will be closed, because dir_open doesn't call inode_open
     struct dir *d = dir_open (inode_open(e.inode_sector));
     bool ret = dir_is_empty(d);
     dir_close (d);
     if (!ret) goto done;
   }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}


/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;
//  if (dir_is_empty(dir)) return false;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (strcmp(e.name, ".") && strcmp(e.name, "..") && e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        }
    }
  return false;
}

static bool dir_is_empty(struct dir * dir) {
   struct dir_entry e;
   size_t ofs;
   int count = 0;
   ASSERT (dir != NULL);
   for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e)
      if (e.in_use)
      count++;
   return count == 2;
}

/*
filename: the file can still be a directory
not support multiple consecutive slashes
*/
void dir_extract_name (char *path_, char *dirname, char *filename) {
    char *path = malloc(strlen(path_)+1);
    strlcpy(path, path_, strlen(path_)+1);
    char *ptr = path;
    if (path[strlen(path)-1] == '/') {
        strlcpy(dirname, path, strlen(path_)+1);  // this should exclude the / at the end
        free(path);
        return;
    }

    char *token, *last_token = NULL, *saveptr;
    // check if absolute path
    if (ptr[0] == '/') {
        strlcat(dirname,"/", strlen(path_)+1);
        ptr++;
    }
    token = strtok_r(ptr, "/", &saveptr);
    while (1) {
        if (token == NULL) {
            strlcpy(filename, last_token,strlen(path_)+1);
            break;
        } else if (last_token != NULL) {
            strlcat(dirname, last_token, strlen(path_)+1);
            strlcat(dirname, "/", strlen(path_)+1);
        }
        last_token = token;
        token = strtok_r(NULL, "/", &saveptr);
    }
    if (strlen(dirname) != 0 && strlen(dirname) != 1)  // not empty nor "/"
        dirname[strlen(dirname)-1] = '\0';
    free(path);
}

