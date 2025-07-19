cmake_minimum_required(VERSION 3.12)
project(UndownUnlockTests VERSION 1.0.0 LANGUAGES CXX)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Use Google Test framework (managed by parent CMakeLists.txt)
if(NOT TARGET gtest)
    message(FATAL_ERROR "Google Test not available. Please ensure BUILD_TESTING is enabled.")
endif()

# Define test source files
set(TEST_SOURCES
    test_pattern_scanner.cpp
    test_hooked_functions.cpp
    test_com_interface_wrapper.cpp
)

# Create test executable
add_executable(undownunlock_tests ${TEST_SOURCES})

# Link against test dependencies and our library
target_link_libraries(undownunlock_tests PRIVATE
    UndownUnlockTestDependencies
    UndownUnlockDXHook
)

# Register tests with CTest
add_test(NAME undownunlock_tests COMMAND undownunlock_tests)

# Set working directory for tests
set_tests_properties(undownunlock_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
) 