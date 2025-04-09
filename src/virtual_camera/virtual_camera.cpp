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
#include <fstream>
#include <vector>
#include <random>
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <Shlwapi.h>

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

// Main thread for video processing
std::thread s_videoProcessingThread;
std::atomic<bool> s_threadRunning(false);

// Random number generator for simulating camera imperfections
std::mt19937 s_randomGenerator(static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count()));

bool VirtualCameraSystem::Initialize(const VirtualCameraConfig& config) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (s_isInitialized) {
        std::cout << "Virtual camera system already initialized" << std::endl;
        return true;
    }
    
    std::cout << "Initializing virtual camera system..." << std::endl;
    
    // Store configuration
    s_config = config;
    
    // Initialize COM for the current thread
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM. Error: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Initialize Media Foundation
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize Media Foundation. Error: " << std::hex << hr << std::endl;
        CoUninitialize();
        return false;
    }
    
    // Check if kernel driver is available, and initialize it if needed
    if (!KernelDriver::IsInstalled()) {
        std::cout << "Kernel driver not installed, installing..." << std::endl;
        
        // Initialize the kernel driver with default configuration
        KernelDriverConfig driverConfig;
        driverConfig.bypassSecurityChecks = true;
        driverConfig.registerAsTrusted = true;
        
        if (!KernelDriver::Initialize(driverConfig)) {
            std::cerr << "Failed to initialize kernel driver" << std::endl;
            MFShutdown();
            CoUninitialize();
            return false;
        }
        
        if (!KernelDriver::Install()) {
            std::cerr << "Failed to install kernel driver" << std::endl;
            KernelDriver::Shutdown();
            MFShutdown();
            CoUninitialize();
            return false;
        }
    }
    
    if (!KernelDriver::IsRunning()) {
        std::cout << "Starting kernel driver..." << std::endl;
        if (!KernelDriver::Start()) {
            std::cerr << "Failed to start kernel driver" << std::endl;
            KernelDriver::Shutdown();
            MFShutdown();
            CoUninitialize();
            return false;
        }
    }
    
    // Set up device signature spoofing
    if (!KernelDriver::SpoofDeviceSignature()) {
        std::cerr << "Warning: Failed to spoof device signature" << std::endl;
        // Continue anyway
    }
    
    // Set video format
    PixelFormat pixelFormat = PixelFormat::MJPG; // Default to MJPG
    if (!KernelDriver::SetVideoFormat(s_config.width, s_config.height, pixelFormat, s_config.frameRate)) {
        std::cerr << "Warning: Failed to set video format" << std::endl;
        // Continue anyway
    }
    
    // Initialize DirectShow integration
    if (!InitializeDirectShow()) {
        std::cerr << "Warning: Failed to initialize DirectShow integration" << std::endl;
        // Continue anyway, as we can still use kernel-mode driver directly
    }
    
    // Initialize Media Foundation integration
    if (!InitializeMediaFoundation()) {
        std::cerr << "Warning: Failed to initialize Media Foundation integration" << std::endl;
        // Continue anyway, as we can still use kernel-mode driver directly
    }
    
    // Initialize facial tracking if enabled
    if (s_config.enableFacialTracking) {
        if (!FacialTracker::Initialize()) {
            std::cerr << "Warning: Failed to initialize facial tracking" << std::endl;
            s_config.enableFacialTracking = false;
        }
    }
    
    // If a video file is specified as the source, load it
    if (!s_config.cameraPath.empty()) {
        if (!SetVideoFileSource(s_config.cameraPath)) {
            std::cerr << "Warning: Failed to set video file source from " << 
                std::string(s_config.cameraPath.begin(), s_config.cameraPath.end()) << std::endl;
            // Continue anyway
        }
    }
    
    // Start video processing thread
    s_threadRunning = true;
    s_videoProcessingThread = std::thread(VideoProcessingThread);
    
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
    
    // Stop the video processing thread
    s_threadRunning = false;
    if (s_videoProcessingThread.joinable()) {
        s_videoProcessingThread.join();
    }
    
    // Shutdown DirectShow integration
    if (s_pMediaControl) {
        s_pMediaControl->Stop();
        s_pMediaControl->Release();
        s_pMediaControl = nullptr;
    }
    
    if (s_pVirtualCamFilter) {
        s_pVirtualCamFilter->Release();
        s_pVirtualCamFilter = nullptr;
    }
    
    if (s_pGraphBuilder) {
        s_pGraphBuilder->Release();
        s_pGraphBuilder = nullptr;
    }
    
    // Shutdown Media Foundation integration
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
    
    // Shutdown facial tracking if it was enabled
    if (s_config.enableFacialTracking) {
        FacialTracker::Shutdown();
    }
    
    // Shutdown Media Foundation
    MFShutdown();
    
    // Shutdown COM
    CoUninitialize();
    
    s_isInitialized = false;
    s_isActive = false;
    
    std::cout << "Virtual camera system shut down successfully" << std::endl;
}

