#include "threadtools.h"
#include <sys/select.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call longjmp(sched_buf, 1).
*/
void sighandler(int signo) 
{
    if(signo == SIGALRM)
    {
        printf("%s\n", "caught SIGALRM");
        alarm(timeslice);
    }
    else if(signo == SIGTSTP)
    {
        printf("%s\n", "caught SIGTSTP");
    }
    longjmp(sched_buf, 1);
}

/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
*/
void reorderwaiting(int n)
{
    //printf("in reorder\n");
    struct timeval timeout={0,0};
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int maxfd = 0;
    //printf("fd_set initialize finish\n");
    for(int i = 0; i < wq_size; i++)
    {
        //printf("in the for loop\n");
        FD_SET(waiting_queue[i]->fd, &read_fds);
        //printf("set the fd\n");
        if(waiting_queue[i]->fd > maxfd)
            maxfd = waiting_queue[i]->fd;
    }
    //printf("add waiting fd finish\n");
    int check;
    if(n == 0)
        check = select(maxfd+1, &read_fds, NULL, NULL, &timeout);
    else    
        check = select(maxfd+1, &read_fds, NULL, NULL, NULL);

    if(check < 0)
    {
        perror("select error : ");
        exit(1);
    }
    //printf("select finish\n");
    for(int i = 0; i < wq_size; i++)
    {
        if(FD_ISSET(waiting_queue[i]->fd, &read_fds)) //waiting is ready
        {
            ready_queue[rq_size] = waiting_queue[i];
            rq_size += 1;
            waiting_queue[i] = NULL;
        }
    }

    int cur = 0;
    for(int i = 0; i < wq_size; i++)
    {
        if(waiting_queue[i] == NULL)
        {
        }
        else
        {
            waiting_queue[cur] = waiting_queue[i];
            if(cur != i)
                waiting_queue[i] = NULL;
            cur += 1;
        }
    }
    wq_size = cur;
}

void scheduler() 
{
    int sch = setjmp(sched_buf);
    if(sch == 0)//from main.c
    {
        //printf("%d %d %d\n", rq_current, rq_size, wq_size);
        longjmp(RUNNING->environment, 1);
    }
    else if(sch == 1)//interrupt //from sighandler
    {
        //printf("%d %d %d\n", rq_current, rq_size, wq_size);
        //move waiting queue //push waiting's ready to ready queue //reorganize waiting 
        if(wq_size > 0)
            reorderwaiting(0);
        //printf("after reorder\n");
        
        //execute the next one
        if(rq_current == rq_size-1)
            rq_current = 0;
        else    
            rq_current += 1;
        //printf("%d %d %d\n", rq_current, rq_size, wq_size);
        //rq_size will only increse or be the same so there won't have the empty problem
        longjmp(RUNNING->environment, 1);
    }
    else if(sch == 2)//from async_read   //execute the same
    {
        //printf("%d %d %d\n", rq_current, rq_size, wq_size);
        //move waiting queue //push waiting's ready to ready queue //reorganize waiting
        if(wq_size > 0)
            reorderwaiting(0);
        //printf("after reorder\n");
        //erase current and push to waiting //reorganize ready
        waiting_queue[wq_size] = ready_queue[rq_current];
        wq_size += 1;

        ready_queue[rq_current] = NULL;
        rq_size -= 1;
        //printf("after push current to waiting\n");
        //printf("%d %d %d\n", rq_current, rq_size, wq_size);
        //because we push one to waiting queue so it won't have the nothing situation

        //we push the last one to waiting queue
        if(rq_size == 0) //rq_cur must be at 0
        {
            rq_current = 0;
            reorderwaiting(1);
        }
        else
        {
            if(rq_current != rq_size)
            {
                ready_queue[rq_current] = ready_queue[rq_size];
                ready_queue[rq_size] = NULL;
            }
            else
            {
                rq_current = 0;
            }
        }
        //printf("after adjusting current\n");
        //printf("%d %d %d\n", rq_current, rq_size, wq_size);
        longjmp(RUNNING->environment, 1);
    }
    else if(sch == 3)//from thread_exit   //execute the same
    {
        //printf("%d %d %d\n", rq_current, rq_size, wq_size);
        //push waiting's ready to ready queue//reorganize waiting
        if(wq_size > 0)
            reorderwaiting(0);
        //printf("after reorder\n");
        //printf("%d %d %d\n", rq_current, rq_size, wq_size);
        //erase current //reorganize ready
        free(ready_queue[rq_current]);
        ready_queue[rq_current] = NULL;
        rq_size -= 1;

        //printf("after delete\n");
        //printf("%d %d %d\n", rq_current, rq_size, wq_size);
        if(rq_size == 0 && wq_size == 0) //nothing to do
            return;
        else if(rq_size == 0)
        {
            rq_current = 0;
            reorderwaiting(1);
        }
        else
        {
            if(rq_current != rq_size)
            {
                ready_queue[rq_current] = ready_queue[rq_size];
                ready_queue[rq_size] = NULL;
            }
            else
            {
                rq_current = 0;
            }
        }
        //printf("after adjusting current\n");
        //printf("%d %d %d\n", rq_current, rq_size, wq_size);
        longjmp(RUNNING->environment, 1);
    }
}
