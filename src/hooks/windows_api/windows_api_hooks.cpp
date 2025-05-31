#include "../../../include/hooks/windows_api_hooks.h"
#include "../../../include/hooks/hooked_functions.h"
#include "../../../include/hooks/hook_utils.h"
// #include "../../../include/hooks/keyboard_hook.h" // Removed as KeyboardHook is now self-contained

namespace UndownUnlock {
namespace WindowsHook {

// Initialize static variables - REMOVED
// BYTE WindowsAPIHooks::s_originalBytesForGetForeground[5] = { 0 };
// BYTE WindowsAPIHooks::s_originalBytesForShowWindow[5] = { 0 };
// BYTE WindowsAPIHooks::s_originalBytesForSetWindowPos[5] = { 0 };
// BYTE WindowsAPIHooks::s_originalBytesForSetFocus[5] = { 0 };
// BYTE WindowsAPIHooks::s_originalBytesForEmptyClipboard[5] = { 0 };
// BYTE WindowsAPIHooks::s_originalBytesForSetClipboardData[5] = { 0 };
// BYTE WindowsAPIHooks::s_originalBytesForTerminateProcess[5] = { 0 };
// BYTE WindowsAPIHooks::s_originalBytesForExitProcess[5] = { 0 };

// bool WindowsAPIHooks::s_isFocusInstalled = false;
// HWND WindowsAPIHooks::s_focusHWND = NULL;
// HWND WindowsAPIHooks::s_bringWindowToTopHWND = NULL;
// HWND WindowsAPIHooks::s_setWindowFocusHWND = NULL;
// HWND WindowsAPIHooks::s_setWindowFocushWndInsertAfter = NULL;
// int WindowsAPIHooks::s_setWindowFocusX = 0;
// int WindowsAPIHooks::s_setWindowFocusY = 0;
// int WindowsAPIHooks::s_setWindowFocuscx = 0;
// int WindowsAPIHooks::s_setWindowFocuscy = 0;
// UINT WindowsAPIHooks::s_setWindowFocusuFlags = 0;

bool WindowsAPIHooks::Initialize() {
    std::cout << "WindowsAPIHooks::Initialize() is deprecated. Hooks are now managed by WindowsApiHookManager." << std::endl;
    // Remove all HookUtils::InstallHook calls.
    // Remove the MessageBox call.
    return true;
}

void WindowsAPIHooks::Shutdown() {
    std::cout << "WindowsAPIHooks::Shutdown() is deprecated. Hooks are now managed by WindowsApiHookManager." << std::endl;
    // Remove all HookUtils::RemoveHook calls.
    // Remove the call to UninstallFocusHooks().
}

void WindowsAPIHooks::InstallFocusHooks() {
    std::cout << "WindowsAPIHooks::InstallFocusHooks() is deprecated. Hooks are now managed by WindowsApiHookManager." << std::endl;
    // Remove all HookUtils::InstallHook calls.
    // Remove s_isFocusInstalled = true;
}

void WindowsAPIHooks::UninstallFocusHooks() {
    std::cout << "WindowsAPIHooks::UninstallFocusHooks() is deprecated. Hooks are now managed by WindowsApiHookManager." << std::endl;
    // Remove all HookUtils::RemoveHook calls.
    // Remove s_isFocusInstalled = false;
}

HWND WindowsAPIHooks::FindMainWindow() {
    return HookUtils::FindMainWindow();
}

// Removed SetupKeyboardHook and UninstallKeyboardHook as KeyboardHook class handles its own lifecycle.
/*
void WindowsAPIHooks::SetupKeyboardHook() {
    KeyboardHook::Initialize();
    KeyboardHook::RunMessageLoop();
}

void WindowsAPIHooks::UninstallKeyboardHook() {
    KeyboardHook::Shutdown();
}
*/

} // namespace WindowsHook
} // namespace UndownUnlock