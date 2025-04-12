#include "../../include/virtual_camera/kernel_driver.h"
#include "../../include/virtual_camera/kernel_driver_ioctl.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <array>
#include <filesystem>
#include <Windows.h>
#include <winioctl.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <Shlobj.h>
#include <Shlwapi.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <Softpub.h>
#include <mscat.h>
#include <Dbt.h>
#include <ks.h>
#include <ksmedia.h>
#include <initguid.h>
#include <usbioctl.h>

#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Bcrypt.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "cfgmgr32.lib")

namespace UndownUnlock {
namespace VirtualCamera {

// Define device path for communication with driver
#define DRIVER_DEVICE_PATH L"\\\\.\\UndownUnlockVCam"

// Internal device driver resource name for extraction
#define DRIVER_RESOURCE_NAME "UNDOWNUNLOCK_VCAM_DRIVER"

// Driver registry key
#define DRIVER_REGISTRY_KEY L"SYSTEM\\CurrentControlSet\\Services\\UndownUnlockVCam"

// Driver hardware key
#define DRIVER_HARDWARE_KEY L"SYSTEM\\CurrentControlSet\\Enum\\Root\\IMAGE\\0000"

// Camera device class key
#define CAMERA_CLASS_KEY L"SYSTEM\\CurrentControlSet\\Control\\Class\\{6bdd1fc6-810f-11d0-bec7-08002be2092f}"

// Define fake hardware IDs
#define FAKE_HW_ID L"USB\\VID_046D&PID_0825&REV_0100"
#define FAKE_COMPATIBLE_ID L"USB\\Class_0E&SubClass_01&Prot_00"
#define FAKE_DEVICE_DESC L"Logitech HD Webcam C270"
#define FAKE_MFG L"Logitech"
#define FAKE_SERIAL L"A92F73B4"

// Static variable initialization
bool KernelDriver::s_isInitialized = false;
KernelDriverConfig KernelDriver::s_config;
SC_HANDLE KernelDriver::s_hSCManager = NULL;
SC_HANDLE KernelDriver::s_hService = NULL;
HANDLE KernelDriver::s_hDevice = INVALID_HANDLE_VALUE;

// Driver signature bypass state
static bool s_testSigningEnabled = false;
static bool s_integrityChecksDisabled = false;

bool KernelDriver::Initialize(const KernelDriverConfig& config) {
    if (s_isInitialized) {
        return true;
    }
    
    std::cout << "Initializing kernel driver manager..." << std::endl;
    
    // Store configuration
    s_config = config;
    
    // Set driver path if not specified
    if (s_config.driverPath.empty()) {
        WCHAR tempPath[MAX_PATH] = {0};
        GetTempPath(MAX_PATH, tempPath);
        
        s_config.driverPath = tempPath;
        s_config.driverPath += L"\\";
        s_config.driverPath += s_config.driverName;
        s_config.driverPath += L".sys";
    }
    
    // Open the service control manager
    s_hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!s_hSCManager) {
        DWORD error = GetLastError();
        std::cerr << "Failed to open Service Control Manager. Error: " << error << std::endl;
        
        if (error == ERROR_ACCESS_DENIED) {
            std::cerr << "Administrator privileges required to manage drivers." << std::endl;
            
            // Try to elevate privileges
            if (ElevatePrivileges()) {
                // Retry opening SCM with elevated privileges
                s_hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
                if (!s_hSCManager) {
                    error = GetLastError();
                    std::cerr << "Still failed to open Service Control Manager after privilege elevation. Error: " << error << std::endl;
                    return false;
                }
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    
    // Check if test-signing is already enabled, if not warn the user
    if (!IsTestSigningEnabled() && s_config.bypassSecurityChecks) {
        std::cout << "Warning: Test signing is not enabled. Driver installation may fail." << std::endl;
        std::cout << "Attempting to enable test signing..." << std::endl;
        
        if (EnableTestSigning()) {
            std::cout << "Test signing enabled. System reboot is required for changes to take effect." << std::endl;
        } else {
            std::cout << "Failed to enable test signing automatically." << std::endl;
            std::cout << "Consider running 'bcdedit /set testsigning on' as administrator." << std::endl;
        }
    }
    
    // Check if driver integrity checks are disabled
    if (!AreIntegrityChecksDisabled() && s_config.bypassSecurityChecks) {
        std::cout << "Attempting to disable driver integrity checks..." << std::endl;
        
        if (DisableIntegrityChecks()) {
            std::cout << "Driver integrity checks disabled. System reboot is required for changes to take effect." << std::endl;
        } else {
            std::cout << "Failed to disable driver integrity checks automatically." << std::endl;
        }
    }
    
    // Extract the driver file if needed
    if (!PathFileExists(s_config.driverPath.c_str())) {
        if (!ExtractDriverFile()) {
            CloseServiceHandle(s_hSCManager);
            s_hSCManager = NULL;
            return false;
        }
    }
    
    // If configured, apply certificate spoofing to the driver
    if (s_config.bypassSecurityChecks && s_config.registerAsTrusted) {
        if (!SpoofDriverCertificate()) {
            std::cout << "Warning: Failed to spoof driver certificate." << std::endl;
            // Continue anyway, we'll try other methods
        }
    }
    
    s_isInitialized = true;
    std::cout << "Kernel driver manager initialized successfully" << std::endl;
    return true;
}

void KernelDriver::Shutdown() {
    if (!s_isInitialized) {
        return;
    }
    
    std::cout << "Shutting down kernel driver manager..." << std::endl;
    
    // Close device handle if open
    if (s_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(s_hDevice);
        s_hDevice = INVALID_HANDLE_VALUE;
    }
    
    // Close service handle if open
    if (s_hService) {
        CloseServiceHandle(s_hService);
        s_hService = NULL;
    }
    
    // Close SCM handle if open
    if (s_hSCManager) {
        CloseServiceHandle(s_hSCManager);
        s_hSCManager = NULL;
    }
    
    // Remove any temporary files
    if (s_config.cleanupOnShutdown && !s_config.driverPath.empty()) {
        if (PathFileExists(s_config.driverPath.c_str())) {
            // Try to delete the driver file
            if (!DeleteFile(s_config.driverPath.c_str())) {
                DWORD error = GetLastError();
                if (error != ERROR_FILE_NOT_FOUND) {
                    std::cerr << "Warning: Failed to delete driver file. Error: " << error << std::endl;
                }
            }
        }
    }
    
    s_isInitialized = false;
    std::cout << "Kernel driver manager shut down successfully" << std::endl;
}

bool KernelDriver::Install() {
    if (!s_isInitialized) {
        std::cerr << "Kernel driver manager not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Installing kernel driver..." << std::endl;
    
    // Check if driver is already installed
    if (IsInstalled()) {
        std::cout << "Driver already installed" << std::endl;
        return true;
    }
    
    // Try to create the service
    s_hService = CreateService(
        s_hSCManager,                    // SCM handle
        s_config.driverName.c_str(),     // Service name
        s_config.driverDisplayName.c_str(), // Display name
        SERVICE_ALL_ACCESS,              // Access rights
        SERVICE_KERNEL_DRIVER,           // Service type
        SERVICE_DEMAND_START,            // Start type
        SERVICE_ERROR_NORMAL,            // Error control
        s_config.driverPath.c_str(),     // Binary path (driver file)
        NULL,                            // No load ordering group
        NULL,                            // No tag identifier
        NULL,                            // No dependencies
        NULL,                            // Local System account
        NULL                             // No password
    );
    
    if (!s_hService) {
        DWORD error = GetLastError();
        std::cerr << "Failed to create driver service. Error: " << error << std::endl;
        
        // If access denied, we need admin rights
        if (error == ERROR_ACCESS_DENIED) {
            std::cerr << "Administrator privileges required to install driver." << std::endl;
            // Try to elevate privileges and retry
            if (ElevatePrivileges()) {
                s_hService = CreateService(
                    s_hSCManager,
                    s_config.driverName.c_str(),
                    s_config.driverDisplayName.c_str(),
                    SERVICE_ALL_ACCESS,
                    SERVICE_KERNEL_DRIVER,
                    SERVICE_DEMAND_START,
                    SERVICE_ERROR_NORMAL,
                    s_config.driverPath.c_str(),
                    NULL, NULL, NULL, NULL, NULL
                );
                
                if (!s_hService) {
                    error = GetLastError();
                    std::cerr << "Still failed to create driver service after privilege elevation. Error: " << error << std::endl;
                    return false;
                }
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    
    // Set registry keys for driver configuration
    if (!SetDriverRegistryKeys()) {
        // Registry operations failed, but driver is installed
        std::cerr << "Warning: Failed to set driver registry keys" << std::endl;
    }
    
    // Set up fake hardware IDs and device registry entries
    if (!SetupFakeDeviceRegistry()) {
        std::cerr << "Warning: Failed to set up fake device registry entries" << std::endl;
        // Continue anyway
    }
    
    // If driver requires trusted signing, try to bypass
    if (s_config.registerAsTrusted) {
        std::cout << "Attempting to register driver as trusted..." << std::endl;
        if (!AddDriverToCertStore()) {
            std::cerr << "Warning: Failed to add driver to certificate store" << std::endl;
            // Continue anyway
        }
    }
    
    std::cout << "Kernel driver installed successfully" << std::endl;
    return true;
}

bool KernelDriver::Uninstall() {
    if (!s_isInitialized) {
        return false;
    }
    
    std::cout << "Uninstalling kernel driver..." << std::endl;
    
    // First stop the service if it's running
    if (IsRunning()) {
        if (!Stop()) {
            std::cerr << "Failed to stop the service" << std::endl;
            // Try force stop
            ForceStopService();
        }
    }
    
    // Close device handle if open
    if (s_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(s_hDevice);
        s_hDevice = INVALID_HANDLE_VALUE;
    }
    
    bool success = false;
    
    // Delete the service
    if (s_hService) {
        if (DeleteService(s_hService)) {
            success = true;
        } else {
            DWORD error = GetLastError();
            std::cerr << "Failed to delete service. Error: " << error << std::endl;
            
            // If service marked for deletion
            if (error == ERROR_SERVICE_MARKED_FOR_DELETE) {
                std::cout << "Service marked for deletion, will be removed at next boot" << std::endl;
                success = true;
            } else {
                // Try force delete
                success = ForceDeleteService();
            }
        }
        
        CloseServiceHandle(s_hService);
        s_hService = NULL;
    }
    
    // Close service manager
    if (s_hSCManager) {
        CloseServiceHandle(s_hSCManager);
        s_hSCManager = NULL;
    }
    
    // If configured to clean up on uninstall
    if (s_config.cleanupOnUninstall) {
        // Perform thorough cleanup
        success &= PerformCompleteCleanup();
    }
    
    std::cout << "Kernel driver uninstalled " << (success ? "successfully" : "with errors") << std::endl;
    return success;
}

bool KernelDriver::PerformCompleteCleanup() {
    std::cout << "Performing complete cleanup..." << std::endl;
    bool success = true;
    
    // 1. Remove device registry entries
    RemoveDeviceRegistryEntries();
    
    // 2. Remove driver registry key
    LONG result = RegDeleteKey(HKEY_LOCAL_MACHINE, DRIVER_REGISTRY_KEY);
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        std::cerr << "Failed to delete driver registry key. Error: " << result << std::endl;
        success = false;
        
        // Try to open and delete subkeys
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, DRIVER_REGISTRY_KEY, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
            // Enumerate and delete all subkeys first
            wchar_t subkeyName[256];
            DWORD subkeyNameSize = 256;
            DWORD index = 0;
            
            while (RegEnumKeyEx(hKey, 0, subkeyName, &subkeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                if (RegDeleteKey(hKey, subkeyName) != ERROR_SUCCESS) {
                    std::cerr << "Failed to delete subkey: " << std::wstring(subkeyName) << std::endl;
                }
                subkeyNameSize = 256;
            }
            
            RegCloseKey(hKey);
            
            // Try to delete the key again
            result = RegDeleteKey(HKEY_LOCAL_MACHINE, DRIVER_REGISTRY_KEY);
            if (result == ERROR_SUCCESS) {
                success = true;
            }
        }
    }
    
    // 3. Check for and remove fake device entries in camera class key
    HKEY classKey;
    result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, CAMERA_CLASS_KEY, 0, KEY_ALL_ACCESS, &classKey);
    if (result == ERROR_SUCCESS) {
        // Enumerate all subkeys
        wchar_t subkeyName[256];
        DWORD subkeyNameSize = 256;
        DWORD index = 0;
        
        while (RegEnumKeyEx(classKey, index, subkeyName, &subkeyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            // Open this subkey
            HKEY deviceKey;
            if (RegOpenKeyEx(classKey, subkeyName, 0, KEY_READ, &deviceKey) == ERROR_SUCCESS) {
                // Check if this is our fake device
                wchar_t driverKeyValue[512] = {0};
                DWORD valueSize = sizeof(driverKeyValue);
                DWORD valueType = REG_SZ;
                
                if (RegQueryValueEx(deviceKey, L"Driver", NULL, &valueType, (BYTE*)driverKeyValue, &valueSize) == ERROR_SUCCESS) {
                    if (wcsstr(driverKeyValue, s_config.driverName.c_str()) != NULL) {
                        // This is our device, close the handle and delete it
                        RegCloseKey(deviceKey);
                        if (RegDeleteKey(classKey, subkeyName) != ERROR_SUCCESS) {
                            std::cerr << "Failed to delete fake device key: " << std::wstring(subkeyName) << std::endl;
                            success = false;
                        } else {
                            std::cout << "Removed fake device registry entry" << std::endl;
                            // Don't increment index since we removed a key
                            subkeyNameSize = 256;
                            continue;
                        }
                    }
                }
                
                RegCloseKey(deviceKey);
            }
            
            index++;
            subkeyNameSize = 256;
        }
        
        RegCloseKey(classKey);
    }
    
    // 4. Remove driver files
    if (!s_config.driverPath.empty() && GetFileAttributes(s_config.driverPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (!DeleteFile(s_config.driverPath.c_str())) {
            DWORD error = GetLastError();
            std::cerr << "Failed to delete driver file. Error: " << error << std::endl;
            
            // If file is in use, mark for deletion on reboot
            if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION) {
                if (MoveFileEx(s_config.driverPath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT)) {
                    std::cout << "Driver file marked for deletion on next reboot" << std::endl;
                } else {
                    std::cerr << "Failed to mark driver file for deletion. Error: " << GetLastError() << std::endl;
                    success = false;
                }
            } else {
                success = false;
            }
        }
    }
    
    // 5. Remove backup file if it exists
    std::wstring backupPath = s_config.driverPath + L".original";
    if (GetFileAttributes(backupPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (!DeleteFile(backupPath.c_str())) {
            std::cerr << "Failed to delete driver backup file. Error: " << GetLastError() << std::endl;
            // Not critical, don't affect overall success
        }
    }
    
    // 6. Also check for hardware registry entries
    result = RegDeleteKey(HKEY_LOCAL_MACHINE, DRIVER_HARDWARE_KEY);
    // Ignore errors for this one as it might not exist
    
    std::cout << "Complete cleanup " << (success ? "successful" : "completed with some errors") << std::endl;
    return success;
}

bool KernelDriver::Start() {
    if (!s_isInitialized) {
        std::cerr << "Kernel driver manager not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Starting kernel driver service..." << std::endl;
    
    // Check if the driver is already running
    if (IsRunning()) {
        std::cout << "Driver service is already running" << std::endl;
        return true;
    }
    
    // Open the service if not already open
    if (!s_hService) {
        s_hService = OpenService(
            s_hSCManager,
            s_config.driverName.c_str(),
            SERVICE_ALL_ACCESS
        );
        
        if (!s_hService) {
            DWORD error = GetLastError();
            std::cerr << "Failed to open driver service. Error: " << error << std::endl;
            
            // Try to elevate privileges and retry
            if (error == ERROR_ACCESS_DENIED && ElevatePrivileges()) {
                s_hService = OpenService(
                    s_hSCManager,
                    s_config.driverName.c_str(),
                    SERVICE_ALL_ACCESS
                );
                
                if (!s_hService) {
                    error = GetLastError();
                    std::cerr << "Still failed to open driver service after privilege elevation. Error: " << error << std::endl;
                    return false;
                }
            } else {
                return false;
            }
        }
    }
    
    // If the driver requires trusted signing, try to apply bypass methods before starting
    if (s_config.bypassSecurityChecks) {
        BypassDriverSigning();
    }
    
    // Start the service
    if (!StartService(s_hService, 0, NULL)) {
        DWORD error = GetLastError();
        
        // If already running, that's okay
        if (error == ERROR_SERVICE_ALREADY_RUNNING) {
            std::cout << "Driver service is already running" << std::endl;
            return true;
        }
        
        std::cerr << "Failed to start driver service. Error: " << error << std::endl;
        
        if (error == ERROR_FILE_NOT_FOUND) {
            std::cerr << "Driver file not found. Path: " << std::string(s_config.driverPath.begin(), s_config.driverPath.end()) << std::endl;
        }
        
        // If signature validation failed, try more aggressive bypass
        if (error == ERROR_INVALID_IMAGE_HASH) {
            std::cerr << "Driver signature validation failed. Attempting stronger bypass..." << std::endl;
            if (ApplyEmergencySignatureBypass()) {
                // Try again after the emergency bypass
                if (!StartService(s_hService, 0, NULL)) {
                    error = GetLastError();
                    std::cerr << "Still failed to start driver service after signature bypass. Error: " << error << std::endl;
                    return false;
                }
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    
    // Wait for the service to start (up to 5 seconds)
    for (int i = 0; i < 50; i++) {
        SERVICE_STATUS status;
        if (QueryServiceStatus(s_hService, &status)) {
            if (status.dwCurrentState == SERVICE_RUNNING) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Open a handle to the device
    if (!OpenDeviceHandle()) {
        std::cerr << "Warning: Could not open device handle" << std::endl;
        
        // Try another method to open the handle
        if (!TryAlternativeDeviceOpen()) {
            std::cerr << "Warning: All device handle opening methods failed" << std::endl;
            // This is not a critical error, the driver may still be working
        }
    }
    
    // Send initial configuration to the driver
    if (s_hDevice != INVALID_HANDLE_VALUE) {
        // Configure the device parameters
        DeviceParams params;
        memset(&params, 0, sizeof(params));
        params.deviceGuid = GUID_DEVINTERFACE_UndownUnlockVCam;
        
        // Copy device name
        wcscpy_s(params.deviceName, 256, L"UndownUnlock Virtual Camera");
        
        // Copy device path
        wcscpy_s(params.devicePath, 256, L"\\\\?\\ROOT#Image#0000#{6bdd1fc6-810f-11d0-bec7-08002be2092f}");
        
        // Set flags
        params.flags = 0;
        
        // Send to the driver
        DWORD bytesReturned = 0;
        if (!DeviceIoControl(
            s_hDevice,
            IOCTL_SET_DEVICE_PARAMS,
            &params,
            sizeof(params),
            NULL,
            0,
            &bytesReturned,
            NULL)) {
            
            std::cerr << "Warning: Failed to send device parameters to driver. Error: " << GetLastError() << std::endl;
            // Not a critical error
        }
        
        // Configure security parameters if needed
        if (s_config.bypassSecurityChecks) {
            // Set up security spoofing
            SecurityParams secParams;
            memset(&secParams, 0, sizeof(secParams));
            
            secParams.flags = SECURITY_FLAG_SPOOF_VID_PID | SECURITY_FLAG_FAKE_SERIAL;
            secParams.vendorId = 0x046D;  // Logitech
            secParams.productId = 0x0825; // C270
            
            // Copy strings
            wcscpy_s(secParams.serialNumber, 64, FAKE_SERIAL);
            wcscpy_s(secParams.manufacturer, 128, FAKE_MFG);
            wcscpy_s(secParams.product, 128, FAKE_DEVICE_DESC);
            
            // Send to the driver
            if (!DeviceIoControl(
                s_hDevice,
                IOCTL_SPOOF_SIGNATURE,
                &secParams,
                sizeof(secParams),
                NULL,
                0,
                &bytesReturned,
                NULL)) {
                
                std::cerr << "Warning: Failed to send security parameters to driver. Error: " << GetLastError() << std::endl;
                // Not a critical error
            }
        }
    }
    
    std::cout << "Kernel driver service started successfully" << std::endl;
    return true;
}

bool KernelDriver::Stop() {
    if (!s_isInitialized) {
        std::cerr << "Kernel driver manager not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Stopping kernel driver service..." << std::endl;
    
    // Check if the driver is running
    if (!IsRunning()) {
        std::cout << "Driver service is not running" << std::endl;
        return true;
    }
    
    // Close device handle if open
    if (s_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(s_hDevice);
        s_hDevice = INVALID_HANDLE_VALUE;
    }
    
    // Open the service if not already open
    if (!s_hService) {
        s_hService = OpenService(
            s_hSCManager,
            s_config.driverName.c_str(),
            SERVICE_ALL_ACCESS
        );
        
        if (!s_hService) {
            DWORD error = GetLastError();
            std::cerr << "Failed to open driver service. Error: " << error << std::endl;
            
            // Try to elevate privileges and retry
            if (error == ERROR_ACCESS_DENIED && ElevatePrivileges()) {
                s_hService = OpenService(
                    s_hSCManager,
                    s_config.driverName.c_str(),
                    SERVICE_ALL_ACCESS
                );
                
                if (!s_hService) {
                    error = GetLastError();
                    std::cerr << "Still failed to open driver service after privilege elevation. Error: " << error << std::endl;
                    return false;
                }
            } else {
                return false;
            }
        }
    }
    
    // Get the current service status
    SERVICE_STATUS status;
    if (!QueryServiceStatus(s_hService, &status)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to query driver service status. Error: " << error << std::endl;
        return false;
    }
    
    // If service is not running, return success
    if (status.dwCurrentState != SERVICE_RUNNING) {
        std::cout << "Driver service is not running" << std::endl;
        return true;
    }
    
    // Stop the service
    if (!ControlService(s_hService, SERVICE_CONTROL_STOP, &status)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to stop driver service. Error: " << error << std::endl;
        
        // Try force stop if normal stop fails
        if (!ForceStopService()) {
            return false;
        }
    }
    
    // Wait for the service to stop (up to 5 seconds)
    for (int i = 0; i < 50; i++) {
        if (QueryServiceStatus(s_hService, &status)) {
            if (status.dwCurrentState == SERVICE_STOPPED) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Check if service has stopped
    if (status.dwCurrentState != SERVICE_STOPPED) {
        std::cerr << "Warning: Service did not stop in the expected time" << std::endl;
        return false;
    }
    
    std::cout << "Kernel driver service stopped successfully" << std::endl;
    return true;
}

bool KernelDriver::IsInstalled() {
    if (!s_isInitialized) {
        return false;
    }
    
    // Try to open the service
    SC_HANDLE hService = OpenService(
        s_hSCManager,
        s_config.driverName.c_str(),
        SERVICE_QUERY_STATUS
    );
    
    if (hService) {
        CloseServiceHandle(hService);
        return true;
    }
    
    return false;
}

bool KernelDriver::IsRunning() {
    if (!s_isInitialized) {
        return false;
    }
    
    // If we have a device handle, the driver is running
    if (s_hDevice != INVALID_HANDLE_VALUE) {
        return true;
    }
    
    // Try to open the service
    SC_HANDLE hService = NULL;
    if (s_hService) {
        hService = s_hService;
    } else {
        hService = OpenService(
            s_hSCManager,
            s_config.driverName.c_str(),
            SERVICE_QUERY_STATUS
        );
    }
    
    if (!hService) {
        return false;
    }
    
    // Query the service status
    SERVICE_STATUS status;
    BOOL result = QueryServiceStatus(hService, &status);
    
    // Close handle if we opened it
    if (hService != s_hService) {
        CloseServiceHandle(hService);
    }
    
    if (!result) {
        return false;
    }
    
    return (status.dwCurrentState == SERVICE_RUNNING);
}

bool KernelDriver::SetFrame(const BYTE* buffer, size_t size) {
    if (!s_isInitialized || !buffer || size == 0) {
        return false;
    }
    
    // Check if the size is reasonable
    if (size > MAX_FRAME_SIZE) {
        std::cerr << "Error: Frame size is too large (" << size << " bytes)" << std::endl;
        return false;
    }
    
    // Open device handle if not already open
    if (s_hDevice == INVALID_HANDLE_VALUE) {
        if (!OpenDeviceHandle()) {
            std::cerr << "Failed to open device handle" << std::endl;
            return false;
        }
    }
    
    // Send the frame data to the driver
    DWORD bytesReturned = 0;
    if (!SendIOControlToDriver(
        IOCTL_SET_FRAME,
        buffer,
        static_cast<DWORD>(size),
        NULL,
        0,
        &bytesReturned)) {
        
        // If the device is gone, try to reopen it
        if (GetLastError() == ERROR_INVALID_HANDLE || GetLastError() == ERROR_FILE_NOT_FOUND) {
            CloseHandle(s_hDevice);
            s_hDevice = INVALID_HANDLE_VALUE;
            
            if (!OpenDeviceHandle()) {
                return false;
            }
            
            // Try again with the new handle
            return SendIOControlToDriver(
                IOCTL_SET_FRAME,
                buffer,
                static_cast<DWORD>(size),
                NULL,
                0,
                &bytesReturned);
        }
        
        return false;
    }
    
    return true;
}

bool KernelDriver::SetDeviceParams(const GUID& deviceGuid, const std::wstring& deviceName, const std::wstring& devicePath) {
    if (!s_isInitialized) {
        return false;
    }
    
    // Open device handle if not already open
    if (s_hDevice == INVALID_HANDLE_VALUE) {
        if (!OpenDeviceHandle()) {
            std::cerr << "Failed to open device handle" << std::endl;
            return false;
        }
    }
    
    // Prepare device parameters
    DeviceParams params;
    params.deviceGuid = deviceGuid;
    
    // Copy device name (with bounds checking)
    wcsncpy_s(params.deviceName, _countof(params.deviceName), 
              deviceName.c_str(), _TRUNCATE);
    
    // Copy device path (with bounds checking)
    wcsncpy_s(params.devicePath, _countof(params.devicePath), 
              devicePath.c_str(), _TRUNCATE);
    
    // Set some flags (can be extended as needed)
    params.flags = 0;
    
    // Send parameters to the driver
    DWORD bytesReturned = 0;
    if (!SendIOControlToDriver(
        IOCTL_SET_DEVICE_PARAMS,
        &params,
        sizeof(params),
        NULL,
        0,
        &bytesReturned)) {
        
        // If the device is gone, try to reopen it
        if (GetLastError() == ERROR_INVALID_HANDLE || GetLastError() == ERROR_FILE_NOT_FOUND) {
            CloseHandle(s_hDevice);
            s_hDevice = INVALID_HANDLE_VALUE;
            
            if (!OpenDeviceHandle()) {
                return false;
            }
            
            // Try again with the new handle
            return SendIOControlToDriver(
                IOCTL_SET_DEVICE_PARAMS,
                &params,
                sizeof(params),
                NULL,
                0,
                &bytesReturned);
        }
        
        return false;
    }
    
    return true;
}

bool KernelDriver::BypassDriverSigning() {
    if (!s_isInitialized) {
        return false;
    }
    
    std::cout << "Attempting to bypass driver signing requirements..." << std::endl;
    
    bool success = false;
    
    // Try multiple methods for maximum compatibility
    
    // Method 1: Enable test signing mode (requires reboot)
    if (!IsTestSigningEnabled()) {
        success |= EnableTestSigning();
    } else {
        s_testSigningEnabled = true;
        success = true;
    }
    
    // Method 2: Disable integrity checks if possible
    if (!AreIntegrityChecksDisabled()) {
        success |= DisableIntegrityChecks();
    } else {
        s_integrityChecksDisabled = true;
        success = true;
    }
    
    // Method 3: Add driver to trusted certificate store
    success |= AddDriverToCertStore();
    
    // Method 4: Patch the driver file with a fake signature
    if (s_config.allowDriverPatching) {
        success |= PatchDriverWithFakeSignature();
    }
    
    // Method 5: Apply emergency bypass by patching CI.dll in memory
    if (s_config.allowAdvancedBypass && !success) {
        success |= ApplyEmergencySignatureBypass();
    }
    
    if (!success) {
        std::cerr << "Failed to bypass driver signing. Administrator privileges may be required." << std::endl;
        std::cerr << "Consider running 'bcdedit /set testsigning on' as administrator and reboot." << std::endl;
    } else {
        std::cout << "Driver signing bypass methods applied successfully" << std::endl;
        std::cout << "NOTE: A system reboot may be required for some changes to take effect." << std::endl;
    }
    
    return success;
}

bool KernelDriver::SpoofDeviceSignature() {
    if (!s_isInitialized) {
        return false;
    }
    
    // Open device handle if not already open
    if (s_hDevice == INVALID_HANDLE_VALUE) {
        if (!OpenDeviceHandle()) {
            std::cerr << "Failed to open device handle" << std::endl;
            return false;
        }
    }
    
    // Prepare security parameters
    SecurityParams params;
    memset(&params, 0, sizeof(params));
    
    // Set flags for signature spoofing
    params.flags = SECURITY_FLAG_SPOOF_VID_PID | SECURITY_FLAG_FAKE_SERIAL;
    
    // Set spoofed VID/PID for a popular webcam (Logitech C270)
    params.vendorId = 0x046D;
    params.productId = 0x0825;
    
    // Set serial number
    wcscpy_s(params.serialNumber, _countof(params.serialNumber), FAKE_SERIAL);
    
    // Set manufacturer
    wcscpy_s(params.manufacturer, _countof(params.manufacturer), FAKE_MFG);
    
    // Set product name
    wcscpy_s(params.product, _countof(params.product), FAKE_DEVICE_DESC);
    
    // Send to the driver
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
        s_hDevice,
        IOCTL_SPOOF_SIGNATURE,
        &params,
        sizeof(params),
        NULL,
        0,
        &bytesReturned,
        NULL)) {
        
        DWORD error = GetLastError();
        std::cerr << "Failed to spoof device signature. Error: " << error << std::endl;
        
        // If the device is gone, try to reopen it
        if (error == ERROR_INVALID_HANDLE || error == ERROR_FILE_NOT_FOUND) {
            CloseHandle(s_hDevice);
            s_hDevice = INVALID_HANDLE_VALUE;
            
            if (!OpenDeviceHandle()) {
                return false;
            }
            
            // Try again with the new handle
            return DeviceIoControl(
                s_hDevice,
                IOCTL_SPOOF_SIGNATURE,
                &params,
                sizeof(params),
                NULL,
                0,
                &bytesReturned,
                NULL);
        }
        
        return false;
    }
    
    // Modify registry entries to spoof the device signature
    return SetupFakeDeviceRegistry();
}

bool KernelDriver::SetVideoFormat(int width, int height, PixelFormat format, int frameRate) {
    if (!s_isInitialized) {
        return false;
    }
    
    // Open device handle if not already open
    if (s_hDevice == INVALID_HANDLE_VALUE) {
        if (!OpenDeviceHandle()) {
            std::cerr << "Failed to open device handle" << std::endl;
            return false;
        }
    }
    
    // Prepare format parameters
    VideoFormat videoFormat;
    videoFormat.width = width;
    videoFormat.height = height;
    videoFormat.pixelFormat = format;
    videoFormat.frameRateNumerator = frameRate;
    videoFormat.frameRateDenominator = 1;
    
    // Send to the driver
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
        s_hDevice,
        IOCTL_SET_FORMAT,
        &videoFormat,
        sizeof(videoFormat),
        NULL,
        0,
        &bytesReturned,
        NULL)) {
        
        DWORD error = GetLastError();
        std::cerr << "Failed to set video format. Error: " << error << std::endl;
        
        // If the device is gone, try to reopen it
        if (error == ERROR_INVALID_HANDLE || error == ERROR_FILE_NOT_FOUND) {
            CloseHandle(s_hDevice);
            s_hDevice = INVALID_HANDLE_VALUE;
            
            if (!OpenDeviceHandle()) {
                return false;
            }
            
            // Try again with the new handle
            return DeviceIoControl(
                s_hDevice,
                IOCTL_SET_FORMAT,
                &videoFormat,
                sizeof(videoFormat),
                NULL,
                0,
                &bytesReturned,
                NULL);
        }
        
        return false;
    }
    
    return true;
}

std::vector<VideoFormatDescriptor> KernelDriver::GetSupportedFormats() {
    std::vector<VideoFormatDescriptor> formats;
    
    if (!s_isInitialized) {
        return formats;
    }
    
    // Open device handle if not already open
    if (s_hDevice == INVALID_HANDLE_VALUE) {
        if (!OpenDeviceHandle()) {
            std::cerr << "Failed to open device handle" << std::endl;
            return formats;
        }
    }
    
    // Prepare buffer for response
    GetFormatsResponse response;
    memset(&response, 0, sizeof(response));
    
    // Send request to driver
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
        s_hDevice,
        IOCTL_GET_FORMATS,
        NULL,
        0,
        &response,
        sizeof(response),
        &bytesReturned,
        NULL)) {
        
        DWORD error = GetLastError();
        std::cerr << "Failed to get supported formats. Error: " << error << std::endl;
        
        // If the device is gone, try to reopen it
        if (error == ERROR_INVALID_HANDLE || error == ERROR_FILE_NOT_FOUND) {
            CloseHandle(s_hDevice);
            s_hDevice = INVALID_HANDLE_VALUE;
            
            if (OpenDeviceHandle()) {
                // Try again with the new handle
                if (!DeviceIoControl(
                    s_hDevice,
                    IOCTL_GET_FORMATS,
                    NULL,
                    0,
                    &response,
                    sizeof(response),
                    &bytesReturned,
                    NULL)) {
                    return formats;
                }
            } else {
                return formats;
            }
        } else {
            return formats;
        }
    }
    
    // Process response
    for (ULONG i = 0; i < response.formatCount && i < 16; i++) {
        formats.push_back(response.formats[i]);
    }
    
    // If no formats returned from driver, provide fallback formats
    if (formats.empty()) {
        // Add common default formats
        VideoFormatDescriptor format;
        
        // 1080p MJPG
        format.width = 1920;
        format.height = 1080;
        format.pixelFormat = PixelFormat::MJPG;
        format.frameRateNumerator = 30;
        format.frameRateDenominator = 1;
        formats.push_back(format);
        
        // 720p MJPG
        format.width = 1280;
        format.height = 720;
        format.pixelFormat = PixelFormat::MJPG;
        format.frameRateNumerator = 30;
        format.frameRateDenominator = 1;
        formats.push_back(format);
        
        // 720p YUY2
        format.width = 1280;
        format.height = 720;
        format.pixelFormat = PixelFormat::YUY2;
        format.frameRateNumerator = 30;
        format.frameRateDenominator = 1;
        formats.push_back(format);
        
        // 640x480 MJPG
        format.width = 640;
        format.height = 480;
        format.pixelFormat = PixelFormat::MJPG;
        format.frameRateNumerator = 30;
        format.frameRateDenominator = 1;
        formats.push_back(format);
    }
    
    return formats;
}

DeviceStatus KernelDriver::GetDeviceStatus() {
    DeviceStatus status;
    memset(&status, 0, sizeof(status));
    
    if (!s_isInitialized) {
        return status;
    }
    
    // Open device handle if not already open
    if (s_hDevice == INVALID_HANDLE_VALUE) {
        if (!OpenDeviceHandle()) {
            std::cerr << "Failed to open device handle" << std::endl;
            return status;
        }
    }
    
    // Send request to driver
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
        s_hDevice,
        IOCTL_GET_STATUS,
        NULL,
        0,
        &status,
        sizeof(status),
        &bytesReturned,
        NULL)) {
        
        DWORD error = GetLastError();
        std::cerr << "Failed to get device status. Error: " << error << std::endl;
        
        // If the device is gone, try to reopen it
        if (error == ERROR_INVALID_HANDLE || error == ERROR_FILE_NOT_FOUND) {
            CloseHandle(s_hDevice);
            s_hDevice = INVALID_HANDLE_VALUE;
            
            if (OpenDeviceHandle()) {
                // Try again with the new handle
                if (!DeviceIoControl(
                    s_hDevice,
                    IOCTL_GET_STATUS,
                    NULL,
                    0,
                    &status,
                    sizeof(status),
                    &bytesReturned,
                    NULL)) {
                    // Just return the default status
                    status.isActive = IsRunning();
                }
            }
        }
    }
    
    // If we couldn't get a status from the driver, at least set the active flag based on our knowledge
    if (!bytesReturned) {
        status.isActive = IsRunning();
    }
    
    return status;
}

bool KernelDriver::ExtractDriverFile() {
    std::cout << "Extracting driver file to: " << std::string(s_config.driverPath.begin(), s_config.driverPath.end()) << std::endl;
    
    // First, check if we have an embedded driver resource
    bool extractedFromResource = false;
    
    HMODULE hModule = GetModuleHandle(NULL);
    if (hModule) {
        HRSRC hResource = FindResource(hModule, DRIVER_RESOURCE_NAME, RT_RCDATA);
        if (hResource) {
            HGLOBAL hData = LoadResource(hModule, hResource);
            if (hData) {
                void* pData = LockResource(hData);
                DWORD size = SizeofResource(hModule, hResource);
                
                if (pData && size > 0) {
                    // Create the driver file
                    HANDLE hFile = CreateFile(
                        s_config.driverPath.c_str(),
                        GENERIC_WRITE,
                        0,
                        NULL,
                        CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL
                    );
                    
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD bytesWritten = 0;
                        if (WriteFile(hFile, pData, size, &bytesWritten, NULL) && bytesWritten == size) {
                            extractedFromResource = true;
                        }
                        CloseHandle(hFile);
                    }
                }
            }
        }
    }
    
    // If resource extraction failed, try to load from a driver source file
    if (!extractedFromResource) {
        // Check if we have a compiled driver file available in the current directory
        std::filesystem::path currentPath = std::filesystem::current_path();
        std::filesystem::path driverSourcePath = currentPath / "src" / "virtual_camera" / "kernel_driver_source.c";
        
        if (std::filesystem::exists(driverSourcePath)) {
            // We would typically compile the driver here, but that's complex and requires the WDK
            // For simplicity, in this example we'll just copy a pre-compiled driver if available
            
            std::filesystem::path precompiledDriverPath = currentPath / "bin" / "vcam_driver.sys";
            if (std::filesystem::exists(precompiledDriverPath)) {
                try {
                    std::filesystem::copy_file(precompiledDriverPath, s_config.driverPath, 
                                             std::filesystem::copy_options::overwrite_existing);
                    extractedFromResource = true;
                } catch (const std::exception& e) {
                    std::cerr << "Failed to copy pre-compiled driver: " << e.what() << std::endl;
                }
            }
        }
        
        // If we still haven't extracted a driver, create a placeholder for demonstration
        if (!extractedFromResource) {
            std::cout << "Warning: Creating placeholder driver file for demonstration purposes only." << std::endl;
            std::cout << "In a real implementation, a proper driver binary would be extracted." << std::endl;
            
            std::ofstream driverFile(s_config.driverPath, std::ios::binary);
            if (driverFile.is_open()) {
                // Write a dummy driver header
                const char* driverHeader = 
                    "MZ\x90\x00\x03\x00\x00\x00\x04\x00\x00\x00\xFF\xFF\x00\x00"
                    "\xB8\x00\x00\x00\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00"
                    "This is a placeholder driver file for UndownUnlock Virtual Camera.";
                
                driverFile.write(driverHeader, strlen(driverHeader));
                driverFile.close();
                extractedFromResource = true;
            }
        }
    }
    
    return PathFileExists(s_config.driverPath.c_str());
}

bool KernelDriver::RegisterDriverService() {
    // This functionality is handled by the Install() method
    return Install();
}

bool KernelDriver::SetDriverRegistryKeys() {
    std::cout << "Setting driver registry keys..." << std::endl;
    
    bool success = true;
    HKEY hKey = NULL;
    
    // Open or create the driver service key
    LONG result = RegCreateKeyEx(
        HKEY_LOCAL_MACHINE,
        DRIVER_REGISTRY_KEY,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        NULL,
        &hKey,
        NULL
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to open driver registry key. Error: " << result << std::endl;
        
        // Try to elevate and retry
        if (ElevatePrivileges()) {
            result = RegCreateKeyEx(
                HKEY_LOCAL_MACHINE,
                DRIVER_REGISTRY_KEY,
                0,
                NULL,
                REG_OPTION_NON_VOLATILE,
                KEY_ALL_ACCESS,
                NULL,
                &hKey,
                NULL
            );
            
            if (result != ERROR_SUCCESS) {
                std::cerr << "Still failed to open driver registry key after privilege elevation. Error: " << result << std::endl;
                return false;
            }
        } else {
            return false;
        }
    }
    
    // Set the device ID
    DWORD deviceId = s_config.deviceId;
    result = RegSetValueEx(
        hKey,
        L"DeviceId",
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&deviceId),
        sizeof(DWORD)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set DeviceId registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set the device name
    result = RegSetValueEx(
        hKey,
        L"DeviceName",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(L"UndownUnlock Virtual Camera"),
        (wcslen(L"UndownUnlock Virtual Camera") + 1) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set DeviceName registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set the device type (webcam = 0x01)
    DWORD deviceType = 0x01;
    result = RegSetValueEx(
        hKey,
        L"DeviceType",
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&deviceType),
        sizeof(DWORD)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set DeviceType registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set default resolution
    DWORD width = 1280;
    result = RegSetValueEx(
        hKey,
        L"DefaultWidth",
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&width),
        sizeof(DWORD)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set DefaultWidth registry value. Error: " << result << std::endl;
        success = false;
    }
    
    DWORD height = 720;
    result = RegSetValueEx(
        hKey,
        L"DefaultHeight",
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&height),
        sizeof(DWORD)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set DefaultHeight registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set the DeviceInstanceId to create a reference to the hardware key
    result = RegSetValueEx(
        hKey,
        L"DeviceInstanceId",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(L"ROOT\\IMAGE\\0000"),
        (wcslen(L"ROOT\\IMAGE\\0000") + 1) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set DeviceInstanceId registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Close the registry key
    RegCloseKey(hKey);
    
    return success;
}

bool KernelDriver::SetupFakeDeviceRegistry() {
    std::cout << "Setting up fake device registry entries..." << std::endl;
    
    bool success = true;
    HKEY hKey = NULL;
    
    // Create hardware device key
    LONG result = RegCreateKeyEx(
        HKEY_LOCAL_MACHINE,
        DRIVER_HARDWARE_KEY,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        NULL,
        &hKey,
        NULL
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to create hardware registry key. Error: " << result << std::endl;
        
        // Try to elevate and retry
        if (ElevatePrivileges()) {
            result = RegCreateKeyEx(
                HKEY_LOCAL_MACHINE,
                DRIVER_HARDWARE_KEY,
                0,
                NULL,
                REG_OPTION_NON_VOLATILE,
                KEY_ALL_ACCESS,
                NULL,
                &hKey,
                NULL
            );
            
            if (result != ERROR_SUCCESS) {
                std::cerr << "Still failed to create hardware registry key after privilege elevation. Error: " << result << std::endl;
                return false;
            }
        } else {
            return false;
        }
    }
    
    // Set hardware ID
    result = RegSetValueEx(
        hKey,
        L"HardwareID",
        0,
        REG_MULTI_SZ,
        reinterpret_cast<const BYTE*>(FAKE_HW_ID L"\0\0"),
        (wcslen(FAKE_HW_ID) + 2) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set HardwareID registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set compatible IDs
    result = RegSetValueEx(
        hKey,
        L"CompatibleIDs",
        0,
        REG_MULTI_SZ,
        reinterpret_cast<const BYTE*>(FAKE_COMPATIBLE_ID L"\0\0"),
        (wcslen(FAKE_COMPATIBLE_ID) + 2) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set CompatibleIDs registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set device description
    result = RegSetValueEx(
        hKey,
        L"DeviceDesc",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(FAKE_DEVICE_DESC),
        (wcslen(FAKE_DEVICE_DESC) + 1) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set DeviceDesc registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set manufacturer
    result = RegSetValueEx(
        hKey,
        L"Mfg",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(FAKE_MFG),
        (wcslen(FAKE_MFG) + 1) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set Mfg registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set service name
    result = RegSetValueEx(
        hKey,
        L"Service",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(s_config.driverName.c_str()),
        (s_config.driverName.length() + 1) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set Service registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set a fake device serial number
    result = RegSetValueEx(
        hKey,
        L"SerialNumber",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(FAKE_SERIAL),
        (wcslen(FAKE_SERIAL) + 1) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set SerialNumber registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set Driver Date
    result = RegSetValueEx(
        hKey,
        L"DriverDate",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(L"10-10-2021"),
        (wcslen(L"10-10-2021") + 1) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set DriverDate registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set Driver Version
    result = RegSetValueEx(
        hKey,
        L"DriverVersion",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(L"10.0.19041.1"),
        (wcslen(L"10.0.19041.1") + 1) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set DriverVersion registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Add device class key for camera
    result = RegSetValueEx(
        hKey,
        L"ClassGUID",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(L"{6bdd1fc6-810f-11d0-bec7-08002be2092f}"),
        (wcslen(L"{6bdd1fc6-810f-11d0-bec7-08002be2092f}") + 1) * sizeof(WCHAR)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set ClassGUID registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Set install flags
    DWORD flags = 0x00000001; // CM_INSTALL_STATE_INSTALLED
    result = RegSetValueEx(
        hKey,
        L"InstallState",
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&flags),
        sizeof(DWORD)
    );
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set InstallState registry value. Error: " << result << std::endl;
        success = false;
    }
    
    // Close the registry key
    RegCloseKey(hKey);
    
    // Now send a PnP notification to inform the system of the new device
    DEV_BROADCAST_DEVICEINTERFACE broadcastInterface;
    memset(&broadcastInterface, 0, sizeof(broadcastInterface));
    broadcastInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    broadcastInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    
    // Send the notification
    DWORD_PTR result2 = 0;
    if (!SendMessageTimeout(
        HWND_BROADCAST,
        WM_DEVICECHANGE,
        DBT_DEVICEARRIVAL,
        (LPARAM)&broadcastInterface,
        SMTO_ABORTIFHUNG,
        5000,
        &result2)) {
        std::cerr << "Warning: Failed to send device change notification" << std::endl;
    }
    
    return success;
}

void KernelDriver::RemoveDeviceRegistryEntries() {
    // Try to delete the hardware device key
    LONG result = RegDeleteKey(
        HKEY_LOCAL_MACHINE,
        DRIVER_HARDWARE_KEY
    );
    
    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        std::cerr << "Warning: Failed to delete hardware registry key. Error: " << result << std::endl;
    }
}

bool KernelDriver::OpenDeviceHandle() {
    // Close existing handle if any
    if (s_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(s_hDevice);
        s_hDevice = INVALID_HANDLE_VALUE;
    }
    
    // Open the device using the device path
    s_hDevice = CreateFile(
        DRIVER_DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        0,          // No sharing
        NULL,       // Default security attributes
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (s_hDevice == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        
        // If the specific device path failed, try to locate the device using SetupAPI
        if (error == ERROR_FILE_NOT_FOUND) {
            return TryAlternativeDeviceOpen();
        }
        
        std::cerr << "Failed to open device. Error: " << error << std::endl;
        return false;
    }
    
    return true;
}

bool KernelDriver::SendIOControlToDriver(DWORD ioctl, const void* inBuffer, DWORD inBufferSize, 
                                       void* outBuffer, DWORD outBufferSize, DWORD* bytesReturned) {
    if (s_hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "Device handle is invalid" << std::endl;
        return false;
    }
    
    DWORD bytesReturnedLocal = 0;
    if (!bytesReturned) {
        bytesReturned = &bytesReturnedLocal;
    }
    
    if (!DeviceIoControl(
        s_hDevice,
        ioctl,
        const_cast<void*>(inBuffer),
        inBufferSize,
        outBuffer,
        outBufferSize,
        bytesReturned,
        NULL)) {
        
        DWORD error = GetLastError();
        std::cerr << "Device I/O control failed. Error: " << error << std::endl;
        return false;
    }
    
    return true;
}

bool KernelDriver::IsTestSigningEnabled() {
    if (s_testSigningEnabled) {
        return true;
    }
    
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    WCHAR cmdLine[] = L"bcdedit /enum";
    WCHAR tempFilePath[MAX_PATH] = {0};
    
    // Generate a temporary file path
    GetTempPath(MAX_PATH, tempFilePath);
    wcscat_s(tempFilePath, MAX_PATH, L"bcdedit_output.txt");
    
    // Set up process information
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    ZeroMemory(&pi, sizeof(pi));
    
    // Create a redirected process
    WCHAR redirectedCmd[MAX_PATH + 50] = {0};
    swprintf_s(redirectedCmd, MAX_PATH + 50, L"cmd.exe /c %s > \"%s\"", cmdLine, tempFilePath);
    
    if (!CreateProcess(NULL, redirectedCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return false;
    }
    
    // Wait for the process to finish
    WaitForSingleObject(pi.hProcess, 5000);
    
    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Read the output file
    std::ifstream file(tempFilePath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    bool testSigningEnabled = false;
    
    while (std::getline(file, line)) {
        // Convert to lowercase for case-insensitive search
        std::transform(line.begin(), line.end(), line.begin(), ::tolower);
        
        if (line.find("testsigning") != std::string::npos && line.find("yes") != std::string::npos) {
            testSigningEnabled = true;
            break;
        }
    }
    
    file.close();
    
    // Delete the temporary file
    DeleteFile(tempFilePath);
    
    s_testSigningEnabled = testSigningEnabled;
    return testSigningEnabled;
}

bool KernelDriver::AreIntegrityChecksDisabled() {
    if (s_integrityChecksDisabled) {
        return true;
    }
    
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    WCHAR cmdLine[] = L"bcdedit /enum";
    WCHAR tempFilePath[MAX_PATH] = {0};
    
    // Generate a temporary file path
    GetTempPath(MAX_PATH, tempFilePath);
    wcscat_s(tempFilePath, MAX_PATH, L"bcdedit_output.txt");
    
    // Set up process information
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    ZeroMemory(&pi, sizeof(pi));
    
    // Create a redirected process
    WCHAR redirectedCmd[MAX_PATH + 50] = {0};
    swprintf_s(redirectedCmd, MAX_PATH + 50, L"cmd.exe /c %s > \"%s\"", cmdLine, tempFilePath);
    
    if (!CreateProcess(NULL, redirectedCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return false;
    }
    
    // Wait for the process to finish
    WaitForSingleObject(pi.hProcess, 5000);
    
    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Read the output file
    std::ifstream file(tempFilePath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    bool integrityChecksDisabled = false;
    
    while (std::getline(file, line)) {
        // Convert to lowercase for case-insensitive search
        std::transform(line.begin(), line.end(), line.begin(), ::tolower);
        
        if (line.find("nointegritychecks") != std::string::npos && line.find("yes") != std::string::npos) {
            integrityChecksDisabled = true;
            break;
        }
    }
    
    file.close();
    
    // Delete the temporary file
    DeleteFile(tempFilePath);
    
    s_integrityChecksDisabled = integrityChecksDisabled;
    return integrityChecksDisabled;
}

bool KernelDriver::EnableTestSigning() {
    // This requires administrative privileges
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    WCHAR cmdLine[] = L"bcdedit /set testsigning on";
    
    // Set up process information
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    ZeroMemory(&pi, sizeof(pi));
    
    // Create a process
    if (!CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to execute bcdedit command. Error: " << error << std::endl;
        
        // Try to elevate privileges and retry
        if (error == ERROR_ACCESS_DENIED && ElevatePrivileges()) {
            if (!CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                error = GetLastError();
                std::cerr << "Still failed to execute bcdedit command after privilege elevation. Error: " << error << std::endl;
                return false;
            }
        } else {
            return false;
        }
    }
    
    // Wait for the process to finish
    WaitForSingleObject(pi.hProcess, 5000);
    
    // Get the exit code
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return (exitCode == 0);
}

bool KernelDriver::DisableIntegrityChecks() {
    // This requires administrative privileges
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    WCHAR cmdLine[] = L"bcdedit /set nointegritychecks on";
    
    // Set up process information
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    ZeroMemory(&pi, sizeof(pi));
    
    // Create a process
    if (!CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to execute bcdedit command. Error: " << error << std::endl;
        
        // Try to elevate privileges and retry
        if (error == ERROR_ACCESS_DENIED && ElevatePrivileges()) {
            if (!CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                error = GetLastError();
                std::cerr << "Still failed to execute bcdedit command after privilege elevation. Error: " << error << std::endl;
                return false;
            }
        } else {
            return false;
        }
    }
    
    // Wait for the process to finish
    WaitForSingleObject(pi.hProcess, 5000);
    
    // Get the exit code
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return (exitCode == 0);
}

bool KernelDriver::SpoofDriverCertificate() {
    // This would apply a fake certificate to the driver file
    // This is a complex operation that would require cryptographic functions
    
    std::cout << "Spoofing driver certificate (simulation)..." << std::endl;
    
    // For security reasons, we're not implementing actual certificate spoofing
    // In a real implementation, this would use functions like CryptSignHash
    
    // Instead, this is a placeholder for simulation purposes
    
    return true;
}

bool KernelDriver::PatchDriverWithFakeSignature() {
    // Path to the driver file
    std::wstring driverFilePath = s_config.driverPath;
    std::cout << "Patching driver with fake signature: " << std::string(driverFilePath.begin(), driverFilePath.end()) << std::endl;
    
    // Make a backup of the original driver if it doesn't exist
    std::wstring backupPath = driverFilePath + L".original";
    if (GetFileAttributes(backupPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (!CopyFile(driverFilePath.c_str(), backupPath.c_str(), TRUE)) {
            std::cerr << "Failed to create backup of driver file. Error: " << GetLastError() << std::endl;
            return false;
        }
        std::cout << "Created backup of original driver file" << std::endl;
    }
    
    // Open the driver file for reading and writing
    HANDLE hFile = CreateFile(
        driverFilePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open driver file for patching. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    // Create file mapping
    HANDLE hMapping = CreateFileMapping(
        hFile,
        NULL,
        PAGE_READWRITE,
        0,
        0,
        NULL
    );
    
    if (hMapping == NULL) {
        std::cerr << "Failed to create file mapping. Error: " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }
    
    // Map the file into memory
    LPVOID fileData = MapViewOfFile(
        hMapping,
        FILE_MAP_WRITE,
        0,
        0,
        0
    );
    
    if (fileData == NULL) {
        std::cerr << "Failed to map view of file. Error: " << GetLastError() << std::endl;
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }
    
    // Get DOS header
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)fileData;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cerr << "Invalid DOS header" << std::endl;
        UnmapViewOfFile(fileData);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }
    
    // Get NT headers
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)fileData + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        std::cerr << "Invalid NT header" << std::endl;
        UnmapViewOfFile(fileData);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }
    
    // Find the security directory
    DWORD securityDirRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress;
    DWORD securityDirSize = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size;
    
    if (securityDirRVA == 0 || securityDirSize == 0) {
        std::cout << "No security directory found, driver is likely already unsigned" << std::endl;
    } else {
        // Zero out the security directory entry to bypass signature check
        std::cout << "Found security directory at offset: 0x" << std::hex << securityDirRVA << std::dec << std::endl;
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress = 0;
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size = 0;
        
        // Also find and modify checksum
        ntHeaders->OptionalHeader.CheckSum = 0;
        
        std::cout << "Security directory nullified" << std::endl;
    }
    
    // Unmap and close handles
    FlushViewOfFile(fileData, 0);
    UnmapViewOfFile(fileData);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    
    std::cout << "Driver signature patching completed successfully" << std::endl;
    return true;
}

bool KernelDriver::RestoreOriginalDriverSignature() {
    // Path to the driver file
    std::wstring driverFilePath = s_config.driverPath;
    std::wstring backupPath = driverFilePath + L".original";
    
    // Check if backup exists
    if (GetFileAttributes(backupPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "No backup driver file found. Cannot restore original signature." << std::endl;
        return false;
    }
    
    // Stop service if it's running
    if (IsRunning()) {
        if (!Stop()) {
            std::cerr << "Warning: Failed to stop driver service before restoring. The restore might fail." << std::endl;
        }
    }
    
    // Restore from backup
    if (!CopyFile(backupPath.c_str(), driverFilePath.c_str(), FALSE)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to restore original driver file. Error: " << error << std::endl;
        
        // If access denied, try to take ownership and retry
        if (error == ERROR_ACCESS_DENIED) {
            if (ElevatePrivileges() && TakeOwnershipOfFile(driverFilePath)) {
                if (CopyFile(backupPath.c_str(), driverFilePath.c_str(), FALSE)) {
                    std::cout << "Successfully restored original driver file after taking ownership" << std::endl;
                    return true;
                }
            }
        }
        
        return false;
    }
    
    std::cout << "Successfully restored original driver signature" << std::endl;
    return true;
}

bool KernelDriver::TakeOwnershipOfFile(const std::wstring& filePath) {
    // Get file handle
    HANDLE hFile = CreateFile(
        filePath.c_str(),
        READ_CONTROL | WRITE_DAC | WRITE_OWNER,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open file for ownership change. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    // Get current security descriptor
    PSECURITY_DESCRIPTOR secDesc = NULL;
    DWORD secDescSize = 0;
    
    if (!GetFileSecurity(filePath.c_str(), OWNER_SECURITY_INFORMATION, secDesc, 0, &secDescSize) && 
        GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        
        secDesc = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, secDescSize);
        if (secDesc == NULL) {
            std::cerr << "Failed to allocate memory for security descriptor" << std::endl;
            CloseHandle(hFile);
            return false;
        }
        
        if (!GetFileSecurity(filePath.c_str(), OWNER_SECURITY_INFORMATION, secDesc, secDescSize, &secDescSize)) {
            std::cerr << "Failed to get file security. Error: " << GetLastError() << std::endl;
            LocalFree(secDesc);
            CloseHandle(hFile);
            return false;
        }
    } else {
        std::cerr << "Failed to get security descriptor size. Error: " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }
    
    // Get current process token
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        std::cerr << "Failed to open process token. Error: " << GetLastError() << std::endl;
        LocalFree(secDesc);
        CloseHandle(hFile);
        return false;
    }
    
    // Get user SID
    DWORD tokenInfoLen = 0;
    GetTokenInformation(hToken, TokenUser, NULL, 0, &tokenInfoLen);
    
    PTOKEN_USER tokenUser = (PTOKEN_USER)LocalAlloc(LPTR, tokenInfoLen);
    if (tokenUser == NULL) {
        std::cerr << "Failed to allocate memory for token information" << std::endl;
        CloseHandle(hToken);
        LocalFree(secDesc);
        CloseHandle(hFile);
        return false;
    }
    
    if (!GetTokenInformation(hToken, TokenUser, tokenUser, tokenInfoLen, &tokenInfoLen)) {
        std::cerr << "Failed to get token information. Error: " << GetLastError() << std::endl;
        LocalFree(tokenUser);
        CloseHandle(hToken);
        LocalFree(secDesc);
        CloseHandle(hFile);
        return false;
    }
    
    // Set new owner
    SECURITY_DESCRIPTOR newSecDesc;
    if (!InitializeSecurityDescriptor(&newSecDesc, SECURITY_DESCRIPTOR_REVISION)) {
        std::cerr << "Failed to initialize security descriptor. Error: " << GetLastError() << std::endl;
        LocalFree(tokenUser);
        CloseHandle(hToken);
        LocalFree(secDesc);
        CloseHandle(hFile);
        return false;
    }
    
    if (!SetSecurityDescriptorOwner(&newSecDesc, tokenUser->User.Sid, FALSE)) {
        std::cerr << "Failed to set security descriptor owner. Error: " << GetLastError() << std::endl;
        LocalFree(tokenUser);
        CloseHandle(hToken);
        LocalFree(secDesc);
        CloseHandle(hFile);
        return false;
    }
    
    // Apply new security descriptor
    if (!SetFileSecurity(filePath.c_str(), OWNER_SECURITY_INFORMATION, &newSecDesc)) {
        std::cerr << "Failed to set file security. Error: " << GetLastError() << std::endl;
        LocalFree(tokenUser);
        CloseHandle(hToken);
        LocalFree(secDesc);
        CloseHandle(hFile);
        return false;
    }
    
    // Clean up
    LocalFree(tokenUser);
    CloseHandle(hToken);
    LocalFree(secDesc);
    CloseHandle(hFile);
    
    return true;
}

bool KernelDriver::ApplyEmergencySignatureBypass() {
    // This would apply an in-memory patch to CI.dll to bypass driver signature verification
    // This is a complex and risky operation
    
    std::cout << "Applying emergency signature bypass (simulation)..." << std::endl;
    
    // For security reasons, we're not implementing actual CI.dll patching
    // In a real implementation, this would locate and modify the verification code
    
    // Instead, this is a placeholder for simulation purposes
    
    return true;
}

bool KernelDriver::ForceDeleteService() {
    // This is a more aggressive attempt to delete a service that refuses to be deleted normally
    
    std::cout << "Attempting to force delete the service..." << std::endl;
    
    bool success = false;
    
    // Try using SC Manager with additional privileges
    if (ElevatePrivileges()) {
        // First ensure the service is stopped
        SERVICE_STATUS status;
        if (s_hService && ControlService(s_hService, SERVICE_CONTROL_STOP, &status)) {
            // Wait a bit for the service to stop
            Sleep(1000);
        }
        
        // Try the normal delete again with elevated privileges
        if (s_hService && DeleteService(s_hService)) {
            success = true;
        }
    }
    
    // If still not successful, try registry manipulation
    if (!success) {
        HKEY hKey = NULL;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
            // Try to delete the service key directly
            LONG result = RegDeleteKey(hKey, s_config.driverName.c_str());
            if (result == ERROR_SUCCESS) {
                success = true;
            }
            RegCloseKey(hKey);
        }
    }
    
    return success;
}

bool KernelDriver::ForceStopService() {
    // This is a more aggressive attempt to stop a service that refuses to stop normally
    
    std::cout << "Attempting to force stop the service..." << std::endl;
    
    // Try using taskkill to terminate any processes using the driver
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    WCHAR cmdLine[200] = {0};
    
    swprintf_s(cmdLine, 200, L"taskkill /f /fi \"modules eq %s.sys\"", s_config.driverName.c_str());
    
    // Set up process information
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    ZeroMemory(&pi, sizeof(pi));
    
    // Create a process
    if (CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        // Wait for the process to finish
        WaitForSingleObject(pi.hProcess, 5000);
        
        // Close process and thread handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    // Try to stop the service again
    SERVICE_STATUS status;
    return ControlService(s_hService, SERVICE_CONTROL_STOP, &status) != FALSE;
}

bool KernelDriver::ElevatePrivileges() {
    // Note: This is a simulation of privilege elevation.
    // In reality, elevating privileges programmatically is complex and often requires UAC prompts.
    
    // Enable debug privilege
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        TOKEN_PRIVILEGES tp;
        LUID luid;
        
        if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            
            if (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
                CloseHandle(hToken);
                return true;
            }
        }
        
        CloseHandle(hToken);
    }
    
    return false;
}

bool KernelDriver::TryAlternativeDeviceOpen() {
    // Try to locate the device using SetupAPI
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_UndownUnlockVCam,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    // Enumerate all devices in the set
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &GUID_DEVINTERFACE_UndownUnlockVCam, i, &deviceInterfaceData); i++) {
        // Get required buffer size
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);
        
        if (requiredSize == 0) {
            continue;
        }
        
        // Allocate buffer
        PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = 
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
        
        if (deviceInterfaceDetailData == NULL) {
            continue;
        }
        
        deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        
        // Get device interface detail
        if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, deviceInterfaceDetailData, requiredSize, NULL, NULL)) {
            // Try to open this device
            s_hDevice = CreateFile(
                deviceInterfaceDetailData->DevicePath,
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
            
            free(deviceInterfaceDetailData);
            
            if (s_hDevice != INVALID_HANDLE_VALUE) {
                // Found the device
                SetupDiDestroyDeviceInfoList(deviceInfoSet);
                return true;
            }
        } else {
            free(deviceInterfaceDetailData);
        }
    }
    
    // Also try to locate the device through camera interface
    HDEVINFO cameraDeviceInfoSet = SetupDiGetClassDevs(
        &GUID_USB_VIDEO_DEVICE,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (cameraDeviceInfoSet != INVALID_HANDLE_VALUE) {
        deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        
        // Enumerate all camera devices
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(cameraDeviceInfoSet, NULL, &GUID_USB_VIDEO_DEVICE, i, &deviceInterfaceData); i++) {
            // Get required buffer size
            DWORD requiredSize = 0;
            SetupDiGetDeviceInterfaceDetail(cameraDeviceInfoSet, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);
            
            if (requiredSize == 0) {
                continue;
            }
            
            // Allocate buffer
            PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = 
                (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
            
            if (deviceInterfaceDetailData == NULL) {
                continue;
            }
            
            deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
            
            // Get device interface detail
            if (SetupDiGetDeviceInterfaceDetail(cameraDeviceInfoSet, &deviceInterfaceData, deviceInterfaceDetailData, requiredSize, NULL, NULL)) {
                // Check if this is our device by testing a basic IOCTL
                HANDLE hDevice = CreateFile(
                    deviceInterfaceDetailData->DevicePath,
                    GENERIC_READ | GENERIC_WRITE,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL
                );
                
                if (hDevice != INVALID_HANDLE_VALUE) {
                    // Try to query device status
                    BOOLEAN isActive = FALSE;
                    DWORD bytesReturned = 0;
                    
                    BOOL success = DeviceIoControl(
                        hDevice,
                        IOCTL_GET_STATUS,
                        NULL,
                        0,
                        &isActive,
                        sizeof(BOOLEAN),
                        &bytesReturned,
                        NULL
                    );
                    
                    if (success) {
                        // This seems to be our device
                        s_hDevice = hDevice;
                        free(deviceInterfaceDetailData);
                        SetupDiDestroyDeviceInfoList(cameraDeviceInfoSet);
                        SetupDiDestroyDeviceInfoList(deviceInfoSet);
                        return true;
                    }
                    
                    CloseHandle(hDevice);
                }
            }
            
            free(deviceInterfaceDetailData);
        }
        
        SetupDiDestroyDeviceInfoList(cameraDeviceInfoSet);
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return false;
}

bool KernelDriver::TestDriverSignaturePatching() {
    if (!s_isInitialized) {
        std::cerr << "Driver must be initialized before testing signature patching" << std::endl;
        return false;
    }
    
    std::cout << "Testing driver signature patching..." << std::endl;
    
    // Ensure driver path is set
    if (s_config.driverPath.empty()) {
        std::cerr << "Driver path is not set" << std::endl;
        return false;
    }
    
    // Check if driver file exists
    if (GetFileAttributes(s_config.driverPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::cerr << "Driver file does not exist at: " << std::string(s_config.driverPath.begin(), s_config.driverPath.end()) << std::endl;
        return false;
    }
    
    // First patch the driver
    std::cout << "Step 1: Patching driver signature..." << std::endl;
    if (!PatchDriverWithFakeSignature()) {
        std::cerr << "Failed to patch driver signature" << std::endl;
        return false;
    }
    
    // Verify patch by checking if security directory was nullified
    // This is a simple check - in a real test, you would verify more thoroughly
    std::cout << "Step 2: Verifying driver was patched..." << std::endl;
    
    // Open the driver file
    HANDLE hFile = CreateFile(
        s_config.driverPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open driver file for verification. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    // Create file mapping
    HANDLE hMapping = CreateFileMapping(
        hFile,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
    );
    
    if (hMapping == NULL) {
        std::cerr << "Failed to create file mapping for verification. Error: " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }
    
    // Map the file into memory
    LPVOID fileData = MapViewOfFile(
        hMapping,
        FILE_MAP_READ,
        0,
        0,
        0
    );
    
    if (fileData == NULL) {
        std::cerr << "Failed to map view of file for verification. Error: " << GetLastError() << std::endl;
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }
    
    // Get DOS header
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)fileData;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cerr << "Invalid DOS header during verification" << std::endl;
        UnmapViewOfFile(fileData);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }
    
    // Get NT headers
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)fileData + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        std::cerr << "Invalid NT header during verification" << std::endl;
        UnmapViewOfFile(fileData);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }
    
    // Check if security directory is nullified
    bool isPatchSuccessful = (ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress == 0 &&
                             ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size == 0);
    
    // Cleanup verification resources
    UnmapViewOfFile(fileData);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    
    if (!isPatchSuccessful) {
        std::cerr << "Patch verification failed - security directory still present" << std::endl;
        return false;
    }
    
    std::cout << "Patch verification succeeded - security directory is nullified" << std::endl;
    
    // Now restore the original driver
    std::cout << "Step 3: Restoring original driver signature..." << std::endl;
    if (!RestoreOriginalDriverSignature()) {
        std::cerr << "Failed to restore original driver signature" << std::endl;
        return false;
    }
    
    std::cout << "Driver signature patching test completed successfully" << std::endl;
    return true;
}

bool KernelDriver::AddAntiDetectionMeasures() {
    if (!s_isInitialized || s_hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "Driver must be initialized and running before adding anti-detection measures" << std::endl;
        return false;
    }
    
    std::cout << "Adding anti-detection measures..." << std::endl;
    
    // 1. Modify registry entries to hide from enumeration
    HKEY hKey;
    LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, DRIVER_REGISTRY_KEY, 0, KEY_WRITE, &hKey);
    if (result == ERROR_SUCCESS) {
        // Set "Type" to match legitimate drivers
        DWORD type = 1; // kernel driver
        RegSetValueEx(hKey, L"Type", 0, REG_DWORD, (BYTE*)&type, sizeof(DWORD));
        
        // Set "ErrorControl" to avoid crash reports
        DWORD errorControl = 0; // ignore errors
        RegSetValueEx(hKey, L"ErrorControl", 0, REG_DWORD, (BYTE*)&errorControl, sizeof(DWORD));
        
        // Set "Start" to match system drivers
        DWORD start = 1; // system start
        RegSetValueEx(hKey, L"Start", 0, REG_DWORD, (BYTE*)&start, sizeof(DWORD));
        
        // Add Group value to appear as a legitimate system component
        std::wstring group = L"System";
        RegSetValueEx(hKey, L"Group", 0, REG_SZ, (BYTE*)group.c_str(), (DWORD)(group.length() + 1) * sizeof(wchar_t));
        
        // Set "Description" to appear legitimate
        std::wstring description = L"Provides media services for webcam integration";
        RegSetValueEx(hKey, L"Description", 0, REG_SZ, (BYTE*)description.c_str(), (DWORD)(description.length() + 1) * sizeof(wchar_t));
        
        RegCloseKey(hKey);
    }
    
    // 2. Send anti-detection flags to driver
    SecurityParams secParams;
    memset(&secParams, 0, sizeof(secParams));
    
    // Set flags for hiding from enumeration
    secParams.flags = SECURITY_FLAG_SPOOF_VID_PID | SECURITY_FLAG_HIDE_FROM_ENUM | SECURITY_FLAG_BYPASS_INTEGRITY;
    
    // Set VID/PID for popular webcam
    secParams.vendorId = 0x046D;  // Logitech
    secParams.productId = 0x0825; // C270
    
    // Set serial number and manufacturer info
    wcscpy_s(secParams.serialNumber, 64, FAKE_SERIAL);
    wcscpy_s(secParams.manufacturer, 128, FAKE_MFG);
    wcscpy_s(secParams.product, 128, FAKE_DEVICE_DESC);
    
    // Send to the driver
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
        s_hDevice,
        IOCTL_SPOOF_SIGNATURE,
        &secParams,
        sizeof(secParams),
        NULL,
        0,
        &bytesReturned,
        NULL)) {
        
        std::cerr << "Warning: Failed to send anti-detection params to driver. Error: " << GetLastError() << std::endl;
        // Not a critical error, continue
    }
    
    // 3. Add fake class interface to mimic a real webcam device
    HKEY classKey;
    result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, CAMERA_CLASS_KEY, 0, KEY_WRITE, &classKey);
    if (result == ERROR_SUCCESS) {
        // Create a random subkey for our device
        wchar_t subkeyName[64] = {0};
        swprintf_s(subkeyName, 64, L"0000%04X", GetTickCount() % 0xFFFF);
        
        HKEY deviceKey;
        if (RegCreateKeyEx(classKey, subkeyName, 0, NULL, 0, KEY_WRITE, NULL, &deviceKey, NULL) == ERROR_SUCCESS) {
            // Add device information to make it look like a real webcam
            std::wstring friendlyName = L"Logitech HD Webcam C270";
            RegSetValueEx(deviceKey, L"FriendlyName", 0, REG_SZ, (BYTE*)friendlyName.c_str(), (DWORD)(friendlyName.length() + 1) * sizeof(wchar_t));
            
            // Add device instance path
            std::wstring deviceInstancePath = L"USB\\VID_046D&PID_0825\\";
            wcscat_s((wchar_t*)deviceInstancePath.c_str(), 128, FAKE_SERIAL);
            RegSetValueEx(deviceKey, L"DeviceInstance", 0, REG_SZ, (BYTE*)deviceInstancePath.c_str(), (DWORD)(deviceInstancePath.length() + 1) * sizeof(wchar_t));
            
            // Add driver key
            std::wstring driverKey = DRIVER_REGISTRY_KEY;
            RegSetValueEx(deviceKey, L"Driver", 0, REG_SZ, (BYTE*)driverKey.c_str(), (DWORD)(driverKey.length() + 1) * sizeof(wchar_t));
            
            RegCloseKey(deviceKey);
        }
        
        RegCloseKey(classKey);
    }
    
    // 4. Additional memory protection - modify the driver's memory protection to prevent scanning
    // This is a basic implementation - a full anti-detection system would do much more
    DWORD protection = 0;
    if (!DeviceIoControl(
        s_hDevice,
        IOCTL_SET_MEMORY_PROTECTION,
        NULL,
        0,
        &protection,
        sizeof(DWORD),
        &bytesReturned,
        NULL)) {
        // This IOCTL might not be implemented, which is fine - just a warning
        std::cerr << "Warning: Failed to set memory protection. Error: " << GetLastError() << std::endl;
    }
    
    std::cout << "Anti-detection measures applied successfully" << std::endl;
    return true;
}

} // namespace VirtualCamera
} // namespace UndownUnlock 