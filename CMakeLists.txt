cmake_minimum_required(VERSION 3.5)

project(rt_orchestra)

find_package(xenomai)
find_package(catkin REQUIRED COMPONENTS roscpp urdf)
#dls_hardware_interface tf controller_manager realtime_tools pronto_wrapper)

include_directories(include ${catkin_INCLUDE_DIRS})

add_library(orchestra include/orchestra.h src/orchestra.cpp)
set_xeno_flags(orchestra)
target_link_libraries(orchestra ${catkin_LIBRARIES} rt)

add_executable(process_spawner src/process_spawner.cpp)
target_link_libraries(process_spawner ${catkin_LIBRARIES} rt orchestra)

# Create the tasks
add_executable(motor src/motor.cpp)
set_xeno_flags(motor)
target_link_libraries(motor ${catkin_LIBRARIES} rt orchestra)
add_executable(task src/task.cpp)
set_xeno_flags(task)
target_link_libraries(task ${catkin_LIBRARIES} rt orchestra)
