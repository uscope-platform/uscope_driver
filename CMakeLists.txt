cmake_minimum_required(VERSION 3.12)
set (CMAKE_CXX_STANDARD 20)
set(JSON_BuildTests OFF CACHE INTERNAL "")

project (uscope_driver)

option(VERBOSE_LOGGING "Configure how verbose is the command line logging (1 for standard 2 for verbose)" OFF)
option(INCLUDE_KERNEL_EMU "Build the kernel emulator (require libfuse)" OFF)

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


find_package(nlohmann_json_schema_validator REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CLI11 REQUIRED)

set(DRIVER_SOURCES /
        src/uscope_driver.cpp
        src/server_frontend/server_connector.cpp
        src/server_frontend/command_processor.cpp
        src/hw_interface/fpga_bridge.cpp
        src/hw_interface/scope_thread.cpp
        src/emulated_data_generator.cpp
        src/hw_emulation/kernel_emulator.cpp
)

add_executable(uscope_driver ${DRIVER_SOURCES})
target_link_libraries(uscope_driver PRIVATE pthread rt
        nlohmann_json::nlohmann_json
        nlohmann_json_schema_validator
        CLI11::CLI11
        libfuse::libfuse
)

add_custom_target(update_version
    COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "Version Header Update"
)

add_custom_command(
        TARGET update_version
        PRE_BUILD
        COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/update_version.py  ${CMAKE_SOURCE_DIR}/includes/version.h
        VERBATIM
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_dependencies(uscope_driver update_version)
endif(CMAKE_BUILD_TYPE STREQUAL "Release")

if(${INCLUDE_KERNEL_EMU})

    find_package(libfuse REQUIRED)
    add_executable(kernel_emulator testing/kernel_emulator_standalone.cpp src/hw_emulation/kernel_emulator.cpp)
    target_link_libraries(kernel_emulator PRIVATE pthread rt
            libfuse::libfuse
    )
endif(${INCLUDE_KERNEL_EMU})