bool VirtualCameraSystem::InstallDriver() {
    // This is just a wrapper around the KernelDriver implementation
    return KernelDriver::Install();
}

bool VirtualCameraSystem::UninstallDriver() {
    // This is just a wrapper around the KernelDriver implementation
    return KernelDriver::Uninstall();
}

bool VirtualCameraSystem::IsDriverInstalled() {
    // This is just a wrapper around the KernelDriver implementation
    return KernelDriver::IsInstalled();
}

bool VirtualCameraSystem::SetFrameSource(const BYTE* buffer, size_t size, int width, int height, int stride) {
    if (!s_isInitialized || !buffer || size == 0) {
        return false;
    }
    
    // If no frame has been set or dimensions have changed, update our tracking info
    static int lastWidth = 0;
    static int lastHeight = 0;
    
    if (width != lastWidth || height != lastHeight) {
        // Set the video format in the kernel driver
        PixelFormat pixelFormat = PixelFormat::RGB24; // Assume RGB24 for simplicity
        if (!KernelDriver::SetVideoFormat(width, height, pixelFormat, s_config.frameRate)) {
            std::cerr << "Warning: Failed to update video format" << std::endl;
            // Continue anyway
        }
        
        lastWidth = width;
        lastHeight = height;
    }
    
    // Make a copy of the buffer that we can manipulate
    std::vector<BYTE> frameData(buffer, buffer + size);
    
    // Apply camera imperfections if enabled
    if (s_config.simulateImperfections) {
        SimulateCameraImperfections(frameData.data(), width, height, stride);
    }
    
    // Apply lighting variations if enabled
    if (s_config.enableLightingVariations) {
        ApplyLightingVariations(frameData.data(), width, height, stride);
    }
    
    // Apply facial tracking if enabled
    if (s_config.enableFacialTracking) {
        ApplyFacialTracking(frameData.data(), width, height, stride);
    }
    
    // Send the frame to the kernel driver
    return KernelDriver::SetFrame(frameData.data(), frameData.size());
}

