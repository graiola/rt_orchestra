#include "orchestra.h"

extern char **environ;

int main(int argc, char* argv[])
{
    char* shrm_name = "motor_task_shrm";
    create_shared_memory(shrm_name);

    spawn_process("motor",shrm_name);
    //spawn_process("cat",shrm_name);

    //close_shared_memory(shrm_name);

    return 0;
}
