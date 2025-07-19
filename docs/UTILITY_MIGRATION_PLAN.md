# Utility Migration Plan - Week 2-3

## Overview

This document outlines the migration plan for integrating the newly created utility components (RAII wrappers, error handling, memory tracking, performance monitoring, and crash reporting) into the existing codebase during Week 2-3 of Improvement #3.

## Migration Goals

- **Zero Memory Leaks**: Ensure all Windows handles and resources are properly managed
- **Comprehensive Error Handling**: Replace ad-hoc error handling with centralized system
- **Performance Monitoring**: Add performance tracking to critical operations
- **Crash Reporting**: Implement crash detection and reporting
- **Memory Tracking**: Monitor memory usage and detect leaks

## Migration Strategy

### Phase 1: Critical Files (Priority 1)
These files handle the most critical operations and should be migrated first.

#### 1. `src/hooks/dx_hook_core.cpp`
**Current Issues:**
- Manual COM interface management
- Inconsistent error handling
- No performance monitoring
- Potential memory leaks

**Migration Steps:**
1. Include utility headers
2. Replace raw COM pointers with RAII wrappers
3. Add centralized error handling
4. Add performance monitoring for hook operations
5. Add memory tracking for DirectX resources

**Expected Changes:**
```cpp
// Before
ID3D11Device* device = nullptr;
HRESULT hr = CreateDevice(..., &device);
if (FAILED(hr)) {
    // Manual error handling
}

// After
utils::ComPtr<ID3D11Device> device;
auto hr = utils::create_device_safe(..., device);
if (FAILED(hr)) {
    utils::ErrorHandler::GetInstance()->error(
        "Failed to create D3D11 device", 
        utils::ErrorCategory::GRAPHICS,
        __FUNCTION__, __FILE__, __LINE__, hr
    );
}
```

#### 2. `src/shared/shared_memory_transport.cpp`
**Current Issues:**
- Manual handle management
- No error recovery
- Silent failures

**Migration Steps:**
1. Replace HANDLE with HandleWrapper
2. Add error context for IPC operations
3. Implement automatic recovery strategies
4. Add performance monitoring for data transfer

#### 3. `src/frame/frame_extractor.cpp`
**Current Issues:**
- Manual texture management
- No error handling for GPU operations
- Memory leaks in staging textures

**Migration Steps:**
1. Use RAII wrappers for DirectX textures
2. Add comprehensive error handling
3. Implement automatic cleanup on errors
4. Add performance monitoring for frame extraction

### Phase 2: Secondary Files (Priority 2)

#### 4. `src/hooks/swap_chain_hook.cpp`
**Migration Steps:**
1. Replace raw swap chain pointers with RAII wrappers
2. Add error handling for Present hook
3. Add performance monitoring for frame presentation
4. Implement automatic recovery for hook failures

#### 5. `src/memory/pattern_scanner.cpp`
**Migration Steps:**
1. Add error handling for memory access violations
2. Use RAII wrappers for memory mappings
3. Add performance monitoring for scan operations
4. Implement memory tracking for scanned regions

#### 6. `src/hooks/windows_api/windows_api_hooks.cpp`
**Migration Steps:**
1. Replace raw handles with RAII wrappers
2. Add comprehensive error handling
3. Add performance monitoring for API calls
4. Implement crash reporting for hook failures

### Phase 3: Remaining Files (Priority 3)

#### 7. `src/hooks/windows_api/hooked_functions.cpp`
#### 8. `src/hooks/windows_api/keyboard_hook.cpp`
#### 9. `src/signatures/dx_signatures.cpp`
#### 10. `src/signatures/lockdown_signatures.cpp`

## Implementation Guidelines

### 1. RAII Wrapper Usage

**For Windows Handles:**
```cpp
// Before
HANDLE event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
if (event != INVALID_HANDLE_VALUE) {
    // Use event
    CloseHandle(event);
}

// After
utils::HandleWrapper event(CreateEventA(nullptr, TRUE, FALSE, nullptr));
if (event.is_valid()) {
    // Use event.get()
    // Automatically closed when wrapper goes out of scope
}
```

**For COM Interfaces:**
```cpp
// Before
ID3D11Device* device = nullptr;
HRESULT hr = CreateDevice(..., &device);
if (SUCCEEDED(hr)) {
    // Use device
    device->Release();
}

// After
utils::ComPtr<ID3D11Device> device;
auto hr = utils::create_device_safe(..., device);
if (SUCCEEDED(hr)) {
    // Use device.get()
    // Automatically released when wrapper goes out of scope
}
```

