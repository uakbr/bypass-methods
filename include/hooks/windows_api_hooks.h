#pragma once

#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include "hook_utils.h" // Include HookUtils for HookInfo

namespace UndownUnlock {
namespace WindowsHook {

/**
 * @brief Class for hooking Windows API functions to prevent security measures
 */
class WindowsAPIHooks {
public:
    /**
     * @brief Initialize and install the Windows API hooks
     * @return True if successful
     */
    static bool Initialize();

    /**
     * @brief Shutdown and remove all Windows API hooks
     */
    static void Shutdown();

    /**
     * @brief Install hooks for focus/window-related functions
     */
    static void InstallFocusHooks();

    /**
     * @brief Uninstall hooks for focus/window-related functions
     */
    static void UninstallFocusHooks();

    /**
     * @brief Find the main window of the current process
     * @return Handle to the main window, or NULL if not found
     */
    static HWND FindMainWindow();

    // State tracking - public to allow access from the keyboard hook
    static bool s_isFocusInstalled;
    static HWND s_focusHWND;
    static HWND s_bringWindowToTopHWND;
    static HWND s_setWindowFocusHWND;
    static HWND s_setWindowFocushWndInsertAfter;
    static int s_setWindowFocusX;
    static int s_setWindowFocusY;
    static int s_setWindowFocuscx;
    static int s_setWindowFocuscy;
    static UINT s_setWindowFocusuFlags;

private:
    // Map to store information about our specific hooks (hookName -> HookInfo)
    // Note: HookUtils also maintains a global map, this might be redundant
    // depending on design, but keeps hooks specific to this class contained.
    // Alternatively, WindowsAPIHooks could just interact with HookUtils by name.
    // For now, we'll keep this local management structure.
    static std::unordered_map<std::string, HookInfo> s_hookMap;

    // Helper to install a specific hook and store its info
    static bool InstallApiHook(const char* moduleName, const char* functionName, void* hookFunction, const std::string& hookName);

    // Helper to remove a specific hook
    static bool RemoveApiHook(const std::string& hookName);
};

} // namespace WindowsHook
} // namespace UndownUnlock 