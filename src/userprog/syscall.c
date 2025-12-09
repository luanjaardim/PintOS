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
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "vm/supt_table.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);
bool create_syscall(const char *file, unsigned initial_size);
bool remove_syscall(const char *file);
int open_syscall(const char *file);
void close_syscall(int fd);
int write_syscall(int fd, void *buffer, uint32_t size);
int read_syscall(int fd, void *buffer, uint32_t size);
int filesize_syscall(int fd);
void seek_syscall(int fd, unsigned position);
unsigned tell_syscall(int fd);
tid_t exec_syscall(const char *command_line_arguments);
mmapid_t mmap_syscall(int fd, void *upage);
bool munmap_syscall(mmapid_t mid);
struct file_desc *get_file_desc(struct thread *t, int id);
struct mmap_desc *get_mmap_desc(struct thread *t, int mid);
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
  case SYS_WAIT:
    {
      int tid;
      read_from_user(f->esp+4, &tid, sizeof(int));
      f->eax = process_wait(tid);
      break;
    }
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
  case SYS_TELL:
    {
      int fd;
      read_from_user(f->esp + 4, &fd, sizeof(int));
      f->eax = tell_syscall(fd);
      break;
    }
  case SYS_SEEK:
    {
      int fd;
      unsigned position;
      read_from_user(f->esp + 4, &fd, sizeof(int));
      read_from_user(f->esp + 8, &position, sizeof(unsigned));
      seek_syscall(fd, position);
      break;
    }
  case SYS_EXEC:
    {
      char *cmd_parameters;
      read_from_user(f->esp + 4, &cmd_parameters, sizeof(cmd_parameters));
      f->eax = exec_syscall(cmd_parameters);
      break;
    }
  case SYS_MMAP:
    {
      int fd;
      void *addr;
      read_from_user(f->esp + 4, &fd, sizeof(fd));
      read_from_user(f->esp + 8, &addr, sizeof(addr));
      f->eax = mmap_syscall(fd, addr);
      break;
    }
  case SYS_MUNMAP:
    {

      mmapid_t mid;
      read_from_user(f->esp + 4, &mid, sizeof(mid));
      f->eax = munmap_syscall(mid);
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

  lock_acquire(&filesys_lock);
  if(fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    len = size;
  } else {
    struct file_desc *desc =  get_file_desc(thread_current(), fd);
    if(desc && desc->f) {
      load_and_set_not_evictable_buffer(buffer, size);
      len = file_write(desc->f, buffer, size);
      unset_not_evictable_buffer(buffer, size);
    } else exit_syscall(-1);
  }
  lock_release(&filesys_lock);
  return len;
}

