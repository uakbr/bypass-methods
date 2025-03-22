cmake_minimum_required(VERSION 3.12)
project(UndownUnlock VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
)

# Find required packages
find_package(directx-headers CONFIG QUIET)
if(NOT directx-headers_FOUND)
    message(STATUS "DirectX headers not found via find_package, using system headers")
endif()

# Define source files for the DLL hooks library
set(DLL_HOOKS_SOURCES
    src/dllmain_refactored.cpp
    src/hooks/windows_api/windows_api_hooks.cpp
    src/hooks/windows_api/hooked_functions.cpp
    src/hooks/windows_api/keyboard_hook.cpp
    src/hooks/windows_api/hook_utils.cpp
    src/hooks/dx_hook_core.cpp
    src/hooks/swap_chain_hook.cpp
    src/memory/memory_scanner.cpp
    src/memory/pattern_scanner.cpp
    src/memory/memory_tracker.cpp
    src/frame/frame_extractor.cpp
    src/shared/shared_memory_transport.cpp
    src/signatures/dx_signatures.cpp
    src/signatures/lockdown_signatures.cpp
    src/com_hooks/factory_hooks.cpp
    src/com_hooks/com_tracker.cpp
)

# Define source files for the test client
set(TEST_CLIENT_SOURCES
    src/test_client.cpp
)

# Add the DLL hooks library
add_library(UndownUnlockDXHook SHARED ${DLL_HOOKS_SOURCES})

# Set up the DLL properties
target_compile_definitions(UndownUnlockDXHook PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
)

# Add the test client executable
add_executable(UndownUnlockTestClient ${TEST_CLIENT_SOURCES})

# Link the test client with necessary libraries
target_link_libraries(UndownUnlockTestClient PRIVATE
    d3d11
    dxgi
    windowscodecs
)

# Link the DLL hooks with necessary libraries
target_link_libraries(UndownUnlockDXHook PRIVATE
    d3d11
    dxgi
    windowscodecs
    dbghelp
)

# Installation rules
install(TARGETS UndownUnlockDXHook UndownUnlockTestClient
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Copy the Python injector script to the output directory
configure_file(
    ${CMAKE_SOURCE_DIR}/src/test_injector.py
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_injector.py
    COPYONLY
)

# Add tests if testing is enabled
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}") 