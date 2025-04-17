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
            include_directories(${LZ4_INCLUDE_DIR})\n            set(COMPRESSION_LIBS ${COMPRESSION_LIBS} ${LZ4_LIBRARY})\n        else()\n            message(WARNING "LZ4 not found, disabling LZ4 compression support")\n            set(USE_LZ4 OFF)\n        endif()\n    endif()\nendif()\n\nif(USE_ZSTD)\n    find_package(zstd CONFIG QUIET)\n    if(zstd_FOUND)\n        message(STATUS "Zstandard found via find_package")\n        add_compile_definitions(USE_ZSTD)\n    else()\n        message(STATUS "Zstandard not found via find_package, checking for system library")\n        find_library(ZSTD_LIBRARY NAMES zstd libzstd)\n        find_path(ZSTD_INCLUDE_DIR NAMES zstd.h)\n        if(ZSTD_LIBRARY AND ZSTD_INCLUDE_DIR)\n            message(STATUS "Found system Zstandard: ${ZSTD_LIBRARY}")\n            add_compile_definitions(USE_ZSTD)\n            include_directories(${ZSTD_INCLUDE_DIR})\n            set(COMPRESSION_LIBS ${COMPRESSION_LIBS} ${ZSTD_LIBRARY})\n        else()\n            message(WARNING "Zstandard not found, disabling ZSTD compression support")\n            set(USE_ZSTD OFF)\n        endif()\n    endif()\nendif()\n\n# Define source files for the DLL hooks library\nset(DLL_HOOKS_SOURCES\n    DLLHooks/dllmain.cpp # Updated path\n    src/hooks/windows_api/windows_api_hooks.cpp\n    src/hooks/windows_api/hooked_functions.cpp\n    src/hooks/windows_api/keyboard_hook.cpp\n    src/hooks/windows_api/hook_utils.cpp\n    src/hooks/dx_hook_core.cpp\n    src/hooks/swap_chain_hook.cpp\n    src/memory/memory_scanner.cpp\n    src/memory/pattern_scanner.cpp\n    # Removed: src/memory/memory_tracker.cpp\n    src/frame/frame_extractor.cpp\n    src/shared/shared_memory_transport.cpp\n    src/signatures/dx_signatures.cpp\n    src/signatures/lockdown_signatures.cpp\n    src/com_hooks/factory_hooks.cpp\n    # Removed: src/com_hooks/com_tracker.cpp\n)\n\n# Define source files for the test client\nset(TEST_CLIENT_SOURCES\n    src/test_client.cpp\n)\n\n# Removed VIRTUAL_CAMERA_SOURCES set\n# Removed VCAM_TEST_SOURCES set\n\n# Add the DLL hooks library\nadd_library(UndownUnlockDXHook SHARED ${DLL_HOOKS_SOURCES})\n\n# Set up the DLL properties\ntarget_compile_definitions(UndownUnlockDXHook PRIVATE\n    WIN32\n    _WINDOWS\n    _USRDLL\n)\n\n# Add the test client executable\nadd_executable(UndownUnlockTestClient ${TEST_CLIENT_SOURCES})\n\n# Removed UndownUnlockVirtualCamera library\n# Removed UndownUnlockVCamTest executable\n\n# Link the test client with necessary libraries\ntarget_link_libraries(UndownUnlockTestClient PRIVATE\n    d3d11\n    dxgi\n    windowscodecs\n    ${COMPRESSION_LIBS}\n)\n\n# Link the DLL hooks with necessary libraries\ntarget_link_libraries(UndownUnlockDXHook PRIVATE\n    d3d11\n    dxgi\n    windowscodecs\n    dbghelp\n    ${COMPRESSION_LIBS}\n)\n\n# Removed linking for UndownUnlockVirtualCamera\n# Removed linking for UndownUnlockVCamTest\n\n# Conditionally link LZ4 and ZSTD using find_package results\nif(USE_LZ4 AND lz4_FOUND)\n    target_link_libraries(UndownUnlockDXHook PRIVATE lz4::lz4)\n    target_link_libraries(UndownUnlockTestClient PRIVATE lz4::lz4)\nendif()\n\nif(USE_ZSTD AND zstd_FOUND)\n    target_link_libraries(UndownUnlockDXHook PRIVATE zstd::libzstd_shared)\n    target_link_libraries(UndownUnlockTestClient PRIVATE zstd::libzstd_shared)\nendif()\n\n# Removed virtual camera driver resource embedding section\n\n# Installation rules\ninstall(TARGETS UndownUnlockDXHook UndownUnlockTestClient\n    RUNTIME DESTINATION bin\n    LIBRARY DESTINATION lib\n    ARCHIVE DESTINATION lib\n)\n\n# Create a bin directory (kept in case needed for other binaries later)\ninstall(DIRECTORY DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin)\n\n# Copy the Python injector script to the output directory\nconfigure_file(\n    ${CMAKE_SOURCE_DIR}/src/test_injector.py\n    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_injector.py\n    COPYONLY\n)\n\n# Add tests if testing is enabled\noption(BUILD_TESTING \"Build tests\" ON)\nif(BUILD_TESTING)\n    enable_testing()\n    add_subdirectory(tests)\n    \n    # Removed add_test for VirtualCameraTest\nendif()\n\n# Print configuration summary\nmessage(STATUS \"CMAKE_GENERATOR: ${CMAKE_GENERATOR}\")\nmessage(STATUS \"CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}\")\nmessage(STATUS \"CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}\")\nmessage(STATUS \"CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}\")\nmessage(STATUS \"CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}\")\nmessage(STATUS \"CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}\")\nmessage(STATUS \"CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}\")\nmessage(STATUS \"CMAKE_SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}\")\nmessage(STATUS \"Compression: LZ4=${USE_LZ4}, ZSTD=${USE_ZSTD}\")\n===