bool VirtualCameraSystem::SetVideoFileSource(const std::wstring& filePath) {
    if (!s_isInitialized) {
        return false;
    }
    
    std::cout << "Setting video file source: " << std::string(filePath.begin(), filePath.end()) << std::endl;
    
    // Update config
    s_config.cameraPath = filePath;
    
    // Check if the file exists
    if (!PathFileExists(filePath.c_str())) {
        std::cerr << "Video file does not exist: " << std::string(filePath.begin(), filePath.end()) << std::endl;
        return false;
    }
    
    // Initialize Media Foundation if not done already
    HRESULT hr = S_OK;
    
    // Clean up any existing source reader
    if (s_pSourceReader) {
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
    }
    
    // Create a source reader for the video file
    hr = MFCreateSourceReaderFromURL(filePath.c_str(), nullptr, &s_pSourceReader);
    if (FAILED(hr)) {
        std::cerr << "Failed to create source reader for video file. Error: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Configure the source reader to output RGB32 video
    hr = s_pSourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (FAILED(hr)) {
        std::cerr << "Failed to deselect all streams. Error: " << std::hex << hr << std::endl;
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
        return false;
    }
    
    hr = s_pSourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    if (FAILED(hr)) {
        std::cerr << "Failed to select video stream. Error: " << std::hex << hr << std::endl;
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
        return false;
    }
    
    // Set the output media type
    IMFMediaType* pMediaType = nullptr;
    hr = MFCreateMediaType(&pMediaType);
    if (FAILED(hr)) {
        std::cerr << "Failed to create media type. Error: " << std::hex << hr << std::endl;
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
        return false;
    }
    
    hr = pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) {
        std::cerr << "Failed to set major type. Error: " << std::hex << hr << std::endl;
        pMediaType->Release();
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
        return false;
    }
    
    hr = pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    if (FAILED(hr)) {
        std::cerr << "Failed to set subtype. Error: " << std::hex << hr << std::endl;
        pMediaType->Release();
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
        return false;
    }
    
    hr = s_pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pMediaType);
    if (FAILED(hr)) {
        std::cerr << "Failed to set current media type. Error: " << std::hex << hr << std::endl;
        pMediaType->Release();
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
        return false;
    }
    
    pMediaType->Release();
    
    // Get the video dimensions
    IMFMediaType* pCurrentType = nullptr;
    hr = s_pSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
    if (FAILED(hr)) {
        std::cerr << "Failed to get current media type. Error: " << std::hex << hr << std::endl;
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
        return false;
    }
    
    UINT32 width = 0, height = 0;
    hr = MFGetAttributeSize(pCurrentType, MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr)) {
        std::cerr << "Failed to get frame size. Error: " << std::hex << hr << std::endl;
        pCurrentType->Release();
        s_pSourceReader->Release();
        s_pSourceReader = nullptr;
        return false;
    }
    
    pCurrentType->Release();
    
    // Update the config with the video dimensions
    s_config.width = width;
    s_config.height = height;
    
    std::cout << "Video file source set successfully: " << width << "x" << height << std::endl;
    return true;
}

bool VirtualCameraSystem::EnableFacialTracking(bool enable) {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    if (!s_isInitialized) {
        return false;
    }
    
    // If enabling facial tracking and it wasn't previously enabled, initialize it
    if (enable && !s_config.enableFacialTracking) {
        if (!FacialTracker::Initialize()) {
            std::cerr << "Failed to initialize facial tracking" << std::endl;
            return false;
        }
    }
    // If disabling facial tracking and it was previously enabled, shut it down
    else if (!enable && s_config.enableFacialTracking) {
        FacialTracker::Shutdown();
    }
    
    s_config.enableFacialTracking = enable;
    return true;
}

bool VirtualCameraSystem::IsActive() {
    return s_isActive;
}

bool VirtualCameraSystem::SpoofDeviceSignature() {
    return KernelDriver::SpoofDeviceSignature();
}

bool VirtualCameraSystem::InitializeDirectShow() {
    // This would initialize DirectShow integration
    // For brevity, this is left as a stub
    return true;
}

bool VirtualCameraSystem::InitializeMediaFoundation() {
    // Media Foundation initialization is already done in the Initialize method
    return true;
}

bool VirtualCameraSystem::CreateVirtualCameraFilters() {
    // This would create the DirectShow filters
    // For brevity, this is left as a stub
    return true;
}

bool VirtualCameraSystem::RegisterVirtualCameraWithSystem() {
    // This would register the virtual camera with the system
    // For brevity, this is left as a stub
    return true;
}

bool VirtualCameraSystem::ApplyDeviceSignatureSpoofing() {
    // This is just a wrapper around the KernelDriver implementation
    return KernelDriver::SpoofDeviceSignature();
}

