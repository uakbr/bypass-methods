#include "../../include/virtual_camera/virtual_camera.h"
#include "../../include/virtual_camera/directshow_filter.h"
#include "../../include/virtual_camera/kernel_driver.h"
#include "../../include/virtual_camera/facial_tracking.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <dshow.h>

#pragma comment(lib, "strmiids.lib")

namespace UndownUnlock {
namespace VirtualCamera {

// Initialize static variables
bool VirtualCameraSystem::s_isInitialized = false;
bool VirtualCameraSystem::s_isActive = false;
VirtualCameraConfig VirtualCameraSystem::s_config;
std::mutex VirtualCameraSystem::s_mutex;

// DirectShow interfaces
IBaseFilter* VirtualCameraSystem::s_pVirtualCamFilter = nullptr;
IMediaControl* VirtualCameraSystem::s_pMediaControl = nullptr;
IGraphBuilder* VirtualCameraSystem::s_pGraphBuilder = nullptr;

// Media Foundation interfaces
IMFMediaSource* VirtualCameraSystem::s_pMediaSource = nullptr;
IMFSourceReader* VirtualCameraSystem::s_pSourceReader = nullptr;
IMFSample* VirtualCameraSystem::s_pCurrentSample = nullptr;

bool VirtualCameraSystem::Initialize(const VirtualCameraConfig& config) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (s_isInitialized) {
        std::cout << "Virtual camera system already initialized" << std::endl;
        return true;
    }
    
    std::cout << "Initializing virtual camera system..." << std::endl;
    
    // Store configuration
    s_config = config;
    
    // Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM. Error: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Check if driver is installed, install if not
    if (!IsDriverInstalled()) {
        std::cout << "Virtual camera driver not installed. Installing..." << std::endl;
        if (!InstallDriver()) {
            std::cerr << "Failed to install virtual camera driver" << std::endl;
            CoUninitialize();
            return false;
        }
    }
    
    // Initialize DirectShow
    if (!InitializeDirectShow()) {
        std::cerr << "Failed to initialize DirectShow" << std::endl;
        CoUninitialize();
        return false;
    }
    
    // Initialize Media Foundation
    if (!InitializeMediaFoundation()) {
        std::cerr << "Failed to initialize Media Foundation" << std::endl;
        CoUninitialize();
        return false;
    }
    
    // If facial tracking is enabled, initialize it
    if (config.enableFacialTracking) {
        if (!FacialDetector::Initialize()) {
            std::cerr << "Warning: Failed to initialize facial tracking" << std::endl;
            // Continue without facial tracking
        }
    }
    
    // Initialize camera simulator for realistic effects
    if (config.simulateImperfections || config.enableLightingVariations) {
        if (!CameraSimulator::Initialize()) {
            std::cerr << "Warning: Failed to initialize camera simulator" << std::endl;
            // Continue without imperfections
        }
    }
    
    // Set up device signature spoofing
    if (!SpoofDeviceSignature()) {
        std::cerr << "Warning: Failed to spoof device signature" << std::endl;
        // Continue without spoofing
    }
    
    // If a video file source was specified, try to load it
    if (!config.cameraPath.empty()) {
        if (!SetVideoFileSource(config.cameraPath)) {
            std::cerr << "Warning: Failed to set video file source" << std::endl;
            // Continue without video source
        }
    }
    
    s_isInitialized = true;
    s_isActive = true;
    std::cout << "Virtual camera system initialized successfully" << std::endl;
    return true;
}

void VirtualCameraSystem::Shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (!s_isInitialized) {
        return;
    }
    
    std::cout << "Shutting down virtual camera system..." << std::endl;
    
    // Release Media Foundation resources
    if (s_pCurrentSample) {
        s_pCurrentSample->Release();
        s_pCurrentSample = nullptr;
    }
    
    if (s_pSourceReader) {
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
    }
    
    if (s_pMediaSource) {
        s_pMediaSource->Release();
        s_pMediaSource = nullptr;
    }
    
    // Release DirectShow resources
    if (s_pMediaControl) {
        s_pMediaControl->Stop();
        s_pMediaControl->Release();
        s_pMediaControl = nullptr;
    }
    
    if (s_pGraphBuilder) {
        s_pGraphBuilder->Release();
        s_pGraphBuilder = nullptr;
    }
    
    if (s_pVirtualCamFilter) {
        s_pVirtualCamFilter->Release();
        s_pVirtualCamFilter = nullptr;
    }
    
    // Shutdown facial tracking if it was initialized
    if (s_config.enableFacialTracking && FacialDetector::IsInitialized()) {
        FacialDetector::Shutdown();
    }
    
