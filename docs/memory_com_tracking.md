# Memory and COM Interface Tracking

This document describes the memory tracking and COM interface tracking systems implemented in the UndownUnlock project.

## Memory Tracking System

The memory tracking system helps identify memory leaks and monitor memory usage patterns in the application. It provides detailed information about allocations, including source file, line number, allocation time, and optional callstack.

### Key Features

- **Allocation Tracking:** Records detailed information about each memory allocation
- **Leak Detection:** Identifies memory that was allocated but never freed
- **Callstack Capture:** Optionally captures the callstack at allocation time for easier debugging
- **Statistics:** Provides detailed statistics about memory usage
- **Tag Filtering:** Allows categorizing allocations with tags and filtering reports
- **Custom New/Delete:** Overloaded global operators automatically track allocations

### Usage

#### Basic Usage

```cpp
// Initialize the memory tracker (true = enable callstack capture)
UndownUnlock::Memory::MemoryTracker::GetInstance().Initialize(true);

// Set threshold to reduce noise from small allocations
UndownUnlock::Memory::MemoryTracker::GetInstance().SetLeakThreshold(1024); // 1KB

// When finished, dump report and shutdown
UndownUnlock::Memory::MemoryTracker::GetInstance().DumpLeakReport();
UndownUnlock::Memory::MemoryTracker::GetInstance().Shutdown();
```

#### Manual Tracking

```cpp
// Manually track an allocation
void* ptr = malloc(1024);
TRACK_ALLOCATION(ptr, 1024);

// Manually track with a tag
void* ptr2 = malloc(2048);
TRACK_ALLOCATION_TAGGED(ptr2, 2048, "TEMPORARY");

// Untrack when freeing
UNTRACK_ALLOCATION(ptr);
free(ptr);
```

#### Statistics

```cpp
MemoryStats stats = UndownUnlock::Memory::MemoryTracker::GetInstance().GetStats();
std::cout << "Current allocations: " << stats.currentAllocations << std::endl;
std::cout << "Peak memory usage: " << stats.peakBytes << " bytes" << std::endl;
```

#### Filter Report by Tag

```cpp
// Only show allocations with specific tags
UndownUnlock::Memory::MemoryTracker::GetInstance().AddTagFilter("DIRECTX", true); 
UndownUnlock::Memory::MemoryTracker::GetInstance().AddTagFilter("TEMPORARY", false); // Exclude these
```

### Implementation Details

The memory tracking system is implemented in:
- `include/memory/memory_tracker.h`
- `src/memory/memory_tracker.cpp`

It uses a singleton pattern for global access and thread-safe tracking. The system is designed to have minimal performance impact when enabled, but can be completely disabled in production builds.

The memory tracking hooks into the global `new`/`delete` operators, so it can track all allocations automatically. Custom allocators can also be integrated by calling the tracking functions manually.

## COM Interface Tracking

The COM interface tracker monitors COM object creation, usage, and destruction to identify reference counting issues and COM interface leaks.

### Key Features

- **Interface Tracking:** Records all tracked COM interfaces
- **Reference Counting:** Monitors AddRef/Release calls
- **Smart Pointer Wrapper:** `ComPtr<T>` template provides automatic reference counting
- **Leak Detection:** Identifies COM interfaces that weren't properly released
- **Callstack Capture:** Optionally captures callstack at creation time

### Usage

#### Smart Pointer Usage

```cpp
// Use ComPtr instead of raw pointers
UndownUnlock::DXHook::ComPtr<IDXGIFactory> factory;

// When creating the interface
hr = CreateDXGIFactory(__uuidof(IDXGIFactory), 
                     reinterpret_cast<void**>(factory.GetAddressOf()));

// ComPtr handles AddRef/Release automatically
{
    UndownUnlock::DXHook::ComPtr<IDXGIFactory> tempFactory = factory;
    // tempFactory is released when it goes out of scope
}

// Query for other interfaces
UndownUnlock::DXHook::ComPtr<IDXGIFactory1> factory1;
factory.As(factory1);
```

#### Manual Tracking

```cpp
// Initialize the COM tracker (true = enable callstack capture)
UndownUnlock::DXHook::ComTracker::GetInstance().Initialize(true);

// Manually track interfaces
IDXGIAdapter* adapter = nullptr;
hr = factory->EnumAdapters(0, &adapter);
if (SUCCEEDED(hr)) {
    UndownUnlock::DXHook::ComTracker::GetInstance().TrackInterface(
        adapter, "IDXGIAdapter", adapter->AddRef());
    adapter->Release(); // Compensate for our AddRef
}

// Check for issues periodically
int issueCount = UndownUnlock::DXHook::ComTracker::GetInstance().CheckForIssues();
if (issueCount > 0) {
    UndownUnlock::DXHook::ComTracker::GetInstance().DumpTrackedInterfaces();
}

// Shutdown when done
UndownUnlock::DXHook::ComTracker::GetInstance().Shutdown(); // Reports leaks
```

### How It Works

The COM tracker works by:
1. Tracking all COM interfaces when they're created
2. Monitoring AddRef/Release calls to update reference counts
3. Detecting when interfaces are leaked (not released properly)
4. Capturing callstack information at creation time (optional)

The `ComPtr<T>` template is a smart pointer that automatically handles AddRef and Release calls, similar to Microsoft's `CComPtr<T>`. It's designed to be a drop-in replacement that makes COM reference counting safer and more automatic.

The COM hooks intercept DirectX interface creation and hook the COM factory functions to track all COM objects in the application.

### Implementation Details

The COM tracking system is implemented in:
- `include/com_hooks/com_ptr.h`
- `include/com_hooks/com_tracker.h`
- `src/com_hooks/com_tracker.cpp`

## Integration in the Application

Both tracking systems are initialized in the DLL entry point (`src/dllmain_refactored.cpp`) and automatically report any leaks or issues when the DLL is unloaded.

A global function `DumpStatus()` is exported from the DLL to dump the current status of memory and COM interfaces on demand:

```cpp
// In your application
using DumpStatusFn = void (*)();
DumpStatusFn DumpStatus = (DumpStatusFn)GetProcAddress(hModule, "DumpStatus");
if (DumpStatus) {
    DumpStatus(); // Dumps memory and COM interface status
}
```

## Performance Considerations

Both tracking systems have some performance overhead, especially when callstack capture is enabled. In performance-critical applications, consider:

1. Disabling callstack capture in production builds
2. Setting a higher threshold for leak reporting
3. Using tagged allocations for specific subsystems
4. Periodically checking for issues instead of tracking everything

## Future Enhancements

Planned improvements for the tracking systems:
1. Sampling mode for high-volume allocations
2. Visual leak analyzer integration
3. Allocation hotspot identification
4. Custom allocator support 