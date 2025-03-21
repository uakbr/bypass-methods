cmake_minimum_required(VERSION 3.10)
project(UndownUnlockDXHook VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable multi-processor compilation
if(MSVC)
    add_compile_options(/MP)
endif()

# Include directories
include_directories(include)

# Source files for the DLL
set(DLL_SOURCES
    src/dllmain.cpp
    src/hooks/dx_hook_core.cpp
    src/hooks/swap_chain_hook.cpp
    src/memory/memory_scanner.cpp
    src/frame/frame_extractor.cpp
    src/shared/shared_memory_transport.cpp
)

# Source files for the test client
set(CLIENT_SOURCES
    src/test_client.cpp
)

# Create the DLL target
add_library(UndownUnlockDXHook SHARED ${DLL_SOURCES})

# Create the client executable target
add_executable(TestClient ${CLIENT_SOURCES})

# Set output directories
set_target_properties(UndownUnlockDXHook PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

set_target_properties(TestClient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Link libraries for the DLL
target_link_libraries(UndownUnlockDXHook PRIVATE
    d3d11.lib
    dxgi.lib
    psapi.lib
)

# Link libraries for the test client
target_link_libraries(TestClient PRIVATE
    d3d11.lib
    dxgi.lib
) 