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


void explain_wait_status(pid_t pid, int status);
void wait_for_ready_children(int cnt);
