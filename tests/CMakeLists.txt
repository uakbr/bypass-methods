cmake_minimum_required(VERSION 3.12)
project(UndownUnlockTests LANGUAGES CXX)

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
    test_memory_tracker.cpp
    test_com_tracker.cpp
)

# Create test executable
add_executable(undownunlock_tests ${TEST_SOURCES})

# Link against Google Test and our library
target_link_libraries(undownunlock_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    UndownUnlockDXHook
    dbghelp
)

# Register tests with CTest
add_test(NAME undownunlock_tests COMMAND undownunlock_tests)

# Set working directory for tests
if(TARGET undownunlock_tests)
    set_tests_properties(undownunlock_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )
endif()

# Windows Integration Enhancement Tests (standalone)
add_executable(windows_integration_test test_windows_integration.cpp)
target_link_libraries(windows_integration_test PRIVATE
    UndownUnlockDXHook
    dbghelp
)

# Add additional system libraries needed for Windows Integration
if(WIN32)
    target_link_libraries(windows_integration_test PRIVATE
        wbemuuid
        oleacc
        Shlwapi
        Shell32
        UIAutomationCore
    )
endif()

# Register Windows Integration test with CTest
add_test(NAME windows_integration_test COMMAND windows_integration_test) 