#include "orchestra.h"

extern char **environ;

int main(int argc, char* argv[])
{
    char* shrm_name = "motor_task_shrm";
    create_shared_memory(shrm_name); 

    int pid_motor = spawn_process("motor",shrm_name);
    int pid_task = spawn_process("task",shrm_name);

    int status;
    if (waitpid(pid_motor, &status, 0) != -1)
        printf("Child motor with status %i\n", status);
    else
        perror("waitpid motor");
    //if (waitpid(pid_task, &status, 0) != -1)
    //    printf("Child task with status %i\n", status);
    //else
    //    perror("waitpid task");


    close_shared_memory(shrm_name);

    return 0;
}
