#include "orchestra.h"

int create_shared_memory(char * shrm_name)
{
    /* Create shared memory object and set its size */
    char buffer [50];
    sprintf(buffer,"/%s",shrm_name);
    int fd = shm_open(buffer, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
        printf("Can not open the shared memory\n");
    if (ftruncate(fd, sizeof(struct shrm)) == -1)
        printf("Shared memory size problem\n");

    return fd;
}

void close_shared_memory(char * shrm_name)
{
    char buffer [50];
    sprintf(buffer,"/%s",shrm_name);
    int fd = shm_unlink(buffer);
    if (fd == -1)
        printf("Can not close the shared memory\n");
}

int spawn_process(char *process_name, char *shrm_name)
{
    pid_t pid;
    char *argv[] = {process_name,shrm_name,NULL};
    int status;
    char buffer [50];
    sprintf(buffer,"./%s",process_name);
    printf("Launch: %s\n", buffer);
    status = posix_spawn(&pid, buffer, NULL, NULL, argv, environ);
    if (status == 0)
        printf("Child pid: %i\n", pid);
    else
        printf("posix_spawn: %s\n", strerror(status));

    return pid;
}

BasicThread::BasicThread():timer_fd_(-1),running_(0){}

int BasicThread::create_periodic_thread( char * name, int priority, int stack_size_ko, unsigned int period_us, void * (*func)(void *) )
{
    pthread_attr_t thread_attributes;
    pthread_attr_init(&thread_attributes);
    pthread_attr_setdetachstate(&thread_attributes, PTHREAD_CREATE_DETACHED /*PTHREAD_CREATE_JOINABLE*/);
    pthread_attr_setschedpolicy(&thread_attributes, SCHED_FIFO);
    struct sched_param paramA = { .sched_priority = priority };
    pthread_attr_setschedparam(&thread_attributes, &paramA);
    pthread_attr_setstacksize(&thread_attributes, stack_size_ko*1024);

    // calc start time of the periodic thread
    struct timespec start_time;
    if ( clock_gettime( CLOCK_MONOTONIC, &start_time ) )
    {
        printf( "Failed to call clock_gettime\n" );
        return -4;
    }
    /* Start one seconde later from now. */
    start_time.tv_sec += START_DELAY_SECS ;

    // if a timerfd is used to make thread periodic (Linux / Xenomai 3),
    // initialize it before launching thread (timer is read in the loop)
    struct itimerspec period_timer_conf;
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, 0);
    if ( timer_fd_==-1 )
    {
        printf( "Failed to create timerfd for thread '%s'\n", name);
        return -3;
    }
    period_timer_conf.it_value = start_time;
    period_timer_conf.it_interval.tv_sec = 0;
    period_timer_conf.it_interval.tv_nsec = period_us*1000;
    if ( timerfd_settime(timer_fd_, TFD_TIMER_ABSTIME, &period_timer_conf, NULL) )
    {
        printf( "Failed to set periodic tor thread '%s', code %d\n", name, errno);
        return -5;
    }

    running_ = 1;
    if ( pthread_create( &th_, &thread_attributes, (void *(*)(void *))func, (void *)NULL )!=0 )
    {
        printf( "Failed to create thread '%s'... !!!!!\n", name );
        return -1;
    }
    else
    {
        pthread_attr_destroy(&thread_attributes);
        pthread_setname_np( th_, name );
        printf( "Created thread '%s' period=%dusecs ok.\n", name, period_us );
        return 0;
    }
}

void BasicThread::wait_periodic_thread( )
{
    int err = 0;
    uint64_t ticks;
    err = read(timer_fd_, &ticks, sizeof(ticks));
    if ( err<0 )
    {
        printf( "TimerFd wait period failed for thread: err %d\n", err );
    }
    if ( ticks>1 )
    {
        printf( "TimerFd wait period missed for thread: overruns: %lu\n", (long unsigned int)ticks );
    }
}
