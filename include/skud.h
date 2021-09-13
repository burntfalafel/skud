#ifndef SKUD_H
#define SKUD_H

#define SCHED_TQ_SEC 100

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

static void new_process(process_t **processlist, char *processname, pid_t pid, enum priority pos);

static process_t *remove_process (process_t *head, pid_t pid);


static process_t *next_curr_process();

static void print_processes (process_t *processlist);

static int prioritize_process(pid_t pid, enum priority new_priority);

#endif
