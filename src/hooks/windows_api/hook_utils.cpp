#include "../../../include/hooks/hook_utils.h"
#include <psapi.h>
#include <iostream>

namespace UndownUnlock {
namespace WindowsHook {

// Initialize static members
std::unordered_map<std::string, HookInfo> HookUtils::s_activeHooks;
std::mutex HookUtils::s_hookMutex;

void* HookUtils::GetFunctionAddress(const char* moduleName, const char* functionName) {
    HMODULE hModule = GetModuleHandleA(moduleName);
    if (!hModule) {
        // Try loading the module if not already loaded
        hModule = LoadLibraryA(moduleName);
        if (!hModule) {
            std::cerr << "Failed to get/load module handle for " << moduleName << std::endl;
            return nullptr;
        }
    }

    void* funcAddr = GetProcAddress(hModule, functionName);
    if (!funcAddr) {
        std::cerr << "Failed to get address for " << functionName << " in " << moduleName << std::endl;
        // No need to FreeLibrary if GetModuleHandle succeeded initially
    }
    return funcAddr;
}

bool HookUtils::InstallHook(void* targetFunction, void* hookFunction, const std::string& hookName) {
    if (!targetFunction || !hookFunction || hookName.empty()) {
        std::cerr << "InstallHook Error: Invalid parameters for hook '" << hookName << "'" << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(s_hookMutex);

    // Check if hook already exists and is active
    auto it = s_activeHooks.find(hookName);
    if (it != s_activeHooks.end() && it->second.isActive) {
        // Hook is already installed and active
        return true;
    }

    HookInfo hookInfo;
    hookInfo.targetFunction = targetFunction;
    hookInfo.hookFunction = hookFunction;

    DWORD oldProtect;
    // Save the original bytes before patching
    if (!ReadProcessMemory(GetCurrentProcess(), targetFunction, hookInfo.originalBytes, 5, nullptr)) {
         std::cerr << "InstallHook Error: Failed to read original bytes for '" << hookName << "' at " << targetFunction << ". Error: " << GetLastError() << std::endl;
         // Attempt to proceed, but restoration might fail
    }

    // Change memory protection to allow writing
    if (!VirtualProtect(targetFunction, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::cerr << "InstallHook Error: Failed to change memory protection for '" << hookName << "' at " << targetFunction << ". Error: " << GetLastError() << std::endl;
        return false;
    }

    // Write a JMP instruction (0xE9) followed by a relative address
    DWORD relativeAddress = reinterpret_cast<DWORD>(hookFunction) - reinterpret_cast<DWORD>(targetFunction) - 5;
    BYTE patch[5] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
    *reinterpret_cast<DWORD*>(&patch[1]) = relativeAddress;

    // Write the patch
    if (!WriteProcessMemory(GetCurrentProcess(), targetFunction, patch, 5, nullptr)) {
        std::cerr << "InstallHook Error: Failed to write patch for '" << hookName << "' at " << targetFunction << ". Error: " << GetLastError() << std::endl;
        // Attempt to restore protection anyway
        VirtualProtect(targetFunction, 5, oldProtect, &oldProtect);
        return false;
    }

    // Restore the original memory protection
    DWORD tempProtect;
    VirtualProtect(targetFunction, 5, oldProtect, &tempProtect);
    FlushInstructionCache(GetCurrentProcess(), targetFunction, 5);

    hookInfo.isActive = true;
    s_activeHooks[hookName] = hookInfo;

    std::cout << "Hook '" << hookName << "' installed successfully for function at " << targetFunction << std::endl;
    return true;
}

bool HookUtils::RemoveHook(const std::string& hookName) {
    std::lock_guard<std::mutex> lock(s_hookMutex);

    auto it = s_activeHooks.find(hookName);
    if (it == s_activeHooks.end() || !it->second.isActive) {
        // Hook not found or not active
        return true;
    }

    HookInfo& hookInfo = it->second;

    DWORD oldProtect;
    // Change memory protection to allow writing
    if (!VirtualProtect(hookInfo.targetFunction, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::cerr << "RemoveHook Error: Failed to change memory protection for '" << hookName << "' at " << hookInfo.targetFunction << ". Error: " << GetLastError() << std::endl;
        return false; // Failed to remove hook
    }

    // Restore the original bytes
    if (!WriteProcessMemory(GetCurrentProcess(), hookInfo.targetFunction, hookInfo.originalBytes, 5, nullptr)) {
         std::cerr << "RemoveHook Error: Failed to restore original bytes for '" << hookName << "' at " << hookInfo.targetFunction << ". Error: " << GetLastError() << std::endl;
         // Attempt to restore protection anyway
         VirtualProtect(hookInfo.targetFunction, 5, oldProtect, &oldProtect);
         return false; // Indicate failure but protection restored if possible
    }

    // Restore the original memory protection
    DWORD tempProtect;
    VirtualProtect(hookInfo.targetFunction, 5, oldProtect, &tempProtect);
    FlushInstructionCache(GetCurrentProcess(), hookInfo.targetFunction, 5);

    hookInfo.isActive = false;
    // Optionally remove from map or just mark inactive
    // s_activeHooks.erase(it); // Or keep it marked inactive

    std::cout << "Hook '" << hookName << "' removed successfully for function at " << hookInfo.targetFunction << std::endl;
    return true;
}

bool HookUtils::GetOriginalBytes(const std::string& hookName, BYTE* originalBytes) {
    if (!originalBytes) return false;

    std::lock_guard<std::mutex> lock(s_hookMutex);

    auto it = s_activeHooks.find(hookName);
    if (it != s_activeHooks.end()) {
        memcpy(originalBytes, it->second.originalBytes, 5);
        return true;
    }
    return false;
}


HWND HookUtils::FindMainWindow() {
    HWND mainWindow = NULL;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&mainWindow));
    return mainWindow;
}

// Helper function to determine if the window is a main window
BOOL IsMainWindow(HWND handle) {
    // Added check for handle validity
    if (!IsWindow(handle)) {
        return FALSE;
    }
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

// Callback for EnumWindows that finds the main window of the current process
BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam) {
    // Added check for handle validity
    if (!IsWindow(handle)) {
        return TRUE; // Continue enumerating
    }

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