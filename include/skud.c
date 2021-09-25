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
#include "util.h"

process_t* processes = NULL;
process_t* curr_proc = NULL;
process_t* hist_proc = NULL;
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
  if(processlist)
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
  }
  else
  {
    printf("\nNo more processes left with skud!\n");
  }
  return;
}

static process_t*
check_if_process_busy(process_t *processlist)
{
  process_t *head = processlist;
  int return_wait = 0;
  int status;
  do
  {
    // printf("Is process terminated? : %d\n", kill(processlist->task_pid, 0));
    return_wait = waitpid(processlist->task_pid, &status, WNOHANG);
    if(processlist->task_pid == return_wait)
    {
      head = remove_process(head, processlist->task_pid);
      if(!head)
        return NULL;
      processlist = head;
      running_tasks--;
    }
    processlist = processlist->next;
  } while (processlist!=head);
  return_wait = waitpid(processlist->task_pid, &status, WNOHANG);  
  if(processlist==head && processlist->task_pid == return_wait)
  {
    head = remove_process(head, processlist->task_pid);
    if(!head)
      return NULL;
    processlist = head;
    running_tasks--;
  }

  return head;
}

static void
free_processes(process_t *processlist)
{
  process_t *head = processlist, *next_proc;
  do
  {
    next_proc = processlist->next;
    free(processlist);
    processlist = next_proc;
  } while (next_proc!=head);
  
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

/* Disable delivery of SIGALRM and SIGCHLD. */
static void
signals_disable(void)
{
	sigset_t sigset;

  /* allow SIGALARAM and SIGCHLD to block proc*/
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
		perror("signals_disable: sigprocmask");
		exit(1);
	}
}

/* Enable delivery of SIGALRM and SIGCHLD.  */
static void
signals_enable(void)
{
	sigset_t sigset;

  /* allow SIGALARAM and SIGCHLD to unblocked proc*/
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
		perror("signals_enable: sigprocmask");
		exit(1);
	}
}


/* Install two signal handlers.
 * One for SIGCHLD, one for SIGALRM.
 * Make sure both signals are masked when one of them is running.
 */
static void
install_signal_handlers(void)
{
	sigset_t sigset;
	struct sigaction sa;

	sa.sa_flags = SA_RESTART;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sa.sa_mask = sigset;
  
	sa.sa_handler = sigalrm_handler;
	if (sigaction(SIGALRM, &sa, NULL) < 0) {
		perror("sigaction: sigalrm");
		exit(1);
	}

	/*
	 * Ignore SIGPIPE, so that write()s to pipes
	 * with no reader do not result in us being killed,
	 * and write() returns EPIPE instead.
	 */
	if (signal(SIGPIPE, SIG_IGN) < 0) {
		perror("signal: sigpipe");
		exit(1);
	}
}

static int shell_request_loop(request_struct *rq)
{
  while(1)
  {
    signals_disable();
    switch (rq->c) {
      case RQ_PRINT_TASK:{
        if(processes)
        {
          processes = check_if_process_busy(processes);
          curr_proc = processes;
          hist_proc = processes;
        }
        print_processes(processes);
        return 0;
      }

      case RQ_KILL_TASK:{
        process_t *found = find_process(processes, rq->pid);
        return kill_by_id(found, rq->pid);
      }

      case RQ_EXEC_TASK:{
        skud_create_process(rq->executable);
        return 0;
      }

      case RQ_START_TASK:{
        // process_t *found = find_process(processes, rq->pid);
        // curr_proc->task_pid = rq->pid;
        running_tasks++;
        kill(rq->pid, SIGCONT);
        return 0;
      }

      case RQ_HALT_TASK:{
        running_tasks--;
        kill(rq->pid, SIGSTOP);
        return 0;
      }

      case RQ_HIGH_TASK:
        return prioritize_process(rq->pid, HIGH);

      case RQ_LOW_TASK:
        return prioritize_process(rq->pid, LOW);

      default:
        return -ENOSYS;
    }
    signals_enable();
  }
}

void shell_print_help(void)
{
  printf(" ?          : print help\n"
	       " q          : quit\n"
	       " p          : print tasks\n"
	       " k <id>     : kill task identified by id\n"
	       " e <program>: execute program\n"
         " st <id>    : start task identified by id\n"
         " pa <id>    : pause task identified by id\n"
	       " h <id>     : set task identified by id to high priority\n"
	       " l <id>     : set task identified by id to low priority\n");
}

