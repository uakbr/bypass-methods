#include "../../../include/hooks/windows_api_hooks.h"
#include "../../../include/hooks/hooked_functions.h"
#include "../../../include/hooks/hook_utils.h"
#include "../../../include/hooks/keyboard_hook.h"

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

} // namespace WindowsHook
} // namespace UndownUnlock 
} // namespace WindowsHook
} // namespace UndownUnlock 
} // namespace UndownUnlock 