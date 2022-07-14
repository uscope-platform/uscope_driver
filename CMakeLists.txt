cmake_minimum_required(VERSION 2.8.9)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set (CMAKE_CXX_STANDARD 17)


option(VERBOSE_LOGGING "Configure how verbose is the command line logging (1 for standard 2 for verbose)" OFF)

if(${VERBOSE_LOGGING})
    add_compile_definitions(VERBOSE_LOGGING)
endif(${VERBOSE_LOGGING})


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

add_executable(uscope_driver src/uscope_driver.cpp src/server_connector.cpp src/fpga_bridge.cpp  src/command_processor.cpp src/scope_thread.cpp includes/response.hpp includes/command.hpp)
target_link_libraries(uscope_driver pthread)
target_link_libraries(uscope_driver rt)
