#include "threadtools.h"
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

void fibonacci(int id, int arg) 
{
    thread_setup(id, arg);
    
    for (RUNNING->i = 1; ; RUNNING->i++) 
    {
        if (RUNNING->i <= 2)
            RUNNING->x = RUNNING->y = 1;
        else 
        {
            int tmp = RUNNING->y;  
            RUNNING->y = RUNNING->x + RUNNING->y;
            RUNNING->x = tmp;
        }
        printf("%d %d\n", RUNNING->id, RUNNING->y);
        sleep(1);

        if (RUNNING->i == RUNNING->arg) {
            thread_exit();
        }
        else {
            thread_yield();
        }
    }
}

void collatz(int id, int arg) 
{
    thread_setup(id, arg);

    for (RUNNING->i = 1; ; RUNNING->i++) 
    {
        if(RUNNING->i == 1)
            RUNNING->x = RUNNING->arg;

        if(RUNNING->x % 2 == 0)
            RUNNING->x /= 2;
        else    
        {
            RUNNING->x *= 3;
            RUNNING->x += 1;
        }

        printf("%d %d\n", RUNNING->id, RUNNING->x);
        sleep(1);

        if (RUNNING->x == 1) {
            thread_exit();
        }
        else {
            thread_yield();
        }
    }
}

void max_subarray(int id, int arg) 
{
    thread_setup(id, arg);

    for (RUNNING->i = 1; ; RUNNING->i++) 
    {
        async_read(5);

        //printf("%d %d : %s", cnt, strlen(RUNNING->buf), RUNNING->buf);
        int t = atoi(RUNNING->buf);

        RUNNING->x = t;
        if(RUNNING->x < RUNNING->x + RUNNING->y)
            RUNNING->y += RUNNING->x;
        else    
            RUNNING->y = RUNNING->x;
        if(RUNNING->y > RUNNING->sum)
            RUNNING->sum = RUNNING->y;

        printf("%d %d\n", RUNNING->id, RUNNING->sum);
        sleep(1);

        if (RUNNING->i == RUNNING->arg) 
        {
            thread_exit();
        }
        else 
        {
            thread_yield();
        }
    }
}