### 2. Error Handling Integration

**Centralized Error Reporting:**
```cpp
auto error_handler = utils::ErrorHandler::GetInstance();

// Set error context for operation
utils::ErrorContext context;
context.set("operation", "frame_extraction");
context.set("frame_number", std::to_string(frame_number));
error_handler->set_error_context(context);

// Report errors with full context
if (FAILED(hr)) {
    error_handler->error(
        "Failed to extract frame data",
        utils::ErrorCategory::GRAPHICS,
        __FUNCTION__, __FILE__, __LINE__, hr
    );
}

// Clear context when done
error_handler->clear_error_context();
```

### 3. Performance Monitoring

**Add Performance Tracking:**
```cpp
auto perf_monitor = utils::PerformanceMonitor::GetInstance();

// Start monitoring critical operation
auto operation_id = perf_monitor->start_operation("frame_extraction");

// Perform operation
// ... frame extraction code ...

// End monitoring
perf_monitor->end_operation(operation_id);

// Check for performance issues
if (perf_monitor->is_operation_slow(operation_id)) {
    error_handler->warning(
        "Frame extraction is taking longer than expected",
        utils::ErrorCategory::PERFORMANCE,
        __FUNCTION__, __FILE__, __LINE__
    );
}
```

### 4. Memory Tracking

**Add Memory Monitoring:**
```cpp
auto memory_tracker = utils::MemoryTracker::GetInstance();

// Track memory allocation
auto allocation_id = memory_tracker->track_allocation(
    "frame_buffer", size, utils::MemoryCategory::GRAPHICS
);

// Use allocated memory
// ... use memory ...

// Release tracking
memory_tracker->release_allocation(allocation_id);

// Check for memory leaks
if (memory_tracker->has_leaks()) {
    error_handler->warning(
        "Potential memory leak detected",
        utils::ErrorCategory::MEMORY,
        __FUNCTION__, __FILE__, __LINE__
    );
}
```

## Testing Strategy

### 1. Unit Tests
- Create tests for each migrated component
- Test error handling scenarios
- Test resource cleanup
- Test performance monitoring

### 2. Integration Tests
- Test complete hook installation flow
- Test frame capture pipeline
- Test error recovery mechanisms
- Test memory leak detection

### 3. Performance Tests
- Measure overhead of utility components
- Test performance monitoring accuracy
- Verify no performance regressions

## Success Criteria

### Week 2 Goals
- [ ] Complete migration of Priority 1 files (3 files)
- [ ] All migrated files pass unit tests
- [ ] No memory leaks in migrated components
- [ ] Comprehensive error handling in place

### Week 3 Goals
- [ ] Complete migration of Priority 2 files (3 files)
- [ ] Complete migration of Priority 3 files (4 files)
- [ ] All components pass integration tests
- [ ] Performance monitoring active across all components
- [ ] Memory tracking operational

### Final Success Metrics
- **Zero Memory Leaks**: No memory leaks detected in production builds
- **<1% Crash Rate**: Hook operations have less than 1% crash rate
- **100% Resource Cleanup**: All resources properly cleaned up in error paths
- **Comprehensive Logging**: All operations logged with structured error reporting
- **Performance Monitoring**: All critical operations monitored for performance

## Risk Mitigation

### 1. Incremental Migration
- Migrate one file at a time
- Test thoroughly after each migration
- Rollback capability for each change

### 2. Comprehensive Testing
- Unit tests for each migrated component
- Integration tests for complete workflows
- Performance regression testing

### 3. Monitoring and Alerting
- Real-time monitoring of migrated components
- Automatic alerts for performance issues
- Memory leak detection alerts

## Timeline

### Week 2 (Days 1-5)
- **Day 1-2**: Migrate `dx_hook_core.cpp`
- **Day 3**: Migrate `shared_memory_transport.cpp`
- **Day 4**: Migrate `frame_extractor.cpp`
- **Day 5**: Testing and validation

### Week 3 (Days 1-5)
- **Day 1-2**: Migrate Priority 2 files
- **Day 3-4**: Migrate Priority 3 files
- **Day 5**: Final testing and validation

## Conclusion

This migration plan ensures systematic integration of utility components while maintaining system stability and performance. The incremental approach minimizes risk while maximizing the benefits of improved error handling, resource management, and monitoring capabilities. 