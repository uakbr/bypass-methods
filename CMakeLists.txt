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

# Compression libraries options
option(USE_LZ4 "Use LZ4 compression" ON)
option(USE_ZSTD "Use Zstandard compression" ON)

# Find compression libraries
if(USE_LZ4)
    find_package(lz4 CONFIG QUIET)
    if(lz4_FOUND)
        message(STATUS "LZ4 found via find_package")
        add_compile_definitions(USE_LZ4)
    else()
        message(STATUS "LZ4 not found via find_package, checking for system library")
        find_library(LZ4_LIBRARY NAMES lz4 liblz4)
        find_path(LZ4_INCLUDE_DIR NAMES lz4.h)
        if(LZ4_LIBRARY AND LZ4_INCLUDE_DIR)
            message(STATUS "Found system LZ4: ${LZ4_LIBRARY}")
            add_compile_definitions(USE_LZ4)
            include_directories(${LZ4_INCLUDE_DIR})
            set(COMPRESSION_LIBS ${COMPRESSION_LIBS} ${LZ4_LIBRARY})
        else()
            message(WARNING "LZ4 not found, disabling LZ4 compression support")
            set(USE_LZ4 OFF)
        endif()
    endif()
endif()

if(USE_ZSTD)
    find_package(zstd CONFIG QUIET)
    if(zstd_FOUND)
        message(STATUS "Zstandard found via find_package")
        add_compile_definitions(USE_ZSTD)
    else()
        message(STATUS "Zstandard not found via find_package, checking for system library")
        find_library(ZSTD_LIBRARY NAMES zstd libzstd)
        find_path(ZSTD_INCLUDE_DIR NAMES zstd.h)
        if(ZSTD_LIBRARY AND ZSTD_INCLUDE_DIR)
            message(STATUS "Found system Zstandard: ${ZSTD_LIBRARY}")
            add_compile_definitions(USE_ZSTD)
            include_directories(${ZSTD_INCLUDE_DIR})
            set(COMPRESSION_LIBS ${COMPRESSION_LIBS} ${ZSTD_LIBRARY})
        else()
            message(WARNING "Zstandard not found, disabling ZSTD compression support")
            set(USE_ZSTD OFF)
        endif()
    endif()
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

# Define source files for the virtual camera library
set(VIRTUAL_CAMERA_SOURCES
    src/virtual_camera/kernel_driver.cpp
    src/virtual_camera/virtual_camera.cpp
    src/virtual_camera/directshow_filter.cpp
    src/virtual_camera/facial_tracking.cpp
)

# Define source files for the virtual camera test
set(VCAM_TEST_SOURCES
    src/tests/test_virtual_camera.cpp
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

# Add the virtual camera library
add_library(UndownUnlockVirtualCamera STATIC ${VIRTUAL_CAMERA_SOURCES})

# Add the virtual camera test application
add_executable(UndownUnlockVCamTest ${VCAM_TEST_SOURCES})

# Link the test client with necessary libraries
target_link_libraries(UndownUnlockTestClient PRIVATE
    d3d11
    dxgi
    windowscodecs
    ${COMPRESSION_LIBS}
)

# Link the DLL hooks with necessary libraries
target_link_libraries(UndownUnlockDXHook PRIVATE
    d3d11
    dxgi
    windowscodecs
    dbghelp
    ${COMPRESSION_LIBS}
)

# Link the virtual camera library with necessary libraries
target_link_libraries(UndownUnlockVirtualCamera PRIVATE
    setupapi
    strmiids
    mfplat
    mf
    mfreadwrite
    mfuuid
    shlwapi
    advapi32
)

# Link the virtual camera test application
target_link_libraries(UndownUnlockVCamTest PRIVATE
    UndownUnlockVirtualCamera
)

# Conditionally link LZ4 and ZSTD using find_package results
if(USE_LZ4 AND lz4_FOUND)
    target_link_libraries(UndownUnlockDXHook PRIVATE lz4::lz4)
    target_link_libraries(UndownUnlockTestClient PRIVATE lz4::lz4)
endif()

if(USE_ZSTD AND zstd_FOUND)
    target_link_libraries(UndownUnlockDXHook PRIVATE zstd::libzstd_shared)
    target_link_libraries(UndownUnlockTestClient PRIVATE zstd::libzstd_shared)
endif()

# Embed the kernel driver binary as a resource
# This would normally include the real driver binary in the resources
if(WIN32)
    # Create a resource file
    file(WRITE ${CMAKE_BINARY_DIR}/vcam_driver.rc "UNDOWNUNLOCK_VCAM_DRIVER RCDATA \"${CMAKE_SOURCE_DIR}/src/virtual_camera/kernel_driver_source.c\"")
    
    # Add the resource file to the virtual camera library
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/vcam_driver.res
        COMMAND rc.exe /fo ${CMAKE_BINARY_DIR}/vcam_driver.res ${CMAKE_BINARY_DIR}/vcam_driver.rc
        DEPENDS ${CMAKE_SOURCE_DIR}/src/virtual_camera/kernel_driver_source.c
        COMMENT "Compiling virtual camera driver resource"
    )
    
    # Add the resource to the virtual camera library
    target_sources(UndownUnlockVirtualCamera PRIVATE ${CMAKE_BINARY_DIR}/vcam_driver.res)
endif()

# Installation rules
install(TARGETS UndownUnlockDXHook UndownUnlockTestClient UndownUnlockVirtualCamera UndownUnlockVCamTest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Create a bin directory for drivers
install(DIRECTORY DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin)

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
    
    # Add the virtual camera test to the tests
    add_test(
        NAME VirtualCameraTest
        COMMAND UndownUnlockVCamTest
    )
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
message(STATUS "Compression: LZ4=${USE_LZ4}, ZSTD=${USE_ZSTD}") 