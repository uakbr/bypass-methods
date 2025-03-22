# DirectX Interface Detection Improvements

## Overview

The DirectX Interface Detection system is responsible for detecting, tracking, and managing COM interfaces used by DirectX applications. This system has been enhanced to support DirectX 12 interfaces, version detection, and fallback mechanisms for cross-version compatibility.

## Key Improvements

### 1. DirectX 12 Interface Support

Added support for detecting and tracking DirectX 12 interfaces:

- **ID3D12Device** and derivatives (ID3D12Device1 through ID3D12Device9)
- **ID3D12CommandQueue** and **ID3D12CommandList** interfaces
- **ID3D12GraphicsCommandList** variations
- **ID3D12Resource** for resource management

```cpp
// Example of how we detect and track DirectX 12 interfaces
if (SUCCEEDED(pDevice->QueryInterface(__uuidof(ID3D12Device), reinterpret_cast<void**>(&d3d12Device)))) {
    ComTracker::GetInstance().TrackInterface(d3d12Device.Get(), "ID3D12Device");
}
```

### 2. Interface Version Detection

The system now automatically detects the highest available interface version supported by a COM object:

- Dynamically discovers the exact version of DirectX interfaces (e.g., IDXGIFactory1 through IDXGIFactory6)
- Uses a hierarchical query approach to find the most feature-rich interface implementation
- Tracks version information in the `DXInterfaceVersion` enumeration

```cpp
// Example of version query progression
ComPtr<IDXGISwapChain4> swapChain4;
if (SUCCEEDED(pSwapChain->QueryInterface(__uuidof(IDXGISwapChain4), reinterpret_cast<void**>(swapChain4.GetAddressOf())))) {
    // Use SwapChain4
} else {
    // Try SwapChain3, SwapChain2, etc.
}
```

### 3. Fallback Mechanisms

For applications that require specific interface versions but can work with compatible alternatives:

- **Automatic Fallback**: When a requested interface isn't available, the system tries compatible previous versions
- **Compatibility Chain**: Each interface maintains a list of compatible alternatives in descending version order
- **Transparent Handling**: Applications can use the `TRY_FALLBACK_INTERFACE` macro to simplify fallback logic

```cpp
// Example of fallback usage
if (!TRY_FALLBACK_INTERFACE(pInterface, __uuidof(IDXGIFactory6), (void**)&factory)) {
    // Handle the case where no compatible interface is available
}
```

### 4. Custom Implementation Detection

For handling non-standard DirectX implementations (e.g., third-party graphics APIs):

- **VTable Analysis**: Examines interface VTables to identify non-standard implementations
- **Module Origin**: Checks whether interface code resides in official Microsoft DLLs
- **Behavioral Adaptation**: Adjusts hooking strategies for custom implementations

```cpp
// Example of custom implementation detection
bool isCustom = ComTracker::GetInstance().IsCustomImplementation(pInterface);
if (isCustom) {
    // Use alternative hooking or tracking approach
}
```

## Usage Guidelines

### 1. Interface Version Detection

```cpp
// Get the interface version
DXInterfaceVersion version = ComTracker::GetInstance().GetInterfaceVersion(pInterface, riid);

// Handle different versions appropriately
switch (version) {
    case DXInterfaceVersion::DXGI_1_6:
        // Use DXGI 1.6 features
        break;
    case DXInterfaceVersion::DXGI_1_5:
    case DXInterfaceVersion::DXGI_1_4:
        // Use features common to 1.4 and 1.5
        break;
    // ...
}
```

### 2. Interface Fallback

```cpp
// Attempt to get IDXGIFactory6, but fall back to earlier versions if needed
IDXGIFactory6* factory6 = nullptr;
void* factoryInterface = nullptr;

// Try direct query first
HRESULT hr = pFactory->QueryInterface(__uuidof(IDXGIFactory6), &factoryInterface);
if (FAILED(hr)) {
    // Try fallback mechanism
    if (ComTracker::GetInstance().GetFallbackInterface(pFactory, __uuidof(IDXGIFactory6), &factoryInterface)) {
        // A compatible interface was found
        factory6 = static_cast<IDXGIFactory6*>(factoryInterface);
    }
}

// Or use the convenience macro
if (TRY_FALLBACK_INTERFACE(pFactory, __uuidof(IDXGIFactory6), (void**)&factory6)) {
    // Use factory6
}
```

### 3. Custom Implementation Handling

```cpp
// Check if the implementation is non-standard
if (ComTracker::GetInstance().IsCustomImplementation(pSwapChain)) {
    std::cout << "Using a custom implementation of IDXGISwapChain" << std::endl;
    // Apply special handling for custom implementations
}
```

## Performance Considerations

1. **Version Detection Caching**: Interface version information is cached to avoid repeated queries
2. **Minimized QueryInterface**: The system minimizes QueryInterface calls to reduce overhead
3. **Targeted Hooking**: Only necessary interfaces are hooked based on detected versions

## Debugging Support

For debugging and diagnostics, the COM tracker now provides:

- **Detailed Interface Information**: Type, version, custom implementation status
- **Version Statistics**: Counts of different interface versions in use
- **Compatibility Reports**: Information about interface fallbacks and compatibility

## Future Enhancements

1. **Dynamic Interface Hooking**: Adapt hooking strategies based on detected interface versions
2. **Version-Specific Feature Tracking**: Track which DirectX features are used by the application
3. **Interface Compatibility Database**: Expand compatibility information for better fallback behavior
4. **Performance Analysis**: Compare performance characteristics across interface versions 