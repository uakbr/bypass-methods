#include "../../../include/hooks/hook_utils.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <intrin.h>

namespace UndownUnlock {
namespace WindowsHook {

// Detect the current processor architecture
Architecture HookUtils::DetectArchitecture() {
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_INTEL:
            return Architecture::X86;
        case PROCESSOR_ARCHITECTURE_AMD64:
            return Architecture::X64;
        case PROCESSOR_ARCHITECTURE_ARM:
            return Architecture::ARM;
        case PROCESSOR_ARCHITECTURE_ARM64:
            return Architecture::ARM64;
        default:
            return Architecture::Unknown;
    }
}

// Generate architecture-specific trampoline
size_t HookUtils::GenerateTrampoline(void* targetFunction, void* hookFunction, 
                                   BYTE* trampolineBuffer, size_t trampolineSize) {
    if (!targetFunction || !hookFunction || !trampolineBuffer || trampolineSize < 5)
        return 0;
        
    Architecture arch = DetectArchitecture();
    
    switch (arch) {
        case Architecture::X86:
        case Architecture::X64:
        {
            // Simple JMP instruction (E9 relative_address)
            trampolineBuffer[0] = 0xE9;
            DWORD relativeAddress = (DWORD)((BYTE*)hookFunction - (BYTE*)targetFunction - 5);
            *(DWORD*)(&trampolineBuffer[1]) = relativeAddress;
            return 5;
        }
        case Architecture::ARM:
        case Architecture::ARM64:
            // ARM trampolines not implemented yet
            return 0;
        default:
            return 0;
    }
}

// Suspend all threads except the current one
std::vector<DWORD> HookUtils::SuspendAllThreadsExceptCurrent() {
    DWORD currentThreadId = GetCurrentThreadId();
    std::vector<DWORD> suspendedThreads;
    
    HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE)
        return suspendedThreads;
        
    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);
    
    if (!Thread32First(hThreadSnap, &te32)) {
        CloseHandle(hThreadSnap);
        return suspendedThreads;
    }
    
    DWORD currentProcessId = GetCurrentProcessId();
    
    do {
        if (te32.th32OwnerProcessID == currentProcessId && te32.th32ThreadID != currentThreadId) {
            HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
            if (hThread) {
                if (SuspendThread(hThread) != (DWORD)-1) {
                    suspendedThreads.push_back(te32.th32ThreadID);
                }
                CloseHandle(hThread);
            }
        }
    } while (Thread32Next(hThreadSnap, &te32));
    
    CloseHandle(hThreadSnap);
    return suspendedThreads;
}

// Resume previously suspended threads
void HookUtils::ResumeThreads(const std::vector<DWORD>& threadIds) {
    for (DWORD threadId : threadIds) {
        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, threadId);
        if (hThread) {
            ResumeThread(hThread);
            CloseHandle(hThread);
        }
    }
}

// Verify hook integrity
HookStatus HookUtils::VerifyHookIntegrity(void* targetFunction, void* hookFunction, 
                                        const BYTE* originalBytes) {
    if (!targetFunction || !hookFunction || !originalBytes)
        return HookStatus::NotInstalled;
        
    // Check if the first byte is a JMP instruction
    if (*((BYTE*)targetFunction) != 0xE9)
        return HookStatus::NotInstalled;
        
    // Calculate expected relative address
    DWORD expectedRelative = (DWORD)((BYTE*)hookFunction - (BYTE*)targetFunction - 5);
    DWORD actualRelative = *((DWORD*)((BYTE*)targetFunction + 1));
    
    // Check if the relative address points to our hook
    if (actualRelative != expectedRelative)
        return HookStatus::Compromised;
        
    return HookStatus::Installed;
}

// Get function address from a DLL
void* HookUtils::GetFunctionAddress(const wchar_t* moduleName, const char* functionName) {
    HMODULE hModule = GetModuleHandle(moduleName);
    if (!hModule)
        return nullptr;
        
    return GetProcAddress(hModule, functionName);
}

