#include "../../include/com_hooks/factory_hooks.h"
#include "../../include/dx_hook_core.h"
#include "../../include/com_hooks/com_ptr.h"
#include "../../include/com_hooks/com_tracker.h"
#include <iostream>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <dxgi1_6.h>
#include <d3d12.h>

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
    
    // Store original CreateSwapChain function (index 10 in IDXGIFactory vtable)
    CreateSwapChain_t originalCreateSwapChain = reinterpret_cast<CreateSwapChain_t>(vtable[10]);
    
    // Already hooked?
    {
        std::lock_guard<std::mutex> lock(m_interfaceMutex);
        if (m_originalCreateSwapChain.find(pFactory) != m_originalCreateSwapChain.end()) {
            return true;
        }
        m_originalCreateSwapChain[pFactory] = originalCreateSwapChain;
    }
    
    // Install hook
    DWORD oldProtect;
    if (VirtualProtect(&vtable[10], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        vtable[10] = reinterpret_cast<void*>(HookFactoryCreateSwapChain);
        VirtualProtect(&vtable[10], sizeof(void*), oldProtect, &oldProtect);
        
        std::cout << "Hooked IDXGIFactory::CreateSwapChain" << std::endl;
        return true;
    }
    
    std::cerr << "Failed to hook IDXGIFactory::CreateSwapChain" << std::endl;
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
    
    // Install detour
    return DXHookCore::GetInstance().InstallHook(*originalFunction, hookFunction, functionName);
}

void FactoryHooks::TrackInterface(void* pInterface, const std::string& interfaceType) {
    if (!pInterface) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_interfaceMutex);
    
    // Already tracked?
    if (m_trackedInterfaces.find(pInterface) != m_trackedInterfaces.end()) {
        return;
    }
    
    // Create tracking record
    TrackedInterface interface;
    interface.pInterface = pInterface;
    interface.interfaceType = interfaceType;
    interface.refCount = 1;  // Assume it starts with 1
    interface.threadId = GetCurrentThreadId();
    
    // Add to tracking map
    m_trackedInterfaces[pInterface] = interface;
    
    // Track with COM tracker
    ComTracker::GetInstance().TrackInterface(pInterface, interfaceType);
}

void FactoryHooks::UntrackInterface(void* pInterface) {
    if (!pInterface) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_interfaceMutex);
    
    // Remove from tracking map
    m_trackedInterfaces.erase(pInterface);
    
    // Untrack from COM tracker
    ComTracker::GetInstance().UntrackInterface(pInterface);
}