    // Shutdown camera simulator
    if (s_config.simulateImperfections || s_config.enableLightingVariations) {
        CameraSimulator::Shutdown();
    }
    
    s_isInitialized = false;
    s_isActive = false;
    
    // Uninitialize COM
    CoUninitialize();
    
    std::cout << "Virtual camera system shut down successfully" << std::endl;
}

bool VirtualCameraSystem::InstallDriver() {
    std::cout << "Installing virtual camera driver..." << std::endl;
    
    // First, check if we need to install the kernel mode driver
    if (!KernelDriver::IsInstalled()) {
        // Initialize the kernel driver manager
        KernelDriverConfig driverConfig;
        if (!KernelDriver::Initialize(driverConfig)) {
            std::cerr << "Failed to initialize kernel driver manager" << std::endl;
            return false;
        }
        
        // Try to bypass driver signing if needed
        if (driverConfig.bypassSecurityChecks) {
            if (!KernelDriver::BypassDriverSigning()) {
                std::cerr << "Warning: Failed to bypass driver signing. Driver installation may fail" << std::endl;
            }
        }
        
        // Install the driver
        if (!KernelDriver::Install()) {
            std::cerr << "Failed to install kernel driver" << std::endl;
            KernelDriver::Shutdown();
            return false;
        }
        
        // Start the driver service
        if (!KernelDriver::Start()) {
            std::cerr << "Failed to start kernel driver service" << std::endl;
            KernelDriver::Uninstall();
            KernelDriver::Shutdown();
            return false;
        }
    }
    
    // Next, register the DirectShow filter
    if (!DirectShowRegistration::IsFilterRegistered()) {
        if (!DirectShowRegistration::RegisterFilter()) {
            std::cerr << "Failed to register DirectShow filter" << std::endl;
            return false;
        }
    }
    
    std::cout << "Virtual camera driver installed successfully" << std::endl;
    return true;
}

bool VirtualCameraSystem::UninstallDriver() {
    std::cout << "Uninstalling virtual camera driver..." << std::endl;
    
    bool result = true;
    
    // Unregister the DirectShow filter
    if (DirectShowRegistration::IsFilterRegistered()) {
        if (!DirectShowRegistration::UnregisterFilter()) {
            std::cerr << "Failed to unregister DirectShow filter" << std::endl;
            result = false;
        }
    }
    
    // Stop and uninstall the kernel driver
    if (KernelDriver::IsRunning()) {
        if (!KernelDriver::Stop()) {
            std::cerr << "Failed to stop kernel driver service" << std::endl;
            result = false;
        }
    }
    
    if (KernelDriver::IsInstalled()) {
        if (!KernelDriver::Uninstall()) {
            std::cerr << "Failed to uninstall kernel driver" << std::endl;
            result = false;
        }
    }
    
    // Shut down the kernel driver manager
    KernelDriver::Shutdown();
    
    if (result) {
        std::cout << "Virtual camera driver uninstalled successfully" << std::endl;
    }
    
    return result;
}

bool VirtualCameraSystem::IsDriverInstalled() {
    return KernelDriver::IsInstalled() && DirectShowRegistration::IsFilterRegistered();
}

bool VirtualCameraSystem::SetFrameSource(const BYTE* buffer, size_t size, int width, int height, int stride) {
    if (!s_isInitialized || !s_isActive) {
        std::cerr << "Virtual camera system not initialized or not active" << std::endl;
        return false;
    }
    
    if (!buffer || size == 0) {
        std::cerr << "Invalid frame buffer" << std::endl;
        return false;
    }
    
    // Allocate memory for a copy of the frame
    std::unique_ptr<BYTE[]> frameCopy(new BYTE[size]);
    memcpy(frameCopy.get(), buffer, size);
    
    // Apply facial tracking if enabled
    if (s_config.enableFacialTracking && FacialDetector::IsInitialized()) {
        // Apply facial movements
        FacialDetector::DetectFeatures(frameCopy.get(), width, height, stride);
    }
    
    // Apply imperfections and lighting variations if enabled
    if (s_config.simulateImperfections) {
        CameraSimulator::ApplyImperfections(frameCopy.get(), width, height, stride);
    }
    
    if (s_config.enableLightingVariations) {
        CameraSimulator::ApplyLightingVariations(frameCopy.get(), width, height, stride);
    }
    
    // Send the frame to the virtual camera
    HRESULT hr = E_FAIL;
    
    // Try to use the DirectShow interface if available
    if (s_pVirtualCamFilter) {
        IUndownUnlockVirtualCamera* pVC = nullptr;
        hr = s_pVirtualCamFilter->QueryInterface(IID_IUndownUnlockVirtualCamera, (void**)&pVC);
        
        if (SUCCEEDED(hr) && pVC) {
            hr = pVC->UpdateFrame(frameCopy.get(), static_cast<LONG>(size));
            pVC->Release();
        }
    }
    
    // If DirectShow failed or unavailable, try to use the kernel driver
    if (FAILED(hr)) {
        if (!KernelDriver::SetFrame(frameCopy.get(), size)) {
            std::cerr << "Failed to send frame to virtual camera" << std::endl;
            return false;
        }
    }
    
    return true;
}

