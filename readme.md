# skud : A simple user-space scheduler

User-space scheduler which provides a shell-interface to control (the start and stop of) each PID you decide to launch with your scheduler. The process datastructure is maintained by a singly circular linked list (round-robin fashion). 

## Working
The scheduler here is the parent process and triggers child processes (provided through arguments) through the shell interface. UNIX signal handling is then done to each of the processes to get the desired affect. A SIGALARM handler has also been added to kill the process if it timesout.

## Usage
```
$ skud <binary1> <binary2> ...
My PID = 19884: Child PID = 19885 has been stopped by a signal, signo = 19
My PID = 19884: Child PID = 19886 has been stopped by a signal, signo = 19
skud >> ?
 ?          : print help
 q          : quit
 p          : print tasks
 k <id>     : kill task identified by id
 e <program>: execute program
 st <id>    : start task identified by id
 st         : start the first process in list. Consecutive calls to this traverses through list
 ha <id>    : pause task identified by id
 ha         : pause the first process running in list. Consecutive calls to this traverses through list
 h <id>     : set task identified by id to high priority
 l <id>     : set task identified by id to low priority
skud >>
```

P.S. The backend of skud uses a `exceve()` call which will only run binaries/scripts if you provide it's full path or if the binary is present in it's pwd. So for e.g. `skud /bin/ls prog` will work fine as long as `prog` is in the same folder (pwd) as skud is. 

## Build instructions
`make all`.
To clean `make clean`.

## Why use it? 
Any shell (take Bourne-compatible shell for example) has `job control` utilities which are used in UNIX systems. One might have encounterd them through the `&` at the end of a scripts/binaries written in shell scripts, allowing them to run in the background. One can interact with them using `fg` and `bg`. 

However it becomes a pain to manage when you have multiple binaries to suspend/resume. Further users might forget to `kill` them, leading to unwanted problems during debugging/deployment time. **skud** not only provides you a friendly interface to allow job control but also manages any process management you might have forgetten. This makes it a very helpful tool in the development world! 

## What is left to do? 
- Create an autoscheduler which schedules processes based on an aging mechanisim similar to what [inspired](https://github.com/dlekkas/proc_scheduler) me to do this project. 
- No low prioirity management yet 
