#include "../../include/dx_hook_core.h"
#include "../../include/frame_extractor.h"
#include "../../include/shared_memory_transport.h"
#include "../../include/com_hooks/factory_hooks.h"
#include "../../include/hooks/com_interface_wrapper.h"
#include <iostream>

namespace UndownUnlock {
namespace DXHook {

// Initialize the singleton instance
DXHookCore* DXHookCore::s_instance = nullptr;

DXHookCore::DXHookCore()
    : m_initialized(false) {
}

DXHookCore::~DXHookCore() {
    Shutdown();
}

DXHookCore& DXHookCore::GetInstance() {
    if (!s_instance) {
        s_instance = new DXHookCore();
    }
    return *s_instance;
}

bool DXHookCore::Initialize() {
    // Avoid double initialization
    if (GetInstance().m_initialized) {
        return true;
    }
    
    DXHookCore& instance = GetInstance();
    
    try {
        std::cout << "Initializing DirectX Hook Core..." << std::endl;
        
        // Create the components
        instance.m_memoryScanner = std::make_unique<MemoryScanner>();
        instance.m_swapChainHook = std::make_unique<SwapChainHook>();
        instance.m_frameExtractor = std::make_unique<FrameExtractor>();
        instance.m_sharedMemory = std::make_unique<SharedMemoryTransport>("UndownUnlockFrameData");
        
        // Initialize the memory scanner
        if (!instance.m_memoryScanner->FindDXModules()) {
            std::cerr << "Failed to find DirectX modules" << std::endl;
            return false;
        }
        
        // Initialize the shared memory transport
        if (!instance.m_sharedMemory->Initialize()) {
            std::cerr << "Failed to initialize shared memory transport" << std::endl;
            return false;
        }
        
        // Set up callback for when a SwapChain is hooked
        instance.m_swapChainHook->SetPresentCallback([&instance](IDXGISwapChain* pSwapChain) {
            // Hook fired - extract a frame
            try {
                // Use RAII wrapper to safely get the device from the swap chain
                auto deviceWrapper = GetInterfaceChecked<ID3D11Device>(pSwapChain, __uuidof(ID3D11Device), "GetDevice");
                
                if (deviceWrapper) {
                    // Get the immediate context using RAII wrapper
                    ID3D11DeviceContext* context = nullptr;
                    HRESULT hr = deviceWrapper->GetImmediateContext(&context);
                    
                    if (SUCCEEDED(hr) && context) {
                        // Wrap the context for automatic cleanup
                        D3D11DeviceContextWrapper contextWrapper(context, true);
                        
                        // Initialize the frame extractor if not already
                        static bool extractorInitialized = false;
                        if (!extractorInitialized) {
                            instance.m_frameExtractor->Initialize(deviceWrapper.Get(), contextWrapper.Get());
                            instance.m_frameExtractor->SetSharedMemoryTransport(instance.m_sharedMemory.get());
                            extractorInitialized = true;
                        }
                        
                        // Extract the frame
                        instance.m_frameExtractor->ExtractFrame(pSwapChain);
                        
                        // RAII wrappers automatically release interfaces when they go out of scope
                    } else {
                        std::cerr << "Failed to get immediate context from device" << std::endl;
                    }
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Exception in Present callback: " << e.what() << std::endl;
            }
        });
        
        // Try to find and hook a SwapChain
        bool hookResult = instance.m_swapChainHook->FindAndHookSwapChain();
        if (!hookResult) {
            std::cout << "Initial SwapChain hook not found, waiting for application to create one..." << std::endl;
            // This is not a fatal error - we'll hook when the app creates a SwapChain
        }
        
        // Initialize the factory hooks for COM interface runtime detection
        bool factoryHookResult = FactoryHooks::GetInstance().Initialize();
        if (!factoryHookResult) {
            std::cerr << "Warning: Failed to initialize factory hooks" << std::endl;
            // Continue anyway, as we might still hook through other methods
        } else {
            std::cout << "COM Interface runtime detection initialized" << std::endl;
        }
        
        // Set flag indicating initialization succeeded
        instance.m_initialized = true;
        std::cout << "DirectX Hook Core initialized successfully" << std::endl;
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in DXHookCore::Initialize: " << e.what() << std::endl;
        return false;
    }
}

void DXHookCore::Shutdown() {
    if (!GetInstance().m_initialized) {
        return;
    }
    
    DXHookCore& instance = GetInstance();
    
    std::cout << "Shutting down DirectX Hook Core..." << std::endl;
    
    // Shut down factory hooks
    FactoryHooks::GetInstance().Shutdown();
    
    // Clear any callbacks
    instance.m_frameCallbacks.clear();
    
    // Release components in reverse order
    instance.m_sharedMemory.reset();
    instance.m_frameExtractor.reset();
    instance.m_swapChainHook.reset();
    instance.m_memoryScanner.reset();
    
    instance.m_initialized = false;
    
    std::cout << "DirectX Hook Core shutdown complete" << std::endl;
}

size_t DXHookCore::RegisterFrameCallback(std::function<void(const void*, size_t, uint32_t, uint32_t)> callback) {
    if (!callback) {
        return 0;
    }
    
    DXHookCore& instance = GetInstance();
    
    std::lock_guard<std::mutex> lock(instance.m_callbackMutex);
    instance.m_frameCallbacks.push_back(callback);
    
    // Return the index as a handle
    return instance.m_frameCallbacks.size() - 1;
}

void DXHookCore::UnregisterFrameCallback(size_t handle) {
    DXHookCore& instance = GetInstance();
    
    std::lock_guard<std::mutex> lock(instance.m_callbackMutex);
    if (handle < instance.m_frameCallbacks.size()) {
        // Replace with an empty function instead of resizing the vector
        // to avoid invalidating other handles
        instance.m_frameCallbacks[handle] = [](const void*, size_t, uint32_t, uint32_t) {};
    }
}

} // namespace DXHook
} // namespace UndownUnlock 