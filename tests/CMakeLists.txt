cmake_minimum_required(VERSION 3.12)
project(UndownUnlockTests NONE) # Using NONE as it's not a standalone library/app build here

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Use Google Test framework
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "Google Test not found, will download and build it")
    # Download Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.11.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Define test source files
set(TEST_SOURCES
    test_pattern_scanner.cpp
    test_hooked_functions.cpp
)

# Create test executable
add_executable(undownunlock_tests ${TEST_SOURCES})

# Link against Google Test and our library
target_link_libraries(undownunlock_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    UndownUnlockDXHook
)

# Register tests with CTest
add_test(NAME undownunlock_tests COMMAND undownunlock_tests)

# Set working directory for tests
set_tests_properties(undownunlock_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
) 