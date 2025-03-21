#include "../../../include/hooks/windows_api_hooks.h"
#include "../../../include/hooks/hooked_functions.h"
#include <psapi.h>

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
HHOOK WindowsAPIHooks::s_keyboardHook = NULL;

bool WindowsAPIHooks::Initialize() {
    std::cout << "Installing Windows API hooks..." << std::endl;
    DWORD oldProtect;

    // Hook EmptyClipboard
    HMODULE hUser32 = GetModuleHandle(L"user32.dll");
    if (hUser32) {
        void* targetEmptyClipboard = GetProcAddress(hUser32, "EmptyClipboard");
        if (targetEmptyClipboard) {
            HookFunction(targetEmptyClipboard, (void*)HookedEmptyClipboard, s_originalBytesForEmptyClipboard);
        }
    }

    // Hook GetForegroundWindow
    if (hUser32) {
        void* targetGetForegroundWindow = GetProcAddress(hUser32, "GetForegroundWindow");
        if (targetGetForegroundWindow) {
            HookFunction(targetGetForegroundWindow, (void*)HookedGetForegroundWindow, s_originalBytesForGetForeground);
        }
    }

    // Hook TerminateProcess
    HMODULE hKernel32 = GetModuleHandle(L"kernel32.dll");
    if (hKernel32) {
        void* targetTerminateProcess = GetProcAddress(hKernel32, "TerminateProcess");
        if (targetTerminateProcess) {
            HookFunction(targetTerminateProcess, (void*)HookedTerminateProcess, s_originalBytesForTerminateProcess);
        }
    }

    // Hook ExitProcess
    if (hKernel32) {
        void* targetExitProcess = GetProcAddress(hKernel32, "ExitProcess");
        if (targetExitProcess) {
            HookFunction(targetExitProcess, (void*)HookedExitProcess, s_originalBytesForExitProcess);
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
            UnhookFunction(targetSetClipboardData, s_originalBytesForSetClipboardData);
        }
    }

    // Unhook EmptyClipboard
    if (hUser32) {
        void* targetEmptyClipboard = GetProcAddress(hUser32, "EmptyClipboard");
        if (targetEmptyClipboard) {
            UnhookFunction(targetEmptyClipboard, s_originalBytesForEmptyClipboard);
        }
    }

    // Unhook GetForegroundWindow
    if (hUser32) {
        void* targetGetForegroundWindow = GetProcAddress(hUser32, "GetForegroundWindow");
        if (targetGetForegroundWindow) {
            UnhookFunction(targetGetForegroundWindow, s_originalBytesForGetForeground);
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
            HookFunction(targetShowWindow, (void*)HookedShowWindow, s_originalBytesForShowWindow);
        }
    }

    // Hook SetWindowPos
    if (hUser32) {
        void* targetSetWindowPos = GetProcAddress(hUser32, "SetWindowPos");
        if (targetSetWindowPos) {
            HookFunction(targetSetWindowPos, (void*)HookedSetWindowPos, s_originalBytesForSetWindowPos);
        }
    }

    // Hook SetFocus
    if (hUser32) {
        void* targetSetFocus = GetProcAddress(hUser32, "SetFocus");
        if (targetSetFocus) {
            HookFunction(targetSetFocus, (void*)HookedSetFocus, s_originalBytesForSetFocus);
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
            UnhookFunction(targetShowWindow, s_originalBytesForShowWindow);
        }
    }

    // Unhook SetWindowPos
    if (hUser32) {
        void* targetSetWindowPos = GetProcAddress(hUser32, "SetWindowPos");
        if (targetSetWindowPos) {
            UnhookFunction(targetSetWindowPos, s_originalBytesForSetWindowPos);
        }
    }

    // Unhook SetFocus
    if (hUser32) {
        void* targetSetFocus = GetProcAddress(hUser32, "SetFocus");
        if (targetSetFocus) {
            UnhookFunction(targetSetFocus, s_originalBytesForSetFocus);
        }
    }
}

