#pragma once

#include <Windows.h>
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace UndownUnlock {
namespace WindowsHook {

/**
 * @brief Defines the processor architecture
 */
enum class Architecture {
    Unknown,
    X86,
    X64,
    ARM,
    ARM64
};

/**
 * @brief Status of a hook
 */
enum class HookStatus {
    NotInstalled,
    Installed,
    Compromised,
    Disabled
};

/**
 * @brief Configuration for hook installation
 */
struct HookConfig {
    bool enableSelfHealing = false;    // Automatically reinstall if tampering detected
    bool enableVerification = true;    // Periodically verify integrity
    bool suspendThreads = false;       // Suspend threads during installation
    size_t verificationInterval = 5000; // Milliseconds between integrity checks
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
     * @param originalBytes Buffer to store the original bytes for later restoration
     * @param config Optional configuration for the hook
     * @return True if the hook was successfully installed
     */
    static bool InstallHook(void* targetFunction, void* hookFunction, BYTE* originalBytes, 
                            const HookConfig& config = HookConfig());

    /**
     * @brief Remove a hook and restore the original function
     * @param targetFunction Pointer to the hooked function
     * @param originalBytes Buffer containing the original bytes
     * @return True if the hook was successfully removed
     */
    static bool RemoveHook(void* targetFunction, BYTE* originalBytes);

    /**
     * @brief Verify hook integrity
     * @param targetFunction Pointer to the hooked function
     * @param hookFunction Pointer to the replacement function
     * @param originalBytes Buffer containing the original bytes
     * @return Status of the hook
     */
    static HookStatus VerifyHookIntegrity(void* targetFunction, void* hookFunction, const BYTE* originalBytes);

    /**
     * @brief Detect the current processor architecture
     * @return The detected architecture
     */
    static Architecture DetectArchitecture();

    /**
     * @brief Generate a proper trampoline based on the current architecture
     * @param targetFunction Pointer to the function to hook
     * @param hookFunction Pointer to the replacement function
     * @param trampolineBuffer Buffer to store the trampoline code
     * @param trampolineSize Size of the trampoline buffer
     * @return Size of the generated trampoline, or 0 if failed
     */
    static size_t GenerateTrampoline(void* targetFunction, void* hookFunction, 
                                     BYTE* trampolineBuffer, size_t trampolineSize);

    /**
     * @brief Find the main window of the current process
     * @return Handle to the main window, or NULL if not found
     */
    static HWND FindMainWindow();

    /**
     * @brief Get a function address from a DLL by name
     * @param moduleName Name of the module containing the function
     * @param functionName Name of the function to find
     * @return Pointer to the function, or NULL if not found
     */
    static void* GetFunctionAddress(const wchar_t* moduleName, const char* functionName);

private:
    // Helper functions for thread management during hook installation
    static std::vector<DWORD> SuspendAllThreadsExceptCurrent();
    static void ResumeThreads(const std::vector<DWORD>& threadIds);
};

// Hook Handler for advanced hook management
class HookHandler {
public:
    HookHandler(void* targetFunction, void* hookFunction, BYTE* originalBytes,
                const HookConfig& config = HookConfig());
    ~HookHandler();

    // Install the hook
    bool Install();
    
    // Uninstall the hook
    bool Uninstall();
    
    // Check if the hook is installed
    bool IsInstalled() const;
    
    // Verify the hook integrity
    HookStatus Verify() const;
    
    // Set a callback for integrity violations
    void SetIntegrityCallback(std::function<void(HookStatus)> callback);

private:
    void* m_targetFunction;
    void* m_hookFunction;
    BYTE* m_originalBytes;
    HookConfig m_config;
    bool m_installed;
    std::function<void(HookStatus)> m_integrityCallback;
};

// Helper function for window enumeration
BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam);
BOOL IsMainWindow(HWND handle);

} // namespace WindowsHook
} // namespace UndownUnlock 