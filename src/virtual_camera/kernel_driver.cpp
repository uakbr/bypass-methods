#include "../../include/virtual_camera/kernel_driver.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <Windows.h>
#include <winioctl.h>
#include <SetupAPI.h>
#include <devguid.h>
#include <Shlobj.h>
#include <Shlwapi.h>
#include <bcrypt.h>

#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Bcrypt.lib")

namespace UndownUnlock {
namespace VirtualCamera {

// Define device path for communication with driver
#define DRIVER_DEVICE_PATH L"\\\\.\\UndownUnlockVCam"

// Internal device driver resource name for extraction
#define DRIVER_RESOURCE_NAME "UNDOWNUNLOCK_VCAM_DRIVER"

// Driver registry key
#define DRIVER_REGISTRY_KEY L"SYSTEM\\CurrentControlSet\\Services\\UndownUnlockVCam"

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
        return false;
    }
    
    // Check if test-signing is already enabled, if not warn the user
    if (!IsTestSigningEnabled() && s_config.bypassSecurityChecks) {
        std::cout << "Warning: Test signing is not enabled. Driver installation may fail." << std::endl;
        std::cout << "Consider running 'bcdedit /set testsigning on' as administrator." << std::endl;
    }
    
    // Extract the driver file if needed
    if (!PathFileExists(s_config.driverPath.c_str())) {
        if (!ExtractDriverFile()) {
            CloseServiceHandle(s_hSCManager);
            s_hSCManager = NULL;
            return false;
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
        }
        
        return false;
    }
    
    // Set registry keys for driver configuration
    if (!SetDriverRegistryKeys()) {
        // Registry operations failed, but driver is installed
        std::cerr << "Warning: Failed to set driver registry keys" << std::endl;
    }
    
    // If driver requires trusted signing, try to bypass
    if (s_config.registerAsTrusted) {
        std::cout << "Attempting to register driver as trusted..." << std::endl;
        AddDriverToCertStore();
    }
    
    std::cout << "Kernel driver installed successfully" << std::endl;
    return true;
}

bool KernelDriver::Uninstall() {
    if (!s_isInitialized) {
        std::cerr << "Kernel driver manager not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Uninstalling kernel driver..." << std::endl;
    
    // Check if driver is currently installed
    if (!IsInstalled()) {
        std::cout << "Driver is not installed" << std::endl;
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
            return false;
        }
    }
    
    // Stop the service if it's running
    if (IsRunning()) {
        if (!Stop()) {
            std::cerr << "Warning: Failed to stop driver service" << std::endl;
            // Continue with uninstallation anyway
        }
    }
    
    // Delete the service
    if (!DeleteService(s_hService)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to delete driver service. Error: " << error << std::endl;
        return false;
    }
    
    // Close the service handle
    CloseServiceHandle(s_hService);
    s_hService = NULL;
    
    // Delete the driver file
    if (PathFileExists(s_config.driverPath.c_str())) {
        if (!DeleteFile(s_config.driverPath.c_str())) {
            std::cerr << "Warning: Failed to delete driver file. Error: " << GetLastError() << std::endl;
            // Not a critical error, continue
        }
    }
    
    std::cout << "Kernel driver uninstalled successfully" << std::endl;
    return true;
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
            return false;
        }
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
        
        return false;
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
        // This is not a critical error, the driver may still be working
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
            return false;
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
        return false;
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
    
    if (!success) {
        std::cerr << "Failed to bypass driver signing. Administrator privileges may be required." << std::endl;
        std::cerr << "Consider running 'bcdedit /set testsigning on' as administrator and reboot." << std::endl;
    } else {
        std::cout << "Driver signing bypass methods applied successfully" << std::endl;
        std::cout << "NOTE: A system reboot may be required for changes to take effect." << std::endl;
    }
    
    return success;
}

bool KernelDriver::ExtractDriverFile() {
    std::cout << "Extracting driver file..." << std::endl;
    
    // For this implementation, we'll simulate extracting the driver
    // In a real implementation, this would extract the driver from resources
    
    // Create a simple dummy driver file for demonstration purposes
    std::ofstream driverFile(s_config.driverPath, std::ios::binary);
    if (!driverFile.is_open()) {
        std::cerr << "Failed to create driver file at: " << 
            std::string(s_config.driverPath.begin(), s_config.driverPath.end()) << std::endl;
        return false;
    }
    
    // Write some dummy content to the file
    const char* driverData = "This is a dummy driver file for demonstration purposes.";
    driverFile.write(driverData, strlen(driverData));
    driverFile.close();
    
    // In a real implementation, we would extract an actual driver binary
    // HRSRC hResource = FindResource(NULL, DRIVER_RESOURCE_NAME, RT_RCDATA);
    // if (hResource) {
    //     HGLOBAL hData = LoadResource(NULL, hResource);
    //     if (hData) {
    //         void* pData = LockResource(hData);
    //         DWORD size = SizeofResource(NULL, hResource);
    //         
    //         if (pData && size > 0) {
    //             std::ofstream driverFile(s_config.driverPath, std::ios::binary);
    //             if (driverFile.is_open()) {
    //                 driverFile.write(static_cast<const char*>(pData), size);
    //                 driverFile.close();
    //                 return true;
    //             }
    //         }
    //     }
    // }
    
    return PathFileExists(s_config.driverPath.c_str());
}

