#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <sys/wait.h>
#include <sys/types.h>

#include "skud.h"

enum priority 
{
  LOW,
  HIGH
};

typedef struct process_t
{
  struct process_t *next;
  pid_t task_pid;
  char *name;
  enum priority rank;
} process_t;

process_t* processes = NULL;
int processlistlen;
int running_tasks;

static void new_process (process_t *processlist, char* processname, pid_t pid, enum priority pos)
{
  process_t* newprocess = malloc(sizeof(process_t));
  newprocess->task_pid = pid;
  newprocess->rank = pos;
  strcpy(newprocess->name, processname);
  processlistlen++;
  processlist->next = NULL;
  if (processlist==NULL)
  {
    processlist = newprocess;
    return;
  }
  while(processlist!=NULL)
    processlist=processlist->next;
  processlist->next=newprocess;
  return;
}

static void remove_process (process_t *processlist, pid_t pid)
{
  process_t *itr = processlist;
  while(itr->next!=NULL)
  {
    if(itr->task_pid == pid)
    {
      free(itr);
      processlistlen--;
      break;
    }
    itr=itr->next;
  }
  return;
}

static void print_processes (process_t *processlist)
{
  while(processlist->next != NULL)
  {
    printf("\nCurrently running %d of %d processes\n", running_tasks, processlistlen);
    if (processlist->rank == LOW)
      printf("Process ID %d \t\t | \t\t Process Name %s \t\t | \t\t Priority: LOW \t\t |\n", processlist->task_pid, processlist->name);
    else if (processlist->rank == HIGH)
      printf("Process ID %d \t\t | \t\t Process Name %s \t\t | \t\t Priority: HIGH \t\t |\n", processlist->task_pid, processlist->name);
    processlist = processlist->next;
  }
  return;
}

static int kill_by_id(process_t *processlist, pid_t pid)
{
  /*
  process_t *temp = processlist; 
  for (int i=0; i<running_tasks; i++)
  {
    if(temp->task_pid == pid)
    {
      kill(pid, SIGKILL); // or SIGTERM? 
      // what about failure condition?
      running_tasks--;
      return 0;
    }
    temp = temp->next;
  }
  */
  kill(pid, SIGKILL); // or SIGTERM? 
  return 1;
}

static void skud_create_process(char *executable)
{
  pid_t pid;
  pid = fork();
  printf("ProcessID %d \n", pid);

  kill_by_id(processes, pid);
  return;
}
int main()
{
  skud_create_process("ls");
  return 0;
}
// https://github.com/dlekkas/proc_scheduler/blob/master/src/scheduler.c
