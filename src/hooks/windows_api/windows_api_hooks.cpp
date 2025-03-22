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

bool WindowsAPIHooks::Initialize() {
    std::cout << "Installing Windows API hooks..." << std::endl;

    // Hook EmptyClipboard
    HMODULE hUser32 = GetModuleHandle(L"user32.dll");
    if (hUser32) {
        void* targetEmptyClipboard = GetProcAddress(hUser32, "EmptyClipboard");
        if (targetEmptyClipboard) {
            HookUtils::InstallHook(targetEmptyClipboard, (void*)HookedEmptyClipboard, s_originalBytesForEmptyClipboard);
        }
    }

    // Hook GetForegroundWindow
    if (hUser32) {
        void* targetGetForegroundWindow = GetProcAddress(hUser32, "GetForegroundWindow");
        if (targetGetForegroundWindow) {
            HookUtils::InstallHook(targetGetForegroundWindow, (void*)HookedGetForegroundWindow, s_originalBytesForGetForeground);
        }
    }

    // Hook TerminateProcess
    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    if (hKernel32) {
        void* targetTerminateProcess = GetProcAddress(hKernel32, "TerminateProcess");
        if (targetTerminateProcess) {
            HookUtils::InstallHook(targetTerminateProcess, (void*)HookedTerminateProcess, s_originalBytesForTerminateProcess);
        }
    }

    // Hook ExitProcess
    if (hKernel32) {
        void* targetExitProcess = GetProcAddress(hKernel32, "ExitProcess");
        if (targetExitProcess) {
            HookUtils::InstallHook(targetExitProcess, (void*)HookedExitProcess, s_originalBytesForExitProcess);
        }
    }

    // Show a message box to indicate successful injection
    MessageBox(NULL, L"UndownUnlock Windows API Hooks Injected", L"UndownUnlock", MB_OK);
    return true;
}

void WindowsAPIHooks::Shutdown() {
    std::cout << "Removing Windows API hooks..." << std::endl;
    
    // Unhook SetClipboardData
    HMODULE hUser32 = GetModuleHandle(L"user32.dll");
    if (hUser32) {
        void* targetSetClipboardData = GetProcAddress(hUser32, "SetClipboardData");
        if (targetSetClipboardData) {
            HookUtils::RemoveHook(targetSetClipboardData, s_originalBytesForSetClipboardData);
        }
    }

    // Unhook EmptyClipboard
    if (hUser32) {
        void* targetEmptyClipboard = GetProcAddress(hUser32, "EmptyClipboard");
        if (targetEmptyClipboard) {
            HookUtils::RemoveHook(targetEmptyClipboard, s_originalBytesForEmptyClipboard);
        }
    }

    // Unhook GetForegroundWindow
    if (hUser32) {
        void* targetGetForegroundWindow = GetProcAddress(hUser32, "GetForegroundWindow");
        if (targetGetForegroundWindow) {
            HookUtils::RemoveHook(targetGetForegroundWindow, s_originalBytesForGetForeground);
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

    // Hook BringWindowToTop
    HMODULE hUser32 = GetModuleHandle(L"user32.dll");
    if (hUser32) {
        void* targetShowWindow = GetProcAddress(hUser32, "BringWindowToTop");
        if (targetShowWindow) {
            HookUtils::InstallHook(targetShowWindow, (void*)HookedShowWindow, s_originalBytesForShowWindow);
        }
    }

    // Hook SetWindowPos
    if (hUser32) {
        void* targetSetWindowPos = GetProcAddress(hUser32, "SetWindowPos");
        if (targetSetWindowPos) {
            HookUtils::InstallHook(targetSetWindowPos, (void*)HookedSetWindowPos, s_originalBytesForSetWindowPos);
        }
    }

    // Hook SetFocus
    if (hUser32) {
        void* targetSetFocus = GetProcAddress(hUser32, "SetFocus");
        if (targetSetFocus) {
            HookUtils::InstallHook(targetSetFocus, (void*)HookedSetFocus, s_originalBytesForSetFocus);
        }
    }
}

void WindowsAPIHooks::UninstallFocusHooks() {
    if (!s_isFocusInstalled) {
        return;
    }
    
    s_isFocusInstalled = false;
    std::cout << "Uninstalling focus-related hooks..." << std::endl;

    // Unhook BringWindowToTop
    HMODULE hUser32 = GetModuleHandle(L"user32.dll");
    if (hUser32) {
        void* targetShowWindow = GetProcAddress(hUser32, "BringWindowToTop");
        if (targetShowWindow) {
            HookUtils::RemoveHook(targetShowWindow, s_originalBytesForShowWindow);
        }
    }

    // Unhook SetWindowPos
    if (hUser32) {
        void* targetSetWindowPos = GetProcAddress(hUser32, "SetWindowPos");
        if (targetSetWindowPos) {
            HookUtils::RemoveHook(targetSetWindowPos, s_originalBytesForSetWindowPos);
        }
    }

    // Unhook SetFocus
    if (hUser32) {
        void* targetSetFocus = GetProcAddress(hUser32, "SetFocus");
        if (targetSetFocus) {
            HookUtils::RemoveHook(targetSetFocus, s_originalBytesForSetFocus);
        }
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