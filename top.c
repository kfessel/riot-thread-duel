#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include <thread.h>
#include <sched.h>
#include <xtimer.h>

#include "clist.h"

#include "debug.h"

#if ENABLE_DEBUG
#define DEBUG_CMD(X) X
#else
#define DEBUG_CMD(X)
#endif

#define CPU_USAGE_PERMILLE 1

static kernel_pid_t check_pid;
static uint32_t     last_time;

static uint64_t     pid_usage[KERNEL_PID_LAST + 1];
static uint32_t     pid_cpu[KERNEL_PID_LAST + 1];

uint64_t get_pid_usage(kernel_pid_t t){
    return pid_usage[t];
}

uint32_t get_pid_cpu_permille(kernel_pid_t t){
    return pid_cpu[t];
}

int pid_usage_cmp(clist_node_t *a, clist_node_t *b)
{
    thread_t * a_thread = container_of(a, thread_t, rq_entry);
    thread_t * b_thread = container_of(b, thread_t, rq_entry);
    int64_t r = pid_usage[a_thread->pid] - pid_usage[b_thread->pid];
    if (r < 0) {
        return -1;
    }
    else if ( r > 0 ) {
        return 1;
    }
    else {
        return 0;
    }
}


#if CPU_USAGE_PERMILLE
int pid_cpu_cmp(clist_node_t *a, clist_node_t *b)
{
    thread_t * a_thread = container_of(a, thread_t, rq_entry);
    thread_t * b_thread = container_of(b, thread_t, rq_entry);
    int32_t r = pid_cpu[a_thread->pid] - pid_cpu[b_thread->pid];
    if (r < 0) {
        return -1;
    }
    else if ( r > 0 ) {
        return 1;
    }
    else {
        return 0;
    }
}
#endif


void update_pid_time(kernel_pid_t active_pid){
    uint32_t now = xtimer_now().ticks32;

#if CPU_USAGE_PERMILLE
    static uint32_t pid_cpu_c[KERNEL_PID_LAST + 1];
    static uint32_t cpu_c;
#endif
    if(active_pid != check_pid){
        DEBUG_CMD(puts("we missed a sched call"));
        //this should trigger on 1st call
        //and if a thread exits
    }else if(active_pid && active_pid != KERNEL_PID_UNDEF ){

        pid_usage[active_pid] += now - last_time;

 #if CPU_USAGE_PERMILLE
        //cpu 1/1024 ~ permille over at least the last 1000 000 ticks
        //using an infinite avg
        pid_cpu_c[active_pid] += now - last_time;
        cpu_c += now - last_time;
        pid_cpu[active_pid] += (now - last_time)/1024;
        if(cpu_c > 1000000){
            // cpu_c = cpu_c / (1024 / 2);
            cpu_c = cpu_c / (1024);

            for(int i = 0; i <= KERNEL_PID_LAST;i++){
                // pid_cpu[i] = pid_cpu[i] / 2 + pid_cpu_c[i] / cpu_c;
                pid_cpu[i] = pid_cpu_c[i] / cpu_c;
                pid_cpu_c[i] = 0;
            }
            cpu_c = 0;
        }
#endif
    }
    last_time = now;
}

void sched_monitor(kernel_pid_t active_pid, kernel_pid_t next_pid){
    //this is a KISS variant of /sys/schedstatistics/schedstatistics.c
    //assume nothing keeps the CPU longer than 2^32 us (70min)
    update_pid_time(active_pid);
    check_pid = next_pid;
}

void sched_monitor_init(void){
    check_pid = thread_get_active()->pid;
    last_time = xtimer_now().ticks32;

    sched_register_cb(sched_monitor);
}


void print_prio_list(void);

int clist_smallest_first(clist_node_t *list, clist_cmp_func_t cmp){
    //clist_smallest_first gets the smallest node in front of the list
    //the order of other nodes depends on the list
    clist_node_t * p;

    //no list or empty list will not be sorted
    if(!list || !list->next ) return 0;

    p = list->next->next;
    while(p->next != list->next){
        if( cmp(p->next, clist_lpeek(list)) < 0 ) {
            clist_node_t * min = p->next;
            p->next = p->next->next;
            clist_lpush(list, min);
        }else p=p->next;
    }
    if( cmp(p->next, clist_lpeek(list)) < 0 ) {
        clist_node_t * min = p->next;
        if(p->next == list->next)
            list->next = p;
        p->next = p->next->next;
        clist_lpush(list, min);
    }
    return 0; //a socond call will not change the order
}

void schedule_rr_n(void * d){
    (void)d;
    //this needs to run in ISR context
    //(it should not be scheduled and should not have its own thread)
    //but it can be started from any thread
    //it will setup a timer to pull itself up (its a müchhausen function)
    static xtimer_t rr_timer = {.callback=schedule_rr_n};
    thread_t * active_thread = thread_get_active();
    if( active_thread != NULL){
        //update current threads time
        //(if the sheduler never runs an update might not happen other wise)
        update_pid_time(active_thread->pid);

        //clist_sort is merge sort bubble once is more effective
        //clist_sort(&sched_runqueues[active_thread->priority],pid_usage_cmp);

        //my bubblers did not work as I intended TODO revisit that later
        //clist_bubbler(&sched_runqueues[active_thread->priority],pid_usage_cmp);

        //clist_smallest_first gets the smallest node in front of the run queue
        clist_smallest_first(&sched_runqueues[active_thread->priority],pid_usage_cmp);

        // print_prio_list();
        //if(sched_runqueues[thread_yield_higher_thread->priority].next != &thread_yield_higher_thread->rq_entry)
        thread_yield_higher();
    }
    xtimer_set(&rr_timer,1000);
}

// there is also a <RIOT>/sys/ps (this is simpler)
void print_top(void){
    printf("pid\t,name:\t,prio\t,stack\t,size\t,satus\t,usage\n");
    for(kernel_pid_t i = 0; i<=KERNEL_PID_LAST;i++){
        thread_t * t = (thread_t *) thread_get(i);
        if(t!=NULL){
            printf("%d\t,%s\t,%d\t,%d\t,%d\t,%d\t,%"PRId32"\n",
                   i,
                   t->name,
                   t->priority,
                   t->sp - t->stack_start,
                   t->stack_size,
                   t->status,
                   pid_cpu[i]);
        }

    }

}

static int print_list_pid( clist_node_t * a , void * arg){
    (void) arg;
    thread_t * a_thread = container_of(a, thread_t, rq_entry);
    printf("\t%d" ,a_thread->pid);
    printf(",%"PRIu64,pid_usage[a_thread->pid]);
    printf(",%u",pid_cpu[a_thread->pid]);
    return 0;
}

void print_prio_list(void){
    for(int i= 0; i < SCHED_PRIO_LEVELS; i++){
        if(sched_runqueues[i].next){
        printf("Pr: %d",i);
        clist_foreach(&sched_runqueues[i], print_list_pid , NULL);
        puts("");}
    }
}

void* top(void* d){
   d=d;
// #define RUNS int i=30;i;i--
#define RUNS ;;
   //endless
   for(RUNS){
        print_top();
        print_prio_list();
        unsigned long sleep = 500 * 1000;
        xtimer_usleep(sleep);
    }
    return NULL;
}

void thread_top(void){
    //TODO  return pid make it stopable
    int size = THREAD_STACKSIZE_DEFAULT;
    char * wech = malloc(size);
    thread_create(wech, size ,3,0, top,NULL,"top");
}

