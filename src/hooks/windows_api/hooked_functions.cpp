#include "../../../include/hooks/hooked_functions.h"
#include "../../../include/hooks/windows_api_hooks.h"
#include <iostream>

namespace UndownUnlock {
namespace WindowsHook {

// Implementation of hooked clipboard functions
HANDLE WINAPI HookedSetClipboardData(UINT uFormat, HANDLE hMem) {
    std::cout << "SetClipboardData hook called, but not setting clipboard data." << std::endl;
    return NULL; // Indicate failure or that the data was not set
}

BOOL WINAPI HookedEmptyClipboard() {
    std::cout << "EmptyClipboard hook called, but not clearing the clipboard." << std::endl;
    return TRUE; // Pretend success
}

// Implementation of hooked process management functions
HANDLE WINAPI HookedOpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
    std::cout << "OpenProcess hook called, but not opening process." << std::endl;
    return NULL;
}

BOOL WINAPI HookedTerminateProcess(HANDLE hProcess, UINT uExitCode) {
    std::cout << "TerminateProcess hook called, but not terminating process." << std::endl;
    return TRUE; // Simulate success
}

VOID WINAPI HookedExitProcess(UINT uExitCode) {
    std::cout << "ExitProcess hook called, but not exiting process." << std::endl;
}

BOOL WINAPI HookedK32EnumProcesses(DWORD* pProcessIds, DWORD cb, DWORD* pBytesReturned) {
    std::cout << "K32EnumProcesses hook called, but pretending no processes exist." << std::endl;
    if (pBytesReturned != NULL) {
        *pBytesReturned = 0; // Indicate no processes were written to the buffer
    }
    return TRUE; // Indicate the function succeeded
}

// Implementation of hooked window management functions
int WINAPI HookedGetWindowTextW(HWND hWnd, LPWSTR lpString, int nMaxCount) {
    std::cout << "GetWindowTextW hook called, but not returning actual window text." << std::endl;
    if (nMaxCount > 0) {
        lpString[0] = L'\0'; // Return an empty string
    }
    return 0; // Indicate that no characters were copied to the buffer
}

BOOL WINAPI HookedSetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) {
    // Update the static tracking variables in WindowsAPIHooks
    WindowsAPIHooks::s_setWindowFocusHWND = hWnd;
    WindowsAPIHooks::s_setWindowFocushWndInsertAfter = hWndInsertAfter;
    WindowsAPIHooks::s_setWindowFocusX = X;
    WindowsAPIHooks::s_setWindowFocusY = Y;
    WindowsAPIHooks::s_setWindowFocuscx = cx;
    WindowsAPIHooks::s_setWindowFocuscy = cy;
    WindowsAPIHooks::s_setWindowFocusuFlags = uFlags;
    
    std::cout << "SetWindowPos hook called, but not changing window position." << std::endl;
    return TRUE; // Pretend success
}

BOOL WINAPI HookedShowWindow(HWND hWnd) {
    WindowsAPIHooks::s_bringWindowToTopHWND = hWnd;
    std::cout << "ShowWindow hook called, but not bringing window to top." << std::endl;
    return TRUE; // Pretend success
}

HWND WINAPI HookedGetWindow(HWND hWnd, UINT uCmd) {
    std::cout << "GetWindow hook called, but pretending no related window." << std::endl;
    return NULL; // Indicate no window found
}

// Implementation of the GetForegroundWindow hook
HWND WINAPI HookedGetForegroundWindow() {
    HWND hWnd = WindowsAPIHooks::FindMainWindow();
    if (hWnd != NULL) {
        std::cout << "Returning the main window of the current application." << std::endl;
        return hWnd;
    }
    std::cout << "Main window not found, returning NULL." << std::endl;
    return NULL;
}

// Implementation of the SetFocus hook
HWND WINAPI HookedSetFocus(HWND hWnd) {
    WindowsAPIHooks::s_focusHWND = hWnd;
    HWND mainWindow = WindowsAPIHooks::FindMainWindow();
    if (mainWindow != NULL) {
        std::cout << "Returning the main window of the current application due to hook." << std::endl;
        return mainWindow;
    }
    else {
        std::cout << "Main window not found, returning NULL." << std::endl;
        return NULL;
    }
}

} // namespace WindowsHook
} // namespace UndownUnlock 