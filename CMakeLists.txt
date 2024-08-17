cmake_minimum_required(VERSION 3.12)
set (CMAKE_CXX_STANDARD 20)
set(JSON_BuildTests OFF CACHE INTERNAL "")

project (uscope_driver)

option(VERBOSE_LOGGING "Configure how verbose is the command line logging (1 for standard 2 for verbose)" OFF)
option(EXCLUDE_KERNEL_EMU "Exclude the the kernel emulator from the build" OFF)

if(${VERBOSE_LOGGING})
    add_compile_definitions(VERBOSE_LOGGING)
endif(${VERBOSE_LOGGING})

include(FetchContent)

FetchContent_Declare(
        fCore_toolchain
        GIT_REPOSITORY https://github.com/uscope-platform/fCore_toolchain.git
        GIT_TAG        main
)

FetchContent_MakeAvailable(fCore_toolchain)
#
# Common settings
#
#set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

# adds ./includes to compiler includes directories
include_directories(/usr/local/include/pthread)
include_directories(/usr/local/include/event2)
include_directories(${CMAKE_SOURCE_DIR}/includes)


find_package(valijson REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CLI11 REQUIRED)
find_package(cppzmq REQUIRED)
find_package(GTest REQUIRED)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(schema_folder "${fcore_toolchain_SOURCE_DIR}/schemas")
    enable_testing()
    add_subdirectory(testing)
else()
    set(schema_folder "/usr/local/etc/fcore_toolchain")
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

# compiles code files from ./src folder
file(GLOB INC_SOURCES "includes/*.h*")



set(DRIVER_SOURCES /
        src/uscope_driver.cpp
        src/server_frontend/infrastructure/server_connector.cpp
        src/server_frontend/infrastructure/command_processor.cpp
        src/server_frontend/infrastructure/schema_validator.cpp
        src/server_frontend/endpoints/cores_endpoints.cpp
        src/server_frontend/endpoints/control_endpoints.cpp
        src/server_frontend/endpoints/scope_endpoints.cpp
        src/server_frontend/endpoints/platform_endpoints.cpp
        src/hw_interface/toolchain_manager.cpp
        src/hw_interface/fpga_bridge.cpp
        src/hw_interface/scope_manager.cpp
        src/hw_interface/channel_metadata.cpp
        src/hw_interface/timing_manager.cpp
        src/emulated_data_generator.cpp
        src/deployment/hil_deployer.cpp
        src/deployment/hil_emulator.cpp
        src/deployment/hil_bus_map.cpp
        src/deployment/deployer_base.cpp
        src/deployment/custom_deployer.cpp
)

add_executable(uscope_driver ${DRIVER_SOURCES})
target_link_libraries(uscope_driver PRIVATE
        rt
        ValiJSON::valijson
        nlohmann_json::nlohmann_json
        CLI11::CLI11
        fcore_cc_lib
        fcore_emu_lib
        cppzmq
)

target_include_directories(uscope_driver PRIVATE ${fCore_toolchain_SOURCE_DIR}/includes)
target_compile_definitions(uscope_driver PRIVATE SCHEMAS_FOLDER="${schema_folder}")


add_custom_target(update_version
    COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "Version Header Update"
)

add_custom_command(
        TARGET update_version
        PRE_BUILD
        COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/update_version.py  ${CMAKE_SOURCE_DIR}/includes/driver_version.h
        VERBATIM
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_dependencies(uscope_driver update_version)
endif(CMAKE_BUILD_TYPE STREQUAL "Release")

if(NOT ${EXCLUDE_KERNEL_EMU})

    find_package(libfuse REQUIRED)
    add_executable(kernel_emulator testing/kernel_emulator_standalone.cpp src/hw_emulation/kernel_emulator.cpp)
    target_link_libraries(kernel_emulator PRIVATE pthread rt
            libfuse::libfuse
    )
endif(NOT ${EXCLUDE_KERNEL_EMU})

install(TARGETS uscope_driver DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(test_hil_deployer
        src/hil_depoyer_standalone.cpp
        src/server_frontend/infrastructure/schema_validator.cpp
        src/deployment/hil_deployer.cpp
        src/deployment/deployer_base.cpp
        src/deployment/custom_deployer.cpp
        src/deployment/hil_bus_map.cpp
        src/hw_interface/fpga_bridge.cpp
)

target_link_libraries(test_hil_deployer PRIVATE
        nlohmann_json::nlohmann_json
        ValiJSON::valijson
        fcore_cc_lib
        fcore_emu_lib
        CLI11::CLI11
)

target_include_directories(test_hil_deployer PRIVATE ${fCore_toolchain_SOURCE_DIR}/includes)
target_compile_definitions(test_hil_deployer PRIVATE SCHEMAS_FOLDER="${schema_folder}")

