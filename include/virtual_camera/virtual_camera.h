#pragma once

#include <Windows.h>
#include <dshow.h>
#include <strmif.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <string>
#include <memory>
#include <vector>
#include <mutex>

namespace UndownUnlock {
namespace VirtualCamera {

/**
 * @brief Configuration for the virtual camera
 */
struct VirtualCameraConfig {
    int width = 1280;                  // Default width
    int height = 720;                  // Default height
    int frameRate = 30;                // Default frame rate
    std::wstring cameraName = L"UndownUnlock Virtual Camera"; // Default camera name
    std::wstring cameraPath = L""; // Path to a source video file (optional)
    bool enableFacialTracking = false; // Whether to enable facial tracking
    bool simulateImperfections = true; // Whether to simulate camera imperfections
    bool enableLightingVariations = true; // Whether to enable lighting variations
};

/**
 * @brief Class for managing a virtual camera system
 */
class VirtualCameraSystem {
public:
    /**
     * @brief Initialize the virtual camera system
     * @param config Configuration for the virtual camera
     * @return True if successful
     */
    static bool Initialize(const VirtualCameraConfig& config = VirtualCameraConfig());

    /**
     * @brief Shutdown the virtual camera system
     */
    static void Shutdown();

    /**
     * @brief Install the virtual camera driver
     * @return True if successful
     */
    static bool InstallDriver();

    /**
     * @brief Uninstall the virtual camera driver
     * @return True if successful
     */
    static bool UninstallDriver();

    /**
     * @brief Check if the virtual camera driver is installed
     * @return True if installed
     */
    static bool IsDriverInstalled();

    /**
     * @brief Set the frame source for the virtual camera
     * @param buffer Pointer to the frame buffer
     * @param size Size of the frame buffer in bytes
     * @param width Width of the frame
     * @param height Height of the frame
     * @param stride Stride of the frame
     * @return True if successful
     */
    static bool SetFrameSource(const BYTE* buffer, size_t size, int width, int height, int stride);

    /**
     * @brief Set the video file source for the virtual camera
     * @param filePath Path to the video file
     * @return True if successful
     */
    static bool SetVideoFileSource(const std::wstring& filePath);

    /**
     * @brief Enable or disable facial tracking
     * @param enable True to enable, false to disable
     * @return True if successful
     */
    static bool EnableFacialTracking(bool enable);

    /**
     * @brief Get the current virtual camera status
     * @return True if active
     */
    static bool IsActive();

    /**
     * @brief Spoof device signature to make the virtual camera appear legitimate
     * @return True if successful
     */
    static bool SpoofDeviceSignature();

private:
    // Private implementation details
    static bool s_isInitialized;
    static bool s_isActive;
    static VirtualCameraConfig s_config;
    static std::mutex s_mutex;
    
    // DirectShow interfaces
    static IBaseFilter* s_pVirtualCamFilter;
    static IMediaControl* s_pMediaControl;
    static IGraphBuilder* s_pGraphBuilder;
    
    // Media Foundation interfaces
    static IMFMediaSource* s_pMediaSource;
    static IMFSourceReader* s_pSourceReader;
    static IMFSample* s_pCurrentSample;
    
    // Private methods
    static bool InstallKernelModeDriver();
    static bool InitializeDirectShow();
    static bool InitializeMediaFoundation();
    static bool CreateVirtualCameraFilters();
    static bool RegisterVirtualCameraWithSystem();
    static bool ApplyDeviceSignatureSpoofing();
    static bool SimulateCameraImperfections(BYTE* buffer, int width, int height, int stride);
    static bool ApplyLightingVariations(BYTE* buffer, int width, int height, int stride);
    static bool ApplyFacialTracking(BYTE* buffer, int width, int height, int stride);
};

/**
 * @brief Class for managing facial tracking
 */
class FacialTracker {
public:
    /**
     * @brief Initialize the facial tracker
     * @return True if successful
     */
    static bool Initialize();

    /**
     * @brief Shutdown the facial tracker
     */
    static void Shutdown();

    /**
     * @brief Track facial movements in the given frame
     * @param frame Pointer to the frame data
     * @param width Width of the frame
     * @param height Height of the frame
     * @param stride Stride of the frame
     * @return True if facial features were detected
     */
    static bool TrackFacialMovements(BYTE* frame, int width, int height, int stride);

    /**
     * @brief Apply facial tracking adjustments to the given frame
     * @param sourceFrame Source frame data
     * @param targetFrame Target frame data where to apply adjustments
     * @param width Width of the frame
     * @param height Height of the frame
     * @param stride Stride of the frame
     * @return True if adjustments were applied
     */
    static bool ApplyFacialAdjustments(const BYTE* sourceFrame, BYTE* targetFrame, 
                                       int width, int height, int stride);

private:
    // Private implementation details
    static bool s_isInitialized;
    static std::mutex s_mutex;
    
    // Tracking state
    static float s_headPosX;
    static float s_headPosY;
    static float s_headRotation;
    static bool s_faceDetected;
    
    // Private methods
    static bool DetectFacialFeatures(const BYTE* frame, int width, int height, int stride);
    static bool TrackHeadPosition(const BYTE* frame, int width, int height, int stride);
    static bool MatchMovementToVideoFrame(BYTE* frame, int width, int height, int stride);
};

} // namespace VirtualCamera
} // namespace UndownUnlock 