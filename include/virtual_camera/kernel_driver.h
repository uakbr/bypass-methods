#pragma once

#include <Windows.h>
#include <string>
#include <memory>
#include <vector>

namespace UndownUnlock {
namespace VirtualCamera {

/**
 * @brief Configuration for the kernel-mode driver
 */
struct KernelDriverConfig {
    std::wstring driverName = L"UndownUnlockVCam";  // Driver name
    std::wstring driverDisplayName = L"UndownUnlock Virtual Camera Driver"; // Driver display name
    std::wstring driverPath = L""; // Path to the driver file (set at runtime)
    bool registerAsTrusted = true; // Whether to register as a trusted driver
    bool bypassSecurityChecks = true; // Whether to bypass security checks
    int deviceId = 1; // Device identifier
};

/**
 * @brief Class for managing kernel-mode driver operations
 */
class KernelDriver {
public:
    /**
     * @brief Initialize the kernel driver manager
     * @param config Configuration for the kernel driver
     * @return True if successful
     */
    static bool Initialize(const KernelDriverConfig& config = KernelDriverConfig());

    /**
     * @brief Shutdown the kernel driver manager
     */
    static void Shutdown();

    /**
     * @brief Install the kernel driver
     * @return True if successful
     */
    static bool Install();

    /**
     * @brief Uninstall the kernel driver
     * @return True if successful
     */
    static bool Uninstall();

    /**
     * @brief Start the kernel driver service
     * @return True if successful
     */
    static bool Start();

    /**
     * @brief Stop the kernel driver service
     * @return True if successful
     */
    static bool Stop();

    /**
     * @brief Check if the driver is installed
     * @return True if installed
     */
    static bool IsInstalled();

    /**
     * @brief Check if the driver is running
     * @return True if running
     */
    static bool IsRunning();

    /**
     * @brief Communicate with the driver to set a frame
     * @param buffer Pointer to the frame buffer
     * @param size Size of the frame buffer in bytes
     * @return True if successful
     */
    static bool SetFrame(const BYTE* buffer, size_t size);

    /**
     * @brief Communicate with the driver to set device parameters
     * @param deviceGuid Device GUID for identification
     * @param deviceName Device name
     * @param devicePath Device path
     * @return True if successful
     */
    static bool SetDeviceParams(const GUID& deviceGuid, const std::wstring& deviceName, const std::wstring& devicePath);

    /**
     * @brief Bypass driver signing requirements
     * @return True if successful
     */
    static bool BypassDriverSigning();

private:
    // Private implementation details
    static bool s_isInitialized;
    static KernelDriverConfig s_config;
    static SC_HANDLE s_hSCManager;
    static SC_HANDLE s_hService;
    static HANDLE s_hDevice;
    
    // Private methods
    static bool ExtractDriverFile();
    static bool RegisterDriverService();
    static bool SetDriverRegistryKeys();
    static bool OpenDeviceHandle();
    static bool SendIOControlToDriver(DWORD ioctl, const void* inBuffer, DWORD inBufferSize, 
                                      void* outBuffer, DWORD outBufferSize, DWORD* bytesReturned);
    static bool EnableTestSigning();
    static bool AddDriverToCertStore();
    static bool DisableIntegrityChecks();
};

// IOCTL codes for driver communication
#define IOCTL_SET_FRAME          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SET_DEVICE_PARAMS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_GET_STATUS         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS)

// Device parameters structure for communication with the driver
struct DeviceParams {
    GUID deviceGuid;
    WCHAR deviceName[256];
    WCHAR devicePath[256];
    DWORD flags;
};

} // namespace VirtualCamera
} // namespace UndownUnlock 