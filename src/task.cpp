#include "orchestra.h"

int fd;
BasicThread thread;

// our really simple thread just reading a variable !
void * func( void * dummy )
{
    printf( "Task Control\n" );

    void *rptr;
    rptr = mmap(NULL, sizeof(struct shrm),
           PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rptr == MAP_FAILED)
    {
        printf( "Control task, Mapping failed!" );
        return 0;
    }

    /* Now we can refer to mapped region using fields of rptr;
       for example, rptr->len */

    while( thread.running_ )
    {

        int ret = sem_wait(&static_cast<shrm*>(rptr)->sync_sem);
        if (ret != 0)
            printf("Sync Semaphore on Task, something went wrong...\n");
        static_cast<shrm*>(rptr)->cnt_task++;
        thread.wait_periodic_thread();
    }

    printf( "Task read value: %d\n" , static_cast<shrm*>(rptr)->cnt_task);

    return 0;
}

int main( int argc, char * argv[] )
{
    if (argc < 2)
    {
        printf("Invalid number of arguments\n");
        return 0;
    }

    char * process_name = argv[0];
    char * shrm_name = argv[1];

    fd = create_shared_memory(shrm_name);

    int err;
    printf("Simple periodic thread using Posix.\n");
    //TODO rt_task_shadow()
    mlockall(MCL_CURRENT | MCL_FUTURE);
    err = thread.create_periodic_thread(process_name, 1/*Priority*/, 4/*StackSizeInKo*/, PERIOD_MICROSECS_TASK/*PeriodMicroSecs*/, func );

    if ( err!=0 )
    {
        printf( "Init %s task error (%d)!\n",process_name ,err );
    }
    else
    {
        printf( "Wait 3 seconds...\n" );
        sleep( 2 );
        thread.running_ = 0; // Stop the threads
        //printf( "Increment value during 10 secs = %d (should be %d)\n", IncValue, ((10-START_DELAY_SECS)*1000*1000)/PERIOD_MICROSECS );
        printf( "Wait 5 seconds to end...\n" );
        sleep( 5 );
    }
    return 0;
}