int read_syscall(int fd, void *buffer, uint32_t size) {
  is_user_loc(buffer);
  is_user_loc(buffer + size - 1);
  int len;

  if(fd == 1 || fd == 2) exit_syscall(-1);
  lock_acquire(&filesys_lock);
  struct file_desc *desc =  get_file_desc(thread_current(), fd);
  if(desc && desc->f) {
    load_and_set_not_evictable_buffer(buffer, size);
    len = file_read(desc->f, buffer, size);
    unset_not_evictable_buffer(buffer, size);
    lock_release(&filesys_lock);
  } else {
    lock_release(&filesys_lock);
    exit_syscall(-1);
  }

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

void seek_syscall(int fd, unsigned position) {
  struct file_desc *desc = get_file_desc(thread_current(), fd);
  if(desc == NULL) exit_syscall(-1);
  lock_acquire(&filesys_lock);
  file_seek(desc->f, position);
  lock_release(&filesys_lock);
}

unsigned tell_syscall(int fd) {
  struct file_desc *desc = get_file_desc(thread_current(), fd);
  if(desc == NULL) exit_syscall(-1);
  lock_acquire(&filesys_lock);
  unsigned pos = file_tell(desc->f);
  lock_release(&filesys_lock);
  return pos;
}

mmapid_t mmap_syscall(int fd, void *upage) {
  if(upage == NULL) return -1;
  if(fd <= 2) return -1;

  struct thread *t = thread_current();
  ASSERT(t->pd != NULL);
  lock_acquire(&filesys_lock);

  struct file *f = NULL;
  struct file_desc* desc = get_file_desc(t, fd);
  if(desc != NULL && desc->f != NULL) {
    f = file_reopen(desc->f);
  }
  if(f == NULL) goto FAIL;

  size_t length = file_length(f);
  if(length == 0) goto FAIL;

  size_t offset;
  void *addr = upage;
  for(offset=0; offset < length; offset += PGSIZE) {
    size_t to_read = (offset + PGSIZE < length ? PGSIZE : length - offset);
    size_t padding = PGSIZE - to_read;
    addr += offset;
    filesys_add_to_supt_table(&t->sup_pg_t, addr, f, offset, to_read, padding);
  }

  mmapid_t mid;
  if(!list_empty(&t->pd->mmap_list))
    mid = list_entry(list_back(&t->pd->mmap_list), struct mmap_desc, e)->id + 1;
  else
    mid = 0;

  struct mmap_desc *md = malloc(sizeof(struct mmap_desc));
  md->id = mid;
  md->f = f;
  md->length = length;
  md->upage = upage;
  list_push_back(&t->pd->mmap_list, &md->e);
  lock_release(&filesys_lock);
  return mid;

  FAIL:
  lock_release(&filesys_lock);
  return -1;
}

bool munmap_syscall(mmapid_t mid) {
  lock_acquire(&filesys_lock);
  struct mmap_desc *md = get_mmap_desc(thread_current(), mid);
  if(md == NULL) {
    lock_release(&filesys_lock);
    return false;
  }

  size_t offset;
  void *addr = md->upage;
  struct thread *t = thread_current();
  for(offset=0; offset < md->length; offset += PGSIZE) {
    size_t to_read = (offset + PGSIZE < md->length ? PGSIZE : md->length - offset);
    size_t padding = PGSIZE - to_read;
    addr += offset;
    struct sup_page_table_entry *sp = sup_get_entry(&t->sup_pg_t, addr);
    if(sp->kpage != NULL)
      remove_from_frame_table(sp->kpage);
    remove_from_supt_table(&t->sup_pg_t, addr, true);
  }

  lock_release(&filesys_lock);
  return true;
}

tid_t exec_syscall(const char *command_line_arguments) {
  tid_t tid;
  is_user_loc(command_line_arguments);
  tid = process_execute(command_line_arguments);
  return tid;
}

// TODO: if exit with a thread we need to release the locks its holding
void exit_syscall(uint32_t code) {
  struct thread *t = thread_current();
  printf("%s: exit(%d)\n", t->name, code);
  if(t->pd)
    t->pd->exit_code = code;
  thread_exit();
}

struct mmap_desc *get_mmap_desc(struct thread *t, int mid) {
  struct list_elem *elem;
  for (elem = list_begin (&t->pd->mmap_list); elem != list_end (&t->pd->mmap_list);
       elem = list_next (elem))
      {
        struct mmap_desc *md = list_entry(elem, struct mmap_desc, e);
        if(md && md->id == (unsigned int)mid) {
          return md;
        }
      }
  return NULL;
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
  if(((void*)uaddr >= PHYS_BASE) || ((void*)uaddr <= (void*)0x08048000)) {
    return -1;
  }
  if(pagedir_get_page(thread_current()->pagedir, uaddr) == NULL) {
    void *upage = pg_round_down(uaddr);
    struct sup_page_table_entry *sp = sup_get_entry(&thread_current()->sup_pg_t, upage);
    if(sp) load_from_swap_again(&thread_current()->sup_pg_t, upage);
    else return -1;
  }
  struct sup_page_table_entry tmp;
  tmp.upage = pg_round_down(uaddr);
  struct sup_page_table_entry *e = hash_find(&thread_current()->sup_pg_t, &tmp.e);;
  ASSERT(e != NULL);
  e->access_time = timer_ticks(); // page access, so update access_time

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