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
  enum priority rank;
  char name[20];
} process_t;

process_t* processes = NULL;
int processlistlen;
int running_tasks;

static void new_process (process_t **processlist, char* processname, pid_t pid, enum priority pos)
{
  process_t* newprocess = malloc(sizeof(process_t));
  newprocess->task_pid = pid;
  newprocess->rank = pos;
  strcpy(newprocess->name, processname);
  processlistlen++;
  newprocess->next = NULL;
  if (*processlist==NULL)
  {
    *processlist = newprocess;
    return;
  }
  while((*processlist)->next!=NULL)
    *processlist=(*processlist)->next;
  (*processlist)->next=newprocess;
  return;
}

static void remove_process (process_t **processlist, pid_t pid)
{
  process_t *itr = *processlist;
  while(itr!=NULL)
  {
    if(itr->task_pid == pid)
    {
      free(itr);
      processlistlen--;
      break;
    }
    itr=itr->next;
  }
  if(processlistlen==0)
    *processlist = NULL;
  return;
}

process_t *find_process(process_t *processlist, pid_t pid)
{
  process_t *itr = processlist;
  while(itr != NULL)
  {
    if(pid==itr->task_pid)
      return itr;
    itr = itr->next;
  }
  return NULL;
}

static void print_processes (process_t *processlist)
{
  while(processlist != NULL)
  {
    printf("\nCurrently running %d of %d processes\n", running_tasks, processlistlen);
    if (processlist->rank == LOW)
      printf("Process ID %d \t | \t Process Name: %s \t | \t Priority: LOW \t |\n", processlist->task_pid, processlist->name);
    else if (processlist->rank == HIGH)
      printf("Process ID %d \t | \t Process Name: %s \t | \tPriority: HIGH \t |\n", processlist->task_pid, processlist->name);
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
  if(pid == -1)
  {
    perror("unable to fork:skud_create_process");
    exit(-1);
  }
  else if (pid==0)
  {
    /* initialize the required arguments for the execve() function call */
		char *newargv[] = { executable, NULL };
		char *newenviron[] = { NULL };

		/* each process stops itself when created, and the 
		 * scheduler starts operating when all the processes
		 * are created and added to the process list */
		raise(SIGSTOP); // equivalent to kill(getpid(), SIGSTOP);

		execve(executable, newargv, newenviron);
		/* execve() only returns on error */
		perror("execve");
		exit(1);
  }
  new_process(&processes, executable, pid, LOW);
  return;
}

static int
prioritize_process(pid_t pid, enum priority new_priority)
{
  process_t *finditr = find_process(processes, pid);
  if(!finditr)
  {
    fprintf(stderr, "Couldn't find pid %d hence failed new priority assignment", pid);
    return 1;
  }
  finditr->rank = new_priority;
  return 0;
}

/* 
 * SIGALRM handler
 */
static void
sigalrm_handler(int signum)
{
	/* stop the execution of the current running process */
	kill(processes->task_pid, SIGSTOP);
}

/* 
 * SIGCHLD handler
 */
static void
sigchld_handler(int signum)
{
	pid_t pid;
	int status;
	while(1)
  {
		/* wait for any child (non-blocking) 
     * https://stackoverflow.com/questions/33508997/waitpid-wnohang-wuntraced-how-do-i-use-these/34845669 
     */
		pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
    if (pid<0)
    {
      perror("waitpid");
      exit(pid);
    } else if (pid==0)
      break;
#ifdef DEBUG
    explain_wait_status(pid, status);
#endif
    

int main()
{
  new_process(&processes, "Hello", 1234, LOW);
  print_processes(processes);
  prioritize_process(1234, HIGH);
  print_processes(processes);
  remove_process(&processes, 1234);
  return 0;
}

