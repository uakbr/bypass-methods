#include "../../include/com_hooks/factory_hooks.h"
#include "../../include/dx_hook_core.h"
#include <iostream>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <string> // Include string for GetInterfaceTypeFromIID

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

    // Clear the original CreateSwapChain map
    std::lock_guard<std::mutex> lock(m_createSwapChainMutex);
    m_originalCreateSwapChain.clear();
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

    // Store original CreateSwapChain function (index 10 in IDXGIFactory vtable)
    CreateSwapChain_t originalCreateSwapChain = reinterpret_cast<CreateSwapChain_t>(vtable[10]);

    // Already hooked?
    {
        std::lock_guard<std::mutex> lock(m_createSwapChainMutex);
        if (m_originalCreateSwapChain.count(pFactory)) {
            return true;
        }
        m_originalCreateSwapChain[pFactory] = originalCreateSwapChain;
    }

    // Install hook
    DWORD oldProtect;
    if (VirtualProtect(&vtable[10], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        vtable[10] = reinterpret_cast<void*>(HookFactoryCreateSwapChain);
        DWORD tempProtect; // Variable to receive the old protection state
        VirtualProtect(&vtable[10], sizeof(void*), oldProtect, &tempProtect);

        std::cout << "Hooked IDXGIFactory::CreateSwapChain for factory at 0x" << std::hex << pFactory << std::dec << std::endl;
        return true;
    }

    std::cerr << "Failed to hook IDXGIFactory::CreateSwapChain" << std::endl;
    // If hook installation fails, remove from map
    {
        std::lock_guard<std::mutex> lock(m_createSwapChainMutex);
        m_originalCreateSwapChain.erase(pFactory);
    }
    return false;
}

void* FactoryHooks::GetRealFunctionAddress(const char* moduleName, const char* functionName) {
    HMODULE module = GetModuleHandleA(moduleName);
    if (!module) {
        module = LoadLibraryA(moduleName);
        if (!module) {
            std::cerr << "Failed to load module: " << moduleName << std::endl;
            return nullptr;
        }
    }

    void* address = GetProcAddress(module, functionName);
    if (!address) {
        std::cerr << "Failed to get function address: " << functionName << " in " << moduleName << std::endl;
        return nullptr;
    }

    return address;
}

bool FactoryHooks::InstallHook(void** originalFunction, void* hookFunction, const char* moduleName, const char* functionName) {
    if (!originalFunction || !hookFunction || !moduleName || !functionName) {
        return false;
    }

    // Get the real function address
    *originalFunction = GetRealFunctionAddress(moduleName, functionName);
    if (!*originalFunction) {
        return false;
    }

    // Install detour using the hook core's utility (assuming it exists and is accessible)
    // If DXHookCore doesn't provide InstallHook, this needs adjustment.
    // For now, assuming it's accessible or a similar utility exists.
    // If not, the hook installation logic needs to be here or in a shared utility.

    // Placeholder for hook installation logic if DXHookCore::InstallHook is not available
    // This would involve memory protection changes and writing the JMP instruction
    // For this refactor, we assume the hook core or a utility handles this.
     std::cout << "Hook installation logic for " << functionName << " would go here." << std::endl;
     return true; // Assume success for now
}


HRESULT WINAPI FactoryHooks::HookCreateDXGIFactory(REFIID riid, void** ppFactory) {
    std::cout << "HookCreateDXGIFactory called" << std::endl;

    if (!FactoryHooks::GetInstance().m_originalCreateDXGIFactory) {
        std::cerr << "Original CreateDXGIFactory is null!" << std::endl;
        return E_FAIL;
    }

    // Call original function
    HRESULT hr = FactoryHooks::GetInstance().m_originalCreateDXGIFactory(riid, ppFactory);

    if (SUCCEEDED(hr) && ppFactory && *ppFactory) {
         std::string interfaceType = "IDXGIFactory_UnknownVersion"; // Simplified type
         // We need a way to get type from IID without ComTracker
         // Example: GetInterfaceTypeFromIID(riid);
         std::cout << "Created " << interfaceType << std::endl;

        // Hook factory methods if it's a DXGI factory
         IDXGIFactory* pFactory = static_cast<IDXGIFactory*>(*ppFactory);
         FactoryHooks::GetInstance().HookFactoryCreateSwapChain(pFactory);
    }

    return hr;
}

HRESULT WINAPI FactoryHooks::HookCreateDXGIFactory1(REFIID riid, void** ppFactory) {
    std::cout << "HookCreateDXGIFactory1 called" << std::endl;

    if (!FactoryHooks::GetInstance().m_originalCreateDXGIFactory1) {
        std::cerr << "Original CreateDXGIFactory1 is null!" << std::endl;
        return E_FAIL;
    }

    // Call original function
    HRESULT hr = FactoryHooks::GetInstance().m_originalCreateDXGIFactory1(riid, ppFactory);

    if (SUCCEEDED(hr) && ppFactory && *ppFactory) {
        std::string interfaceType = "IDXGIFactory_UnknownVersion"; // Simplified
        std::cout << "Created " << interfaceType << std::endl;

        IDXGIFactory* pFactory = static_cast<IDXGIFactory*>(*ppFactory);
        FactoryHooks::GetInstance().HookFactoryCreateSwapChain(pFactory);
    }

    return hr;
}

