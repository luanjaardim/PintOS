#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);
void split_path(const char *name, char *dir_name, char *file_name) {
  int len = strlen(name);
  // Separate directory path and file name
  for(int i = len - 1; i >= 0; i--) {
    if(name[i] == '/') {
      memcpy(dir_name, name, i+1);
      dir_name[i+1] = '\0';
      memcpy(file_name, name + i + 1, len - i);
      break;
    }
    if(i == 0) {
      memcpy(file_name, name, len + 1);
      dir_name[0] = '\0';
    }
  }
}

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
bool
filesys_create (const char *name, off_t initial_size, bool is_dir)
{
  block_sector_t inode_sector = 0;
  int len = strlen(name);
  if(len == 0) return false;
  char dir_name[len + 1];
  char file_name[len + 1];
  split_path(name, dir_name, file_name);
  struct dir *dir = dir_open_rec(dir_name);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && (is_dir ? dir_create (inode_sector, initial_size)
                             : inode_create (inode_sector, initial_size, false))
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct inode *inode = NULL;
  int len = strlen(name);
  char dir_name[len + 1];
  char file_name[len + 1];
  split_path(name, dir_name, file_name);
  struct dir *dir = dir_open_rec(dir_name);
  if(dir == NULL) return NULL;

  if(strlen(file_name) == 0) {
    inode = dir_get_inode(dir);
  } else {
    dir_lookup (dir, file_name, &inode);
    dir_close (dir);
  }
  // TODO: check for removed inodes
  if(inode == NULL) return NULL;

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  int len = strlen(name);
  char dir_name[len + 1];
  char file_name[len + 1];
  split_path(name, dir_name, file_name);
  struct dir *dir = dir_open_rec(dir_name);
  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 

  return success;
}

bool filesys_chdir(const char *dir) {
  struct dir *target_dir = dir_open_rec(dir);
  if(target_dir == NULL) {
    return false;
  }
  struct thread *t = thread_current();
  if(t->working_dir != NULL) {
    dir_close(t->working_dir);
  }
  t->working_dir = target_dir;
  return true;
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
