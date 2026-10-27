cmake_minimum_required(VERSION 3.12)
set (CMAKE_CXX_STANDARD 23)
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

find_package(asio REQUIRED)
find_package(spdlog REQUIRED)
find_package(valijson REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CLI11 REQUIRED)
find_package(GTest REQUIRED)
find_package(cppcodec REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# compiles code files from ./src folder
file(GLOB INC_SOURCES "includes/*.h*")



set(DRIVER_SOURCES /
        src/server_frontend/infrastructure/server_connector.cpp
        src/server_frontend/infrastructure/command_processor.cpp
        src/server_frontend/infrastructure/schema_validator.cpp
        src/server_frontend/endpoints/cores_endpoints.cpp
        src/server_frontend/endpoints/control_endpoints.cpp
        src/server_frontend/endpoints/platform_endpoints.cpp
        src/hw_interface/toolchain_manager.cpp
        src/hw_interface/fpga_bridge.cpp
        src/hw_interface/channel_metadata.cpp
        src/hw_interface/timing_manager.cpp
        src/emulated_data_generator.cpp
        src/deployment/hil_deployer.cpp
        src/deployment/hil_emulator.cpp
        src/deployment/hil_bus_map.cpp
        src/deployment/deployer_base.cpp
        src/deployment/custom_deployer.cpp
        src/hw_interface/bus/bus_accessor.cpp
        includes/hw_interface/bus/bus_accessor.hpp
        src/hw_interface/bus/scope_accessor.cpp
        src/server_frontend/data_plane/scope_server.cpp
        src/server_frontend/data_plane/scope_controller.cpp
        src/server_frontend/data_plane/scope_reader.cpp
        src/server_frontend/data_plane/scope_session.cpp
        src/emulation/emulation_context.cpp
)

set(DRIVER_VERSION_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/generated/driver_version.cpp)

# Generate a single translation unit holding the build version strings. The
# script only rewrites the file when its content changes (preserving the mtime),
# so rebuilding after a commit recompiles just this one TU instead of every TU
# that includes the stable driver_version.h header. Runs for all build types so
# the Debug build works out of the box too.
add_custom_target(driver_version_generation ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/update_version.py ${DRIVER_VERSION_SOURCE} ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    BYPRODUCTS ${DRIVER_VERSION_SOURCE}
    COMMENT "Generating driver version source"
    VERBATIM
)

set_source_files_properties(${DRIVER_VERSION_SOURCE} PROPERTIES GENERATED TRUE)



add_executable(uscope_driver src/uscope_driver.cpp ${DRIVER_SOURCES})
target_link_libraries(uscope_driver PRIVATE
        rt
        ValiJSON::valijson
        nlohmann_json::nlohmann_json
        CLI11::CLI11
        cppcodec::cppcodec
        fcore_cc_lib
        fcore_emu_lib_gp
        fcore_emu_lib
        asio::asio
        Boost::headers
)

target_include_directories(uscope_driver PRIVATE ${fCore_toolchain_SOURCE_DIR}/includes)

target_sources(uscope_driver PRIVATE ${DRIVER_VERSION_SOURCE})
add_dependencies(uscope_driver driver_version_generation)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
else()
    enable_testing()
    add_subdirectory(testing)
endif(CMAKE_BUILD_TYPE STREQUAL "Release")

if(NOT ${EXCLUDE_KERNEL_EMU})

add_executable(kernel_emulator testing/kernel_emulator_standalone.cpp)
target_link_libraries(kernel_emulator PRIVATE rt)

endif(NOT ${EXCLUDE_KERNEL_EMU})

install(TARGETS uscope_driver DESTINATION ${CMAKE_INSTALL_BINDIR})
add_dependencies(uscope_driver schema_assembler)

add_executable(test_hil_deployer
        src/hil_depoyer_standalone.cpp
        src/server_frontend/infrastructure/schema_validator.cpp
        src/deployment/hil_deployer.cpp
        src/deployment/deployer_base.cpp
        src/deployment/custom_deployer.cpp
        src/deployment/hil_bus_map.cpp
        src/hw_interface/fpga_bridge.cpp
        src/hw_interface/bus/bus_accessor.cpp
)

target_link_libraries(test_hil_deployer PRIVATE
        nlohmann_json::nlohmann_json
        ValiJSON::valijson
        fcore_cc_lib
        cppcodec::cppcodec
        fcore_emu_lib_gp
        fcore_emu_lib
        CLI11::CLI11
)

target_include_directories(test_hil_deployer PRIVATE ${fCore_toolchain_SOURCE_DIR}/includes)
execute_process(COMMAND uname -r
        OUTPUT_VARIABLE KERNEL_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)

set(KERNEL_DIR /usr/src/linux-headers-${KERNEL_VERSION})
set(MODULE_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/module_build)

file(MAKE_DIRECTORY ${MODULE_BUILD_DIR})

# Copy mock_module.c with dependency tracking
add_custom_command(
        OUTPUT ${MODULE_BUILD_DIR}/mock_module.c
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mock_module/mock_module.c ${MODULE_BUILD_DIR}/mock_module.c
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mock_module/mock_module.c
        COMMENT "Copying mock_module.c to build folder"
)

# Copy Makefile with dependency tracking (if needed)
add_custom_command(
        OUTPUT ${MODULE_BUILD_DIR}/Makefile
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mock_module/Makefile ${MODULE_BUILD_DIR}/Makefile
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mock_module/Makefile
        COMMENT "Copying Makefile to build folder"
)

# Dummy target so CLion sees the sources
add_custom_target(mock_dev_sources ALL
        DEPENDS ${MODULE_BUILD_DIR}/mock_module.c ${MODULE_BUILD_DIR}/Makefile
        COMMENT "Adding mock_module sources to a target for CLion"
)

# Build the kernel module
add_custom_target(mock_dev_ko ALL
        COMMAND make -C ${KERNEL_DIR} M=${MODULE_BUILD_DIR} modules
        WORKING_DIRECTORY ${MODULE_BUILD_DIR}
        COMMENT "Building mock_module.ko"
)

add_dependencies(mock_dev_ko mock_dev_sources)

# Module signing, required to load the module when Secure Boot is enabled.
# BOOT_KEY (env) is the path of the enrolled MOK key pair without extension
# (<BOOT_KEY>.priv and <BOOT_KEY>.der).
execute_process(COMMAND mokutil --sb-state
        OUTPUT_VARIABLE SECURE_BOOT_STATE
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)

if (SECURE_BOOT_STATE MATCHES "SecureBoot enabled")
    set(MODULE_SIGN_FILE ${KERNEL_DIR}/scripts/sign-file)
    set(MODULE_SIGN_KEY "$ENV{BOOT_KEY}.priv")
    set(MODULE_SIGN_CERT "$ENV{BOOT_KEY}.der")

    if (NOT DEFINED ENV{BOOT_KEY} OR NOT EXISTS ${MODULE_SIGN_KEY} OR NOT EXISTS ${MODULE_SIGN_CERT})
        message(FATAL_ERROR "Secure Boot is enabled: set BOOT_KEY to the MOK key path "
                "without extension (expects BOOT_KEY.priv and BOOT_KEY.der) to sign mock_module.ko")
    endif ()

    add_custom_command(TARGET mock_dev_ko POST_BUILD
            COMMAND ${MODULE_SIGN_FILE} sha256 ${MODULE_SIGN_KEY} ${MODULE_SIGN_CERT} ${MODULE_BUILD_DIR}/mock_module.ko
            COMMENT "Signing mock_module.ko"
    )
endif ()
