cmake_minimum_required(VERSION 2.8.9)
set(CMAKE_BUILD_TYPE Debug)
#
# Common settings
#
#set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

# adds ./includes to compiler includes directories
include_directories(/usr/local/include/hiredis)
include_directories(/usr/local/include/pthread)
include_directories(/usr/local/include/event2)
include_directories(${CMAKE_SOURCE_DIR}/includes)

# compiles code files from ./src folder
file(GLOB LIB_SOURCES "src/*.c")

#
# first project (static lib)
#
project (uscope_driver)

add_executable(${CMAKE_PROJECT_NAME} ${LIB_SOURCES} src/scope_handler.c includes/scope_handler.h src/data_linked_list.c includes/data_linked_list.h)
target_link_libraries(${CMAKE_PROJECT_NAME} hiredis)
target_link_libraries(${CMAKE_PROJECT_NAME} event)
target_link_libraries(${CMAKE_PROJECT_NAME} pthread)
target_link_libraries(${CMAKE_PROJECT_NAME} rt)
