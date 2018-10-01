// ROS include
#include <ros/ros.h>
#include <realtime_tools/realtime_clock.h>

#include <alchemy/timer.h>
#include <alchemy/task.h>
#include <alchemy/mutex.h>
#include <alchemy/heap.h>
#include <alchemy/sem.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/*#include "SL.h"
#include "SL_common.h"
#include "SL_rostask_servo.h"
#include "SL_collect_data.h"
#include "SL_shared_memory.h"
#include "SL_unix_common.h"
#include "SL_xeno_common.h"
#include "SL_man.h"
#include "SL_dynamics.h"*/

#define TIME_OUT_NS  1000000

// Global variables
int delay_ns = FALSE;

extern "C" {

// Local variables
static RT_TASK servo_ptr;
static int use_spawn = TRUE;
static int servo_priority = 90;
static int servo_stack_size = 20000000;
static int cpuID = 0;

// local functions
static void task_servo(void *dummy);

}


int main(int argc, char**argv)
{
	// Miscelaneos variables
	int rc;
	char name[100];

	ros::init(argc, argv, "rostask_servo");

	// Parsibng command line options
	parseOptions(argc, argv);

	// Adjusting settings if SL runs for a real robot
	setRealRobotOptions();

	// Signal handlers
	installSignalHandlers();

	// Initializing xenomai specific variables and real-time environment
	initXeno("task");

	// Initializing the servo
	init_rostask_servo();
	read_whichDOFs(config_files[WHICHDOFS], "task_servo");

	// get the servo parameters
	sprintf(name,"%s_servo",servo_name);
	read_servoParameters(config_files[SERVOPARAMETERS], name, &servo_priority,
	                       &servo_stack_size, &cpuID,&delay_ns);

	// Starting asynchronous ROS spinner
	ros::AsyncSpinner spinner(1);
	spinner.start();

	// Reseting task_servo variables
	servo_enabled = 1;
	rostask_servo_calls = 0;
	rostask_servo_time = 0;
	last_rostask_servo_time = 0;
	rostask_servo_errors = 0;
	rostask_servo_rate = servo_base_rate / (double) task_servo_ratio;

	changeCollectFreq(rostask_servo_rate);

	// spawn command line interface thread
	//spawnCommandLineThread(NULL);

	// signal that this process is initialized
	semGive(sm_init_process_ready_sem);

	// Running the servo loop
	if (use_spawn) {
		sprintf(name,"%s_servo_%d",servo_name, parent_process_id);
          #ifndef __COBALT__
          if ((rc=rt_task_spawn(&servo_ptr,name,servo_stack_size,servo_priority,
                           T_FPU | T_CPU(cpuID) | T_JOINABLE, task_servo,NULL)))
          {
              printf("rt_task_spawn returned %d\n",rc);
          #else

               cpu_set_t cpu_set;
               CPU_ZERO(&cpu_set);
               CPU_SET(cpuID,&cpu_set);

               if ((rc=rt_task_create(&servo_ptr,name,servo_stack_size,servo_priority,T_JOINABLE)))
                   printf("rt_task_create returned %d\n",rc);
               if ((rc=rt_task_set_affinity(&servo_ptr,&cpu_set)))
                   printf("rt_task_set_affinity returned %d\n",rc);
               if ((rc=rt_task_start(&servo_ptr,task_servo,NULL)))
                   printf("rt_task_start returned %d\n",rc);

          if(rc)
          {
          #endif
	    printf("rt_task_spawn returned %d\n",rc);
	  }

		// spawn command line interface thread
		//spawnCommandLineThread(NULL);//(initial_user_command);

		// signal that this process is initialized
		semGive(sm_init_process_ready_sem);

		// wait for the task to finish
		rt_task_join(&servo_ptr);
	} else {
		// spawn command line interface thread
		//spawnCommandLineThread(NULL);//(initial_user_command);

		// signal that this process is initialized
		semGive(sm_init_process_ready_sem);

		// run this servo
		task_servo(NULL);
	}

	printf("RosTask Servo Error Count = %d\n", rostask_servo_errors);
	spinner.stop();

	return TRUE;
}


static void task_servo(void *dummy)
{
	int rc;

	//forces the mode switch
	rt_printf("entering task servo\n");

	// warn upon mode switch
	if ((rc = rt_task_set_mode(0,T_WARNSW,NULL)))
		printf("rt_task_set_mode returned %d\n",rc);

	// ROS realtime-safe clock
	realtime_tools::RealtimeClock ros_rtclock;

	// run the servo loop
	while (servo_enabled) {
		// force delay ticks if user wishes
		if (delay_ns > 0)
			taskDelay(ns2ticks(delay_ns));

    // wait to take semaphore
		if (semTake(sm_task_servo_sem,WAIT_FOREVER) == ERROR) {
			char msg[] = "semTake Time Out -- Servo Terminated";
			stop(msg);
		}

    // lock out the keyboard interaction
		sl_rt_mutex_lock(&mutex1);

    // run the task servo routines
		if (!run_rostask_servo(ros_rtclock))
			break;

    // continue keyboard interaction
		sl_rt_mutex_unlock(&mutex1);
	}  /* end servo while loop */
}