bool VirtualCameraSystem::SimulateCameraImperfections(BYTE* buffer, int width, int height, int stride) {
    if (!buffer) {
        return false;
    }
    
    // Simulate camera noise - randomly adjust pixel values
    std::uniform_int_distribution<int> noiseDist(-5, 5);
    
    // Only process a subset of pixels for performance (every 4th pixel)
    for (int y = 0; y < height; y += 2) {
        for (int x = 0; x < width; x += 2) {
            int index = y * stride + x * 3;
            
            // Make sure we're within bounds
            if (index + 2 >= width * height * 3) {
                break;
            }
            
            // Add slight noise to each color channel
            int noise = noiseDist(s_randomGenerator);
            
            // Apply noise with clamping to valid range [0-255]
            buffer[index] = static_cast<BYTE>(std::clamp(static_cast<int>(buffer[index]) + noise, 0, 255));
            buffer[index + 1] = static_cast<BYTE>(std::clamp(static_cast<int>(buffer[index + 1]) + noise, 0, 255));
            buffer[index + 2] = static_cast<BYTE>(std::clamp(static_cast<int>(buffer[index + 2]) + noise, 0, 255));
        }
    }
    
    // Occasionally simulate a camera glitch (every ~200 frames)
    static int frameCount = 0;
    frameCount++;
    
    if (frameCount % 200 == 0) {
        // Simulate a horizontal glitch line
        std::uniform_int_distribution<int> lineDist(0, height - 1);
        int glitchLine = lineDist(s_randomGenerator);
        
        for (int x = 0; x < width; x++) {
            int index = glitchLine * stride + x * 3;
            
            // Make sure we're within bounds
            if (index + 2 >= width * height * 3) {
                break;
            }
            
            // Create a bright line
            buffer[index] = 255;     // R
            buffer[index + 1] = 255; // G
            buffer[index + 2] = 255; // B
        }
    }
    
    return true;
}

bool VirtualCameraSystem::ApplyLightingVariations(BYTE* buffer, int width, int height, int stride) {
    if (!buffer) {
        return false;
    }
    
    // Apply a slow pulsing effect to simulate lighting changes
    static float lightingPhase = 0.0f;
    lightingPhase += 0.01f;
    if (lightingPhase > 2.0f * 3.14159f) {
        lightingPhase -= 2.0f * 3.14159f;
    }
    
    // Calculate a lighting factor that varies between 0.9 and 1.1
    float lightingFactor = 1.0f + 0.1f * sin(lightingPhase);
    
    // Apply the lighting factor to every pixel
    for (int y = 0; y < height; y += 4) {  // Process every 4th line for performance
        for (int x = 0; x < width; x += 4) {  // Process every 4th pixel for performance
            int index = y * stride + x * 3;
            
            // Make sure we're within bounds
            if (index + 2 >= width * height * 3) {
                break;
            }
            
            // Apply lighting factor with clamping
            buffer[index] = static_cast<BYTE>(std::clamp(static_cast<int>(buffer[index] * lightingFactor), 0, 255));
            buffer[index + 1] = static_cast<BYTE>(std::clamp(static_cast<int>(buffer[index + 1] * lightingFactor), 0, 255));
            buffer[index + 2] = static_cast<BYTE>(std::clamp(static_cast<int>(buffer[index + 2] * lightingFactor), 0, 255));
        }
    }
    
    return true;
}

bool VirtualCameraSystem::ApplyFacialTracking(BYTE* buffer, int width, int height, int stride) {
    if (!buffer || !s_config.enableFacialTracking) {
        return false;
    }
    
    // Apply facial tracking if the tracker is initialized
    return FacialTracker::TrackFacialMovements(buffer, width, height, stride);
}

