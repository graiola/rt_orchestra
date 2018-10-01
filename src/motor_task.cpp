#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <sys/timerfd.h>
#include <fcntl.h> /* For O_* constants */

#define PERIOD_MICROSECS_MOTOR 1000 //1millisecs
#define PERIOD_MICROSECS_CONTROL 10000 //10millisecs
#define START_DELAY_SECS 1 //1sec

pthread_t MyPosixThread;
int TimerFdForThread = -1;
char ThreadRunning = 0;
int IncValue = 0;

struct region {        /* Defines "structure" of shared memory */
   long long cnt;
};
int fd;


int CreatePosixTask( char * TaskName, int Priority, int StackSizeInKo, unsigned int PeriodMicroSecs, void * (*pTaskFunction)(void *) )
{
    pthread_attr_t ThreadAttributes;
    pthread_attr_init(&ThreadAttributes);
    pthread_attr_setdetachstate(&ThreadAttributes, PTHREAD_CREATE_DETACHED /*PTHREAD_CREATE_JOINABLE*/);
#if defined(__XENO__) || defined(__COBALT__)
    pthread_attr_setschedpolicy(&ThreadAttributes, SCHED_FIFO);
    struct sched_param paramA = { .sched_priority = Priority };
    pthread_attr_setschedparam(&ThreadAttributes, &paramA);
#endif
    pthread_attr_setstacksize(&ThreadAttributes, StackSizeInKo*1024);

    // calc start time of the periodic thread
    struct timespec start_time;
#ifdef __XENO__
    if ( clock_gettime( CLOCK_REALTIME, &start_time ) )
#else
    if ( clock_gettime( CLOCK_MONOTONIC, &start_time ) )
#endif
    {
        printf( "Failed to call clock_gettime\n" );
        return -4;
    }
    /* Start one seconde later from now. */
    start_time.tv_sec += START_DELAY_SECS ;

    // if a timerfd is used to make thread periodic (Linux / Xenomai 3),
    // initialize it before launching thread (timer is read in the loop)
#ifndef __XENO__
    struct itimerspec period_timer_conf;
    TimerFdForThread = timerfd_create(CLOCK_MONOTONIC, 0);
    if ( TimerFdForThread==-1 )
    {
        printf( "Failed to create timerfd for thread '%s'\n", TaskName);
        return -3;
    }
    period_timer_conf.it_value = start_time;
    period_timer_conf.it_interval.tv_sec = 0;
    period_timer_conf.it_interval.tv_nsec = PeriodMicroSecs*1000;
    if ( timerfd_settime(TimerFdForThread, TFD_TIMER_ABSTIME, &period_timer_conf, NULL) )
    {
        printf( "Failed to set periodic tor thread '%s', code %d\n", TaskName, errno);
        return -5;
    }
#endif

    ThreadRunning = 1;
    if ( pthread_create( &MyPosixThread, &ThreadAttributes, (void *(*)(void *))pTaskFunction, (void *)NULL )!=0 )
    {
        printf( "Failed to create thread '%s'... !!!!!\n", TaskName );
        return -1;
    }
    else
    {
        // make thread periodic for Xenomai 2 with pthread_make_periodic_np() function.
#ifdef __XENO__
        struct timespec period_timespec;
        int err = 0;
        period_timespec.tv_sec = 0;
        period_timespec.tv_nsec = PeriodMicroSecs*1000;
        if ( pthread_make_periodic_np(MyPosixThread, &start_time, &period_timespec)!=0 )
        {
            printf("Xenomai make_periodic failed for thread '%s',: err %d\n", TaskName, err);
            return -2;
        }
#endif

        pthread_attr_destroy(&ThreadAttributes);
#ifdef __XENO__
        pthread_set_name_np( MyPosixThread, TaskName );
#else
        pthread_setname_np( MyPosixThread, TaskName );
#endif
        printf( "Created thread '%s' period=%dusecs ok.\n", TaskName, PeriodMicroSecs );
        return 0;
    }
}

void WaitPeriodicPosixTask( )
{
    int err = 0;
#ifdef __XENO__
    unsigned long overruns;
    err = pthread_wait_np(&overruns);
    if (err || overruns)
    {
        printf( "Xenomai wait_period failed for thread: err %d, overruns: %lu\n", err, overruns );
    }
#else
    uint64_t ticks;
    err = read(TimerFdForThread, &ticks, sizeof(ticks));
    if ( err<0 )
    {
        printf( "TimerFd wait period failed for thread: err %d\n", err );
    }
    if ( ticks>1 )
    {
        printf( "TimerFd wait period missed for thread: overruns: %lu\n", (long unsigned int)ticks );
    }
#endif
}

// our really simple thread just incrementing a variable in loop !
void * MySimpleMotorTask( void * dummy )
{

    printf( "Task Motor\n" );

    void *rptr;
    rptr = mmap(NULL, sizeof(struct region),
           PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rptr == MAP_FAILED)
    {
        printf( "Motor task, Mapping failed!" );
        return 0;
    }

    /* Now we can refer to mapped region using fields of rptr;
       for example, rptr->len */

    while( ThreadRunning )
    {
        WaitPeriodicPosixTask( );
        IncValue++;
        static_cast<region*>(rptr)->cnt++;
    }

    printf( "Motor read value: %d\n" , static_cast<region*>(rptr)->cnt);

    return 0;
}

// our really simple thread just reading a variable !
void * MySimpleControlTask( void * dummy )
{

    printf( "Task Control\n" );

    void *rptr;
    rptr = mmap(NULL, sizeof(struct region),
           PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rptr == MAP_FAILED)
    {
        printf( "Control task, Mapping failed!" );
        return 0;
    }

    /* Now we can refer to mapped region using fields of rptr;
       for example, rptr->len */

    while( ThreadRunning )
    {
        WaitPeriodicPosixTask( );
        IncValue++;
    }

    printf( "Control read value: %d\n" , static_cast<region*>(rptr)->cnt);

    return 0;
}

int main( int argc, char * argv[] )
{

    /* Create shared memory object and set its size */
    fd = shm_open("/myregion", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
        /* Handle error */;
    if (ftruncate(fd, sizeof(struct region)) == -1)
        /* Handle error */;

    /* Map shared memory object */

    int err_motor, err_control;
    printf("Simple periodic thread using Posix.\n");
#if defined( __XENO__ ) || defined( __COBALT__ )
    printf("Version compiled for Xenomai 2 or 3.\n");
    mlockall(MCL_CURRENT | MCL_FUTURE);
#endif
    err_motor = CreatePosixTask( "motor", 1/*Priority*/, 4/*StackSizeInKo*/, PERIOD_MICROSECS_MOTOR/*PeriodMicroSecs*/, MySimpleMotorTask );
    err_control = CreatePosixTask( "task", 1/*Priority*/, 4/*StackSizeInKo*/, PERIOD_MICROSECS_CONTROL /*PeriodMicroSecs*/, MySimpleControlTask );
    if ( err_motor!=0 || err_control!=0 )
    {
        printf( "Init motor task error (%d)!\n",err_motor );
        printf( "Init control task error (%d)!\n",err_control );
    }
    else
    {
        printf( "Wait 2 seconds...\n" );
        sleep( 2 );
        ThreadRunning = 0; // Stop the threads
        //printf( "Increment value during 10 secs = %d (should be %d)\n", IncValue, ((10-START_DELAY_SECS)*1000*1000)/PERIOD_MICROSECS );
        printf( "Wait 3 seconds to end...\n" );
        sleep( 10 );
    }
    return 0;
}
