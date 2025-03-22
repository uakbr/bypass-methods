#pragma once

#include <Windows.h>
#include <iostream>
#include <vector>
#include <memory>
#include <wbemidl.h>
#include <oleacc.h>
#include <UIAutomation.h>
#include <shlobj.h>
#include <shlwapi.h>

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

    /**
     * @brief Install keyboard hook to control hooking behavior
     */
    static void SetupKeyboardHook();

    /**
     * @brief Uninstall the keyboard hook
     */
    static void UninstallKeyboardHook();

    /**
     * @brief Initialize Windows Accessibility API for window control
     * @return True if successful
     */
    static bool InitializeAccessibilityAPI();

    /**
     * @brief Shutdown Windows Accessibility API
     */
    static void ShutdownAccessibilityAPI();

    /**
     * @brief Use Accessibility API to control windows
     * @param targetWindow Handle to the target window
     * @param command Command to execute (focus, click, etc.)
     * @param param1 First parameter for the command
     * @param param2 Second parameter for the command
     * @return True if successful
     */
    static bool AccessibilityControlWindow(HWND targetWindow, int command, LPARAM param1 = 0, LPARAM param2 = 0);

    /**
     * @brief Initialize WMI for window monitoring
     * @return True if successful
     */
    static bool InitializeWMI();

    /**
     * @brief Shutdown WMI
     */
    static void ShutdownWMI();

    /**
     * @brief Use WMI to monitor windows
     * @param callback Callback function to be called when a window is created or destroyed
     * @return True if successful
     */
    static bool MonitorWindowsWithWMI(void (*callback)(HWND, bool));

    /**
     * @brief Initialize Shell Extension for window management
     * @return True if successful
     */
    static bool InitializeShellExtension();

    /**
     * @brief Shutdown Shell Extension
     */
    static void ShutdownShellExtension();

    /**
     * @brief Use Shell Extension to manage windows
     * @param targetWindow Handle to the target window
     * @param command Command to execute
     * @param param Parameter for the command
     * @return True if successful
     */
    static bool ShellExtensionManageWindow(HWND targetWindow, int command, LPARAM param = 0);

    /**
     * @brief Check Windows version compatibility
     * @return True if the current Windows version is supported
     */
    static bool CheckWindowsVersionCompatibility();

    /**
     * @brief Implement enhanced security mechanisms for the hooks
     * @return True if successful
     */
    static bool EnhanceHookSecurity();

    /**
     * @brief Test if hooks are functioning correctly
     * @return True if all hooks are working
     */
    static bool TestHookFunctionality();

    // Original bytes storage for hook restoration
    static BYTE s_originalBytesForGetForeground[5];
    static BYTE s_originalBytesForShowWindow[5];
    static BYTE s_originalBytesForSetWindowPos[5];
    static BYTE s_originalBytesForSetFocus[5];
    static BYTE s_originalBytesForEmptyClipboard[5];
    static BYTE s_originalBytesForSetClipboardData[5];
    static BYTE s_originalBytesForTerminateProcess[5];
    static BYTE s_originalBytesForExitProcess[5];
    static BYTE s_originalBytesForGetWindowTextW[5];
    static BYTE s_originalBytesForGetWindow[5];
    static BYTE s_originalBytesForOpenProcess[5];
    static BYTE s_originalBytesForK32EnumProcesses[5];

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

    // New state variables for enhanced functionality
    static bool s_isAccessibilityInitialized;
    static bool s_isWMIInitialized;
    static bool s_isShellExtensionInitialized;
    static DWORD s_currentWindowsVersion;
    static bool s_isEnhancedSecurityEnabled;

private:
    // Helper methods for hooking and unhooking
    static bool HookFunction(void* targetFunction, void* hookFunction, BYTE* originalBytes);
    static bool UnhookFunction(void* targetFunction, BYTE* originalBytes);

    // Keyboard hook callback
    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);

    // WMI related variables
    static IWbemServices* s_pWbemServices;
    static IWbemLocator* s_pWbemLocator;

    // Accessibility related variables
    static IAccessible* s_pAccessible;
    static IUIAutomation* s_pUIAutomation;

    // Shell extension related variables
    static IShellWindows* s_pShellWindows;
};

// Helper function for window enumeration
BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam);
BOOL IsMainWindow(HWND handle);

// Accessibility command constants
enum AccessibilityCommand {
    ACC_CMD_FOCUS = 1,
    ACC_CMD_CLICK = 2,
    ACC_CMD_DOUBLECLICK = 3,
    ACC_CMD_RIGHTCLICK = 4,
    ACC_CMD_SENDKEYS = 5,
    ACC_CMD_MAXIMIZE = 6,
    ACC_CMD_MINIMIZE = 7,
    ACC_CMD_RESTORE = 8,
    ACC_CMD_CLOSE = 9
};

// Shell extension command constants
enum ShellExtensionCommand {
    SHELL_CMD_EXPLORE = 1,
    SHELL_CMD_OPEN = 2,
    SHELL_CMD_PROPERTIES = 3,
    SHELL_CMD_NAVIGATE = 4,
    SHELL_CMD_EXECUTE = 5
};

} // namespace WindowsHook
} // namespace UndownUnlock 