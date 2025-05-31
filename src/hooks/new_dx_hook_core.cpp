#include "../../include/hooks/dx_hook_core.h"
#include <iostream>
#include <windows.h>

// --- DirectXHookManager Method Implementations ---

DirectXHookManager& DirectXHookManager::GetInstance() {
    static DirectXHookManager instance;
    return instance;
}

DirectXHookManager::DirectXHookManager()
    : activeSwapChain_(nullptr),
      dependenciesChecked_(false),
      presentHookInstalled_(false) { // Initialize new members
    std::cout << "[DirectXHookManager] Constructor." << std::endl;
}

DirectXHookManager::~DirectXHookManager() {
    std::cout << "[DirectXHookManager] Destructor." << std::endl;
    Shutdown();
}

void DirectXHookManager::Initialize() {
    if (dependenciesChecked_ && presentHookInstalled_) { // If already fully up
        std::cout << "[DirectXHookManager::Initialize] Already initialized and Present hook is active." << std::endl;
        return;
    }
    if (dependenciesChecked_ && !activeSwapChain_){ // Initialized but no swapchain found yet
        std::cout << "[DirectXHookManager::Initialize] Initialized, but still waiting for SwapChain..." << std::endl;
        return;
    }

    std::cout << "[DirectXHookManager::Initialize] Initializing..." << std::endl;

    // 1. Check if D3D11/DXGI are loaded in the process (only once)
    if (!dependenciesChecked_) {
        HMODULE hD3D11 = GetModuleHandleA("d3d11.dll");
        HMODULE hDXGI = GetModuleHandleA("dxgi.dll");

        if (!hD3D11 || !hDXGI) {
            std::cout << "[DirectXHookManager::Initialize] D3D11/DXGI DLLs not found. DirectX hooking cannot proceed." << std::endl;
            dependenciesChecked_ = true;
            return;
        }
        std::cout << "[DirectXHookManager::Initialize] D3D11.dll and DXGI.dll are loaded." << std::endl;
        dependenciesChecked_ = true;
    }

    // The D3D11CreateDeviceAndSwapChainHook (installed by WindowsApiHookManager)
    // will call StoreSwapChainPointer when a swap chain is created.
    // Initialize() here ensures dependencies are checked and logs readiness.

    // Verify that WindowsApiHookManager has installed the D3D11CreateDeviceAndSwapChainHook.
    // This is crucial for DirectXHookManager to receive swap chain pointers.
    auto* apiManager = &WindowsApiHookManager::GetInstance(); // Ensure it's initialized if called first by any chance.
    auto* createDeviceHook = apiManager->GetHook<D3D11CreateDeviceAndSwapChainHook>();

    if (createDeviceHook && createDeviceHook->IsInstalled()) {
        std::cout << "[DirectXHookManager::Initialize] D3D11CreateDeviceAndSwapChainHook is installed by WindowsApiHookManager." << std::endl;
    } else {
        std::cerr << "[DirectXHookManager::Initialize] CRITICAL: D3D11CreateDeviceAndSwapChainHook is NOT installed by WindowsApiHookManager. DirectXHookManager will not receive SwapChain pointers." << std::endl;
        // No point in proceeding if the capture mechanism isn't there.
        // However, dependenciesChecked_ should still be true if DLLs are present.
        return;
    }

    // If activeSwapChain_ is already populated by a previous call to StoreSwapChainPointer,
    // and the hook isn't installed, StoreSwapChainPointer would have tried to install it.
    // So, just log current state.
    if (activeSwapChain_ && swapChainHook_ && swapChainHook_->IsInstalled()){
         presentHookInstalled_ = true;
         std::cout << "[DirectXHookManager::Initialize] Present hook is active on SwapChain: " << activeSwapChain_ << std::endl;
    } else if (activeSwapChain_ && swapChainHook_ && !swapChainHook_->IsInstalled()){
        std::cerr << "[DirectXHookManager::Initialize] SwapChain is stored (" << activeSwapChain_ << ") but Present hook failed to install earlier. Check logs from StoreSwapChainPointer." << std::endl;
    }
    else {
        std::cout << "[DirectXHookManager::Initialize] Waiting for D3D11CreateDeviceAndSwapChain detour to provide a SwapChain pointer..." << std::endl;
    }
}