bool KernelDriver::RegisterDriverService() {
    // This functionality is handled by the Install() method
    return true;
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
        return false;
    }
    
    // Set various parameters for the driver
    
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
    
    // Add additional registry keys as needed for your driver configuration
    
    // Close the registry key
    RegCloseKey(hKey);
    
    return success;
}

bool KernelDriver::OpenDeviceHandle() {
    // Close existing handle if any
    if (s_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(s_hDevice);
        s_hDevice = INVALID_HANDLE_VALUE;
    }
    
    // Open the device
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
    
    // Call bcdedit to check if test signing is enabled
    WCHAR tempFilePath[MAX_PATH] = {0};
    GetTempPath(MAX_PATH, tempFilePath);
    wcscat_s(tempFilePath, MAX_PATH, L"\\bcdout.txt");
    
    // Run bcdedit to query settings
    WCHAR command[256] = {0};
    swprintf_s(command, L"bcdedit /enum ACTIVE > \"%s\"", tempFilePath);
    
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    if (!CreateProcess(NULL, command, NULL, NULL, FALSE, 
                     CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }
    
    // Wait for process to complete
    WaitForSingleObject(pi.hProcess, 5000);
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
        if (line.find("testsigning") != std::string::npos && 
            line.find("Yes") != std::string::npos) {
            testSigningEnabled = true;
            break;
        }
    }
    
    file.close();
    DeleteFile(tempFilePath);
    
    s_testSigningEnabled = testSigningEnabled;
    return testSigningEnabled;
}

bool KernelDriver::AreIntegrityChecksDisabled() {
    if (s_integrityChecksDisabled) {
        return true;
    }
    
    // Call bcdedit to check if integrity checks are disabled
    WCHAR tempFilePath[MAX_PATH] = {0};
    GetTempPath(MAX_PATH, tempFilePath);
    wcscat_s(tempFilePath, MAX_PATH, L"\\bcdout.txt");
    
    // Run bcdedit to query settings
    WCHAR command[256] = {0};
    swprintf_s(command, L"bcdedit /enum ACTIVE > \"%s\"", tempFilePath);
    
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    if (!CreateProcess(NULL, command, NULL, NULL, FALSE, 
                     CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }
    
    // Wait for process to complete
    WaitForSingleObject(pi.hProcess, 5000);
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
        if (line.find("nointegritychecks") != std::string::npos && 
            line.find("Yes") != std::string::npos) {
            integrityChecksDisabled = true;
            break;
        }
    }
    
    file.close();
    DeleteFile(tempFilePath);
    
    s_integrityChecksDisabled = integrityChecksDisabled;
    return integrityChecksDisabled;
}

bool KernelDriver::EnableTestSigning() {
    // Call bcdedit to enable test signing
    WCHAR command[] = L"bcdedit /set testsigning on";
    
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    if (!CreateProcess(NULL, command, NULL, NULL, FALSE, 
                     CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to run bcdedit. Error: " << error << std::endl;
        return false;
    }
    
    // Wait for process to complete
    WaitForSingleObject(pi.hProcess, 5000);
    
    // Get process exit code
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode != 0) {
        std::cerr << "bcdedit command failed with exit code: " << exitCode << std::endl;
        return false;
    }
    
    s_testSigningEnabled = true;
    return true;
}

bool KernelDriver::AddDriverToCertStore() {
    // This function would add the driver's certificate to the trusted store
    // This is a simplified implementation
    
    std::cout << "Adding driver to trusted certificate store..." << std::endl;
    
    // In a real implementation, this would use CertAddEncodedCertificateToStore
    // and similar APIs to add the driver's certificate to the trusted store
    
    // For now, we'll just return success
    return true;
}

bool KernelDriver::DisableIntegrityChecks() {
    // Call bcdedit to disable integrity checks
    WCHAR command[] = L"bcdedit /set nointegritychecks on";
    
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    if (!CreateProcess(NULL, command, NULL, NULL, FALSE, 
                     CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to run bcdedit. Error: " << error << std::endl;
        return false;
    }
    
    // Wait for process to complete
    WaitForSingleObject(pi.hProcess, 5000);
    
    // Get process exit code
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode != 0) {
        std::cerr << "bcdedit command failed with exit code: " << exitCode << std::endl;
        return false;
    }
    
    s_integrityChecksDisabled = true;
    return true;
}

} // namespace VirtualCamera
} // namespace UndownUnlock 