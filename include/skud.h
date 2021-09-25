#ifndef SKUD_H
#define SKUD_H

#define SCHED_TQ_SEC 100
#define DEBUG 1
#define TASK_NAME_SZ 100
#define SHELL_CMDLINE_SZ 100

#include <sys/types.h>

enum priority 
{
  LOW,
  HIGH };

typedef struct process_t
{
  struct process_t *next;
  pid_t task_pid;
  enum priority rank;
  char name[20];
} process_t;

enum request_type
{
    RQ_PRINT_TASK,
    RQ_KILL_TASK,
    RQ_EXEC_TASK,
    RQ_HIGH_TASK,
    RQ_LOW_TASK,
    RQ_START_TASK,
    RQ_HALT_TASK
};

typedef struct request_struct
{
  process_t *proc;
  pid_t pid;
  char *executable;
  enum request_type c;
} request_struct;

static void new_process(process_t **processlist, char *processname, pid_t pid, enum priority pos);

static process_t *remove_process (process_t *head, pid_t pid);


static process_t *next_curr_process();

static void print_processes (process_t *processlist);

static int prioritize_process(pid_t pid, enum priority new_priority);

static void signals_disable(void);

static void signals_enable(void);

static void install_signal_handlers(void);

#endif