void VirtualCameraSystem::VideoProcessingThread() {
    // This thread handles processing video frames from the source
    std::cout << "Video processing thread started" << std::endl;
    
    // If we have a video file source, process frames from it
    bool useVideoFile = (s_pSourceReader != nullptr);
    
    // Frame counter
    int frameCount = 0;
    
    // Calculate frame delay in milliseconds
    int frameDelayMs = 1000 / s_config.frameRate;
    
    while (s_threadRunning) {
        // Process based on source type
        if (useVideoFile && s_pSourceReader) {
            // Read a frame from the video file
            DWORD streamIndex = 0;
            DWORD streamFlags = 0;
            LONGLONG timestamp = 0;
            IMFSample* pSample = nullptr;
            
            HRESULT hr = s_pSourceReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0,
                &streamIndex,
                &streamFlags,
                &timestamp,
                &pSample
            );
            
            if (SUCCEEDED(hr) && pSample) {
                // Get the buffer from the sample
                IMFMediaBuffer* pBuffer = nullptr;
                hr = pSample->GetBufferByIndex(0, &pBuffer);
                
                if (SUCCEEDED(hr)) {
                    // Lock the buffer
                    BYTE* pData = nullptr;
                    DWORD maxLength = 0;
                    DWORD currentLength = 0;
                    
                    hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
                    
                    if (SUCCEEDED(hr)) {
                        // Send the frame to the kernel driver
                        SetFrameSource(pData, currentLength, s_config.width, s_config.height, s_config.width * 4);
                        
                        // Unlock the buffer
                        pBuffer->Unlock();
                    }
                    
                    pBuffer->Release();
                }
                
                pSample->Release();
                
                // If we reached the end of the file, loop back to the beginning
                if (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
                    std::cout << "End of video file reached, restarting..." << std::endl;
                    
                    PROPVARIANT prop;
                    PropVariantInit(&prop);
                    prop.vt = VT_I8;
                    prop.hVal.QuadPart = 0;
                    
                    hr = s_pSourceReader->SetCurrentPosition(GUID_NULL, prop);
                    PropVariantClear(&prop);
                }
            }
        } else {
            // If no source is set, generate a test pattern
            int width = s_config.width;
            int height = s_config.height;
            
            // Create a simple test pattern (colored bars)
            std::vector<BYTE> testFrame(width * height * 3);
            
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    int index = (y * width + x) * 3;
                    
                    // Divide the width into 8 segments
                    int segment = (x * 8) / width;
                    
                    // Set color based on segment
                    switch (segment) {
                        case 0: // Red
                            testFrame[index] = 255;
                            testFrame[index + 1] = 0;
                            testFrame[index + 2] = 0;
                            break;
                        case 1: // Green
                            testFrame[index] = 0;
                            testFrame[index + 1] = 255;
                            testFrame[index + 2] = 0;
                            break;
                        case 2: // Blue
                            testFrame[index] = 0;
                            testFrame[index + 1] = 0;
                            testFrame[index + 2] = 255;
                            break;
                        case 3: // Yellow
                            testFrame[index] = 255;
                            testFrame[index + 1] = 255;
                            testFrame[index + 2] = 0;
                            break;
                        case 4: // Cyan
                            testFrame[index] = 0;
                            testFrame[index + 1] = 255;
                            testFrame[index + 2] = 255;
                            break;
                        case 5: // Magenta
                            testFrame[index] = 255;
                            testFrame[index + 1] = 0;
                            testFrame[index + 2] = 255;
                            break;
                        case 6: // White
                            testFrame[index] = 255;
                            testFrame[index + 1] = 255;
                            testFrame[index + 2] = 255;
                            break;
                        case 7: // Black
                            testFrame[index] = 0;
                            testFrame[index + 1] = 0;
                            testFrame[index + 2] = 0;
                            break;
                    }
                }
            }
            
            // Add a frame counter to the test pattern
            std::string frameCountStr = "Frame: " + std::to_string(frameCount);
            for (size_t i = 0; i < frameCountStr.length(); i++) {
                int x = 10 + i * 8;
                int y = 10;
                
                if (x < width - 8 && y < height - 8) {
                    // Draw a simple text character
                    for (int cy = 0; cy < 8; cy++) {
                        for (int cx = 0; cx < 8; cx++) {
                            int index = ((y + cy) * width + (x + cx)) * 3;
                            
                            // Make sure we're within bounds
                            if (index + 2 < testFrame.size()) {
                                // Set to white
                                testFrame[index] = 255;
                                testFrame[index + 1] = 255;
                                testFrame[index + 2] = 255;
                            }
                        }
                    }
                }
            }
            
            // Send the test frame to the kernel driver
            SetFrameSource(testFrame.data(), testFrame.size(), width, height, width * 3);
            
            // Increment the frame counter
            frameCount++;
        }
        
        // Sleep until the next frame
        std::this_thread::sleep_for(std::chrono::milliseconds(frameDelayMs));
    }
    
    std::cout << "Video processing thread stopped" << std::endl;
}

} // namespace VirtualCamera
} // namespace UndownUnlock 