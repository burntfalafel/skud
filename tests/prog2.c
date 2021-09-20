#include <stdio.h>
#include <unistd.h>
#define DELAY 1 
#define MAX 10

int main(void)
{
  for (int i=0; i<MAX; i++)
  {
    printf("This is the output of prog2{%d}\n", i);
    sleep(DELAY);
  }
  return 0;
}
