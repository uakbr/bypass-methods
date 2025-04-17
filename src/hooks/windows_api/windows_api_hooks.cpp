#include "../../../include/hooks/windows_api_hooks.h"
#include "../../../include/hooks/hooked_functions.h"
#include "../../../include/hooks/hook_utils.h"
#include "../../../include/hooks/keyboard_hook.h" // Include for keyboard hook management

namespace UndownUnlock {
namespace WindowsHook {

// Initialize static variables
std::unordered_map<std::string, HookInfo> WindowsAPIHooks::s_hookMap;
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

// Helper to install a specific hook and manage its info
bool WindowsAPIHooks::InstallApiHook(const char* moduleName, const char* functionName, void* hookFunction, const std::string& hookName) {
    void* targetFunction = HookUtils::GetFunctionAddress(moduleName, functionName);
    if (!targetFunction) {
        std::cerr << "Failed to get address for " << functionName << std::endl;
        return false;
    }
    
    if (HookUtils::InstallHook(targetFunction, hookFunction, hookName)) {
        // Optionally store hook info locally if needed for specific logic
        // HookInfo info;
        // info.targetFunction = targetFunction;
        // info.hookFunction = hookFunction;
        // HookUtils::GetOriginalBytes(hookName, info.originalBytes); // Retrieve bytes if needed
        // info.isActive = true;
        // s_hookMap[hookName] = info;
        return true;
    }
    return false;
}

// Helper to remove a specific hook
bool WindowsAPIHooks::RemoveApiHook(const std::string& hookName) {
    // We primarily rely on HookUtils to manage the removal
    if (HookUtils::RemoveHook(hookName)) {
        // Remove from local map if it exists
        s_hookMap.erase(hookName);
        return true;
    }
    return false;
}

bool WindowsAPIHooks::Initialize() {
    std::cout << "Installing Windows API hooks..." << std::endl;
    bool success = true;

    // Install hooks using the helper function
    success &= InstallApiHook("user32.dll", "EmptyClipboard", (void*)HookedEmptyClipboard, "EmptyClipboard");
    success &= InstallApiHook("user32.dll", "SetClipboardData", (void*)HookedSetClipboardData, "SetClipboardData");
    success &= InstallApiHook("user32.dll", "GetForegroundWindow", (void*)HookedGetForegroundWindow, "GetForegroundWindow");
    success &= InstallApiHook("kernel32.dll", "TerminateProcess", (void*)HookedTerminateProcess, "TerminateProcess");
    success &= InstallApiHook("kernel32.dll", "ExitProcess", (void*)HookedExitProcess, "ExitProcess");
    success &= InstallApiHook("kernel32.dll", "OpenProcess", (void*)HookedOpenProcess, "OpenProcess");
    success &= InstallApiHook("user32.dll", "GetWindowTextW", (void*)HookedGetWindowTextW, "GetWindowTextW");
    success &= InstallApiHook("user32.dll", "GetWindow", (void*)HookedGetWindow, "GetWindow");
    // Consider hooking K32EnumProcesses if necessary, might require finding it in kernelbase.dll or kernel32.dll depending on OS
    // success &= InstallApiHook("kernel32.dll", "K32EnumProcesses", (void*)HookedK32EnumProcesses, "K32EnumProcesses");

    if(success) {
        // Show a message box to indicate successful injection
        MessageBox(NULL, L"UndownUnlock Windows API Hooks Initialized", L"UndownUnlock", MB_OK);
    } else {
         MessageBox(NULL, L"UndownUnlock Windows API Hooks Failed Initialization", L"UndownUnlock Error", MB_OK | MB_ICONERROR);
    }

    return success;
}

void WindowsAPIHooks::Shutdown() {
    std::cout << "Removing Windows API hooks..." << std::endl;

    // Remove hooks using the helper function
    RemoveApiHook("EmptyClipboard");
    RemoveApiHook("SetClipboardData");
    RemoveApiHook("GetForegroundWindow");
    RemoveApiHook("TerminateProcess");
    RemoveApiHook("ExitProcess");
    RemoveApiHook("OpenProcess");
    RemoveApiHook("GetWindowTextW");
    RemoveApiHook("GetWindow");
    // RemoveApiHook("K32EnumProcesses");

    // Uninstall focus hooks if they were installed
    UninstallFocusHooks(); // Ensures focus hooks are also removed on general shutdown
}

void WindowsAPIHooks::InstallFocusHooks() {
    if (s_isFocusInstalled) {
        return;
    }

    std::cout << "Installing focus-related hooks..." << std::endl;
    bool success = true;

    // Install focus-related hooks
    success &= InstallApiHook("user32.dll", "BringWindowToTop", (void*)HookedShowWindow, "BringWindowToTop"); // Note: HookedShowWindow handles this logic
    success &= InstallApiHook("user32.dll", "SetWindowPos", (void*)HookedSetWindowPos, "SetWindowPos");
    success &= InstallApiHook("user32.dll", "SetFocus", (void*)HookedSetFocus, "SetFocus");

    s_isFocusInstalled = success;
    if (!success) {
        std::cerr << "Failed to install one or more focus hooks." << std::endl;
        // Attempt to uninstall any that might have succeeded
        UninstallFocusHooks();
    }
}

void WindowsAPIHooks::UninstallFocusHooks() {
    if (!s_isFocusInstalled) {
        return;
    }

    std::cout << "Uninstalling focus-related hooks..." << std::endl;

    // Remove focus-related hooks
    RemoveApiHook("BringWindowToTop");
    RemoveApiHook("SetWindowPos");
    RemoveApiHook("SetFocus");

    s_isFocusInstalled = false; // Mark as uninstalled regardless of RemoveApiHook return value
}

HWND WindowsAPIHooks::FindMainWindow() {
    return HookUtils::FindMainWindow();
}

} // namespace WindowsHook
} // namespace UndownUnlock 