bool VirtualCameraSystem::SetVideoFileSource(const std::wstring& filePath) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (!s_isInitialized) {
        std::cerr << "Virtual camera system not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Setting video file source: " << std::string(filePath.begin(), filePath.end()) << std::endl;
    
    // Update config
    s_config.cameraPath = filePath;
    
    // Create Media Foundation source reader for the video file
    if (s_pSourceReader) {
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
    }
    
    // Create a source reader for the video file
    HRESULT hr = MFCreateSourceReaderFromURL(filePath.c_str(), nullptr, &s_pSourceReader);
    
    if (FAILED(hr)) {
        std::cerr << "Failed to create source reader for video file. Error: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Configure the source reader to output uncompressed video
    hr = s_pSourceReader->SetCurrentMediaType(
        (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        nullptr,
        nullptr); // Use native type
    
    if (FAILED(hr)) {
        std::cerr << "Failed to set media type for source reader. Error: " << std::hex << hr << std::endl;
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
        return false;
    }
    
    std::cout << "Video file source set successfully" << std::endl;
    return true;
}

bool VirtualCameraSystem::EnableFacialTracking(bool enable) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    s_config.enableFacialTracking = enable;
    
    if (enable) {
        // Initialize facial tracking if not already initialized
        if (!FacialDetector::IsInitialized()) {
            if (!FacialDetector::Initialize()) {
                std::cerr << "Failed to initialize facial tracking" << std::endl;
                s_config.enableFacialTracking = false;
                return false;
            }
        }
    } else {
        // Shutdown facial tracking if it was initialized
        if (FacialDetector::IsInitialized()) {
            FacialDetector::Shutdown();
        }
    }
    
    return true;
}

bool VirtualCameraSystem::IsActive() {
    return s_isActive;
}

bool VirtualCameraSystem::SpoofDeviceSignature() {
    std::cout << "Spoofing device signature..." << std::endl;
    
    // Generate a stable but unique GUID based on machine info
    GUID deviceGuid = CLSID_UndownUnlockVirtualCamera; // Use our base GUID for now
    
    // Create a device name that looks legitimate
    std::wstring deviceName = L"Microsoft® LifeCam HD-3000";
    std::wstring devicePath = L"\\\\?\\usb#vid_045e&pid_0810&mi_00#7&18d80ef9&0&0000#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\\global";
    
    // Set the device parameters in the kernel driver
    if (!KernelDriver::SetDeviceParams(deviceGuid, deviceName, devicePath)) {
        std::cerr << "Failed to set device parameters in kernel driver" << std::endl;
        return false;
    }
    
    // Also set in DirectShow filter if available
    if (s_pVirtualCamFilter) {
        IUndownUnlockVirtualCamera* pVC = nullptr;
        HRESULT hr = s_pVirtualCamFilter->QueryInterface(IID_IUndownUnlockVirtualCamera, (void**)&pVC);
        
        if (SUCCEEDED(hr) && pVC) {
            hr = pVC->SetSpoofingParams(deviceGuid, deviceName.c_str(), devicePath.c_str());
            pVC->Release();
            
            if (FAILED(hr)) {
                std::cerr << "Failed to set spoofing parameters in DirectShow filter. Error: " << std::hex << hr << std::endl;
                return false;
            }
        }
    }
    
    std::cout << "Device signature spoofed successfully" << std::endl;
    return true;
}

bool VirtualCameraSystem::InitializeDirectShow() {
    std::cout << "Initializing DirectShow..." << std::endl;
    
    // Create the Filter Graph Manager
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
                                 IID_IGraphBuilder, (void**)&s_pGraphBuilder);
    
    if (FAILED(hr)) {
        std::cerr << "Failed to create FilterGraph. Error: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Query for the media control interface
    hr = s_pGraphBuilder->QueryInterface(IID_IMediaControl, (void**)&s_pMediaControl);
    
    if (FAILED(hr)) {
        std::cerr << "Failed to get IMediaControl interface. Error: " << std::hex << hr << std::endl;
        s_pGraphBuilder->Release();
        s_pGraphBuilder = nullptr;
        return false;
    }
    
    // Create the virtual camera filter
    if (!CreateVirtualCameraFilters()) {
        std::cerr << "Failed to create virtual camera filters" << std::endl;
        s_pMediaControl->Release();
        s_pMediaControl = nullptr;
        s_pGraphBuilder->Release();
        s_pGraphBuilder = nullptr;
        return false;
    }
    
    std::cout << "DirectShow initialized successfully" << std::endl;
    return true;
}

bool VirtualCameraSystem::InitializeMediaFoundation() {
    std::cout << "Initializing Media Foundation..." << std::endl;
    
    // Initialize Media Foundation
    HRESULT hr = MFStartup(MF_VERSION);
    
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize Media Foundation. Error: " << std::hex << hr << std::endl;
        return false;
    }
    
    std::cout << "Media Foundation initialized successfully" << std::endl;
    return true;
}

