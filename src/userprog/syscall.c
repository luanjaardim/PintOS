#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
uint32_t sys_write(int fd, void *buffer, uint32_t size);
void sys_exit(uint32_t code);
static int memread_user (void *src, void *dst, size_t bytes);
static bool put_user (uint8_t *udst, uint8_t byte);
static int32_t get_user (const uint8_t *uaddr);
static void check_user (const uint8_t *uaddr);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
  case SYS_WRITE:
    {
      int fd;
      void *buffer;
      unsigned size;
      memread_user(f->esp + 4, &fd, sizeof(int));
      memread_user(f->esp + 8, &buffer, sizeof(void *));
      memread_user(f->esp + 12, &size, sizeof(unsigned));

      f->eax = sys_write(fd, buffer, size);
      break;
    }
  default:
    printf("syscall: %d, not implemented yet\n", sys_code);
    break;
  }
}

uint32_t sys_write(int fd, void *buffer, uint32_t size) {
  check_user(buffer);
  check_user(buffer + size - 1);

  if(fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    return size;
  } else {
    printf("SYS_WRTIE ON FILES: TODO!\n");
    thread_exit();
    return 0;
  }
}

// TODO: if exit with a thread we need to release the locks its holding
void sys_exit(uint32_t code) {
  printf("%s: exit(%d)\n", thread_current()->name, code);
  thread_exit();
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