#include "orchestra.h"

int fd;

// our really simple thread just reading a variable !
void * task_func( void * dummy )
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

    while( ThreadRunning )
    {
        WaitPeriodicPosixTask( );
        IncValue++;
    }

    printf( "Control read value: %d\n" , static_cast<shrm*>(rptr)->cnt);

    return 0;
}

int main( int argc, char * argv[] )
{

    if (argc < 3)
    {
        printf("Invalid number of arguments\n");
        return 0;
    }

    char * process_name = argv[0];
    char * shrm_name = argv[1];

    fd = create_shared_memory(shrm_name);

    int err;
    printf("Simple periodic thread using Posix.\n");
#if defined( __XENO__ ) || defined( __COBALT__ )
    //TODO rt_task_shadow()
    printf("Version compiled for Xenomai 2 or 3.\n");
    mlockall(MCL_CURRENT | MCL_FUTURE);
#endif
    err = CreatePosixTask( process_name, 1/*Priority*/, 4/*StackSizeInKo*/, PERIOD_MICROSECS_CONTROL/*PeriodMicroSecs*/, task_func );

    if ( err!=0 )
    {
        printf( "Init %s task error (%d)!\n",process_name ,err );
    }
    else
    {
        printf( "Wait 2 seconds...\n" );
        sleep( 2 );
        ThreadRunning = 0; // Stop the threads
        //printf( "Increment value during 10 secs = %d (should be %d)\n", IncValue, ((10-START_DELAY_SECS)*1000*1000)/PERIOD_MICROSECS );
        printf( "Wait 3 seconds to end...\n" );
        sleep( 3 );
    }
    return 0;
}
