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

process_t* processes = NULL;
process_t* curr_proc = NULL;
int processlistlen;
int running_tasks;

static void new_process (process_t **processlist, char* processname, pid_t pid, enum priority pos)
{
  process_t* newprocess = malloc(sizeof(process_t));
  newprocess->task_pid = pid;
  newprocess->rank = pos;
  strcpy(newprocess->name, processname);
  processlistlen++;
  if (*processlist==NULL)
  {
    newprocess->next = newprocess;
    *processlist = newprocess;
    return;
  }
  process_t *head = *processlist;
  while((*processlist)->next!=head)
    *processlist=(*processlist)->next;
  (*processlist)->next=newprocess;
  newprocess->next = head;
  return;
}

static process_t *remove_process (process_t *head, pid_t pid)
{
  process_t *itr = head;
  process_t *prev = NULL;
  while(1)
  {
    if(!prev)
      prev = itr;
    if(itr->task_pid == pid)
    {
      //free(itr);
      processlistlen--;
      break;
    }
    prev=itr;
    itr=itr->next;
    if(itr == head)
    {
      fprintf(stderr, "Couldn't find the PID to remove\n");
      return NULL;
    }
  }


  // if more than one node, check if first
  if (itr == head && itr->next != head)
  {
    prev = head;
    while(prev->next != head)
      prev = prev->next;
    head = itr->next;
    prev->next = head;
    free(itr);
  }
  // check if node is last node
  else if ( itr->next == head && itr == head)
  {
    free(itr);
    head=NULL;
  }
  else
  {
    prev->next = itr->next;
    free(itr);
  }
  return head;
}

static process_t *next_curr_process()
{
  process_t *temp = curr_proc->next;
  for (int i = 0; i <= running_tasks; i++){
		if (temp->rank == HIGH)
			return temp;
		temp = temp->next;
	}
  return curr_proc->next;
}

process_t *find_process(process_t *processlist, pid_t pid)
{
  process_t *itr = processlist;
  process_t *head = processlist;
  do
  {
    if(pid==itr->task_pid)
      return itr;
    itr = itr->next;
  } while(itr != head);
  return NULL;
}

static void print_processes (process_t *processlist)
{
  process_t* head = processlist;
  do
  {
    printf("\nCurrently running %d of %d processes\n", running_tasks, processlistlen);
    if (processlist->rank == LOW)
      printf("Process ID %d \t | \t Process Name: %s \t | \t Priority: LOW \t |\n", processlist->task_pid, processlist->name);
    else if (processlist->rank == HIGH)
      printf("Process ID %d \t | \t Process Name: %s \t | \tPriority: HIGH \t |\n", processlist->task_pid, processlist->name);
    processlist = processlist->next;
  } while(processlist != head);
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
    if (WIFEXITED(status) || WIFSIGNALED(status))
    {
      running_tasks--;
      if((curr_proc->task_pid==pid) && (curr_proc->next->task_pid != curr_proc->task_pid))
      {
        remove_process(processes, pid);
        curr_proc = next_curr_process();
      }
      if(curr_proc->next==processes)
      {
        remove_process(processes, pid);
        curr_proc = NULL;
        printf("All processes terminated! \n");
        exit(0);
      }
    }
    /* if the process stopped because its available time has ended then
		 * schedule the next available process in the process list */
		if (WIFSTOPPED(status))
			/* current process now becomes the next process in the process list */
			curr_proc = next_curr_process();
    /* continue the execution of the process that must be scheduled */
		kill(curr_proc->rank, SIGCONT);
		/* set the alarm -> SIGALRM signal will be sent after SHED_TQ_SEC seconds */
		alarm(SCHED_TQ_SEC);
  }
}

