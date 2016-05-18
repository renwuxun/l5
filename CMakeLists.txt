cmake_minimum_required(VERSION 2.8)
project(l5worker)


set(CMAKE_C_FLAGS_DEBUG, "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUG, "${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -Wall")


add_subdirectory(wxworker)


add_executable(l5worker l5worker.c conn.c tool.c l5.c)

target_link_libraries(l5worker wxworker)
#target_link_libraries(l5worker wxworker profiler)