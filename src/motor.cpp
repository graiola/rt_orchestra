#include "orchestra.h"

int fd;
static long long motor_calls = 0;
static int task_ratio = 4; //PERIOD_MICROSECS_TASK/PERIOD_MICROSECS_MOTOR;
BasicThread thread;

void trigger_synchronization(sem_t* sem)
{
  // note: synchronizing on the remainder=1 allows starting all servos
  // immediately in the first run of the motor servo
  int ret = 0;
  if (task_ratio > 1) {
    if (motor_calls%task_ratio==1)
        ret = sem_post(sem);
  } else
    ret = sem_post(sem);

  if (ret != 0)
      printf("Sync Semaphore on Motor, something went wrong...\n");
  if (errno == EINVAL)
      printf("EINVAL");
  if (errno == EOVERFLOW)
      printf("EOVERFLOW");

  motor_calls++;
}

// our really simple thread just incrementing a variable in loop !
void * func( void * dummy )
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

    int ret = sem_init(&static_cast<shrm*>(rptr)->sync_sem,1,0);
    if(ret != 0)
        printf("Init Semaphore on Motor, something went wrong...\n");
    if (errno == EBUSY)
        printf("EBUSY");
    if (errno == ENOSPC)
        printf("ENOSPC");
    if (errno == EINVAL)
        printf("EINVAL");

    //static_cast<shrm*>(rptr)->sync_sem = sem_open("sync_sem", O_CREAT | O_EXCL);

    //sem_unlink ("motor_sem");

    while( thread.running_ )
    {

        // Do some magic

        static_cast<shrm*>(rptr)->cnt_motor++;

        thread.wait_periodic_thread();

        trigger_synchronization(&static_cast<shrm*>(rptr)->sync_sem);

    }

    printf( "Motor read value: %d\n" , static_cast<shrm*>(rptr)->cnt_motor);

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
    err = thread.create_periodic_thread(process_name, 1/*Priority*/, 4/*StackSizeInKo*/, PERIOD_MICROSECS_MOTOR/*PeriodMicroSecs*/, func );

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
