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

	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);
	sigaddset(&sigset, SIGALRM);
	sa.sa_mask = sigset;
	if (sigaction(SIGCHLD, &sa, NULL) < 0) {
		perror("sigaction: sigchld");
		exit(1);
	}

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
      case RQ_PRINT_TASK:
        print_processes(processes);
        return 0;

      case RQ_KILL_TASK:
        return kill_by_id(processes, rq->pid);

      case RQ_EXEC_TASK:
        skud_create_process(rq->executable);
        return 0;

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
	       " h <id>     : set task identified by id to high priority\n"
	       " l <id>     : set task identified by id to low priority\n");
}

void shell_print_loop(char* cmdline, request_struct *rq_tmp)
{
    
    /* if nothing */
    if (strlen(cmdline) == 0 || strcmp(cmdline, "?") == 0){
      shell_print_help();
      return;
    }

    /* Quit */
    if (strcmp(cmdline, "q") == 0 || strcmp(cmdline, "Q") == 0) {
      fprintf(stderr, "Shell: Exiting. Goodbye.\n");
      exit(0);
    }

    /* Print Tasks */
    if (strcmp(cmdline, "p") == 0 || strcmp(cmdline, "P") == 0) {
      rq_tmp->c = RQ_PRINT_TASK;
      return;
    }

    /* Kill Task */
    if ((cmdline[0] == 'k' || cmdline[0] == 'K') &&
        cmdline[1] == ' ') {
      rq_tmp->c = RQ_KILL_TASK;
      return;
    }

    /* Exec Task */
    if ((cmdline[0] == 'e' || cmdline[0] == 'E') && cmdline[1] == ' ') {
      rq_tmp->c = RQ_EXEC_TASK;
      return;
    }

    /* High-prioritize task */
    if ((cmdline[0] == 'h' || cmdline[0] == 'H') && cmdline[1] == ' ') {
      rq_tmp->c = RQ_HIGH_TASK;
      return;
    }

    /* Low-prioritize task */
    if ((cmdline[0] == 'l' || cmdline[0] == 'L') && cmdline[1] == ' ') {
      rq_tmp->c = RQ_LOW_TASK;
      return;
    }

    /* Parse error, malformed command, whatever... */
    printf("command `%s': Bad Command.\n", cmdline);
    
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
		if (pid < 0){
			perror("main: fork");
			exit(1);
		}
		if (pid == 0){
			/* initialize the required arguments for the execve() function call */
			strncpy(executable, argv[i], TASK_NAME_SZ);
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

    if (nproc == 0) {
		fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
		exit(1);
	}

	/* set the alarm -> SIGALRM signal will be sent after SHED_TQ_SEC seconds */
	alarm(SCHED_TQ_SEC);

	/* start the execution of the first process in the
	 * process list by sending a SIGCONT signal because
	 * all the processes to be scheduled have raised the
	 * SIGSTOP signal until all the processes to be 
	 * are created */
	curr_proc = processes;
	kill(curr_proc->task_pid, SIGCONT);
  request_struct *rq_tmp; 
  rq_tmp->pid = curr_proc->task_pid;
  char cmdline[SHELL_CMDLINE_SZ];
  while(1)
  {
    printf("skud >> ");
    fflush(stdout);
    if (fgets(cmdline, SHELL_CMDLINE_SZ, stdin)==NULL)
    {
      fprintf(stderr, "skud: could not read command line, exiting \n");
      exit(1);
    }
    request_struct *rq_tmp;
    if(cmdline[strlen(cmdline)-1] == "\n")
      cmdline[strlen(cmdline)-1 ] = '\0';
    shell_print_loop(cmdline, rq_tmp);
    shell_request_loop(rq_tmp);
    
  }

    while (pause())
        ;

	return 0;

  return 0;
}
