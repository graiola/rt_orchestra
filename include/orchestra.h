#ifndef ORCHESTRA_H
#define ORCHESTRA_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <sys/timerfd.h>
#include <fcntl.h> /* For O_* constants */
#include <string.h>

#define PERIOD_MICROSECS_MOTOR 1000 //1millisecs
#define PERIOD_MICROSECS_CONTROL 10000 //10millisecs
#define START_DELAY_SECS 1 //1sec

// TODO, Move to a class with mutexs
struct shrm {        /* Defines "structure" of shared memory */
   long long cnt;
};

int create_shared_memory(char * shrm_name);

void close_shared_memory(char * shrm_name);

void spawn_process(char *process_name, char *shrm_name);

class BasicThread
{

  public:

    BasicThread();

    int create_periodic_thread( char * name, int priority, int stack_size_ko, unsigned int period_us, void * (*func)(void *) );

    void wait_periodic_thread( );

    int timer_fd_;
    int running_;
    pthread_t th_;
};

#endif
