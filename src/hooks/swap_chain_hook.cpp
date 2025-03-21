#include "../../include/dx_hook_core.h"
#include <iostream>
#include <Windows.h>

namespace UndownUnlock {
namespace DXHook {

// Static instance for callback from static hook methods
SwapChainHook* SwapChainHook::s_instance = nullptr;

SwapChainHook::SwapChainHook()
    : m_originalPresent(nullptr)
    , m_hookedVTable(nullptr)
    , m_hooksInstalled(false) {
    
    // Store the instance for static method access
    s_instance = this;
}

SwapChainHook::~SwapChainHook() {
    // Remove hooks if still installed
    RemoveHooks();
    
    // Clear static instance
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void** VTableHook::GetVTable(void* interfacePtr) {
    if (!interfacePtr) return nullptr;
    
    // The first member of a COM interface is a pointer to its vtable
    return *reinterpret_cast<void***>(interfacePtr);
}

void* VTableHook::HookVTableEntry(void** vtable, int index, void* hookFunction) {
    if (!vtable || !hookFunction) return nullptr;
    
    // Store the original function
    void* originalFunction = vtable[index];
    
    // Change the memory protection to allow writing
    DWORD oldProtect;
    if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        std::cerr << "Failed to change memory protection for vtable entry" << std::endl;
        return nullptr;
    }
    
    // Install the hook
    vtable[index] = hookFunction;
    
    // Restore memory protection
    DWORD temp;
    VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &temp);
    
    // Return the original function
    return originalFunction;
}

bool SwapChainHook::InstallHooks(void* interfacePtr) {
    if (!interfacePtr || m_hooksInstalled) {
        return false;
    }
    
    // Get the vtable
    void** vtable = GetVTable(interfacePtr);
    if (!vtable) {
        std::cerr << "Failed to get SwapChain vtable" << std::endl;
        return false;
    }
    
    // Store the vtable for unhooking later
    m_hookedVTable = vtable;
    
    // Install the hook for Present (index 8 in IDXGISwapChain)
    m_originalPresent = reinterpret_cast<Present_t>(
        HookVTableEntry(vtable, 8, reinterpret_cast<void*>(&SwapChainHook::HookPresent))
    );
    
    if (!m_originalPresent) {
        std::cerr << "Failed to hook Present method" << std::endl;
        return false;
    }
    
    m_hooksInstalled = true;
    std::cout << "Successfully installed SwapChain hooks" << std::endl;
    
    return true;
}

void SwapChainHook::RemoveHooks() {
    if (!m_hooksInstalled || !m_hookedVTable) {
        return;
    }
    
    // Restore the original Present function
    if (m_originalPresent) {
        HookVTableEntry(m_hookedVTable, 8, reinterpret_cast<void*>(m_originalPresent));
    }
    
    m_originalPresent = nullptr;
    m_hookedVTable = nullptr;
    m_hooksInstalled = false;
    
    std::cout << "Removed SwapChain hooks" << std::endl;
}

bool SwapChainHook::FindAndHookSwapChain() {
    // This will attempt to create a D3D11 device and swap chain
    // to hook it directly rather than waiting for the application to create one
    
    // Create a temporary window for the swap chain
    HWND tempWindow = CreateWindowExA(
        0, "STATIC", "Temp Window", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, NULL, NULL, GetModuleHandleA(NULL), NULL
    );
    
    if (!tempWindow) {
        std::cerr << "Failed to create temporary window" << std::endl;
        return false;
    }
    
    bool result = false;
    
    try {
        // Set up the swap chain description
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferCount = 1;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferDesc.Width = 100;
        swapChainDesc.BufferDesc.Height = 100;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = tempWindow;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.Windowed = TRUE;
        
        // Create the device and swap chain
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        IDXGISwapChain* swapChain = nullptr;
        
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,                  // Default adapter
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,                  // No software renderer
            0,                       // Flags
            nullptr,                  // Feature levels
            0,                        // Number of feature levels
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &swapChain,
            &device,
            nullptr,                  // Feature level
            &context
        );
        
        if (SUCCEEDED(hr) && swapChain) {
            // Hook the swap chain
            result = InstallHooks(swapChain);
            
            // Clean up
            swapChain->Release();
            context->Release();
            device->Release();
        } else {
            std::cerr << "Failed to create D3D11 device and swap chain: " << std::hex << hr << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in FindAndHookSwapChain: " << e.what() << std::endl;
    }
    
    // Clean up the temporary window
    DestroyWindow(tempWindow);
    
    return result;
}

void SwapChainHook::SetPresentCallback(std::function<void(IDXGISwapChain*)> callback) {
    m_presentCallback = callback;
}

// Static hook function for Present
HRESULT STDMETHODCALLTYPE SwapChainHook::HookPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    // Make sure we have a valid instance
    if (!s_instance || !s_instance->m_originalPresent) {
        return E_FAIL;
    }
    
    // Call the callback if set
    if (s_instance->m_presentCallback) {
        s_instance->m_presentCallback(pSwapChain);
    }
    
    // Call the original Present function
    return s_instance->m_originalPresent(pSwapChain, SyncInterval, Flags);
}

} // namespace DXHook
} // namespace UndownUnlock 