#include<stdio.h>
#include<string.h>
#include<stdlib.h>
int main()
{
printf("This is a test.\n");
char c[] = "This is a test message for   Project 2.";
char *delim = " ";
char *ptr;
char *token;
int argv = 0;
int len = 0;
char *esp=malloc(200);
esp = esp + 50;
ptr = c;
char *itr = esp;
do
{
token = strtok_r(ptr, delim, &ptr);
if (token != NULL)
{
argv++;
len = len + strlen(token) + 1;
itr = esp - len;
strcpy(itr, token);
}
}while (token != NULL);
int pad = (len%4==0)?0:(4 - len%4);
esp = itr - pad - 4;
for (int i=0; i<pad+4; i++)
{
*esp = 0;
esp++;
}
esp = itr - pad - 4;
//hexdump(0, esp, 30, 1);
//printf("%s\n", esp);
int intr;
for (int i=0; i<argv; i++)
{
esp--;
intr = itr;
*esp = intr<<24|(intr&0x0000FF00)<<8|(intr&0x00FF0000)>>8|intr>>24;
itr = itr + strlen(itr) + 1;
}
esp = esp - 4;
for (int i=0; i<4; i++)
{
*esp = 0;
esp++;
}
esp = esp - 4;
printf("%x\n", itr);
printf("%x\n", intr<<24|(intr&0x0000FF00)<<8|(intr&0x00FF0000)>>8|intr>>24);
printf("%d, %d, %d\n",argv, len, esp-itr);
}
