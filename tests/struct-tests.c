#include <stdio.h>

#include "skud.h"


int main()
{
  process_t* processes = NULL;

  process_t* curr_proc = NULL;
  int processlistlen;
  int running_tasks;

  new_process(&processes, "Hello", 1234, LOW);
  print_processes(processes);
  prioritize_process(1234, HIGH);
  print_processes(processes);
  new_process(&processes, "HelloIsBack", 4321, LOW);
  print_processes(processes);
  processes = remove_process(processes, 1234);
  print_processes(processes);
  processes = remove_process(processes, 4321);
  
  printf("\n\n Test 2 \n\n");
  new_process(&processes, "HelloIsBack2", 9871, LOW);
  new_process(&processes, "HelloIsBack4", 9872, LOW);
  print_processes(processes);
  prioritize_process(9871, HIGH);
  print_processes(processes);
  new_process(&processes, "HelloIsBack3", 8972, LOW);
  print_processes(processes);
  processes = remove_process(processes, 9871);
  print_processes(processes);
  processes = remove_process(processes, 8972);
  processes = remove_process(processes, 9872);
  
  return 0;
}
