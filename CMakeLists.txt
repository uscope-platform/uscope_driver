cmake_minimum_required(VERSION 2.8.9)
set(CMAKE_BUILD_TYPE Debug)
set (CMAKE_CXX_STANDARD 17)

#
# Common settings
#
#set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

# adds ./includes to compiler includes directories
include_directories(/usr/local/include/pthread)
include_directories(/usr/local/include/event2)
include_directories(${CMAKE_SOURCE_DIR}/includes)


# compiles code files from ./src folder
file(GLOB INC_SOURCES "includes/*.h*")

#
# first project (static lib)
#
project (uscope_driver)

add_executable(uscope_driver src/uscope_driver.cpp src/server_connector.cpp src/fpga_bridge.cpp src/data_linked_list.c src/command_processor.cpp src/scope_thread.cpp)
target_link_libraries(uscope_driver event)
target_link_libraries(uscope_driver pthread)
target_link_libraries(uscope_driver rt)
