#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <thread.h>
#include <sched.h>

#include <math.h>
#include <xtimer.h>
void schedule_rr(void * d);
void schedule_rr_n(void * d);

void bad_wait(uint64_t us){
    //keep the cpu busy waiting for some time to pass
    uint64_t until = xtimer_now_usec64() + us;
    while(xtimer_now_usec64() < until);//do nothing
}

void nice_wait(uint64_t us){
    //be nice give the core some time to do other things or rest
    xtimer_usleep64(us);
}

void* thread_bad_nice(void* d){
        uint64_t w = 0;
        xtimer_usleep64(200*1000);//allways be nice at start
        int16_t pid = thread_getpid();
        if(* (char*) d < 0 || * (char*) d >10)
            * (char*) d = 5;

        uint32_t nice =  *(char*) d * 20;
        uint32_t bad =  200 - nice;
        uint32_t work = bad;
        uint32_t rest = nice;

        //work some time and rest
        for (;;){
            printf("T-Pid %i: %llu, %d\n",pid, w, *(char*)d);
            bad_wait(work * 1000);
            w += work;
            nice_wait(rest * 1000);
        }
}


void* thread_bad(void* d){
        uint64_t w = 0;
        xtimer_usleep64(200*1000);//allways be nice at start
        int16_t pid = thread_getpid();
        if(* (char*) d < 0 || * (char*) d >10)
            * (char*) d = 5;
        uint32_t nice =  *(char*) d * 20;
        uint32_t bad =  200 - nice;
        uint32_t work = bad;
        uint32_t rest = nice;

        //work some time yield rest blocking
        for (;;){
            printf("T-Pid %i: %llu, %d\n",pid, w, *(char*)d);
            bad_wait(work * 1000);
            w += work;
            thread_yield();
            bad_wait(rest * 1000);
        }
}

void* thread_xbad(void* d){
        uint64_t w = 0;
        xtimer_usleep64(200*1000);//allways be nice at start
        int16_t pid = thread_getpid();
        if(* (char*) d < 0 || * (char*) d >10)
            * (char*) d = 5;

        uint32_t nice =  *(char*) d * 20;
        uint32_t bad =  200 - nice;
        uint32_t work = bad;
        uint32_t rest = nice;
        //work some time and rest blocking
        for (;;){
            printf("T-Pid %i: %llu, %d\n",pid, w, *(char*)d);
            bad_wait(work * 1000);
            w += work;
            bad_wait(rest * 1000);
        }
}

void* thread_restless(void* d){
        uint64_t w = 0;
        xtimer_usleep64(200*1000);//allways be nice at start
        int16_t pid = thread_getpid();
        if(* (char*) d < 0 || * (char*) d >10)
            * (char*) d = 5;

        uint32_t nice =  *(char*) d ;
        uint32_t work = 100*(10 - nice);
        //work for some time and yield
        for (;;){
            if(w % 100000 == 0)printf("T-Pid %i: %llu, %d\n",pid, w, *(char*)d);
            bad_wait(work);
            w += work;
            thread_yield();
        }
}


void* thread_xrestless(void* d){
        uint64_t w = 0;
        xtimer_usleep64(200*1000);//allways be nice at start
        int16_t pid = thread_getpid();
        if(* (char*) d < 0 || * (char*) d >10)
            * (char*) d = 5;

        uint32_t nice =  *(char*) d ;
        uint32_t work = 100*(10 - nice);
        //continue working
        for (;;){
            if(w % 100000 == 0)printf("T-Pid %i: %llu, %d\n",pid, w, *(char*)d);
            bad_wait(work);
            w += work;
        }
}


void thread_top(void);
void sched_monitor_init(void);

int main(void){
    start_schedule_rr();
    sched_monitor_init();
    if(1) thread_top();
    {
        int size = THREAD_STACKSIZE_DEFAULT ;
        char * wech = malloc(size + 4);
        * wech = 1; // 0-10 niceness
        thread_create(wech+4, size ,7,0, thread_restless,wech,"T1");
    }
    {
        int size = THREAD_STACKSIZE_DEFAULT;
        char * wech = malloc(size + 4);
        * wech = 5; // 0-10 niceness
        thread_create(wech+4, size ,7,0, thread_restless,wech,"T2");
    }
    {
        int size = THREAD_STACKSIZE_DEFAULT ;
        char * wech = malloc(size + 4);
        * wech = 5; // 0-10 niceness
        thread_create(wech+4, size ,7,0, thread_xrestless,wech,"T3");
    }
//     char buff[200];
//     snprintf(buff, sizeof(buff),"%s ", main);

//     puts(buff);

//     return puts("test");
//     for(;;)nice_wait(200); //keep main running
    //TODO how to give this a new purpose //thread erbschaft
}