bool WindowsAPIHooks::HookFunction(void* targetFunction, void* hookFunction, BYTE* originalBytes) {
    if (!targetFunction || !hookFunction || !originalBytes) {
        return false;
    }

    DWORD oldProtect;
    // Save the original bytes for later restoration
    memcpy(originalBytes, targetFunction, 5);

    // Change memory protection to allow writing
    if (VirtualProtect(targetFunction, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        // Write a JMP instruction (0xE9) followed by a relative address
        DWORD relativeAddress = (DWORD)hookFunction - (DWORD)targetFunction - 5;
        *((BYTE*)targetFunction) = 0xE9;
        *((DWORD*)((BYTE*)targetFunction + 1)) = relativeAddress;

        // Restore the original memory protection
        VirtualProtect(targetFunction, 5, oldProtect, &oldProtect);
        return true;
    }

    return false;
}

bool WindowsAPIHooks::UnhookFunction(void* targetFunction, BYTE* originalBytes) {
    if (!targetFunction || !originalBytes) {
        return false;
    }

    DWORD oldProtect;
    // Change memory protection to allow writing
    if (VirtualProtect(targetFunction, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        // Restore the original bytes
        memcpy(targetFunction, originalBytes, 5);

        // Restore the original memory protection
        VirtualProtect(targetFunction, 5, oldProtect, &oldProtect);
        return true;
    }

    return false;
}

HWND WindowsAPIHooks::FindMainWindow() {
    HWND mainWindow = NULL;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&mainWindow));
    return mainWindow;
}

void WindowsAPIHooks::SetupKeyboardHook() {
    s_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, nullptr, 0);
    
    // Message loop to keep the hook alive
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void WindowsAPIHooks::UninstallKeyboardHook() {
    if (s_keyboardHook) {
        UnhookWindowsHookEx(s_keyboardHook);
        s_keyboardHook = NULL;
    }
}

LRESULT CALLBACK WindowsAPIHooks::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
        if (wParam == WM_KEYDOWN) {
            switch (p->vkCode) {
                // VK_UP is the virtual key code for the Up arrow key
                case VK_UP:
                    InstallFocusHooks();
                    std::cout << "Up arrow key pressed, installing focus hooks." << std::endl;
                    break;
                    
                // VK_DOWN is the virtual key code for the Down arrow key
                case VK_DOWN:
                    UninstallFocusHooks();
                    
                    // Apply the original functions to restore normal behavior
                    if (s_focusHWND != NULL) {
                        SetFocus(s_focusHWND);
                    }
                    
                    if (s_bringWindowToTopHWND != NULL) {
                        BringWindowToTop(s_bringWindowToTopHWND);
                    }
                    
                    if (s_setWindowFocusHWND != NULL) {
                        SetWindowPos(
                            s_setWindowFocusHWND, 
                            s_setWindowFocushWndInsertAfter, 
                            s_setWindowFocusX, 
                            s_setWindowFocusY, 
                            s_setWindowFocuscx, 
                            s_setWindowFocuscy, 
                            s_setWindowFocusuFlags
                        );
                    }
                    
                    std::cout << "Down arrow key pressed, uninstalling focus hooks." << std::endl;
                    break;
            }
        }
    }
    
    // Call the next hook in the chain
    return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
}

// Helper function implementations for window management
BOOL IsMainWindow(HWND handle) {
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam) {
    DWORD processID = 0;
    GetWindowThreadProcessId(handle, &processID);
    
    if (GetCurrentProcessId() == processID && IsMainWindow(handle)) {
        // Stop enumeration if a main window is found, and return its handle
        *reinterpret_cast<HWND*>(lParam) = handle;
        return FALSE;
    }
    
    return TRUE;
}

} // namespace WindowsHook
} // namespace UndownUnlock 