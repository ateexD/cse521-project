#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <string.h>
#include "filesys/file.h"
#include "devices/shutdown.h"

//static int *esp;
//static int *eip;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

int write (int fd, const void *buffer, unsigned size);

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
//int *eip = f->eip;
int *esp = f->esp;
 // hex_dump(PHYS_BASE - 128, PHYS_BASE - 128, 128, true);
  //printf("%x %d %d\n",esp,*esp, eip);
  //printf("%d %d %d\n",*(esp + 1), *(esp + 2), *(esp+32));
 // printf("%d %d \n",*(esp+16), *(esp-16));
 // for(int i =1; i<=10;i++){
 // 	printf("%d \n ",*(esp+i));
 //}
//  printf("%d\n", (0x8049c8e));
  //intr_dump_frame(f);
  //printf ("system call!\n");
  //thread_exit ();

  switch(*esp){
	case 0: //printf("HALT\n");
		f->eax = halt_sys(esp);
		break;
	case 1: //printf("EXIT\n");
		//printf(" *** EXIT TID : %d\n",thread_current()->tid);
	        exit_sys(esp);
		//thread_exit();
		//printf("Hi");
                break;
	case 3: //printf("WAIT\n");
		f->eax = wait_sys(esp);
		break;
	case 9: //printf("Write \n");
		//int n = write (int fd, const void *buffer, unsigned size);
	//	printf(" ***TID : %d\n",thread_current()->tid);
		f->eax = write_sys(esp);
		//thread_exit();
		break;

	default: printf("INSIDE DEFAULT : %d\n",*esp);
		break;
  }
  //return;
  //thread_exit();
}

int wait_sys(int *esp)
{
  int pid = *(esp + 1);

  return process_wait(pid);
}

int
halt_sys(void *esp)
{
shutdown_power_off();
}

void
exit_sys(int *esp){
  int status = *(esp+1);
  char *delim = " ";
  char *ptr;
  char *file_name = thread_current()->name;
  file_name = strtok_r(file_name, delim, &ptr);
  printf("%s: exit(%d)\n", file_name, status) ;
  //return status;
  thread_exit();
 // printf ("%s: exit(%d)\n", );
 // return status;
}

int
write_sys(int *esp)
{
  char *buff;
  int fd;
  int size;

//  df = f->esp; 
  fd = *(esp+1);
  buff = (char *)*(esp+2);
  size = *(esp+3);

  struct file *file;
//hex_dump(PHYS_BASE - 512, PHYS_BASE - 512, 512, true);

  putbuf(buff, size);
  //file_write(&file, *(esp+1), 4); 
 return size;
}