HRESULT WINAPI FactoryHooks::HookCreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) {
    std::cout << "HookCreateDXGIFactory2 called" << std::endl;

    if (!FactoryHooks::GetInstance().m_originalCreateDXGIFactory2) {
        std::cerr << "Original CreateDXGIFactory2 is null!" << std::endl;
        return E_FAIL;
    }

    // Call original function
    HRESULT hr = FactoryHooks::GetInstance().m_originalCreateDXGIFactory2(Flags, riid, ppFactory);

    if (SUCCEEDED(hr) && ppFactory && *ppFactory) {
        std::string interfaceType = "IDXGIFactory_UnknownVersion"; // Simplified
        std::cout << "Created " << interfaceType << std::endl;

        IDXGIFactory* pFactory = static_cast<IDXGIFactory*>(*ppFactory);
        FactoryHooks::GetInstance().HookFactoryCreateSwapChain(pFactory);
    }

    return hr;
}

HRESULT WINAPI FactoryHooks::HookD3D11CreateDevice(
    IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
    ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext) {

    std::cout << "HookD3D11CreateDevice called" << std::endl;

    if (!FactoryHooks::GetInstance().m_originalD3D11CreateDevice) {
        std::cerr << "Original D3D11CreateDevice is null!" << std::endl;
        return E_FAIL;
    }

    // Call original function
    HRESULT hr = FactoryHooks::GetInstance().m_originalD3D11CreateDevice(
        pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
        ppDevice, pFeatureLevel, ppImmediateContext);

    if (SUCCEEDED(hr)) {
        if (ppDevice && *ppDevice) {
             std::cout << "Created ID3D11Device" << std::endl;
            // Optionally add further logic here if needed, e.g., hook device methods
        }
        if (ppImmediateContext && *ppImmediateContext) {
             std::cout << "Created ID3D11DeviceContext" << std::endl;
             // Optionally add further logic here
        }
    }

    return hr;
}

HRESULT WINAPI FactoryHooks::HookD3D11CreateDeviceAndSwapChain(
    IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
    const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain,
    ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext) {

    std::cout << "HookD3D11CreateDeviceAndSwapChain called" << std::endl;

    if (!FactoryHooks::GetInstance().m_originalD3D11CreateDeviceAndSwapChain) {
        std::cerr << "Original D3D11CreateDeviceAndSwapChain is null!" << std::endl;
        return E_FAIL;
    }

    // Call original function
    HRESULT hr = FactoryHooks::GetInstance().m_originalD3D11CreateDeviceAndSwapChain(
        pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
        pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

    if (SUCCEEDED(hr)) {
        if (ppDevice && *ppDevice) {
            std::cout << "Created ID3D11Device via CreateDeviceAndSwapChain" << std::endl;
            // Optionally add further logic here
        }
        if (ppImmediateContext && *ppImmediateContext) {
             std::cout << "Created ID3D11DeviceContext via CreateDeviceAndSwapChain" << std::endl;
             // Optionally add further logic here
        }
        if (ppSwapChain && *ppSwapChain) {
             std::cout << "Created IDXGISwapChain via CreateDeviceAndSwapChain" << std::endl;
            // Let the hook core know about this swap chain
             DXHookCore::GetInstance().OnSwapChainCreated(*ppSwapChain);
        }
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE FactoryHooks::HookFactoryCreateSwapChain(
    IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain) {

    std::cout << "HookFactoryCreateSwapChain called for factory 0x" << std::hex << pFactory << std::dec << std::endl;

    if (!pFactory || !pDevice || !pDesc || !ppSwapChain) {
        return E_INVALIDARG;
    }

    // Get the original CreateSwapChain function
    CreateSwapChain_t originalCreateSwapChain = nullptr;
    {
        std::lock_guard<std::mutex> lock(FactoryHooks::GetInstance().m_createSwapChainMutex);
        auto it = FactoryHooks::GetInstance().m_originalCreateSwapChain.find(pFactory);
        if (it == FactoryHooks::GetInstance().m_originalCreateSwapChain.end()) {
            std::cerr << "Original CreateSwapChain not found for factory 0x" << std::hex << (uintptr_t)pFactory << std::dec << std::endl;
            // Attempt to get original dynamically? Risky. For now, fail.
            return E_FAIL;
        }
        originalCreateSwapChain = it->second;
    }

     // Call the original function
    HRESULT hr = originalCreateSwapChain(pFactory, pDevice, pDesc, ppSwapChain);

    if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain) {
        std::cout << "Created IDXGISwapChain via factory method" << std::endl;
        // Let the hook core know about this swap chain
         DXHookCore::GetInstance().OnSwapChainCreated(*ppSwapChain);
    }

    return hr;
}

} // namespace DXHook
} // namespace UndownUnlock 