bool FactoryHooks::ValidateInterface(IUnknown* pInterface, REFIID riid) {
    if (!pInterface) {
        return false;
    }
    
    // Try to query for the interface to make sure it's valid
    ComPtr<IUnknown> testInterface;
    HRESULT hr = pInterface->QueryInterface(riid, reinterpret_cast<void**>(testInterface.GetAddressOf()));
    
    // If QueryInterface failed, try to fallback to a compatible interface
    if (FAILED(hr)) {
        void* fallbackInterface = nullptr;
        if (ComTracker::GetInstance().GetFallbackInterface(pInterface, riid, &fallbackInterface)) {
            if (fallbackInterface) {
                // Release it since we don't need it, just testing
                static_cast<IUnknown*>(fallbackInterface)->Release();
                std::cout << "Validated interface using fallback" << std::endl;
                return true;
            }
        }
        
        // No fallback worked
        std::cerr << "Failed to validate interface, HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }
    
    // Check if this is a custom implementation
    bool isCustom = ComTracker::GetInstance().IsCustomImplementation(pInterface);
    if (isCustom) {
        std::cout << "Detected custom implementation of " 
                 << ComTracker::GetInstance().GetInterfaceTypeFromIID(riid) << std::endl;
    }
    
    return true;
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
        std::string interfaceType = ComTracker::GetInstance().GetInterfaceTypeFromIID(riid);
        std::cout << "Created " << interfaceType << std::endl;
        
        // Track the factory
        FactoryHooks::GetInstance().TrackInterface(*ppFactory, interfaceType);
        
        // Hook factory methods if it's a DXGI factory
        if (riid == __uuidof(IDXGIFactory) || riid == __uuidof(IDXGIFactory1) || 
            riid == __uuidof(IDXGIFactory2) || riid == __uuidof(IDXGIFactory3) ||
            riid == __uuidof(IDXGIFactory4) || riid == __uuidof(IDXGIFactory5) ||
            riid == __uuidof(IDXGIFactory6)) {
            FactoryHooks::GetInstance().HookFactoryCreateSwapChain(static_cast<IDXGIFactory*>(*ppFactory));
        }
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
        std::string interfaceType = ComTracker::GetInstance().GetInterfaceTypeFromIID(riid);
        std::cout << "Created " << interfaceType << std::endl;
        
        // Track the factory
        FactoryHooks::GetInstance().TrackInterface(*ppFactory, interfaceType);
        
        // Hook factory methods if it's a DXGI factory
        if (riid == __uuidof(IDXGIFactory) || riid == __uuidof(IDXGIFactory1) || 
            riid == __uuidof(IDXGIFactory2) || riid == __uuidof(IDXGIFactory3) ||
            riid == __uuidof(IDXGIFactory4) || riid == __uuidof(IDXGIFactory5) ||
            riid == __uuidof(IDXGIFactory6)) {
            FactoryHooks::GetInstance().HookFactoryCreateSwapChain(static_cast<IDXGIFactory*>(*ppFactory));
        }
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
        std::string interfaceType = ComTracker::GetInstance().GetInterfaceTypeFromIID(riid);
        std::cout << "Created " << interfaceType << std::endl;
        
        // Track the factory
        FactoryHooks::GetInstance().TrackInterface(*ppFactory, interfaceType);
        
        // Hook factory methods if it's a DXGI factory
        if (riid == __uuidof(IDXGIFactory) || riid == __uuidof(IDXGIFactory1) || 
            riid == __uuidof(IDXGIFactory2) || riid == __uuidof(IDXGIFactory3) ||
            riid == __uuidof(IDXGIFactory4) || riid == __uuidof(IDXGIFactory5) ||
            riid == __uuidof(IDXGIFactory6)) {
            FactoryHooks::GetInstance().HookFactoryCreateSwapChain(static_cast<IDXGIFactory*>(*ppFactory));
        }
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
        // Track created interfaces
        if (ppDevice && *ppDevice) {
            // Try to get the highest available device interface version
            ComPtr<ID3D11Device5> device5;
            if (SUCCEEDED((*ppDevice)->QueryInterface(__uuidof(ID3D11Device5), reinterpret_cast<void**>(device5.GetAddressOf())))) {
                FactoryHooks::GetInstance().TrackInterface(device5.Get(), "ID3D11Device5");
            }
            else {
                ComPtr<ID3D11Device4> device4;
                if (SUCCEEDED((*ppDevice)->QueryInterface(__uuidof(ID3D11Device4), reinterpret_cast<void**>(device4.GetAddressOf())))) {
                    FactoryHooks::GetInstance().TrackInterface(device4.Get(), "ID3D11Device4");
                }
                else {
                    ComPtr<ID3D11Device3> device3;
                    if (SUCCEEDED((*ppDevice)->QueryInterface(__uuidof(ID3D11Device3), reinterpret_cast<void**>(device3.GetAddressOf())))) {
                        FactoryHooks::GetInstance().TrackInterface(device3.Get(), "ID3D11Device3");
                    }
                    else {
                        ComPtr<ID3D11Device2> device2;
                        if (SUCCEEDED((*ppDevice)->QueryInterface(__uuidof(ID3D11Device2), reinterpret_cast<void**>(device2.GetAddressOf())))) {
                            FactoryHooks::GetInstance().TrackInterface(device2.Get(), "ID3D11Device2");
                        }
                        else {
                            ComPtr<ID3D11Device1> device1;
                            if (SUCCEEDED((*ppDevice)->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(device1.GetAddressOf())))) {
                                FactoryHooks::GetInstance().TrackInterface(device1.Get(), "ID3D11Device1");
                            }
                            else {
                                FactoryHooks::GetInstance().TrackInterface(*ppDevice, "ID3D11Device");
                            }
                        }
                    }
                }
            }
        }
        
        if (ppImmediateContext && *ppImmediateContext) {
            // Try to get the highest available context interface version
            ComPtr<ID3D11DeviceContext4> context4;
            if (SUCCEEDED((*ppImmediateContext)->QueryInterface(__uuidof(ID3D11DeviceContext4), reinterpret_cast<void**>(context4.GetAddressOf())))) {
                FactoryHooks::GetInstance().TrackInterface(context4.Get(), "ID3D11DeviceContext4");
            }
            else {
                ComPtr<ID3D11DeviceContext3> context3;
                if (SUCCEEDED((*ppImmediateContext)->QueryInterface(__uuidof(ID3D11DeviceContext3), reinterpret_cast<void**>(context3.GetAddressOf())))) {
                    FactoryHooks::GetInstance().TrackInterface(context3.Get(), "ID3D11DeviceContext3");
                }
                else {
                    ComPtr<ID3D11DeviceContext2> context2;
                    if (SUCCEEDED((*ppImmediateContext)->QueryInterface(__uuidof(ID3D11DeviceContext2), reinterpret_cast<void**>(context2.GetAddressOf())))) {
                        FactoryHooks::GetInstance().TrackInterface(context2.Get(), "ID3D11DeviceContext2");
                    }
                    else {
                        ComPtr<ID3D11DeviceContext1> context1;
                        if (SUCCEEDED((*ppImmediateContext)->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(context1.GetAddressOf())))) {
                            FactoryHooks::GetInstance().TrackInterface(context1.Get(), "ID3D11DeviceContext1");
                        }
                        else {
                            FactoryHooks::GetInstance().TrackInterface(*ppImmediateContext, "ID3D11DeviceContext");
                        }
                    }
                }
            }
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
        // Track created interfaces - same as in HookD3D11CreateDevice
        if (ppDevice && *ppDevice) {
            ComPtr<ID3D11Device5> device5;
            if (SUCCEEDED((*ppDevice)->QueryInterface(__uuidof(ID3D11Device5), reinterpret_cast<void**>(device5.GetAddressOf())))) {
                FactoryHooks::GetInstance().TrackInterface(device5.Get(), "ID3D11Device5");
            }
            else {
                // Try progressively lower versions...
                ComPtr<ID3D11Device1> device1;
                if (SUCCEEDED((*ppDevice)->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(device1.GetAddressOf())))) {
                    FactoryHooks::GetInstance().TrackInterface(device1.Get(), "ID3D11Device1");
                }
                else {
                    FactoryHooks::GetInstance().TrackInterface(*ppDevice, "ID3D11Device");
                }
            }
        }
        
        if (ppImmediateContext && *ppImmediateContext) {
            ComPtr<ID3D11DeviceContext4> context4;
            if (SUCCEEDED((*ppImmediateContext)->QueryInterface(__uuidof(ID3D11DeviceContext4), reinterpret_cast<void**>(context4.GetAddressOf())))) {
                FactoryHooks::GetInstance().TrackInterface(context4.Get(), "ID3D11DeviceContext4");
            }
            else {
                // Try progressively lower versions...
                ComPtr<ID3D11DeviceContext1> context1;
                if (SUCCEEDED((*ppImmediateContext)->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(context1.GetAddressOf())))) {
                    FactoryHooks::GetInstance().TrackInterface(context1.Get(), "ID3D11DeviceContext1");
                }
                else {
                    FactoryHooks::GetInstance().TrackInterface(*ppImmediateContext, "ID3D11DeviceContext");
                }
            }
        }
        
        // Also track the swap chain
        if (ppSwapChain && *ppSwapChain) {
            // Try to get the highest available swapchain interface version
            ComPtr<IDXGISwapChain4> swapChain4;
            if (SUCCEEDED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain4), reinterpret_cast<void**>(swapChain4.GetAddressOf())))) {
                FactoryHooks::GetInstance().TrackInterface(swapChain4.Get(), "IDXGISwapChain4");
            }
            else {
                ComPtr<IDXGISwapChain3> swapChain3;
                if (SUCCEEDED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(swapChain3.GetAddressOf())))) {
                    FactoryHooks::GetInstance().TrackInterface(swapChain3.Get(), "IDXGISwapChain3");
                }
                else {
                    ComPtr<IDXGISwapChain2> swapChain2;
                    if (SUCCEEDED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain2), reinterpret_cast<void**>(swapChain2.GetAddressOf())))) {
                        FactoryHooks::GetInstance().TrackInterface(swapChain2.Get(), "IDXGISwapChain2");
                    }
                    else {
                        ComPtr<IDXGISwapChain1> swapChain1;
                        if (SUCCEEDED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(swapChain1.GetAddressOf())))) {
                            FactoryHooks::GetInstance().TrackInterface(swapChain1.Get(), "IDXGISwapChain1");
                        }
                        else {
                            FactoryHooks::GetInstance().TrackInterface(*ppSwapChain, "IDXGISwapChain");
                        }
                    }
                }
            }
        }
    }
    
    return hr;
}

