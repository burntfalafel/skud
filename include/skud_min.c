#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <sys/wait.h>
#include <sys/types.h>

#define SECOND_PID_TEST 1

#include "util.h"
pid_t pid;

/* 
 * SIGALRM handler
 */
static void
sigalrm_handler(int signum)
{
	/* stop the execution of the current running process */
	kill(pid, SIGSTOP);
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
        exit(0);
    }
    /* if the process stopped because its available time has ended then
		 * schedule the next available process in the process list */
		if (WIFSTOPPED(status))
			/* current process now becomes the next process in the process list */
      printf("WIFSTOPPED here\n");
    /* continue the execution of the process that must be scheduled */
    printf("Hello father\n");
		kill(pid, SIGCONT);
    printf("Hello father\n");
		/* set the alarm -> SIGALRM signal will be sent after SHED_TQ_SEC seconds */
		alarm(12);
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

#ifdef SECOND_PID_TEST
pid_t savepid;
#endif
int main(int argc, char* argv[])
{
	 for (int i = 1; i < argc; i++){
		pid = fork();
	  char executable[100];
		if (pid < 0){
			perror("main: fork");
			exit(1);
		}
		if (pid == 0){
			/* initialize the required arguments for the execve() function call */
			strncpy(executable, argv[i], 100);
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
    #ifdef SECOND_PID_TEST
      if(i==1)
        savepid = pid;
#endif


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
	alarm(12);

  // delay has been added for stubbing
  sleep(3);
  printf("savepid %d\n", savepid);
printf("pid %d\n", pid);
  kill(pid, SIGCONT);
#ifdef SECOND_PID_TEST
  sleep(3);
  kill(pid, SIGSTOP);
  kill(savepid, SIGCONT);
    sleep(3);
    kill(pid, SIGSTOP);
  sleep(3);
#endif
  return 0;
}
	
