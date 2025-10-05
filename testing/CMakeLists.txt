set(TEST_SOURCES
        testing_main.cpp
        deployment/hil_deployer_v2.cpp
        deployment/custom_deployer_test.cpp
        deployment/emulation.cpp
        deployment/fpga_bridge.cpp
        endpoints/cores_endpoints.cpp
        endpoints/control_endpoints.cpp
        )

set(DRIVER_SOURCES "${DRIVER_SOURCES}" PARENT_SCOPE)

list(TRANSFORM DRIVER_SOURCES PREPEND "../")
add_executable(test_target ${TEST_SOURCES} ${DRIVER_SOURCES})
target_link_libraries(test_target PUBLIC ${GTEST_LIBRARIES}
        spdlog::spdlog
        ValiJSON::valijson
        fcore_cc_lib
        fcore_emu_lib
        fcore_emu_lib_gp
        cppcodec::cppcodec
        nlohmann_json::nlohmann_json
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        CLI11::CLI11
        )

target_include_directories(test_target PRIVATE ${fCore_toolchain_SOURCE_DIR}/includes)

include(GoogleTest)
gtest_discover_tests(test_target)