void shell_print_loop(char* cmdline, request_struct *rq_tmp)
{
    
    /* if nothing */
    if (strlen(cmdline) == 0 || cmdline[0] == '?'){
      shell_print_help();
      rq_tmp->c = -1;
      return;
    }

    /* Quit */
    if (cmdline[0] == 'q' || cmdline[0] == 'Q')  {
      fprintf(stderr, "Shell: Exiting. Goodbye.\n");
      free(rq_tmp);
      if(processes)
        free_processes(processes);
      exit(0);
    }

    /* Print Tasks */
    if (cmdline[0] == 'p' || cmdline[0] == 'P') {
      rq_tmp->c = RQ_PRINT_TASK;
      return;
    }

    /* Kill Task */
    if ((cmdline[0] == 'k' || cmdline[0] == 'K')) {
      rq_tmp->c = RQ_KILL_TASK;
      return;
    }

    /* Exec Task */
    if ((cmdline[0] == 'e' || cmdline[0] == 'E')) {
      pid_t inp_pid;
      rq_tmp->executable = cmdline+2;
      rq_tmp->c = RQ_EXEC_TASK;
      return;
    }

    if (cmdline[0] == 's' && cmdline[1] == 't')
    {
      pid_t inp_pid;
      inp_pid = atol(cmdline+3);
      if(!inp_pid)
      {
        inp_pid = curr_proc->task_pid;
        curr_proc = curr_proc->next;
      }
      rq_tmp->pid = inp_pid;
      rq_tmp->c = RQ_START_TASK;
      return;
    
    }

    if (cmdline[0] == 'h' && cmdline[1] == 'a')
    {
      pid_t inp_pid;
      inp_pid = atol(cmdline+3);
      if(!inp_pid)
      {
        inp_pid = hist_proc->task_pid;
        hist_proc = hist_proc->next;
      }
      rq_tmp->pid = inp_pid;
      rq_tmp->c = RQ_HALT_TASK;
      return;
    }

    /* High-prioritize task */
    if ((cmdline[0] == 'h' || cmdline[0] == 'H')) {
      rq_tmp->c = RQ_HIGH_TASK;
      return;
    }

    /* Low-prioritize task */
    if ((cmdline[0] == 'l' || cmdline[0] == 'L')) {
      rq_tmp->c = RQ_LOW_TASK;
      return;
    }

    /* Parse error, malformed command, whatever... */
    rq_tmp->c = -1;
    printf("command '%s': Bad Command.\n", cmdline);
    
}

int main(int argc, char *argv[])
{
    /*
	 * For each of argv[1] to argv[argc - 1],
	 * create a new child process, add it to the process list.
	 */
	 pid_t pid;
	 for (int i = 1; i < argc; i++){
		pid = fork();
	  char executable[TASK_NAME_SZ];
		strncpy(executable, argv[i], TASK_NAME_SZ);
    if (pid < 0){
			perror("main: fork");
			exit(1);
		}
		if (pid == 0){
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
		/* add process to the process list */
		new_process(&processes, executable, pid, LOW);
	}	
	int nproc = argc;  /* number of proccesses goes here */

	/* Wait for all children to raise SIGSTOP before exec()ing. */
	wait_for_ready_children(nproc);

	/* Install SIGALRM and SIGCHLD handlers. */
	install_signal_handlers();

  if (nproc == 1) {
		fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
		exit(1);
	}

	/* set the alarm -> SIGALRM signal will be sent after SHED_TQ_SEC seconds */
	alarm(SCHED_TQ_SEC);
  
  /* We want to intialize variables before loop
   */
  curr_proc = processes;
  hist_proc = processes;
	request_struct *rq_tmp = malloc(sizeof(request_struct)); 
    
  while(1)
  {
    /* start the execution of the first process in the
    * process list by sending a SIGCONT signal because
    * all the processes to be scheduled have raised the
    * SIGSTOP signal until all the processes to be 
    * are created */
    if(processes)
      rq_tmp->pid = processes->task_pid;
    char cmdline[SHELL_CMDLINE_SZ];
    
    printf("skud >> ");
    fflush(stdout);
    if (fgets(cmdline, SHELL_CMDLINE_SZ, stdin)==NULL)
    {
      fprintf(stderr, "skud: could not read command line, exiting \n");
      exit(1);
    }
    if(cmdline[strlen(cmdline)-1] == '\n')
      cmdline[strlen(cmdline)-1 ] = '\0';
    
    shell_print_loop(cmdline, rq_tmp);
    shell_request_loop(rq_tmp);
    rq_tmp->c = NULL;
    
  }
  free(rq_tmp);

    while (pause())
        ;

	return 0;

}
