#include "../../include/com_hooks/factory_hooks.h"
#include "../../include/dx_hook_core.h"
#include "../../include/com_hooks/com_ptr.h"
#include "../../include/com_hooks/com_tracker.h"
#include <iostream>

namespace UndownUnlock {
namespace DXHook {

// Initialize the singleton instance
FactoryHooks* FactoryHooks::s_instance = nullptr;

FactoryHooks::FactoryHooks()
    : m_originalCreateDXGIFactory(nullptr)
    , m_originalCreateDXGIFactory1(nullptr)
    , m_originalCreateDXGIFactory2(nullptr)
    , m_originalD3D11CreateDevice(nullptr)
    , m_originalD3D11CreateDeviceAndSwapChain(nullptr) {
}

FactoryHooks::~FactoryHooks() {
    Shutdown();
}

FactoryHooks& FactoryHooks::GetInstance() {
    if (!s_instance) {
        s_instance = new FactoryHooks();
    }
    return *s_instance;
}

bool FactoryHooks::Initialize() {
    std::cout << "Initializing DXGI Factory hooks..." << std::endl;
    
    // Initialize COM tracker
    ComTracker::GetInstance().Initialize(true); // Enable callstack capture
    
    // Install hooks for factory creation functions
    bool factory = InstallHook(
        reinterpret_cast<void**>(&m_originalCreateDXGIFactory),
        reinterpret_cast<void*>(HookCreateDXGIFactory),
        "dxgi.dll", "CreateDXGIFactory"
    );
    
    bool factory1 = InstallHook(
        reinterpret_cast<void**>(&m_originalCreateDXGIFactory1),
        reinterpret_cast<void*>(HookCreateDXGIFactory1),
        "dxgi.dll", "CreateDXGIFactory1"
    );
    
    bool factory2 = InstallHook(
        reinterpret_cast<void**>(&m_originalCreateDXGIFactory2),
        reinterpret_cast<void*>(HookCreateDXGIFactory2),
        "dxgi.dll", "CreateDXGIFactory2"
    );
    
    // Install hooks for device creation functions
    bool device = InstallHook(
        reinterpret_cast<void**>(&m_originalD3D11CreateDevice),
        reinterpret_cast<void*>(HookD3D11CreateDevice),
        "d3d11.dll", "D3D11CreateDevice"
    );
    
    bool deviceAndSwapChain = InstallHook(
        reinterpret_cast<void**>(&m_originalD3D11CreateDeviceAndSwapChain),
        reinterpret_cast<void*>(HookD3D11CreateDeviceAndSwapChain),
        "d3d11.dll", "D3D11CreateDeviceAndSwapChain"
    );
    
    // Log the results
    std::cout << "CreateDXGIFactory hook: " << (factory ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "CreateDXGIFactory1 hook: " << (factory1 ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "CreateDXGIFactory2 hook: " << (factory2 ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "D3D11CreateDevice hook: " << (device ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "D3D11CreateDeviceAndSwapChain hook: " << (deviceAndSwapChain ? "SUCCESS" : "FAILED") << std::endl;
    
    return factory || factory1 || factory2 || device || deviceAndSwapChain;
}

void FactoryHooks::Shutdown() {
    std::cout << "Shutting down DXGI Factory hooks..." << std::endl;
    
    // Clear tracked interfaces
    std::lock_guard<std::mutex> lock(m_interfaceMutex);
    m_trackedInterfaces.clear();
    m_originalCreateSwapChain.clear();
    
    // Shutdown COM tracker
    ComTracker::GetInstance().Shutdown();
}

bool FactoryHooks::HookFactoryCreateSwapChain(IDXGIFactory* pFactory) {
    if (!pFactory) {
        return false;
    }
    
    // Get the vtable from the factory
    void** vtable = *reinterpret_cast<void***>(pFactory);
    if (!vtable) {
        return false;
    }
    
    // Store the original CreateSwapChain function (index 10 in IDXGIFactory vtable)
    CreateSwapChain_t originalFunc = reinterpret_cast<CreateSwapChain_t>(vtable[10]);
    m_originalCreateSwapChain[pFactory] = originalFunc;
    
    // Install our hook
    DWORD oldProtect;
    if (!VirtualProtect(&vtable[10], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::cerr << "Failed to change memory protection for IDXGIFactory::CreateSwapChain" << std::endl;
        return false;
    }
    
    // Replace the function pointer
    vtable[10] = reinterpret_cast<void*>(HookFactoryCreateSwapChain);
    
    // Restore memory protection
    DWORD temp;
    VirtualProtect(&vtable[10], sizeof(void*), oldProtect, &temp);
    
    std::cout << "Hooked IDXGIFactory::CreateSwapChain at " << pFactory << std::endl;
    
    // Track the Factory interface
    ComTracker::GetInstance().TrackInterface(pFactory, "IDXGIFactory", 1);
    
    return true;
}

void* FactoryHooks::GetRealFunctionAddress(const char* moduleName, const char* functionName) {
    HMODULE hModule = GetModuleHandleA(moduleName);
    if (!hModule) {
        std::cerr << "Failed to get module handle for " << moduleName << std::endl;
        return nullptr;
    }
    
    void* funcAddr = GetProcAddress(hModule, functionName);
    if (!funcAddr) {
        std::cerr << "Failed to get address for " << functionName << " in " << moduleName << std::endl;
        return nullptr;
    }
    
    return funcAddr;
}

bool FactoryHooks::InstallHook(void** originalFunction, void* hookFunction, const char* moduleName, const char* functionName) {
    // Get the real function address
    void* realFunc = GetRealFunctionAddress(moduleName, functionName);
    if (!realFunc) {
        return false;
    }
    
    // Store the original function
    *originalFunction = realFunc;
    
    // Install the hook using VirtualProtect
    DWORD oldProtect;
    if (!VirtualProtect(realFunc, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::cerr << "Failed to change memory protection for " << functionName << std::endl;
        return false;
    }
    
    // Create a relative jump to our hook function
    // Format: E9 xx xx xx xx (JMP rel32)
    BYTE* code = reinterpret_cast<BYTE*>(realFunc);
    DWORD relativeAddress = reinterpret_cast<DWORD>(hookFunction) - reinterpret_cast<DWORD>(realFunc) - 5;
    
    code[0] = 0xE9; // JMP opcode
    *reinterpret_cast<DWORD*>(&code[1]) = relativeAddress;
    
    // Restore memory protection
    DWORD temp;
    VirtualProtect(realFunc, 5, oldProtect, &temp);
    
    // Flush instruction cache to ensure the CPU picks up the changes
    FlushInstructionCache(GetCurrentProcess(), realFunc, 5);
    
    return true;
}

void FactoryHooks::TrackInterface(void* pInterface, const std::string& interfaceType) {
    if (!pInterface) {
        return;
    }
    
    // Use ComTracker instead of direct tracking
    ComTracker::GetInstance().TrackInterface(pInterface, interfaceType, 1);
}

void FactoryHooks::UntrackInterface(void* pInterface) {
    if (!pInterface) {
        return;
    }
    
    // Use ComTracker instead of direct tracking
    ComTracker::GetInstance().UntrackInterface(pInterface);
}

bool FactoryHooks::ValidateInterface(IUnknown* pInterface, REFIID riid) {
    if (!pInterface) {
        return false;
    }
    
    // Basic validation - try to call a method on the interface
    HRESULT hr = pInterface->QueryInterface(riid, nullptr);
    if (hr != E_NOINTERFACE && hr != S_OK) {
        std::cerr << "Interface validation failed with HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }
    
    return true;
}

HRESULT WINAPI FactoryHooks::HookCreateDXGIFactory(REFIID riid, void** ppFactory) {
    // Call the original function
    HRESULT hr = GetInstance().m_originalCreateDXGIFactory(riid, ppFactory);
    
    // If successful, hook the factory
    if (SUCCEEDED(hr) && ppFactory && *ppFactory) {
        std::cout << "CreateDXGIFactory called, returned 0x" << std::hex << hr << std::dec << std::endl;
        
        // Track the created factory
        std::string interfaceType = ComTracker::GetInstance().GetInterfaceTypeFromIID(riid);
        ComTracker::GetInstance().TrackInterface(*ppFactory, interfaceType, 1);
        
        // Hook CreateSwapChain if it's an IDXGIFactory
        if (riid == __uuidof(IDXGIFactory)) {
            GetInstance().HookFactoryCreateSwapChain(static_cast<IDXGIFactory*>(*ppFactory));
        }
    }
    
    return hr;
}

HRESULT WINAPI FactoryHooks::HookCreateDXGIFactory1(REFIID riid, void** ppFactory) {
    // Call the original function
    HRESULT hr = GetInstance().m_originalCreateDXGIFactory1(riid, ppFactory);
    
    // If successful, hook the factory
    if (SUCCEEDED(hr) && ppFactory && *ppFactory) {
        std::cout << "CreateDXGIFactory1 called, returned 0x" << std::hex << hr << std::dec << std::endl;
        
        // Track the created factory
        std::string interfaceType = ComTracker::GetInstance().GetInterfaceTypeFromIID(riid);
        ComTracker::GetInstance().TrackInterface(*ppFactory, interfaceType, 1);
        
        // Hook CreateSwapChain if it's an IDXGIFactory or IDXGIFactory1
        if (riid == __uuidof(IDXGIFactory) || riid == __uuidof(IDXGIFactory1)) {
            GetInstance().HookFactoryCreateSwapChain(static_cast<IDXGIFactory*>(*ppFactory));
        }
    }
    
    return hr;
}

HRESULT WINAPI FactoryHooks::HookCreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) {
    // Call the original function
    HRESULT hr = GetInstance().m_originalCreateDXGIFactory2(Flags, riid, ppFactory);
    
    // If successful, hook the factory
    if (SUCCEEDED(hr) && ppFactory && *ppFactory) {
        std::cout << "CreateDXGIFactory2 called, returned 0x" << std::hex << hr << std::dec << std::endl;
        
        // Track the created factory
        std::string interfaceType = ComTracker::GetInstance().GetInterfaceTypeFromIID(riid);
        ComTracker::GetInstance().TrackInterface(*ppFactory, interfaceType, 1);
        
        // Hook CreateSwapChain if it's a compatible interface
        if (riid == __uuidof(IDXGIFactory) || riid == __uuidof(IDXGIFactory1) || riid == __uuidof(IDXGIFactory2)) {
            GetInstance().HookFactoryCreateSwapChain(static_cast<IDXGIFactory*>(*ppFactory));
        }
    }
    
    return hr;
}

HRESULT WINAPI FactoryHooks::HookD3D11CreateDevice(
    IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
    ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext) {
    
    // Call the original function
    HRESULT hr = GetInstance().m_originalD3D11CreateDevice(
        pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
        SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext
    );
    
    // Track created interfaces
    if (SUCCEEDED(hr)) {
        if (ppDevice && *ppDevice) {
            ComTracker::GetInstance().TrackInterface(*ppDevice, "ID3D11Device", 1);
        }
        
        if (ppImmediateContext && *ppImmediateContext) {
            ComTracker::GetInstance().TrackInterface(*ppImmediateContext, "ID3D11DeviceContext", 1);
        }
    }
    
    return hr;
}

HRESULT WINAPI FactoryHooks::HookD3D11CreateDeviceAndSwapChain(
    IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
    const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain,
    ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext) {
    
    // Call the original function
    HRESULT hr = GetInstance().m_originalD3D11CreateDeviceAndSwapChain(
        pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
        SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext
    );
    
    // Track created interfaces
    if (SUCCEEDED(hr)) {
        if (ppSwapChain && *ppSwapChain) {
            ComTracker::GetInstance().TrackInterface(*ppSwapChain, "IDXGISwapChain", 1);
        }
        
        if (ppDevice && *ppDevice) {
            ComTracker::GetInstance().TrackInterface(*ppDevice, "ID3D11Device", 1);
        }
        
        if (ppImmediateContext && *ppImmediateContext) {
            ComTracker::GetInstance().TrackInterface(*ppImmediateContext, "ID3D11DeviceContext", 1);
        }
    }
    
    return hr;
}

HRESULT STDMETHODCALLTYPE FactoryHooks::HookFactoryCreateSwapChain(
    IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain) {
    
    // Get the original function
    CreateSwapChain_t originalCreateSwapChain = nullptr;
    {
        std::lock_guard<std::mutex> lock(GetInstance().m_interfaceMutex);
        auto it = GetInstance().m_originalCreateSwapChain.find(pFactory);
        if (it == GetInstance().m_originalCreateSwapChain.end()) {
            std::cerr << "ERROR: Original CreateSwapChain not found for factory " << pFactory << std::endl;
            return E_FAIL;
        }
        originalCreateSwapChain = it->second;
    }
    
    // Call the original function
    HRESULT hr = originalCreateSwapChain(pFactory, pDevice, pDesc, ppSwapChain);
    
    // Track the created swap chain
    if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain) {
        ComTracker::GetInstance().TrackInterface(*ppSwapChain, "IDXGISwapChain", 1);
        
        std::cout << "CreateSwapChain called, created swap chain: " << *ppSwapChain << std::endl;
        
        // Get the device's ID3D11Device interface for validation
        ComPtr<ID3D11Device> d3dDevice;
        if (SUCCEEDED(pDevice->QueryInterface(__uuidof(ID3D11Device), 
                                             reinterpret_cast<void**>(d3dDevice.GetAddressOf())))) {
            std::cout << "CreateSwapChain: Device is ID3D11Device" << std::endl;
        }
    }
    
    return hr;
}

} // namespace DXHook
} // namespace UndownUnlock 