bool VirtualCameraSystem::CreateVirtualCameraFilters() {
    std::cout << "Creating virtual camera filters..." << std::endl;
    
    // Create instance of our virtual camera filter
    HRESULT hr = CoCreateInstance(CLSID_UndownUnlockVirtualCamera, NULL, CLSCTX_INPROC_SERVER,
                                 IID_IBaseFilter, (void**)&s_pVirtualCamFilter);
    
    if (FAILED(hr)) {
        std::cerr << "Failed to create virtual camera filter. Error: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Set filter name
    IPropertyBag* pPropertyBag = nullptr;
    hr = s_pVirtualCamFilter->QueryInterface(IID_IPropertyBag, (void**)&pPropertyBag);
    
    if (SUCCEEDED(hr) && pPropertyBag) {
        VARIANT var;
        VariantInit(&var);
        var.vt = VT_BSTR;
        var.bstrVal = SysAllocString(s_config.cameraName.c_str());
        
        hr = pPropertyBag->Write(L"FriendlyName", &var);
        
        VariantClear(&var);
        pPropertyBag->Release();
    }
    
    // Configure the output format
    IUndownUnlockVirtualCamera* pVC = nullptr;
    hr = s_pVirtualCamFilter->QueryInterface(IID_IUndownUnlockVirtualCamera, (void**)&pVC);
    
    if (SUCCEEDED(hr) && pVC) {
        hr = pVC->SetOutputFormat(s_config.width, s_config.height, s_config.frameRate);
        pVC->Release();
        
        if (FAILED(hr)) {
            std::cerr << "Failed to set output format for virtual camera. Error: " << std::hex << hr << std::endl;
            s_pVirtualCamFilter->Release();
            s_pVirtualCamFilter = nullptr;
            return false;
        }
    }
    
    // Add the filter to the filter graph
    hr = s_pGraphBuilder->AddFilter(s_pVirtualCamFilter, s_config.cameraName.c_str());
    
    if (FAILED(hr)) {
        std::cerr << "Failed to add virtual camera filter to filter graph. Error: " << std::hex << hr << std::endl;
        s_pVirtualCamFilter->Release();
        s_pVirtualCamFilter = nullptr;
        return false;
    }
    
    // Start the filter graph
    hr = s_pMediaControl->Run();
    
    if (FAILED(hr)) {
        std::cerr << "Failed to start filter graph. Error: " << std::hex << hr << std::endl;
        s_pGraphBuilder->RemoveFilter(s_pVirtualCamFilter);
        s_pVirtualCamFilter->Release();
        s_pVirtualCamFilter = nullptr;
        return false;
    }
    
    std::cout << "Virtual camera filters created successfully" << std::endl;
    return true;
}

bool VirtualCameraSystem::ApplyDeviceSignatureSpoofing() {
    // This method's implementation is covered in SpoofDeviceSignature()
    return SpoofDeviceSignature();
}

bool VirtualCameraSystem::RegisterVirtualCameraWithSystem() {
    // This method's implementation is covered in InstallDriver()
    return true;
}

bool VirtualCameraSystem::SimulateCameraImperfections(BYTE* buffer, int width, int height, int stride) {
    return CameraSimulator::ApplyImperfections(buffer, width, height, stride);
}

bool VirtualCameraSystem::ApplyLightingVariations(BYTE* buffer, int width, int height, int stride) {
    return CameraSimulator::ApplyLightingVariations(buffer, width, height, stride);
}

bool VirtualCameraSystem::ApplyFacialTracking(BYTE* buffer, int width, int height, int stride) {
    // This is a stub implementation, the real one would use the FacialDetector class
    return FacialDetector::TrackFacialMovements(buffer, width, height, stride);
}

bool VirtualCameraSystem::InstallKernelModeDriver() {
    // This method's implementation is covered in InstallDriver()
    return true;
}

} // namespace VirtualCamera
} // namespace UndownUnlock 