#pragma once

#include <Windows.h>
#include <dxgi.h>
#include <dxgi1_2.h> // Include necessary DXGI versions
#include <d3d11.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>

namespace UndownUnlock {
namespace DXHook {

// Forward declare VTableHook from dx_hook_core.h if it exists there
// or define it locally if needed. Assuming it's in dx_hook_core.h
class VTableHook;

/**
 * @brief Class for hooking DXGI Factory creation and tracking interfaces
 */
class FactoryHooks {
public:
    FactoryHooks();
    ~FactoryHooks();

    /**
     * @brief Initialize factory hooks
     * @return True if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown and remove factory hooks
     */
    void Shutdown();

    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static FactoryHooks& GetInstance();

    /**
     * @brief Hook IDXGIFactory::CreateSwapChain method
     * @param pFactory Pointer to IDXGIFactory interface
     * @return True if hook was successfully installed
     */
    bool HookFactoryCreateSwapChain(IDXGIFactory* pFactory);

private:
    // Private singleton instance
    static FactoryHooks* s_instance;

    // Original function pointers for factory creation
    typedef HRESULT (WINAPI *CreateDXGIFactory_t)(REFIID riid, void** ppFactory);
    typedef HRESULT (WINAPI *CreateDXGIFactory1_t)(REFIID riid, void** ppFactory);
    typedef HRESULT (WINAPI *CreateDXGIFactory2_t)(UINT Flags, REFIID riid, void** ppFactory);

    CreateDXGIFactory_t m_originalCreateDXGIFactory;
    CreateDXGIFactory1_t m_originalCreateDXGIFactory1;
    CreateDXGIFactory2_t m_originalCreateDXGIFactory2;

    // Original function pointers for device creation
    typedef HRESULT (WINAPI *D3D11CreateDevice_t)(
        IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, 
        const D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, 
        D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

    typedef HRESULT (WINAPI *D3D11CreateDeviceAndSwapChain_t)(
        IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
        const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*,
        IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

    D3D11CreateDevice_t m_originalD3D11CreateDevice;
    D3D11CreateDeviceAndSwapChain_t m_originalD3D11CreateDeviceAndSwapChain;

    // Hook functions
    static HRESULT WINAPI HookCreateDXGIFactory(REFIID riid, void** ppFactory);
    static HRESULT WINAPI HookCreateDXGIFactory1(REFIID riid, void** ppFactory);
    static HRESULT WINAPI HookCreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory);

    static HRESULT WINAPI HookD3D11CreateDevice(
        IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
        const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
        ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext);

    static HRESULT WINAPI HookD3D11CreateDeviceAndSwapChain(
        IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
        const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
        const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain,
        ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext);

    // Factory method hooks for CreateSwapChain
    typedef HRESULT (STDMETHODCALLTYPE *CreateSwapChain_t)(
        IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);

    std::mutex m_createSwapChainMutex; // Mutex for accessing the map
    std::unordered_map<IDXGIFactory*, CreateSwapChain_t> m_originalCreateSwapChain;

    static HRESULT STDMETHODCALLTYPE HookFactoryCreateSwapChain(
        IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain);

    // Helper functions
    void* GetRealFunctionAddress(const char* moduleName, const char* functionName);
    bool InstallHook(void** originalFunction, void* hookFunction, const char* moduleName, const char* functionName);
    // Removed TrackInterface, UntrackInterface, ValidateInterface
};

// Removed IDXGISwapChainHook example class as it used ComPtr
// It can be re-added using raw pointers if needed.

} // namespace DXHook
} // namespace UndownUnlock 