void DirectXHookManager::StoreSwapChainPointer(IDXGISwapChain* pSwapChain) {
    std::cout << "[DirectXHookManager::StoreSwapChainPointer] Called with pSwapChain: " << pSwapChain << std::endl;

    if (!pSwapChain) {
        std::cerr << "[DirectXHookManager::StoreSwapChainPointer] Received null SwapChain pointer." << std::endl;
        return;
    }

    if (activeSwapChain_ == pSwapChain && swapChainHook_ && swapChainHook_->IsInstalled()) {
        std::cout << "[DirectXHookManager::StoreSwapChainPointer] Same SwapChain instance received, Present hook already installed." << std::endl;
        presentHookInstalled_ = true; // Ensure flag is correct
        return;
    }

    // If there's an existing (different) swap chain hook, uninstall and release it.
    if (swapChainHook_ && swapChainHook_->IsInstalled()) {
        std::cout << "[DirectXHookManager::StoreSwapChainPointer] Uninstalling hook from old SwapChain: " << activeSwapChain_ << std::endl;
        swapChainHook_->Uninstall();
        presentHookInstalled_ = false;
    }
    swapChainHook_.reset(); // Important to reset before releasing activeSwapChain_ if it's the same object
                            // that pSwapChain might point to after a Release/AddRef cycle elsewhere.

    if (activeSwapChain_) {
        std::cout << "[DirectXHookManager::StoreSwapChainPointer] Releasing previously stored SwapChain: " << activeSwapChain_ << std::endl;
        activeSwapChain_->Release(); // Release our reference to the old one
        activeSwapChain_ = nullptr;
    }

    // Store and AddRef the new swap chain
    activeSwapChain_ = pSwapChain;
    activeSwapChain_->AddRef();
    std::cout << "[DirectXHookManager::StoreSwapChainPointer] Stored new SwapChain: " << activeSwapChain_
              << ". Ref count should be at least 2 (one from app, one from us)." << std::endl;

    // Create and install the hook for the new swap chain's Present method
    try {
        swapChainHook_ = std::make_unique<SwapChainHook>(activeSwapChain_);
        if (swapChainHook_->Install()) {
            std::cout << "[DirectXHookManager::StoreSwapChainPointer] SwapChainHook for Present() installed successfully on new SwapChain." << std::endl;
            presentHookInstalled_ = true;
        } else {
            std::cerr << "[DirectXHookManager::StoreSwapChainPointer] Failed to install SwapChainHook on new SwapChain." << std::endl;
            activeSwapChain_->Release(); // Release our ref if hook install failed
            activeSwapChain_ = nullptr;
            swapChainHook_.reset();
            presentHookInstalled_ = false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DirectXHookManager::StoreSwapChainPointer] Exception during SwapChainHook creation/installation: " << e.what() << std::endl;
        if (activeSwapChain_){ // activeSwapChain_ would have been set before the try block
             activeSwapChain_->Release();
             activeSwapChain_ = nullptr;
        }
        swapChainHook_.reset();
        presentHookInstalled_ = false;
    }
}


void DirectXHookManager::Shutdown() {
    std::cout << "[DirectXHookManager::Shutdown] Shutting down..." << std::endl;

    if (swapChainHook_) {
        if (swapChainHook_->IsInstalled()) {
            if (swapChainHook_->Uninstall()) {
                std::cout << "[DirectXHookManager::Shutdown] SwapChainHook uninstalled successfully." << std::endl;
            } else {
                std::cerr << "[DirectXHookManager::Shutdown] Failed to uninstall SwapChainHook." << std::endl;
            }
        }
        swapChainHook_.reset();
    }

    if (activeSwapChain_) {
        std::cout << "[DirectXHookManager::Shutdown] Releasing stored SwapChain: " << activeSwapChain_ << std::endl;
        activeSwapChain_->Release(); // Release our reference
        activeSwapChain_ = nullptr;
    }

    dependenciesChecked_ = false;
    presentHookInstalled_ = false;
    std::cout << "[DirectXHookManager::Shutdown] Shutdown complete." << std::endl;
}
