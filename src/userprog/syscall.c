#include "userprog/syscall.h"
#include <stdio.h>
#include "devices/shutdown.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static struct lock file_system_lock;
void exit (int status);
int write(int fd, const void *buffer, unsigned size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&file_system_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
 // printf("\n\n");
  int *esp = f->esp;
 // printf("%x\n", esp);
  int syscall_num = *esp;
  int b = 100;
 // hex_dump(esp,  esp, 256, true);
  
  if (syscall_num == SYS_EXIT)
      exit (0);

  if (syscall_num == SYS_HALT)
      halt ();
   
  if (syscall_num == SYS_WRITE)
  {
    int fd = *++esp;
    void* buffer = *++esp;
    unsigned size = *++esp;
    f->eax = write(fd, buffer, size); 
  }

  //printf ("system call!\n");
//  thread_exit ();
}

void exit(int status)
{
  thread_exit();
}
int write(int fd, const void *buffer, unsigned size)
{
//  printf("FD - %d, buffer - %s, size - %u\n\n", fd, buffer, size);
//  printf("sizeof - %d\n", sizeof buffer);
//  lock_acquire(&file_system_lock);
   
  if(buffer == NULL)
    return -1;
  
  if (fd == 1)
    putbuf(buffer, size);
//  lock_release(&file_system_lock);
  return size;
}
void halt()
{
  shutdown_power_off();
}
