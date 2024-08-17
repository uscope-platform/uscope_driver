set(TEST_SOURCES
        testing_main.cpp
        deployer.cpp
        )




add_executable(test_target ${TEST_SOURCES})
target_link_libraries(test_target PUBLIC ${GTEST_LIBRARIES}
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        )


include(GoogleTest)
gtest_discover_tests(test_target)