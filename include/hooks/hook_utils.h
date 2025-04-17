#pragma once

#include <Windows.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex> // Include mutex for thread safety

namespace UndownUnlock {
namespace WindowsHook {

// Structure to hold information about an installed hook
struct HookInfo {
    void* targetFunction = nullptr;
    void* hookFunction = nullptr;
    BYTE originalBytes[5] = {0}; // Standard 5 bytes for x86/x64 JMP hook
    bool isActive = false;
};

/**
 * @brief Utility functions for hooking Windows API functions
 */
class HookUtils {
public:
    /**
     * @brief Install a hook by modifying the target function with a JMP instruction
     * @param targetFunction Pointer to the function to hook
     * @param hookFunction Pointer to the replacement function
     * @param hookName A unique name for the hook
     * @return True if the hook was successfully installed or is already active
     */
    static bool InstallHook(void* targetFunction, void* hookFunction, const std::string& hookName);

    /**
     * @brief Remove a previously installed hook
     * @param hookName The unique name of the hook to remove
     * @return True if the hook was successfully removed or was not active
     */
    static bool RemoveHook(const std::string& hookName);

    /**
     * @brief Retrieve the original bytes of a hooked function
     * @param hookName The unique name of the hook
     * @param originalBytes Buffer to copy the original bytes into (must be at least 5 bytes)
     * @return True if the hook was found and bytes were copied
     */
    static bool GetOriginalBytes(const std::string& hookName, BYTE* originalBytes);

    /**
     * @brief Find the main window of the current process
     * @return Handle to the main window, or NULL if not found
     */
    static HWND FindMainWindow();

    /**
     * @brief Get the address of a function from a module.
     * @param moduleName Name of the module (e.g., "user32.dll").
     * @param functionName Name of the function.
     * @return Address of the function, or nullptr if not found.
     */
    static void* GetFunctionAddress(const char* moduleName, const char* functionName);

private:
    // Map to store active hooks (hookName -> HookInfo)
    static std::unordered_map<std::string, HookInfo> s_activeHooks;
    static std::mutex s_hookMutex; // Mutex for thread safety
};

// Helper function for window enumeration
BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam);
BOOL IsMainWindow(HWND handle);

} // namespace WindowsHook
} // namespace UndownUnlock 