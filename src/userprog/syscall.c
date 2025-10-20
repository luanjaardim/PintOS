#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
bool sys_create(const char *file, unsigned initial_size);
bool sys_remove(const char *file);
int sys_open(const char *file);
void sys_close(int fd);
uint32_t sys_write(int fd, void *buffer, uint32_t size);
uint32_t sys_read(int fd, void *buffer, uint32_t size);
uint32_t sys_filesize(int fd);
void sys_exit(uint32_t code);
struct file_desc *get_file_desc(struct thread *t, int id);
static int memread_user (void *src, void *dst, size_t bytes);
static bool put_user (uint8_t *udst, uint8_t byte);
static int32_t get_user (const uint8_t *uaddr);
static void check_user (const uint8_t *uaddr);

struct lock filesys_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  ASSERT(f != NULL);
  ASSERT(f->esp != NULL);
  int sys_code;
  memread_user(f->esp, &sys_code, sizeof(int));

  switch (sys_code)
  {
  case SYS_HALT:
    shutdown_power_off();
    break;
  case SYS_EXIT:
    {
    int code;
    memread_user(f->esp + 4, &code, sizeof(int));
    sys_exit(code);
    break;
    }
  case SYS_CREATE:
    {
      const char *file;
      unsigned initial_size;
      memread_user(f->esp + 4, &file, sizeof(file));
      memread_user(f->esp + 8, &initial_size, sizeof(initial_size));
      f->eax = sys_create(file, initial_size);
      break;
    }
  case SYS_REMOVE:
  case SYS_OPEN:
    {
      const char *file;
      memread_user(f->esp + 4, &file, sizeof(file));
      if(sys_code == SYS_REMOVE)
        f->eax = sys_remove(file);
      else
        f->eax = sys_open(file);
      break;
    }
  case SYS_CLOSE:
    {
      int fd;
      memread_user(f->esp + 4, &fd, sizeof(fd));
      if(fd >= 0 && fd < 3) sys_exit(-1);
      else sys_close(fd);
      break;
    }
  case SYS_READ:
  case SYS_WRITE:
    {
      int fd;
      void *buffer;
      unsigned size;
      memread_user(f->esp + 4, &fd, sizeof(int));
      memread_user(f->esp + 8, &buffer, sizeof(void *));
      memread_user(f->esp + 12, &size, sizeof(unsigned));

      f->eax = sys_code == SYS_WRITE ? sys_write(fd, buffer, size) : sys_read(fd, buffer, size);
      break;
    }
  case SYS_FILESIZE:
    {
      int fd;
      memread_user(f->esp + 4, &fd, sizeof(int));
      f->eax = sys_filesize(fd);
      break;
    }
  default:
    printf("syscall: %d, not implemented yet\n", sys_code);
    break;
  }
}

bool sys_create(const char *file, unsigned initial_size) {
  check_user(file);
  lock_acquire(&filesys_lock);
  bool created = filesys_create(file, initial_size);
  lock_release(&filesys_lock);
  return created;
}

bool sys_remove(const char *file) {
  check_user(file);
  lock_acquire(&filesys_lock);
  bool removed = filesys_remove(file);
  lock_release(&filesys_lock);
  return removed;
}

int sys_open(const char *file) {
  check_user(file);

  struct file_desc *fd = malloc(sizeof(struct file_desc));
  if(fd == NULL) return -1;

  lock_acquire(&filesys_lock);
  struct file *f = filesys_open(file);
  if(f == NULL) {
    lock_release(&filesys_lock);
    return -1;
  }
  fd->f = f;
  struct list *fd_list = &thread_current()->file_descriptors;
  if (list_empty(fd_list)) {
    fd->id = 3;
  } else {
    fd->id = list_entry(list_back(fd_list), struct file_desc, e)->id + 1;
  }
  list_push_back(fd_list, &fd->e);

  lock_release(&filesys_lock);
  return fd->id;
}

