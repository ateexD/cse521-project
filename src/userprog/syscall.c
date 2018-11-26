#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include <string.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include <stdlib.h>
struct file_mapping
{
  int fd;
  int tid;
  struct file *f;
  char *fname;
  struct list_elem file_elem;
};
struct lock file_system_lock;
struct list fd_list;
int fd_cnt;


static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  fd_cnt = 2;
  lock_init(&file_system_lock);
  list_init(&fd_list);
}

struct file_mapping* add_to_fd_list(int tid, struct file *f, char *fname)
{
  if (tid == TID_ERROR)
      exit_sys(-1);
  lock_acquire(&file_system_lock);
  int fd = ++fd_cnt;
  lock_release(&file_system_lock);
  struct file_mapping *fm = malloc(sizeof(struct file_mapping));
  if (fm == NULL)
      return NULL;
  fm->f = f;
  fm->tid = tid;
  fm->fd = fd;
  fm->fname = fname;
  list_push_back(&fd_list, &fm->file_elem);
  return fm;
}

struct file_mapping* look_up_fd_list(int tid, int fd)
{
  if (tid == TID_ERROR)
      exit_sys(-1);
  struct list_elem *e;

  for(e = list_begin(&fd_list); e != list_end(&fd_list); e = list_next(e))
  {
    struct file_mapping *fm = list_entry(e, struct file_mapping, file_elem);
    if (fm->fd == fd && fm->tid == tid)
        return fm;
  }
  return NULL;
}
int read_sys(int*);
bool create_sys(int *);
bool success;
bool remove_sys(int *);
unsigned tell_sys(int *);


bool check_addr(char *esp)
{
if(esp == NULL)
    return false;
  if (!is_user_vaddr(esp))
    return false;
  if (pagedir_get_page (thread_current()->pagedir, esp) == NULL)
    return false;
 return true;
}

bool
check_valid_addr(char *esp)
{
  if(!check_addr(esp) || !(check_addr(esp+3)))
    return false;
  return true;
}

static void
syscall_handler (struct intr_frame *f) 
{
  int *esp = f->esp;
  if(!check_valid_addr(esp))
    exit_sys(-1);

  if(*esp < 0 || *esp >13 )
    exit_sys(-1);
  

  switch(*esp)
  {
	case SYS_HALT: 
		f->eax = halt_sys(esp);
		break;

    case SYS_EXIT:
        if(!check_valid_addr(esp + 1))
          exit_sys(-1);
        exit_sys(*(++esp));	
        break;
	
    case SYS_WAIT: 
		f->eax = wait_sys(esp);
		break;
    
    case SYS_FILESIZE:
        f->eax = filesize_sys(esp);
        break;
    
    case SYS_WRITE: 
         if(!check_valid_addr(esp + 1) || !check_valid_addr(esp + 2) || !check_valid_addr(*(esp + 2)) || !check_valid_addr(esp + 3))
            exit_sys(-1);		
         if ((int)*(esp + 3) == 0)
         {
           f->eax = 0;
           return 0;
         }
        f->eax = write_sys(esp);
   		break;
    case SYS_READ:
        if(!check_valid_addr(esp + 1) || !check_valid_addr(esp + 2) || !check_valid_addr(*(esp + 2)) || !check_valid_addr(esp + 3))
           exit_sys(-1);
        if ((int)*(esp + 3) == 0)
        {
          f->eax = 0;
          return 0;
        }
        f->eax = read_sys(esp);
        break;
    case SYS_OPEN:
        if(!check_valid_addr(esp + 1) || !check_valid_addr(*(esp + 1)))
            exit_sys(-1);
        f->eax = open_sys(esp);
        break;
    case SYS_CREATE:
        if (!check_valid_addr(esp + 1) || !check_valid_addr(esp + 2) || !check_valid_addr(*(esp + 1)))
            exit_sys(-1);
        f->eax = create_sys(esp);
        break;
    case SYS_CLOSE:
        if (!check_valid_addr(esp + 1))
            exit_sys(-1);
        close_sys(esp);
        break;
    case SYS_SEEK:
        seek_sys(esp);
        break;
    case SYS_EXEC:
        if (!check_valid_addr((esp + 1)) ||  (!check_valid_addr(*(esp + 1))))
            exit_sys(-1);
        f->eax = exec_sys(esp);
        break;

     case SYS_REMOVE:
         if(!check_valid_addr(esp + 1))
           exit_sys(-1);
         f->eax = remove_sys(esp);
         break;

     case SYS_TELL:
         if (!check_valid_addr(esp + 1))
           exit_sys(-1);
         f->eax = tell_sys(esp);
     default:
         exit_sys(-1);
  }
}

int exec_sys(int *esp)
{
  char *file = *(++esp);
  return process_execute(file);
}

