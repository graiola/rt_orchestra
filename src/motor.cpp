#include "orchestra.h"

int fd;
BasicThread thread;

// our really simple thread just incrementing a variable in loop !
void * motor_func( void * dummy )
{

    printf( "Task Motor\n" );

    void *rptr;
    rptr = mmap(NULL, sizeof(struct shrm),
           PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rptr == MAP_FAILED)
    {
        printf( "Motor task, Mapping failed!" );
        return 0;
    }

    /* Now we can refer to mapped region using fields of rptr;
       for example, rptr->len */

    while( thread.running_ )
    {
        thread.wait_periodic_thread();
        static_cast<shrm*>(rptr)->cnt++;
    }

    printf( "Motor read value: %d\n" , static_cast<shrm*>(rptr)->cnt);

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
    err = thread.create_periodic_thread(process_name, 1/*Priority*/, 4/*StackSizeInKo*/, PERIOD_MICROSECS_MOTOR/*PeriodMicroSecs*/, motor_func );

    if ( err!=0 )
    {
        printf( "Init %s task error (%d)!\n",process_name ,err );
    }
    else
    {
        printf( "Wait 2 seconds...\n" );
        sleep( 2 );
        thread.running_ = 0; // Stop the threads
        //printf( "Increment value during 10 secs = %d (should be %d)\n", IncValue, ((10-START_DELAY_SECS)*1000*1000)/PERIOD_MICROSECS );
        printf( "Wait 3 seconds to end...\n" );
        sleep( 3 );
    }
    return 0;
}