HRESULT STDMETHODCALLTYPE FactoryHooks::HookFactoryCreateSwapChain(
    IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain) {
    
    std::cout << "HookFactoryCreateSwapChain called" << std::endl;
    
    if (!pFactory || !pDevice || !pDesc || !ppSwapChain) {
        return E_INVALIDARG;
    }
    
    // Get the original CreateSwapChain function
    CreateSwapChain_t originalCreateSwapChain = nullptr;
    {
        std::lock_guard<std::mutex> lock(FactoryHooks::GetInstance().m_interfaceMutex);
        auto it = FactoryHooks::GetInstance().m_originalCreateSwapChain.find(pFactory);
        if (it == FactoryHooks::GetInstance().m_originalCreateSwapChain.end()) {
            std::cerr << "Original CreateSwapChain not found for factory 0x" << std::hex << (uintptr_t)pFactory << std::dec << std::endl;
            return E_FAIL;
        }
        originalCreateSwapChain = it->second;
    }
    
    // Validate that the device interface is a DirectX device
    if (!FactoryHooks::GetInstance().ValidateInterface(pDevice, __uuidof(ID3D11Device))) {
        // Not a D3D11 device, try D3D12
        if (!FactoryHooks::GetInstance().ValidateInterface(pDevice, __uuidof(ID3D12Device))) {
            std::cerr << "Unknown device interface type" << std::endl;
            // Continue anyway, it might be a custom device implementation
        }
    }
    
    // Call the original function
    HRESULT hr = originalCreateSwapChain(pFactory, pDevice, pDesc, ppSwapChain);
    
    if (SUCCEEDED(hr) && ppSwapChain && *ppSwapChain) {
        // Try to detect which version of IDXGISwapChain we have
        ComPtr<IDXGISwapChain4> swapChain4;
        if (SUCCEEDED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain4), reinterpret_cast<void**>(swapChain4.GetAddressOf())))) {
            std::cout << "Detected IDXGISwapChain4" << std::endl;
            FactoryHooks::GetInstance().TrackInterface(swapChain4.Get(), "IDXGISwapChain4");
        }
        else {
            ComPtr<IDXGISwapChain3> swapChain3;
            if (SUCCEEDED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(swapChain3.GetAddressOf())))) {
                std::cout << "Detected IDXGISwapChain3" << std::endl;
                FactoryHooks::GetInstance().TrackInterface(swapChain3.Get(), "IDXGISwapChain3");
            }
            else {
                ComPtr<IDXGISwapChain2> swapChain2;
                if (SUCCEEDED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain2), reinterpret_cast<void**>(swapChain2.GetAddressOf())))) {
                    std::cout << "Detected IDXGISwapChain2" << std::endl;
                    FactoryHooks::GetInstance().TrackInterface(swapChain2.Get(), "IDXGISwapChain2");
                }
                else {
                    ComPtr<IDXGISwapChain1> swapChain1;
                    if (SUCCEEDED((*ppSwapChain)->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(swapChain1.GetAddressOf())))) {
                        std::cout << "Detected IDXGISwapChain1" << std::endl;
                        FactoryHooks::GetInstance().TrackInterface(swapChain1.Get(), "IDXGISwapChain1");
                    }
                    else {
                        std::cout << "Detected IDXGISwapChain" << std::endl;
                        FactoryHooks::GetInstance().TrackInterface(*ppSwapChain, "IDXGISwapChain");
                    }
                }
            }
        }
        
        // Let the hook core know about this swap chain
        DXHookCore::GetInstance().OnSwapChainCreated(*ppSwapChain);
    }
    
    return hr;
}

} // namespace DXHook
} // namespace UndownUnlock 