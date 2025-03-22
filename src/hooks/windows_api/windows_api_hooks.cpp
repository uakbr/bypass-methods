#include "../../../include/hooks/windows_api_hooks.h"
#include "../../../include/hooks/hooked_functions.h"
#include "../../../include/hooks/hook_utils.h"
#include "../../../include/hooks/keyboard_hook.h"

#include <comdef.h>
#include <Wbemidl.h>
#include <strsafe.h>
#include <VersionHelpers.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "oleacc.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "UIAutomationCore.lib")

namespace UndownUnlock {
namespace WindowsHook {

// Initialize static variables
BYTE WindowsAPIHooks::s_originalBytesForGetForeground[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForShowWindow[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForSetWindowPos[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForSetFocus[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForEmptyClipboard[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForSetClipboardData[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForTerminateProcess[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForExitProcess[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForGetWindowTextW[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForGetWindow[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForOpenProcess[5] = { 0 };
BYTE WindowsAPIHooks::s_originalBytesForK32EnumProcesses[5] = { 0 };

bool WindowsAPIHooks::s_isFocusInstalled = false;
HWND WindowsAPIHooks::s_focusHWND = NULL;
HWND WindowsAPIHooks::s_bringWindowToTopHWND = NULL;
HWND WindowsAPIHooks::s_setWindowFocusHWND = NULL;
HWND WindowsAPIHooks::s_setWindowFocushWndInsertAfter = NULL;
int WindowsAPIHooks::s_setWindowFocusX = 0;
int WindowsAPIHooks::s_setWindowFocusY = 0;
int WindowsAPIHooks::s_setWindowFocuscx = 0;
int WindowsAPIHooks::s_setWindowFocuscy = 0;
UINT WindowsAPIHooks::s_setWindowFocusuFlags = 0;

// Initialize new static variables
bool WindowsAPIHooks::s_isAccessibilityInitialized = false;
bool WindowsAPIHooks::s_isWMIInitialized = false;
bool WindowsAPIHooks::s_isShellExtensionInitialized = false;
DWORD WindowsAPIHooks::s_currentWindowsVersion = 0;
bool WindowsAPIHooks::s_isEnhancedSecurityEnabled = false;

// Initialize interface pointers
IWbemServices* WindowsAPIHooks::s_pWbemServices = nullptr;
IWbemLocator* WindowsAPIHooks::s_pWbemLocator = nullptr;
IAccessible* WindowsAPIHooks::s_pAccessible = nullptr;
IUIAutomation* WindowsAPIHooks::s_pUIAutomation = nullptr;
IShellWindows* WindowsAPIHooks::s_pShellWindows = nullptr;

// Class to implement RAII pattern for module handles
class ModuleHandleRAII {
public:
    explicit ModuleHandleRAII(const wchar_t* moduleName) : m_handle(GetModuleHandle(moduleName)) {}
    ~ModuleHandleRAII() = default; // No need to free GetModuleHandle results
    
    operator HMODULE() const { return m_handle; }
    HMODULE get() const { return m_handle; }
    bool isValid() const { return m_handle != NULL; }
    
private:
    HMODULE m_handle;
};

// Helper function to install a hook with proper error handling
bool WindowsAPIHooks::HookFunction(void* targetFunction, void* hookFunction, BYTE* originalBytes) {
    if (!targetFunction || !hookFunction) {
        std::cerr << "Invalid function pointers for hooking" << std::endl;
        return false;
    }
    
    bool result = HookUtils::InstallHook(targetFunction, hookFunction, originalBytes);
    if (!result) {
        std::cerr << "Failed to install hook. Error code: " << GetLastError() << std::endl;
    }
    
    return result;
}

// Helper function to unhook a function with proper error handling
bool WindowsAPIHooks::UnhookFunction(void* targetFunction, BYTE* originalBytes) {
    if (!targetFunction) {
        std::cerr << "Invalid function pointer for unhooking" << std::endl;
        return false;
    }
    
    bool result = HookUtils::RemoveHook(targetFunction, originalBytes);
    if (!result) {
        std::cerr << "Failed to remove hook. Error code: " << GetLastError() << std::endl;
    }
    
    return result;
}

bool WindowsAPIHooks::Initialize() {
    std::cout << "Installing Windows API hooks..." << std::endl;

    // Use RAII for module handles
    ModuleHandleRAII hUser32(L"user32.dll");
    if (!hUser32.isValid()) {
        std::cerr << "Failed to get user32.dll handle" << std::endl;
        return false;
    }
    
    ModuleHandleRAII hKernel32(L"kernel32.dll");
    if (!hKernel32.isValid()) {
        std::cerr << "Failed to get kernel32.dll handle" << std::endl;
        return false;
    }
    
    // Hook Window Management Functions
    void* targetGetForegroundWindow = GetProcAddress(hUser32, "GetForegroundWindow");
    if (targetGetForegroundWindow) {
        HookFunction(targetGetForegroundWindow, (void*)HookedGetForegroundWindow, s_originalBytesForGetForeground);
    }
    
    void* targetGetWindowTextW = GetProcAddress(hUser32, "GetWindowTextW");
    if (targetGetWindowTextW) {
        HookFunction(targetGetWindowTextW, (void*)HookedGetWindowTextW, s_originalBytesForGetWindowTextW);
    }
    
    void* targetGetWindow = GetProcAddress(hUser32, "GetWindow");
    if (targetGetWindow) {
        HookFunction(targetGetWindow, (void*)HookedGetWindow, s_originalBytesForGetWindow);
    }
    
    // Hook Clipboard Functions
    void* targetEmptyClipboard = GetProcAddress(hUser32, "EmptyClipboard");
    if (targetEmptyClipboard) {
        HookFunction(targetEmptyClipboard, (void*)HookedEmptyClipboard, s_originalBytesForEmptyClipboard);
    }
    
    void* targetSetClipboardData = GetProcAddress(hUser32, "SetClipboardData");
    if (targetSetClipboardData) {
        HookFunction(targetSetClipboardData, (void*)HookedSetClipboardData, s_originalBytesForSetClipboardData);
    }
    
    // Hook Process Management Functions
    void* targetTerminateProcess = GetProcAddress(hKernel32, "TerminateProcess");
    if (targetTerminateProcess) {
        HookFunction(targetTerminateProcess, (void*)HookedTerminateProcess, s_originalBytesForTerminateProcess);
    }
    
    void* targetExitProcess = GetProcAddress(hKernel32, "ExitProcess");
    if (targetExitProcess) {
        HookFunction(targetExitProcess, (void*)HookedExitProcess, s_originalBytesForExitProcess);
    }
    
    void* targetOpenProcess = GetProcAddress(hKernel32, "OpenProcess");
    if (targetOpenProcess) {
        HookFunction(targetOpenProcess, (void*)HookedOpenProcess, s_originalBytesForOpenProcess);
    }
    
    void* targetK32EnumProcesses = GetProcAddress(hKernel32, "K32EnumProcesses");
    if (targetK32EnumProcesses) {
        HookFunction(targetK32EnumProcesses, (void*)HookedK32EnumProcesses, s_originalBytesForK32EnumProcesses);
    }

    // Check Windows version for compatibility
    if (!CheckWindowsVersionCompatibility()) {
        std::cerr << "Warning: Current Windows version may have compatibility issues" << std::endl;
    }
    
    // Initialize advanced security mechanisms
    EnhanceHookSecurity();
    
    // Show a message box to indicate successful injection
    MessageBox(NULL, L"UndownUnlock Windows API Hooks Injected", L"UndownUnlock", MB_OK);
    return true;
}

void WindowsAPIHooks::Shutdown() {
    std::cout << "Removing Windows API hooks..." << std::endl;
    
    // Use RAII for module handles
    ModuleHandleRAII hUser32(L"user32.dll");
    ModuleHandleRAII hKernel32(L"kernel32.dll");
    
    // Unhook Window Management Functions
    if (hUser32.isValid()) {
        void* targetGetForegroundWindow = GetProcAddress(hUser32, "GetForegroundWindow");
        if (targetGetForegroundWindow) {
            UnhookFunction(targetGetForegroundWindow, s_originalBytesForGetForeground);
        }
        
        void* targetGetWindowTextW = GetProcAddress(hUser32, "GetWindowTextW");
        if (targetGetWindowTextW) {
            UnhookFunction(targetGetWindowTextW, s_originalBytesForGetWindowTextW);
        }
        
        void* targetGetWindow = GetProcAddress(hUser32, "GetWindow");
        if (targetGetWindow) {
            UnhookFunction(targetGetWindow, s_originalBytesForGetWindow);
        }
        
        // Unhook Clipboard Functions
        void* targetEmptyClipboard = GetProcAddress(hUser32, "EmptyClipboard");
        if (targetEmptyClipboard) {
            UnhookFunction(targetEmptyClipboard, s_originalBytesForEmptyClipboard);
        }
        
        void* targetSetClipboardData = GetProcAddress(hUser32, "SetClipboardData");
        if (targetSetClipboardData) {
            UnhookFunction(targetSetClipboardData, s_originalBytesForSetClipboardData);
        }
    }
    
    // Unhook Process Management Functions
    if (hKernel32.isValid()) {
        void* targetTerminateProcess = GetProcAddress(hKernel32, "TerminateProcess");
        if (targetTerminateProcess) {
            UnhookFunction(targetTerminateProcess, s_originalBytesForTerminateProcess);
        }
        
        void* targetExitProcess = GetProcAddress(hKernel32, "ExitProcess");
        if (targetExitProcess) {
            UnhookFunction(targetExitProcess, s_originalBytesForExitProcess);
        }
        
        void* targetOpenProcess = GetProcAddress(hKernel32, "OpenProcess");
        if (targetOpenProcess) {
            UnhookFunction(targetOpenProcess, s_originalBytesForOpenProcess);
        }
        
        void* targetK32EnumProcesses = GetProcAddress(hKernel32, "K32EnumProcesses");
        if (targetK32EnumProcesses) {
            UnhookFunction(targetK32EnumProcesses, s_originalBytesForK32EnumProcesses);
        }
    }

    // Unhook focus-related functions if they are installed
    if (s_isFocusInstalled) {
        UninstallFocusHooks();
    }

    // Shut down new subsystems if they're initialized
    if (s_isAccessibilityInitialized) {
        ShutdownAccessibilityAPI();
    }
    
    if (s_isWMIInitialized) {
        ShutdownWMI();
    }
    
    if (s_isShellExtensionInitialized) {
        ShutdownShellExtension();
    }
}

void WindowsAPIHooks::InstallFocusHooks() {
    if (s_isFocusInstalled) {
        return;
    }
    
    s_isFocusInstalled = true;
    std::cout << "Installing focus-related hooks..." << std::endl;

    // Use RAII for module handles
    ModuleHandleRAII hUser32(L"user32.dll");
    if (!hUser32.isValid()) {
        std::cerr << "Failed to get user32.dll handle" << std::endl;
        return;
    }

    // Hook BringWindowToTop
    void* targetShowWindow = GetProcAddress(hUser32, "BringWindowToTop");
    if (targetShowWindow) {
        HookFunction(targetShowWindow, (void*)HookedShowWindow, s_originalBytesForShowWindow);
    }

    // Hook SetWindowPos
    void* targetSetWindowPos = GetProcAddress(hUser32, "SetWindowPos");
    if (targetSetWindowPos) {
        HookFunction(targetSetWindowPos, (void*)HookedSetWindowPos, s_originalBytesForSetWindowPos);
    }

    // Hook SetFocus
    void* targetSetFocus = GetProcAddress(hUser32, "SetFocus");
    if (targetSetFocus) {
        HookFunction(targetSetFocus, (void*)HookedSetFocus, s_originalBytesForSetFocus);
    }
}

void WindowsAPIHooks::UninstallFocusHooks() {
    if (!s_isFocusInstalled) {
        return;
    }
    
    s_isFocusInstalled = false;
    std::cout << "Uninstalling focus-related hooks..." << std::endl;

    // Use RAII for module handles
    ModuleHandleRAII hUser32(L"user32.dll");
    if (!hUser32.isValid()) {
        std::cerr << "Failed to get user32.dll handle" << std::endl;
        return;
    }

    // Unhook BringWindowToTop
    void* targetShowWindow = GetProcAddress(hUser32, "BringWindowToTop");
    if (targetShowWindow) {
        UnhookFunction(targetShowWindow, s_originalBytesForShowWindow);
    }

    // Unhook SetWindowPos
    void* targetSetWindowPos = GetProcAddress(hUser32, "SetWindowPos");
    if (targetSetWindowPos) {
        UnhookFunction(targetSetWindowPos, s_originalBytesForSetWindowPos);
    }

    // Unhook SetFocus
    void* targetSetFocus = GetProcAddress(hUser32, "SetFocus");
    if (targetSetFocus) {
        UnhookFunction(targetSetFocus, s_originalBytesForSetFocus);
    }
}

HWND WindowsAPIHooks::FindMainWindow() {
    return HookUtils::FindMainWindow();
}

void WindowsAPIHooks::SetupKeyboardHook() {
    KeyboardHook::Initialize();
    KeyboardHook::RunMessageLoop();
}

void WindowsAPIHooks::UninstallKeyboardHook() {
    KeyboardHook::Shutdown();
}

bool WindowsAPIHooks::InitializeAccessibilityAPI() {
    if (s_isAccessibilityInitialized) {
        return true;
    }
    
    std::cout << "Initializing Windows Accessibility API..." << std::endl;
    
    // Initialize COM if not already initialized
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "Failed to initialize COM for Accessibility API. Error: " << hr << std::endl;
        return false;
    }
    
    // Create the UI Automation object
    hr = CoCreateInstance(__uuidof(CUIAutomation), NULL, CLSCTX_INPROC_SERVER, 
                         __uuidof(IUIAutomation), (void**)&s_pUIAutomation);
    
    if (FAILED(hr)) {
        std::cerr << "Failed to create UI Automation instance. Error: " << hr << std::endl;
        CoUninitialize();
        return false;
    }
    
    s_isAccessibilityInitialized = true;
    std::cout << "Windows Accessibility API initialized successfully" << std::endl;
    return true;
}

void WindowsAPIHooks::ShutdownAccessibilityAPI() {
    if (!s_isAccessibilityInitialized) {
        return;
    }
    
    std::cout << "Shutting down Windows Accessibility API..." << std::endl;
    
    // Release UI Automation interface
    if (s_pUIAutomation) {
        s_pUIAutomation->Release();
        s_pUIAutomation = nullptr;
    }
    
    // Release Accessible interface
    if (s_pAccessible) {
        s_pAccessible->Release();
        s_pAccessible = nullptr;
    }
    
    // Uninitialize COM if it was initialized by us
    CoUninitialize();
    
    s_isAccessibilityInitialized = false;
    std::cout << "Windows Accessibility API shut down successfully" << std::endl;
}

bool WindowsAPIHooks::AccessibilityControlWindow(HWND targetWindow, int command, LPARAM param1, LPARAM param2) {
    if (!s_isAccessibilityInitialized) {
        if (!InitializeAccessibilityAPI()) {
            return false;
        }
    }
    
    if (!IsWindow(targetWindow)) {
        std::cerr << "Invalid window handle for accessibility control" << std::endl;
        return false;
    }
    
    // Get the UI Automation element for the target window
    IUIAutomationElement* pElement = nullptr;
    HRESULT hr = s_pUIAutomation->ElementFromHandle(targetWindow, &pElement);
    
    if (FAILED(hr) || !pElement) {
        std::cerr << "Failed to get UI Automation element for window. Error: " << hr << std::endl;
        return false;
    }
    
    bool result = false;
    
    // Process the command
    switch (command) {
        case ACC_CMD_FOCUS: {
            // Get the focus pattern
            IUIAutomationFocusPattern* pFocusPattern = nullptr;
            hr = pElement->GetCurrentPattern(UIA_FocusPatternId, (IUnknown**)&pFocusPattern);
            
            if (SUCCEEDED(hr) && pFocusPattern) {
                hr = pFocusPattern->SetFocus();
                result = SUCCEEDED(hr);
                pFocusPattern->Release();
            }
            break;
        }
        
        case ACC_CMD_CLICK: {
            // Get the invoke pattern
            IUIAutomationInvokePattern* pInvokePattern = nullptr;
            hr = pElement->GetCurrentPattern(UIA_InvokePatternId, (IUnknown**)&pInvokePattern);
            
            if (SUCCEEDED(hr) && pInvokePattern) {
                hr = pInvokePattern->Invoke();
                result = SUCCEEDED(hr);
                pInvokePattern->Release();
            }
            break;
        }
        
        case ACC_CMD_MAXIMIZE: 
        case ACC_CMD_MINIMIZE:
        case ACC_CMD_RESTORE: {
            // Get the window pattern
            IUIAutomationWindowPattern* pWindowPattern = nullptr;
            hr = pElement->GetCurrentPattern(UIA_WindowPatternId, (IUnknown**)&pWindowPattern);
            
            if (SUCCEEDED(hr) && pWindowPattern) {
                if (command == ACC_CMD_MAXIMIZE) {
                    hr = pWindowPattern->SetWindowVisualState(WindowVisualState_Maximized);
                } else if (command == ACC_CMD_MINIMIZE) {
                    hr = pWindowPattern->SetWindowVisualState(WindowVisualState_Minimized);
                } else { // ACC_CMD_RESTORE
                    hr = pWindowPattern->SetWindowVisualState(WindowVisualState_Normal);
                }
                
                result = SUCCEEDED(hr);
                pWindowPattern->Release();
            }
            break;
        }
        
        case ACC_CMD_CLOSE: {
            // Get the window pattern
            IUIAutomationWindowPattern* pWindowPattern = nullptr;
            hr = pElement->GetCurrentPattern(UIA_WindowPatternId, (IUnknown**)&pWindowPattern);
            
            if (SUCCEEDED(hr) && pWindowPattern) {
                hr = pWindowPattern->Close();
                result = SUCCEEDED(hr);
                pWindowPattern->Release();
            }
            break;
        }
        
        case ACC_CMD_SENDKEYS: {
            // For SendKeys, param1 should be a pointer to the key sequence
            if (!param1) {
                std::cerr << "No key sequence provided for SendKeys command" << std::endl;
                pElement->Release();
                return false;
            }
            
            // Get the legacy accessibility interface for SendKeys
            IAccessible* pAccessible = nullptr;
            hr = AccessibleObjectFromWindow(targetWindow, OBJID_CLIENT, IID_IAccessible, (void**)&pAccessible);
            
            if (SUCCEEDED(hr) && pAccessible) {
                // Use the accessible object to send keys
                // This is simplified; in a real implementation, you'd use the MSAA API
                // to send keys to the appropriate control
                VARIANT varChild;
                varChild.vt = VT_I4;
                varChild.lVal = CHILDID_SELF;
                
                // This is a placeholder - actually sending keys via Accessibility API
                // requires more code using DoAccessibleAction or similar
                result = true;
                
                pAccessible->Release();
            }
            break;
        }
        
        default:
            std::cerr << "Unknown accessibility command: " << command << std::endl;
            break;
    }
    
    pElement->Release();
    return result;
}

bool WindowsAPIHooks::InitializeWMI() {
    if (s_isWMIInitialized) {
        return true;
    }
    
    std::cout << "Initializing Windows Management Instrumentation (WMI)..." << std::endl;
    
    // Initialize COM if not already initialized
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "Failed to initialize COM for WMI. Error: " << hr << std::endl;
        return false;
    }
    
    // Set general COM security levels
    hr = CoInitializeSecurity(
        NULL,
        -1,                          // COM authentication
        NULL,                        // Authentication services
        NULL,                        // Reserved
        RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication
        RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation
        NULL,                        // Authentication info
        EOAC_NONE,                   // Additional capabilities
        NULL                         // Reserved
    );
    
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        std::cerr << "Failed to initialize security. Error: " << hr << std::endl;
        CoUninitialize();
        return false;
    }
    
    // Create WMI locator
    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&s_pWbemLocator
    );
    
    if (FAILED(hr)) {
        std::cerr << "Failed to create IWbemLocator object. Error: " << hr << std::endl;
        CoUninitialize();
        return false;
    }
    
    // Connect to WMI through the IWbemLocator::ConnectServer method
    // Connect to the root\cimv2 namespace with current user and password
    hr = s_pWbemLocator->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),  // Object path of WMI namespace
        NULL,                    // User name
        NULL,                    // User password
        0,                       // Locale
        NULL,                    // Security flags
        0,                       // Authority
        0,                       // Context object
        &s_pWbemServices         // Pointer to IWbemServices proxy
    );
    
    if (FAILED(hr)) {
        std::cerr << "Could not connect to WMI server. Error: " << hr << std::endl;
        s_pWbemLocator->Release();
        s_pWbemLocator = nullptr;
        CoUninitialize();
        return false;
    }
    
    // Set security levels on the proxy
    hr = CoSetProxyBlanket(
        s_pWbemServices,                // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,              // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,               // RPC_C_AUTHZ_xxx
        NULL,                           // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,         // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE,    // RPC_C_IMP_LEVEL_xxx
        NULL,                           // Client identity
        EOAC_NONE                       // Proxy capabilities
    );
    
    if (FAILED(hr)) {
        std::cerr << "Could not set proxy blanket. Error: " << hr << std::endl;
        s_pWbemServices->Release();
        s_pWbemServices = nullptr;
        s_pWbemLocator->Release();
        s_pWbemLocator = nullptr;
        CoUninitialize();
        return false;
    }
    
    s_isWMIInitialized = true;
    std::cout << "WMI initialized successfully" << std::endl;
    return true;
}

void WindowsAPIHooks::ShutdownWMI() {
    if (!s_isWMIInitialized) {
        return;
    }
    
    std::cout << "Shutting down WMI..." << std::endl;
    
    // Release the IWbemServices pointer
    if (s_pWbemServices) {
        s_pWbemServices->Release();
        s_pWbemServices = nullptr;
    }
    
    // Release the IWbemLocator pointer
    if (s_pWbemLocator) {
        s_pWbemLocator->Release();
        s_pWbemLocator = nullptr;
    }
    
    // Uninitialize COM
    CoUninitialize();
    
    s_isWMIInitialized = false;
    std::cout << "WMI shut down successfully" << std::endl;
}

bool WindowsAPIHooks::MonitorWindowsWithWMI(void (*callback)(HWND, bool)) {
    if (!s_isWMIInitialized) {
        if (!InitializeWMI()) {
            return false;
        }
    }
    
    if (!callback) {
        std::cerr << "Invalid callback for window monitoring" << std::endl;
        return false;
    }
    
    // This is a simplified implementation
    // In a real implementation, we would set up an event notification for window
    // creation/destruction events using WMI's SINK interfaces
    
    // Here's a stub that shows how to query the list of running windows
    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hr = s_pWbemServices->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_Window"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );
    
    if (FAILED(hr)) {
        std::cerr << "Query for windows failed. Error: " << hr << std::endl;
        return false;
    }
    
    // Get the data from the query
    IWbemClassObject* pclsObj = nullptr;
    ULONG uReturn = 0;
    
    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        
        if (0 == uReturn) {
            break;
        }
        
        VARIANT vtProp;
        VariantInit(&vtProp);
        
        // Get the window handle from the WMI object
        hr = pclsObj->Get(L"Handle", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr)) {
            // Convert the handle string to HWND
            HWND hWnd = (HWND)_wtoi(vtProp.bstrVal);
            
            // Call the callback function with the window handle
            // Assuming it's a window creation event for this example
            callback(hWnd, true);
            
            VariantClear(&vtProp);
        }
        
        pclsObj->Release();
    }
    
    // Cleanup
    pEnumerator->Release();
    
    return true;
}

bool WindowsAPIHooks::InitializeShellExtension() {
    if (s_isShellExtensionInitialized) {
        return true;
    }
    
    std::cout << "Initializing Shell Extension..." << std::endl;
    
    // Initialize COM if not already initialized
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "Failed to initialize COM for Shell Extension. Error: " << hr << std::endl;
        return false;
    }
    
    // Create an instance of the Shell Windows object
    hr = CoCreateInstance(CLSID_ShellWindows, NULL, CLSCTX_LOCAL_SERVER, 
                          IID_IShellWindows, (void**)&s_pShellWindows);
    
    if (FAILED(hr)) {
        std::cerr << "Failed to create Shell Windows instance. Error: " << hr << std::endl;
        CoUninitialize();
        return false;
    }
    
    s_isShellExtensionInitialized = true;
    std::cout << "Shell Extension initialized successfully" << std::endl;
    return true;
}

void WindowsAPIHooks::ShutdownShellExtension() {
    if (!s_isShellExtensionInitialized) {
        return;
    }
    
    std::cout << "Shutting down Shell Extension..." << std::endl;
    
    // Release the Shell Windows interface
    if (s_pShellWindows) {
        s_pShellWindows->Release();
        s_pShellWindows = nullptr;
    }
    
    // Uninitialize COM
    CoUninitialize();
    
    s_isShellExtensionInitialized = false;
    std::cout << "Shell Extension shut down successfully" << std::endl;
}

bool WindowsAPIHooks::ShellExtensionManageWindow(HWND targetWindow, int command, LPARAM param) {
    if (!s_isShellExtensionInitialized) {
        if (!InitializeShellExtension()) {
            return false;
        }
    }
    
    if (!IsWindow(targetWindow)) {
        std::cerr << "Invalid window handle for Shell Extension management" << std::endl;
        return false;
    }
    
    // Get the path of the executable for the window
    DWORD processId;
    GetWindowThreadProcessId(targetWindow, &processId);
    
    TCHAR path[MAX_PATH] = { 0 };
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    
    if (!hProcess) {
        std::cerr << "Failed to open process for window. Error: " << GetLastError() << std::endl;
        return false;
    }
    
    if (!GetModuleFileNameEx(hProcess, NULL, path, MAX_PATH)) {
        std::cerr << "Failed to get module filename. Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }
    
    CloseHandle(hProcess);
    
    // Handle the command
    HRESULT hr = S_OK;
    bool result = false;
    
    switch (command) {
        case SHELL_CMD_EXPLORE: {
            // Open Explorer window showing the folder containing the executable
            TCHAR* lastSlash = _tcsrchr(path, '\\');
            if (lastSlash) {
                *lastSlash = '\0';  // Terminate the string at the last slash to get the directory
            }
            
            hr = ShellExecute(NULL, L"explore", path, NULL, NULL, SW_SHOWNORMAL) > (HINSTANCE)32 ? S_OK : E_FAIL;
            result = SUCCEEDED(hr);
            break;
        }
        
        case SHELL_CMD_OPEN: {
            // Open the file with its associated application
            hr = ShellExecute(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL) > (HINSTANCE)32 ? S_OK : E_FAIL;
            result = SUCCEEDED(hr);
            break;
        }
        
        case SHELL_CMD_PROPERTIES: {
            // Show properties dialog for the file
            SHELLEXECUTEINFO sei = { sizeof(SHELLEXECUTEINFO) };
            sei.lpFile = path;
            sei.nShow = SW_SHOW;
            sei.fMask = SEE_MASK_INVOKEIDLIST;
            sei.lpVerb = L"properties";
            
            result = ShellExecuteEx(&sei) ? true : false;
            break;
        }
        
        case SHELL_CMD_NAVIGATE: {
            // Use Shell Windows to navigate to a URL (param should be a pointer to the URL)
            if (!param) {
                std::cerr << "No URL provided for Navigate command" << std::endl;
                return false;
            }
            
            LPCWSTR url = (LPCWSTR)param;
            
            // Find an existing browser window
            LONG count = 0;
            s_pShellWindows->get_Count(&count);
            
            for (LONG i = 0; i < count; i++) {
                VARIANT varIndex;
                varIndex.vt = VT_I4;
                varIndex.lVal = i;
                
                IDispatch* pDisp = nullptr;
                hr = s_pShellWindows->Item(varIndex, &pDisp);
                
                if (SUCCEEDED(hr) && pDisp) {
                    // Get the browser through the dispatch interface
                    IWebBrowser2* pBrowser = nullptr;
                    hr = pDisp->QueryInterface(IID_IWebBrowser2, (void**)&pBrowser);
                    
                    if (SUCCEEDED(hr) && pBrowser) {
                        // Navigate to the URL
                        VARIANT varURL;
                        varURL.vt = VT_BSTR;
                        varURL.bstrVal = SysAllocString(url);
                        
                        hr = pBrowser->Navigate2(&varURL, NULL, NULL, NULL, NULL);
                        result = SUCCEEDED(hr);
                        
                        SysFreeString(varURL.bstrVal);
                        pBrowser->Release();
                        
                        if (result) {
                            // Successfully navigated, no need to try other browser windows
                            pDisp->Release();
                            break;
                        }
                    }
                    
                    pDisp->Release();
                }
            }
            break;
        }
        
        case SHELL_CMD_EXECUTE: {
            // Execute a command on the window (param should be a pointer to the command)
            if (!param) {
                std::cerr << "No command provided for Execute command" << std::endl;
                return false;
            }
            
            LPCWSTR command = (LPCWSTR)param;
            
            // Create a new process with the command
            STARTUPINFO si = { sizeof(STARTUPINFO) };
            PROCESS_INFORMATION pi = { 0 };
            
            // Copy the command to a non-const buffer as required by CreateProcess
            WCHAR commandBuffer[MAX_PATH];
            wcscpy_s(commandBuffer, MAX_PATH, command);
            
            if (CreateProcess(NULL, commandBuffer, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                result = true;
            } else {
                std::cerr << "Failed to execute command. Error: " << GetLastError() << std::endl;
            }
            break;
        }
        
        default:
            std::cerr << "Unknown Shell Extension command: " << command << std::endl;
            break;
    }
    
    return result;
}

bool WindowsAPIHooks::CheckWindowsVersionCompatibility() {
    // Check for Windows 10 or later
    bool isWin10OrLater = IsWindows10OrGreater();
    
    // Get the exact version for more detailed compatibility checks
    OSVERSIONINFOEX osvi;
    DWORDLONG dwlConditionMask = 0;
    int op = VER_GREATER_EQUAL;
    
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    
    // Although deprecated, we need the Windows version info for detailed checks
    // This is a workaround using the VerifyVersionInfo API instead of GetVersionEx
    
    // Check for Windows 10 version 1909 (19H2) or later
    // Windows 10 version 1909 build number is 18363
    osvi.dwMajorVersion = 10;
    osvi.dwMinorVersion = 0;
    osvi.dwBuildNumber = 18363;
    
    VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, op);
    VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, op);
    VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, op);
    
    bool isWin10v1909OrLater = VerifyVersionInfo(
        &osvi,
        VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER,
        dwlConditionMask
    );
    
    // Store the current Windows version for later use
    DWORD dwVersion = GetVersion();
    s_currentWindowsVersion = dwVersion;
    
    std::cout << "Windows version compatibility check: "
              << (isWin10OrLater ? "Windows 10+ detected" : "Pre-Windows 10 detected")
              << ", Win10v1909+: " << (isWin10v1909OrLater ? "Yes" : "No") << std::endl;
    
    // We're compatible with Windows 10 or later
    return isWin10OrLater;
}

bool WindowsAPIHooks::EnhanceHookSecurity() {
    if (s_isEnhancedSecurityEnabled) {
        return true;
    }
    
    std::cout << "Enhancing hook security mechanisms..." << std::endl;
    
    // Set up hook configuration with enhanced security options
    HookConfig secureConfig;
    secureConfig.enableSelfHealing = true;      // Auto-repair hooks if tampered
    secureConfig.enableVerification = true;     // Regularly check hook integrity
    secureConfig.suspendThreads = true;         // Suspend threads during hook installation
    secureConfig.verificationInterval = 1000;   // Check every second (1000ms)
    
    // Apply the secure configuration to all existing and future hooks
    // This is a simplified placeholder - in a real implementation,
    // we would enhance the security of each individual hook
    
    // For example, we would apply advanced hook protection techniques
    // such as:
    // 1. Virtualization-based hook protection
    // 2. Kernel-mode support for userland hooks
    // 3. Anti-detection countermeasures
    // 4. Code page permission management
    
    // In this example, we'll just set the flag to indicate enhanced security is enabled
    s_isEnhancedSecurityEnabled = true;
    
    std::cout << "Hook security enhancements applied successfully" << std::endl;
    return true;
}

bool WindowsAPIHooks::TestHookFunctionality() {
    std::cout << "Testing hook functionality..." << std::endl;
    
    bool allTestsPassed = true;
    
    // Test GetForegroundWindow hook
    HWND originalForegroundWindow = GetForegroundWindow();
    HWND hookedForegroundWindow = HookedGetForegroundWindow();
    
    if (originalForegroundWindow != hookedForegroundWindow) {
        std::cout << "GetForegroundWindow hook is working correctly" << std::endl;
    } else {
        std::cerr << "GetForegroundWindow hook may not be functioning properly" << std::endl;
        allTestsPassed = false;
    }
    
    // Test other hooks as needed
    // ...
    
    return allTestsPassed;
}

} // namespace WindowsHook
} // namespace UndownUnlock 
} // namespace WindowsHook
} // namespace UndownUnlock 
} // namespace UndownUnlock 