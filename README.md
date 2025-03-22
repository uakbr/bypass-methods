# UndownUnlock: DirectX Hooking Library

## Overview
UndownUnlock is a DirectX hooking system designed to bypass security protections in applications by intercepting DirectX calls and extracting frame data without detection. It can be used to capture screen content from applications that try to prevent screenshots, like examination proctoring software.

## Key Components
1. **DirectX Hook Core**: Central manager for all hooking functionality
2. **SwapChain Hook**: Intercepts DirectX rendering calls
3. **Frame Extractor**: Captures rendered frames from the GPU
4. **Shared Memory Transport**: Transfers frames between processes with cache-aligned ring buffer and compression
5. **Pattern Scanner**: Locates functions in memory by signature patterns with multithreaded optimization
6. **Windows API Hooks**: Prevents detection by hooking security-related APIs
7. **COM Interface Tracking**: Manages DirectX interface lifecycles with reference counting
8. **Memory Leak Detection**: Tracks memory allocations with callstack capture
9. **DirectX Interface Detection**: Identifies different versions of DirectX interfaces with fallback mechanisms

## Performance Optimizations

### Shared Memory Transport
The shared memory transport has been optimized with:
- **Cache-aligned ring buffer** to prevent false sharing and improve multi-thread performance
- **Frame compression** with multiple algorithms (RLE, LZ4, ZSTD) to reduce memory usage and increase throughput
- **Memory alignment** for optimal data transfer between processes
- **Efficient synchronization** using slim reader/writer locks

See [Shared Memory Transport Documentation](docs/shared_memory_transport.md) for details.

### Pattern Scanning
The pattern scanner uses:
- **Boyer-Moore-Horspool algorithm** for fast pattern matching
- **Multithreaded scanning** to utilize all CPU cores
- **PE section analysis** to focus on code sections first

### DirectX Interface Detection
The DirectX interface detection system provides:
- **Comprehensive interface support** for DirectX 11 and DirectX 12
- **Version detection** to identify and utilize the highest available interface version
- **Fallback mechanisms** for cross-version compatibility
- **Custom implementation detection** for third-party graphics APIs

See [DirectX Interface Detection Documentation](docs/directx_interface_detection.md) for details.

## Building from Source

### Prerequisites
- CMake 3.12 or newer
- Visual Studio 2019 or newer with C++17 support
- DirectX SDK (usually included with Windows SDK)
- Python 3.6+ (for the injector script)
- Optional: LZ4 and ZSTD libraries for enhanced compression

### Build Steps (Windows)

#### Using CMake GUI
1. Clone the repository
   ```
   git clone https://github.com/yourusername/UndownUnlock.git
   cd UndownUnlock
   ```

2. Create a build directory
   ```
   mkdir build
   cd build
   ```

3. Configure and build
   ```
   cmake ..
   cmake --build . --config Release
   ```

4. With compression support enabled
   ```
   cmake .. -DUSE_LZ4=ON -DUSE_ZSTD=ON
   cmake --build . --config Release
   ```

#### Using Visual Studio
1. Open the project folder in Visual Studio
2. Select "File" > "Open" > "CMake" and open the CMakeLists.txt file
3. Choose "Release" configuration
4. Click "Build" > "Build All"

### Build Steps (Cross-Platform with Visual Studio Code)
1. Install Visual Studio Code with the CMake Tools extension
2. Open the project folder in VSCode
3. Press Ctrl+Shift+P, type "CMake: Configure" and press Enter
4. Press Ctrl+Shift+P, type "CMake: Build" and press Enter

## Usage

### DLL Injection
1. Build the project
2. Use the Python injector script to inject the DLL:
   ```
   python python/tools/test_injector.py --name "TargetProcess.exe"
   ```
   or
   ```
   python python/tools/test_injector.py --pid 1234
   ```

### Using the Test Client
1. Inject the DLL into a DirectX application
2. Run the test client to view captured frames:
   ```
   UndownUnlockTestClient.exe
   ```

### Key Controls
- **Up Arrow**: Install focus hooks to prevent the application from detecting lost focus
- **Down Arrow**: Uninstall focus hooks to restore normal behavior

## Testing
The project includes unit tests using Google Test:

```
cd build
ctest -C Release
```

## Project Structure
- **DLLHooks/**: DLL project files for Visual Studio
- **src/**: C++ implementation files
  - **com_hooks/**: COM interface hooking implementation
  - **frame/**: DirectX frame extraction
  - **hooks/**: Windows API and DirectX hooking
  - **memory/**: Memory scanning and pattern matching
  - **shared/**: Shared memory transport
  - **signatures/**: DirectX function signatures
- **include/**: C++ header files
  - **com_hooks/**: COM hooking declarations
  - **hooks/**: API hooking declarations
  - **memory/**: Memory operations
  - **signatures/**: Pattern definitions
- **python/**: Python code
  - **capture/**: Screen capture utilities
  - **tools/**: Injection and utility scripts
  - **accessibility/**: Accessibility control modules
  - **tests/**: Python-based tests
  - **examples/**: Example usage scripts
  - **requirements/**: Python dependencies
- **tests/**: C++ test code
- **docs/**: Documentation
  - **planning/**: Project plans and todo lists
  - **shared_memory_transport.md**: Shared memory optimization details
  - **directx_interface_detection.md**: DirectX interface detection details
  - **memory_com_tracking.md**: Memory and COM tracking documentation
  - **hook_system.md**: Hook system documentation
  - **pattern_scanning.md**: Pattern scanning optimization details
- **scripts/**: Batch files and automation scripts
- **build/**: Build output directory (not in repo)

## Contributing
Contributions are welcome! Please feel free to submit a Pull Request.

## License
This project is licensed under the MIT License - see the LICENSE file for details.

## Disclaimer
This software is intended for educational purposes only. Use of this software to bypass security measures may violate terms of service or laws in your jurisdiction. Use at your own risk.
