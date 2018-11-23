#include <syscall.h>

int main (int, char *[]);
void _start (int argc, char *argv[]);

void
_start (int argc, char *argv[]) 
{
  //printf("Inside _start.c\n");
  exit (main (argc, argv));
}
