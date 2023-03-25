cmake_minimum_required(VERSION 3.12)
set (CMAKE_CXX_STANDARD 17)
set(JSON_BuildTests OFF CACHE INTERNAL "")

include(FetchContent)

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json
        GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
        json_schema
        GIT_REPOSITORY https://github.com/pboettch/json-schema-validator
        GIT_TAG       2.1.0
)
FetchContent_MakeAvailable(json_schema)

FetchContent_Declare(
        cli11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11
        GIT_TAG        v2.3.0
)
FetchContent_MakeAvailable(cli11)


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

set(DRIVER_SOURCES /
        src/uscope_driver.cpp
        src/server_connector.cpp
        src/fpga_bridge.cpp
        src/command_processor.cpp
        src/scope_thread.cpp
        src/emulated_data_generator.cpp
        )

add_executable(uscope_driver ${DRIVER_SOURCES})
target_link_libraries(uscope_driver PRIVATE pthread rt  nlohmann_json::nlohmann_json nlohmann_json_schema_validator CLI11::CLI11)
