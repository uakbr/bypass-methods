# UndownUnlock: DirectX Hooking Library

## Overview
UndownUnlock is a DirectX hooking system designed to bypass security protections in applications by intercepting DirectX calls and extracting frame data without detection. It can be used to capture screen content from applications that try to prevent screenshots, like examination proctoring software.

## Key Components
1. **DirectX Hook Core**: Central manager for all hooking functionality
2. **SwapChain Hook**: Intercepts DirectX rendering calls
3. **Frame Extractor**: Captures rendered frames from the GPU
4. **Shared Memory Transport**: Transfers frames between processes
5. **Pattern Scanner**: Locates functions in memory by signature patterns
6. **Windows API Hooks**: Prevents detection by hooking security-related APIs

## Building from Source

### Prerequisites
- CMake 3.12 or newer
- Visual Studio 2019 or newer with C++17 support
- DirectX SDK (usually included with Windows SDK)
- Python 3.6+ (for the injector script)

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
   python test_injector.py --name "TargetProcess.exe"
   ```
   or
   ```
   python test_injector.py --pid 1234
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
- **DLLHooks/**: Original DLL injection project (Visual Studio)
- **include/**: Header files
  - **com_hooks/**: COM interface hooking code
  - **memory/**: Memory manipulation functions
  - **signatures/**: DirectX function signatures
  - **hooks/**: Windows API hooking code
- **src/**: Implementation files
  - **com_hooks/**: COM hooking implementation
  - **frame/**: Frame extraction from DirectX
  - **hooks/**: DirectX and Windows API hooking
  - **memory/**: Memory scanning implementation
  - **shared/**: Shared memory transport
  - **signatures/**: Pattern implementation

## Contributing
Contributions are welcome! Please feel free to submit a Pull Request.

## License
This project is licensed under the MIT License - see the LICENSE file for details.

## Disclaimer
This software is intended for educational purposes only. Use of this software to bypass security measures may violate terms of service or laws in your jurisdiction. Use at your own risk.
