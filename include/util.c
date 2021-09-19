#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/mman.h>

/*
 * This function receives an integer status value,
 * as returned by wait()/waitpid() and explains what
 * has happened to the child process.
 *
 * The child process may have:
 *    * been terminated because it received an unhandled signal (WIFSIGNALED)
 *    * terminated gracefully using exit() (WIFEXITED)
 *    * stopped because it did not handle one of SIGTSTP, SIGSTOP, SIGTTIN, SIGTTOU
 *      (WIFSTOPPED)
 *
 * For every case, a relevant diagnostic is output to standard error.
 */
void
explain_wait_status(pid_t pid, int status)
{
	if (WIFEXITED(status))
		fprintf(stderr, "My PID = %ld: Child PID = %ld terminated normally, exit status = %d\n",
			(long)getpid(), (long)pid, WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		fprintf(stderr, "My PID = %ld: Child PID = %ld was terminated by a signal, signo = %d\n",
			(long)getpid(), (long)pid, WTERMSIG(status));
	else if (WIFSTOPPED(status))
		fprintf(stderr, "My PID = %ld: Child PID = %ld has been stopped by a signal, signo = %d\n",
			(long)getpid(), (long)pid, WSTOPSIG(status));
	else {
		fprintf(stderr, "%s: Internal error: Unhandled case, PID = %ld, status = %d\n",
			__func__, (long)pid, status);
		exit(1);
	}
	fflush(stderr);
}

/*
 * This will NOT work if children use pause() to wait for SIGCONT.
 */
void 
wait_for_ready_children(int cnt)
{
	int i;
	pid_t p;
	int status;

	for (i = 0; i < cnt; i++) {
		/* Wait for any child, also get status for stopped children */
		p = waitpid(-1, &status, WUNTRACED);
		explain_wait_status(p, status);
		if (!WIFSTOPPED(status)) {
			fprintf(stderr, "Parent: Child with PID %ld has died unexpectedly!\n",
				(long)p);
			exit(1);
		}
	}
}
