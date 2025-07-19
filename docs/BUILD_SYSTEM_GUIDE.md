# UndownUnlock Build System Guide

## Overview

The UndownUnlock project uses a modern CMake-based build system with centralized dependency management, automated testing, and CI/CD integration. This guide covers all aspects of building, testing, and deploying the project.

## Quick Start

### Prerequisites

- **Windows 10/11** (primary supported platform)
- **Visual Studio 2019 or later** with C++17 support
- **CMake 3.12 or later**
- **Python 3.9+** (for Python components)
- **Git** (for dependency fetching)

### Building the Project

```bash
# Clone the repository
git clone <repository-url>
cd bypass-methods

# Configure the build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build build --config Release --parallel

# Run tests
ctest --test-dir build -C Release
```

### Building with Visual Studio

```bash
# Generate Visual Studio solution
cmake -B build -G "Visual Studio 17 2022" -A x64

# Open in Visual Studio
cmake --open build
```

## Build System Architecture

### Core Components

1. **Main CMakeLists.txt** - Project configuration and target definitions
2. **cmake/dependencies.cmake** - Centralized dependency management
3. **tests/CMakeLists.txt** - Test configuration and execution
4. **GitHub Actions** - Automated CI/CD pipeline

### Dependency Management

The project uses `FetchContent` for managing external dependencies:

- **Google Test** - Unit testing framework
- **DirectX Headers** - Microsoft DirectX headers (optional)
- **System Libraries** - D3D11, DXGI, Windows Codecs

### Build Targets

- **UndownUnlockDXHook** - Main DLL library
- **UndownUnlockTestClient** - Test client executable
- **undownunlock_tests** - Unit test suite

## Configuration Options

### CMake Options

```bash
# Build options
-DBUILD_TESTING=ON          # Enable unit tests (default: ON)
-DENABLE_COVERAGE=OFF       # Enable code coverage (default: OFF)

# Dependency options
-DUSE_SYSTEM_GOOGLETEST=OFF # Use system Google Test (default: OFF)
-DUSE_SYSTEM_DIRECTX_HEADERS=OFF # Use system DirectX Headers (default: OFF)
```

### Build Types

- **Release** - Optimized build for production
- **Debug** - Debug build with symbols
- **RelWithDebInfo** - Release build with debug info
- **MinSizeRel** - Minimal size release build

## Testing

### Running Tests

```bash
# Run all tests
ctest --test-dir build -C Release

# Run tests with verbose output
ctest --test-dir build -C Release --output-on-failure

# Run specific test
ctest --test-dir build -C Release -R test_name
```

### Test Categories

1. **Unit Tests** - Individual component testing
2. **Integration Tests** - End-to-end functionality testing
3. **Python Tests** - Python component testing

### Test Coverage

```bash
# Enable coverage
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON

# Build and run tests
cmake --build build --config Debug
ctest --test-dir build -C Debug

# Generate coverage report
# (Implementation depends on coverage tool)
```

## CI/CD Pipeline

### GitHub Actions Workflows

The project includes comprehensive CI/CD automation:

1. **Build and Test** - Automated building and testing on Windows
2. **Static Analysis** - Code quality checks with clang-tidy
3. **Code Coverage** - Test coverage reporting
4. **Security Scan** - Security vulnerability scanning
5. **Build Validation** - Artifact validation and integration testing

### Workflow Triggers

- **Push to main/develop** - Full build and test
- **Pull Request** - Build validation and testing
- **Manual trigger** - Available for all workflows

### Artifacts

- **Build artifacts** - Compiled binaries and libraries
- **Test results** - Test execution reports
- **Coverage reports** - Code coverage data
- **Static analysis reports** - Code quality metrics

## Troubleshooting

### Common Issues

#### CMake Configuration Errors

```bash
# Error: Missing project() declaration
# Solution: Ensure tests/CMakeLists.txt has project() declaration

# Error: Google Test not found
# Solution: Ensure BUILD_TESTING=ON and dependencies are properly configured

# Error: Visual Studio not found
# Solution: Install Visual Studio 2019+ with C++ development tools
```

#### Build Errors

```bash
# Error: DirectX headers not found
# Solution: Install Windows SDK or use system headers

# Error: Linker errors
# Solution: Ensure all dependencies are properly linked

# Error: Python dependencies missing
# Solution: Run pip install -r python/requirements/requirements.txt
```

#### Test Failures

```bash
# Error: Tests not found
# Solution: Ensure BUILD_TESTING=ON and tests are built

# Error: Test timeout
# Solution: Increase test timeout or fix hanging tests

# Error: Missing test data
# Solution: Ensure test data files are present
```

### Validation Script

Use the build validation script to check for common issues:

```bash
# Run validation
python scripts/validate_build.py

# Generate detailed report
python scripts/validate_build.py --report
```

## Development Workflow

### Local Development

1. **Setup environment**
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build --config Debug
   ```

2. **Run tests**
   ```bash
   ctest --test-dir build -C Debug --output-on-failure
   ```

3. **Validate changes**
   ```bash
   python scripts/validate_build.py
   ```

### Contributing

1. **Create feature branch**
   ```bash
   git checkout -b feature/new-feature
   ```

2. **Make changes and test**
   ```bash
   cmake --build build --config Debug
   ctest --test-dir build -C Debug
   ```

3. **Create pull request**
   - CI/CD will automatically validate changes
   - Ensure all tests pass
   - Address any code review feedback

## Performance Optimization

### Build Performance

```bash
# Use parallel builds
cmake --build build --parallel

# Use ccache for faster rebuilds
cmake -B build -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

# Use precompiled headers
# (Configured automatically in CMakeLists.txt)
```

### Runtime Performance

- **Release builds** are optimized for performance
- **Debug builds** include additional checks and symbols
- **Profile builds** can be configured for performance analysis

## Deployment

### Release Builds

```bash
# Create release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Install to system
cmake --install build --prefix /path/to/install
```

### Package Creation

```bash
# Create installer package
cmake --build build --target package

# Create source package
cmake --build build --target package_source
```

## Monitoring and Maintenance

### Build Metrics

- **Build time** - Tracked in CI/CD
- **Test coverage** - Reported automatically
- **Code quality** - Static analysis results
- **Performance** - Benchmark results

### Dependency Updates

- **Automatic updates** - CI/CD checks for updates
- **Manual updates** - Update versions in cmake/dependencies.cmake
- **Security updates** - Automated vulnerability scanning

## Support

### Getting Help

1. **Check documentation** - This guide and inline comments
2. **Run validation** - Use validation script for diagnostics
3. **Check CI/CD logs** - Review automated build results
4. **Create issue** - Report bugs or request features

### Contributing to Build System

1. **Follow CMake best practices**
2. **Add tests for new features**
3. **Update documentation**
4. **Validate changes locally**
5. **Submit pull request**

## Future Improvements

### Planned Enhancements

1. **Cross-platform support** - Linux and macOS builds
2. **Advanced testing** - Performance and stress testing
3. **Package management** - Conan or vcpkg integration
4. **IDE integration** - Enhanced IDE support
5. **Documentation generation** - Automated API documentation

### Migration Path

The build system is designed for gradual migration:

1. **Phase 1** - Fix existing issues (✅ Complete)
2. **Phase 2** - Add CI/CD pipeline (✅ Complete)
3. **Phase 3** - Enhance testing framework
4. **Phase 4** - Performance optimization
5. **Phase 5** - Advanced features 