void sys_close(int fd) {
  lock_acquire(&filesys_lock);
  struct file_desc *desc = get_file_desc(thread_current(), fd);
  if(desc) {
    file_close(desc->f);
    list_remove(&desc->e);
    free(desc);
  }
  lock_release(&filesys_lock);
}

uint32_t sys_write(int fd, void *buffer, uint32_t size) {
  check_user(buffer);
  check_user(buffer + size - 1);
  int len;

  if(fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    len = size;
  } else {
    struct file_desc *desc =  get_file_desc(thread_current(), fd);
    if(desc && desc->f) {
      lock_acquire(&filesys_lock);
      len = file_write(desc->f, buffer, size);
      lock_release(&filesys_lock);
    } else sys_exit(-1);
  }
  return len;
}

uint32_t sys_read(int fd, void *buffer, uint32_t size) {
  check_user(buffer);
  check_user(buffer + size - 1);
  int len;

  if(fd == 1 || fd == 2) sys_exit(-1);
  struct file_desc *desc =  get_file_desc(thread_current(), fd);
  if(desc && desc->f) {
    lock_acquire(&filesys_lock);
    len = file_read(desc->f, buffer, size);
    lock_release(&filesys_lock);
  } else sys_exit(-1);

  return len;
}

uint32_t sys_filesize(int fd) {
  int len;
  if(fd >= 0 && fd <= 2) sys_exit(-1);
  struct file_desc *desc = get_file_desc(thread_current(), fd);
  if(desc && desc->f) {
    lock_acquire(&filesys_lock);
    len = file_length(desc->f);
    lock_release(&filesys_lock);
  } else sys_exit(-1);

  return len;
}

// TODO: if exit with a thread we need to release the locks its holding
void sys_exit(uint32_t code) {
  printf("%s: exit(%d)\n", thread_current()->name, code);
  thread_exit();
}

struct file_desc *get_file_desc(struct thread *t, int id) {
  struct list_elem *elem;
  for (elem = list_begin (&t->file_descriptors); elem != list_end (&t->file_descriptors);
       elem = list_next (elem))
      {
        struct file_desc *fd = list_entry(elem, struct file_desc, e);
        if(fd && fd->id == id) {
          return fd;
        }
      }
  return NULL;
}

static void
check_user (const uint8_t *uaddr) {
  // check uaddr range or segfaults
  if(get_user (uaddr) == -1)
    sys_exit(-1);
}

/**
 * Reads a single 'byte' at user memory admemory at 'uaddr'.
 * 'uaddr' must be below PHYS_BASE.
 *
 * Returns the byte value if successful (extract the least significant byte),
 * or -1 in case of error (a segfault occurred or invalid uaddr)
 */
static int32_t
get_user (const uint8_t *uaddr) {
  // check that a user pointer `uaddr` points below PHYS_BASE
  if(((void*)uaddr > PHYS_BASE) || ((void*)uaddr < 0x08048000)) {
    return -1;
  }

  // as suggested in the reference manual
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes a single byte (content is 'byte') to user address 'udst'.
 * 'udst' must be below PHYS_BASE.
 *
 * Returns true if successful, false if a segfault occurred.
 */
static bool
put_user (uint8_t *udst, uint8_t byte) {
  // check that a user pointer `udst` points below PHYS_BASE
  if (! ((void*)udst < PHYS_BASE)) {
    return false;
  }

  int error_code;

  // as suggested in the reference manual, see (3.1.5)
  asm ("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}


/**
 * Reads a consecutive `bytes` bytes of user memory with the
 * starting address `src` (uaddr), and writes to dst.
 *
 * Returns the number of bytes read.
 * In case of invalid memory access, exit() is called and consequently
 * the process is terminated with return code -1.
 */
static int
memread_user (void *src, void *dst, size_t bytes)
{
  int32_t value;
  size_t i;
  for(i=0; i<bytes; i++) {
    value = get_user(src + i);
    if(value == -1) // segfault or invalid memory access
      sys_exit(-1);

    *(char*)(dst + i) = value & 0xff;
  }
  return (int)bytes;
}