// Install a hook with the specified configuration
bool HookUtils::InstallHook(void* targetFunction, void* hookFunction, BYTE* originalBytes, 
                          const HookConfig& config) {
    if (!targetFunction || !hookFunction || !originalBytes) {
        return false;
    }

    // Save the original bytes for later restoration
    memcpy(originalBytes, targetFunction, 5);
    
    // Optionally suspend threads
    std::vector<DWORD> suspendedThreads;
    if (config.suspendThreads) {
        suspendedThreads = SuspendAllThreadsExceptCurrent();
    }
    
    DWORD oldProtect;
    
    // Change memory protection to allow writing
    if (VirtualProtect(targetFunction, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        // Generate trampoline code
        BYTE trampolineCode[5] = {0};
        if (GenerateTrampoline(targetFunction, hookFunction, trampolineCode, sizeof(trampolineCode)) > 0) {
            // Write the trampoline
            memcpy(targetFunction, trampolineCode, 5);
            
            // Restore the original memory protection
            VirtualProtect(targetFunction, 5, oldProtect, &oldProtect);
            
            // Optionally resume threads
            if (config.suspendThreads) {
                ResumeThreads(suspendedThreads);
            }
            
            // Flush instruction cache to ensure CPU recognizes the changes
            FlushInstructionCache(GetCurrentProcess(), targetFunction, 5);
            
            return true;
        }
        
        // Restore protection if trampoline generation failed
        VirtualProtect(targetFunction, 5, oldProtect, &oldProtect);
    }
    
    // Resume threads if we failed
    if (config.suspendThreads) {
        ResumeThreads(suspendedThreads);
    }
    
    return false;
}

bool HookUtils::RemoveHook(void* targetFunction, BYTE* originalBytes) {
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
        
        // Flush instruction cache
        FlushInstructionCache(GetCurrentProcess(), targetFunction, 5);
        
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
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

// Callback for EnumWindows that finds the main window of the current process
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

// HookHandler implementation
HookHandler::HookHandler(void* targetFunction, void* hookFunction, BYTE* originalBytes,
                        const HookConfig& config)
    : m_targetFunction(targetFunction), m_hookFunction(hookFunction), 
      m_originalBytes(originalBytes), m_config(config), m_installed(false) {
}

HookHandler::~HookHandler() {
    // Automatically uninstall if still installed
    if (m_installed) {
        Uninstall();
    }
}

bool HookHandler::Install() {
    if (m_installed) {
        return true; // Already installed
    }
    
    m_installed = HookUtils::InstallHook(m_targetFunction, m_hookFunction, m_originalBytes, m_config);
    return m_installed;
}

bool HookHandler::Uninstall() {
    if (!m_installed) {
        return true; // Already uninstalled
    }
    
    bool result = HookUtils::RemoveHook(m_targetFunction, m_originalBytes);
    if (result) {
        m_installed = false;
    }
    
    return result;
}

bool HookHandler::IsInstalled() const {
    return m_installed;
}

HookStatus HookHandler::Verify() const {
    if (!m_installed) {
        return HookStatus::NotInstalled;
    }
    
    HookStatus status = HookUtils::VerifyHookIntegrity(
        m_targetFunction, m_hookFunction, m_originalBytes);
        
    // Call integrity callback if set
    if (status == HookStatus::Compromised && m_integrityCallback) {
        m_integrityCallback(status);
    }
    
    // Self-healing: reinstall if compromised and enabled
    if (status == HookStatus::Compromised && m_config.enableSelfHealing) {
        const_cast<HookHandler*>(this)->Install();
    }
    
    return status;
}

void HookHandler::SetIntegrityCallback(std::function<void(HookStatus)> callback) {
    m_integrityCallback = callback;
}

} // namespace WindowsHook
} // namespace UndownUnlock 