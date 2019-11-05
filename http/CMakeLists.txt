cmake_minimum_required(VERSION 2.8)
project(http C)

find_package(Threads)


set(CMAKE_C_STANDARD 99)

add_compile_options(-pthread)

aux_source_directory(. SRC_LIST)
#aux_source_directory(./src SRC_LIST)

INCLUDE_DIRECTORIES(.)

add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

#add_executable(ctest2 main.c)
