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
bool create_syscall(const char *file, unsigned initial_size);
bool remove_syscall(const char *file);
int open_syscall(const char *file);
void close_syscall(int fd);
int write_syscall(int fd, void *buffer, uint32_t size);
int read_syscall(int fd, void *buffer, uint32_t size);
int filesize_syscall(int fd);
tid_t exec_syscall(const char *command_line_arguments);
void exit_syscall(uint32_t code);
struct file_desc *get_file_desc(struct thread *t, int id);
static int read_from_user (void *src, void *dst, size_t bytes);
static bool put_user (uint8_t *udst, uint8_t byte);
static int32_t get_user (const uint8_t *uaddr);
static void is_user_loc (const void *uaddr);

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
  read_from_user(f->esp, &sys_code, sizeof(int));

  switch (sys_code)
  {
  case SYS_HALT:
    shutdown_power_off();
    break;
  case SYS_EXIT:
    {
    int code;
    read_from_user(f->esp + 4, &code, sizeof(int));
    exit_syscall(code);
    break;
    }
  case SYS_CREATE:
    {
      const char *file;
      unsigned initial_size;
      read_from_user(f->esp + 4, &file, sizeof(file));
      read_from_user(f->esp + 8, &initial_size, sizeof(initial_size));
      f->eax = create_syscall(file, initial_size);
      break;
    }
  case SYS_REMOVE:
  case SYS_OPEN:
    {
      const char *file;
      read_from_user(f->esp + 4, &file, sizeof(file));
      if(sys_code == SYS_REMOVE)
        f->eax = remove_syscall(file);
      else
        f->eax = open_syscall(file);
      break;
    }
  case SYS_CLOSE:
    {
      int fd;
      read_from_user(f->esp + 4, &fd, sizeof(fd));
      if(fd >= 0 && fd < 3) exit_syscall(-1);
      else close_syscall(fd);
      break;
    }
  case SYS_READ:
  case SYS_WRITE:
    {
      int fd;
      void *buffer;
      unsigned size;
      read_from_user(f->esp + 4, &fd, sizeof(int));
      read_from_user(f->esp + 8, &buffer, sizeof(void *));
      read_from_user(f->esp + 12, &size, sizeof(unsigned));

      f->eax = sys_code == SYS_WRITE ? write_syscall(fd, buffer, size) : read_syscall(fd, buffer, size);
      break;
    }
  case SYS_FILESIZE:
    {
      int fd;
      read_from_user(f->esp + 4, &fd, sizeof(int));
      f->eax = filesize_syscall(fd);
      break;
    }
  case SYS_EXEC:
    {
      char *cmd_parameters;
      read_from_user(f->esp + 4, &cmd_parameters, sizeof(cmd_parameters));
      f->eax = exec_syscall(cmd_parameters);
      break;
    }
  default:
    printf("syscall: %d, not implemented yet\n", sys_code);
    break;
  }
}

bool create_syscall(const char *file, unsigned initial_size) {
  is_user_loc(file);
  lock_acquire(&filesys_lock);
  bool created = filesys_create(file, initial_size);
  lock_release(&filesys_lock);
  return created;
}

bool remove_syscall(const char *file) {
  is_user_loc(file);
  lock_acquire(&filesys_lock);
  bool removed = filesys_remove(file);
  lock_release(&filesys_lock);
  return removed;
}

int open_syscall(const char *file) {
  is_user_loc(file);

  struct file_desc *fd = malloc(sizeof(struct file_desc));
  if(fd == NULL) return -1;

  lock_acquire(&filesys_lock);
  struct file *f = filesys_open(file);
  if(f == NULL) {
    lock_release(&filesys_lock);
    return -1;
  }
  fd->f = f;
  struct list *fd_list = &thread_current()->pd->file_descriptors;
  if (list_empty(fd_list)) {
    fd->id = 3;
  } else {
    fd->id = list_entry(list_back(fd_list), struct file_desc, e)->id + 1;
  }
  list_push_back(fd_list, &fd->e);

  lock_release(&filesys_lock);
  return fd->id;
}

void close_syscall(int fd) {
  lock_acquire(&filesys_lock);
  struct file_desc *desc = get_file_desc(thread_current(), fd);
  if(desc) {
    file_close(desc->f);
    list_remove(&desc->e);
    free(desc);
  }
  lock_release(&filesys_lock);
}

int write_syscall(int fd, void *buffer, uint32_t size) {
  is_user_loc(buffer);
  is_user_loc(buffer + size - 1);
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
    } else exit_syscall(-1);
  }
  return len;
}

int read_syscall(int fd, void *buffer, uint32_t size) {
  is_user_loc(buffer);
  is_user_loc(buffer + size - 1);
  int len;

  if(fd == 1 || fd == 2) exit_syscall(-1);
  struct file_desc *desc =  get_file_desc(thread_current(), fd);
  if(desc && desc->f) {
    lock_acquire(&filesys_lock);
    len = file_read(desc->f, buffer, size);
    lock_release(&filesys_lock);
  } else exit_syscall(-1);

  return len;
}

int filesize_syscall(int fd) {
  int len;
  if(fd >= 0 && fd <= 2) exit_syscall(-1);
  struct file_desc *desc = get_file_desc(thread_current(), fd);
  if(desc && desc->f) {
    lock_acquire(&filesys_lock);
    len = file_length(desc->f);
    lock_release(&filesys_lock);
  } else exit_syscall(-1);

  return len;
}

tid_t exec_syscall(const char *command_line_arguments) {
  tid_t tid;
  lock_acquire(&filesys_lock);
  tid = process_execute(command_line_arguments);
  lock_release(&filesys_lock);

  return tid;
}

// TODO: if exit with a thread we need to release the locks its holding
void exit_syscall(uint32_t code) {
  printf("%s: exit(%d)\n", thread_current()->name, code);
  thread_exit();
}

struct file_desc *get_file_desc(struct thread *t, int id) {
  struct list_elem *elem;
  for (elem = list_begin (&t->pd->file_descriptors); elem != list_end (&t->pd->file_descriptors);
       elem = list_next (elem))
      {
        struct file_desc *fd = list_entry(elem, struct file_desc, e);
        if(fd && fd->id == (unsigned int)id) {
          return fd;
        }
      }
  return NULL;
}

static void is_user_loc (const void *uaddr) {
  // check for a valid pointer
  if(get_user (uaddr) == -1)
    exit_syscall(-1);
}

static int get_user (const uint8_t *uaddr) {
  // check for valid pointer
  if(((void*)uaddr > PHYS_BASE) || ((void*)uaddr < (void*)0x08048000)) {
    return -1;
  }

  // as suggested in the reference manual
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
  return result;
}

static bool put_user (uint8_t *udst, uint8_t byte) {
  // check for valid pointer
  if (((void*)udst > PHYS_BASE) || ((void*)udst < (void*)0x08048000)) {
    return false;
  }

  int error_code;

  // as suggested in the reference manual
  asm ("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

static int read_from_user (void *src, void *dst_, size_t bytes) {
  int value;
  size_t i;
  char *dst = dst_;
  for(i=0; i<bytes; i++) {
    value = get_user(src + i);
    if(value == -1) // segfault or invalid memory access
      exit_syscall(-1);

    dst[i] = value & 0xff;
  }
  return (int)bytes;
}