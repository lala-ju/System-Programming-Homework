#ifndef THREADTOOL
#define THREADTOOL
#include <setjmp.h>
#include <sys/signal.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define THREAD_MAX 16  // maximum number of threads created
#define BUF_SIZE 512
struct tcb 
{
    int id;  // the thread id
    jmp_buf environment;  // where the scheduler should jump to
    int arg;  // argument to the function
    int fd;  // file descriptor for the thread
    char buf[BUF_SIZE];  // buffer for the thread
    int i, x, y;  // declare the variables you wish to keep between switches
    int sum;
};

extern int timeslice;
extern jmp_buf sched_buf;
extern struct tcb *ready_queue[THREAD_MAX], *waiting_queue[THREAD_MAX];
/*
 * rq_size: size of the ready queue
 * rq_current: current thread in the ready queue
 * wq_size: size of the waiting queue
 */
extern int rq_size, rq_current, wq_size;
/*
* base_mask: blocks both SIGTSTP and SIGALRM
* tstp_mask: blocks only SIGTSTP
* alrm_mask: blocks only SIGALRM
*/
extern sigset_t base_mask, tstp_mask, alrm_mask;
/*
 * Use this to access the running thread.
 */
#define RUNNING (ready_queue[rq_current])

void sighandler(int signo);
void scheduler();

#define thread_create(func, id, arg) {\
    func(id, arg);\
}

#define thread_setup(id, arg) {\
    char fifoname[512] = {0};\
    sprintf(fifoname, "%d_%s", id, __FUNCTION__);\
    ready_queue[rq_size] = malloc(sizeof(struct tcb));\
    ready_queue[rq_size]->id = id;\
    ready_queue[rq_size]->arg = arg;\
    sprintf(ready_queue[rq_size]->buf, "%s", "");\
    ready_queue[rq_size]->i = 0;\
    ready_queue[rq_size]->x = 0;\
    ready_queue[rq_size]->y = 0;\
    ready_queue[rq_size]->sum = 0;\
    mkfifo(fifoname, 0666);\
    ready_queue[rq_size]->fd = open(fifoname, O_RDONLY | O_NONBLOCK);\
    int jump = setjmp(ready_queue[rq_size]->environment);\
    if(jump == 0) {\
        rq_size += 1;\
        return;\
    }\
}

#define thread_exit() {\
    close(RUNNING->fd);\
    char fifoname[512] = {0};\
    sprintf(fifoname, "%d_%s", RUNNING->id, __FUNCTION__);\
    if(unlink(fifoname) < 0) perror("delete fifo error: ");\
    longjmp(sched_buf, 3);\
}

#define thread_yield() {\
    int jump = setjmp(RUNNING->environment);\
    if(jump == 0) {\
        sigprocmask(SIG_SETMASK, &alrm_mask, NULL);\
        sigprocmask(SIG_SETMASK, &tstp_mask, NULL);\
        sigprocmask(SIG_SETMASK, &base_mask, NULL);\
    }else \
        sigprocmask(SIG_SETMASK, &base_mask, NULL);\
}

#define async_read(count) ({\
    int jump = setjmp(RUNNING->environment);\
    if(jump == 0)\
        longjmp(sched_buf, 2);\
    else\
        read(RUNNING->fd, &RUNNING->buf, count);\
})

#endif // THREADTOOL