bool 
create_sys (int *esp)
{
  return filesys_create((char *) *(esp + 1), (int) *(esp + 2));
}

void seek_sys(int *esp)
{
  int fd = (int) *(esp + 1);
  int pos = (int) *(esp + 2);
  lock_acquire(&file_system_lock);
  struct file_mapping *fm = look_up_fd_list(thread_current()->tid, fd);
  if (fm == NULL)
  {
    lock_release(&file_system_lock);
    exit_sys(-1);
  }
  file_seek (fm->f, pos);
  lock_release(&file_system_lock);
}

void
close_sys(int *esp)
{
  int fd = (int) *(esp + 1);
  lock_acquire(&file_system_lock);
  struct file_mapping *fm = look_up_fd_list(thread_current()->tid, fd);
  if (fm == NULL)
  {
    lock_release(&file_system_lock);
    return;
  }
  list_remove(&fm->file_elem);
  free(fm);
  lock_release(&file_system_lock);
}
    
int
open_sys (int *esp)
{
  char *fname = *(esp + 1);
  lock_acquire(&file_system_lock);
  struct file *f = filesys_open(fname);
  if (f == NULL)
  {
   lock_release(&file_system_lock);
   return -1;
  }
  lock_release(&file_system_lock);
  struct file_mapping *fm =  add_to_fd_list(thread_current()->tid, f, fname);
  if (fm == NULL)
  {
    lock_release(&file_system_lock);
    return -1;
  }
  return fm->fd;
}

int 
wait_sys(int *esp)
{
  int pid = *(esp + 1);
  return process_wait(pid);
}

int filesize_sys(int *esp)
{
  lock_acquire(&file_system_lock);
  struct file_mapping *fm = look_up_fd_list(thread_current()->tid, (int) *(esp + 1));
  if (fm == NULL)
  {
    lock_release(&file_system_lock);
    return -1;
  }
  lock_release(&file_system_lock);
  return file_length(fm->f);
}
int
halt_sys(void *esp)
{
  shutdown_power_off();
}

void
exit_sys(int status)
{
 process_exit(status);
}

int 
read_sys(int *esp)
{
  int fd = *(esp + 1);
  char *buffer = *(esp + 2);
  unsigned size = *(esp + 3);
  lock_acquire(&file_system_lock);
  if (fd == 0)
  {
    int i = 0;
    while(size--)
      buffer[i++] = (void *)input_getc();
    lock_release(&file_system_lock);
    return i;
  }
  struct file_mapping *fm = look_up_fd_list(thread_current()->tid, fd);
  if (fm == NULL)
    {
      lock_release(&file_system_lock);
      return -1;
    }
  int actual_size = file_read(fm->f, buffer, size);
  buffer[actual_size] = '\0';
  lock_release(&file_system_lock);
  return actual_size;
}

int
write_sys(int *esp)
{
  int fd = *(esp + 1);
  char *buffer = *(esp + 2);
  unsigned size = *(esp + 3);
  lock_acquire(&file_system_lock);
  if(fd == 1)
  {
    putbuf(buffer, size);
    lock_release(&file_system_lock);
    return size;
  }
  struct file_mapping *fm = look_up_fd_list(thread_current()->tid, fd);
  if (fm == NULL)
  {
    lock_release(&file_system_lock);
    return -1;
  }
  struct thread *cur = thread_current();
  char *delim = " ";
  char *ptr;
  char *fn_copy;
  fn_copy = malloc(sizeof(strlen(cur->name)));
  if(fn_copy == NULL)
  {
      lock_release(&file_system_lock);
      return -1;
  }
  strlcpy(fn_copy, cur->name, strlen(cur->name)+1);
  fn_copy = strtok_r(fn_copy, delim, &ptr);
  
  if (strcmp(fm->fname, fn_copy)==0)
  {
    lock_release(&file_system_lock);
    return 0;
  }
  free(fn_copy);
  int actual_size = file_write(fm->f, buffer, size);
  lock_release(&file_system_lock);
  return actual_size;
}

bool
remove_sys(int *esp)
{
  char *fname = (char *)*(esp + 1);
  bool res;
  lock_acquire(&file_system_lock);
  res = filesys_remove(fname);
  lock_release(&file_system_lock);
  return res;
}

unsigned
tell_sys(int *esp)
{
  int fd = (int) *(esp + 1);
  unsigned res;
  lock_acquire(&file_system_lock);
  struct file_mapping *fm = look_up_fd_list(thread_current()->tid, fd);
  if(fm == NULL)
  {
    lock_release(&file_system_lock);
    exit_sys(-1);
  }
  res = file_tell(fm->f);
  lock_release(&file_system_lock);
  return res;
}
