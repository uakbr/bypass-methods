#include "../../include/com_hooks/com_tracker.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <dbghelp.h>

#pragma comment(lib, "dbghelp.lib")

namespace UndownUnlock {
namespace DXHook {

// Initialize static members
ComTracker* ComTracker::s_instance = nullptr;

ComTracker& ComTracker::GetInstance() {
    if (!s_instance) {
        s_instance = new ComTracker();
    }
    return *s_instance;
}

ComTracker::ComTracker()
    : m_enableCallstackCapture(false)
    , m_totalInterfacesCreated(0)
    , m_totalInterfacesDestroyed(0) {
}

ComTracker::~ComTracker() {
    Shutdown();
}

bool ComTracker::Initialize(bool enableCallstackCapture) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enableCallstackCapture = enableCallstackCapture;
    
    // Initialize symbol handler for callstack capture if enabled
    if (m_enableCallstackCapture) {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        if (!SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
            std::cerr << "Failed to initialize symbol handler: " << GetLastError() << std::endl;
            m_enableCallstackCapture = false;
        }
    }
    
    std::cout << "COM Interface Tracker initialized. Callstack capture " 
              << (m_enableCallstackCapture ? "enabled" : "disabled") << std::endl;
    return true;
}

void ComTracker::Shutdown() {
    // Report leaks before clearing
    DumpTrackedInterfaces(true);
    
    // Clear all tracked interfaces
    std::lock_guard<std::mutex> lock(m_mutex);
    m_trackedInterfaces.clear();
    
    // Cleanup symbol handler if it was initialized
    if (m_enableCallstackCapture) {
        SymCleanup(GetCurrentProcess());
    }
    
    std::cout << "COM Interface Tracker shutdown. Total interfaces: Created=" 
              << m_totalInterfacesCreated << ", Destroyed=" << m_totalInterfacesDestroyed << std::endl;
}

bool ComTracker::TrackInterface(void* pInterface, const std::string& interfaceType, ULONG initialRefCount) {
    if (!pInterface) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if this interface is already being tracked
    auto it = m_trackedInterfaces.find(pInterface);
    if (it != m_trackedInterfaces.end()) {
        std::cerr << "WARNING: Interface " << pInterface << " (" << interfaceType 
                  << ") is already being tracked as " << it->second.interfaceType << std::endl;
        return false;
    }
    
    // Create a new tracked interface
    TrackedComInterface trackedInterface;
    trackedInterface.pInterface = pInterface;
    trackedInterface.interfaceType = interfaceType;
    trackedInterface.refCount = initialRefCount;
    trackedInterface.creationThreadId = GetCurrentThreadId();
    trackedInterface.creationTime = std::time(nullptr);
    
    // Capture callstack if enabled
    if (m_enableCallstackCapture) {
        trackedInterface.callstack = CaptureCallstack(2); // Skip this function and the caller
    }
    
    // Add to tracking map
    m_trackedInterfaces[pInterface] = trackedInterface;
    m_totalInterfacesCreated++;
    
    return true;
}

bool ComTracker::UpdateRefCount(void* pInterface, ULONG newRefCount, bool isAddRef) {
    if (!pInterface) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_trackedInterfaces.find(pInterface);
    if (it == m_trackedInterfaces.end()) {
        // This could be normal for interfaces created before tracking was initialized
        return false;
    }
    
    // Update the reference count
    it->second.refCount = newRefCount;
    
    // Check for unexpected reference count changes
    if (isAddRef && newRefCount <= 0) {
        std::cerr << "WARNING: Interface " << pInterface << " (" << it->second.interfaceType 
                  << ") has invalid reference count after AddRef: " << newRefCount << std::endl;
    }
    
    // If this is a Release that brought the count to 0, untrack the interface
    if (!isAddRef && newRefCount == 0) {
        m_trackedInterfaces.erase(it);
        m_totalInterfacesDestroyed++;
    }
    
    return true;
}

bool ComTracker::UntrackInterface(void* pInterface) {
    if (!pInterface) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_trackedInterfaces.find(pInterface);
    if (it == m_trackedInterfaces.end()) {
        return false;
    }
    
    // Remove from tracking map
    m_trackedInterfaces.erase(it);
    m_totalInterfacesDestroyed++;
    
    return true;
}

int ComTracker::CheckForIssues() {
    std::lock_guard<std::mutex> lock(m_mutex);
    int issueCount = 0;
    
    // Look for interfaces with unusually high reference counts
    // or interfaces that have been alive for too long
    std::time_t currentTime = std::time(nullptr);
    const std::time_t MAX_INTERFACE_AGE = 3600; // 1 hour in seconds
    const ULONG SUSPICIOUS_REF_COUNT = 1000;    // Arbitrarily high ref count
    
    for (const auto& pair : m_trackedInterfaces) {
        const TrackedComInterface& interface = pair.second;
        
        // Check for high reference counts
        if (interface.refCount > SUSPICIOUS_REF_COUNT) {
            std::cerr << "WARNING: Interface " << interface.pInterface << " (" << interface.interfaceType 
                      << ") has suspiciously high reference count: " << interface.refCount << std::endl;
            issueCount++;
        }
        
        // Check for long-lived interfaces
        std::time_t interfaceAge = currentTime - interface.creationTime;
        if (interfaceAge > MAX_INTERFACE_AGE) {
            std::cerr << "WARNING: Interface " << interface.pInterface << " (" << interface.interfaceType 
                      << ") has been alive for " << interfaceAge << " seconds" << std::endl;
            issueCount++;
        }
    }
    
    return issueCount;
}

void ComTracker::DumpTrackedInterfaces(bool onlyLeaked) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_trackedInterfaces.empty()) {
        std::cout << "No COM interfaces currently being tracked." << std::endl;
        return;
    }
    
    size_t count = 0;
    std::cout << "======================= COM Interface Tracking Report =======================" << std::endl;
    std::cout << std::left << std::setw(18) << "Address" 
              << std::setw(30) << "Type" 
              << std::setw(12) << "RefCount" 
              << std::setw(12) << "Thread ID" 
              << std::setw(20) << "Creation Time" 
              << std::endl;
    std::cout << "--------------------------------------------------------------------------" << std::endl;
    
    for (const auto& pair : m_trackedInterfaces) {
        const TrackedComInterface& interface = pair.second;
        
        // Skip if we only want to show leaked interfaces and this one doesn't look leaked
        if (onlyLeaked && interface.refCount <= 1) {
            continue;
        }
        
        // Format creation time
        char timeBuffer[64];
        std::tm* timeinfo = std::localtime(&interface.creationTime);
        std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        
        // Print interface info
        std::cout << std::left 
                  << "0x" << std::hex << std::setw(16) << (uintptr_t)interface.pInterface << std::dec << " "
                  << std::setw(30) << interface.interfaceType 
                  << std::setw(12) << interface.refCount
                  << std::setw(12) << interface.creationThreadId
                  << std::setw(20) << timeBuffer
                  << std::endl;
        
        // Print callstack if available
        if (!interface.callstack.empty()) {
            std::cout << "    Callstack:" << std::endl;
            for (const auto& frame : interface.callstack) {
                std::cout << "        " << frame << std::endl;
            }
            std::cout << std::endl;
        }
        
        count++;
    }
    
    std::cout << "--------------------------------------------------------------------------" << std::endl;
    std::cout << "Total: " << count << " interfaces" 
              << (onlyLeaked ? " potentially leaked" : " tracked") 
              << " out of " << m_trackedInterfaces.size() << " total tracked interfaces." << std::endl;
    std::cout << "Created: " << m_totalInterfacesCreated << ", Destroyed: " << m_totalInterfacesDestroyed << std::endl;
    std::cout << "==========================================================================" << std::endl;
}

size_t ComTracker::GetTrackedInterfaceCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_trackedInterfaces.size();
}

size_t ComTracker::GetPotentialLeakCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    
    for (const auto& pair : m_trackedInterfaces) {
        if (pair.second.refCount > 1) {
            count++;
        }
    }
    
    return count;
}

std::vector<std::string> ComTracker::CaptureCallstack(int skipFrames, int maxFrames) {
    std::vector<std::string> callstack;
    
    if (!m_enableCallstackCapture) {
        return callstack;
    }
    
    // Capture the raw callstack
    void* callstackAddresses[64];
    WORD framesCaptured = CaptureStackBackTrace(skipFrames, min(maxFrames, 64), callstackAddresses, NULL);
    
    // Allocate symbol info buffer
    constexpr size_t MAX_SYMBOL_NAME = 256;
    SYMBOL_INFO* symbolInfo = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + MAX_SYMBOL_NAME * sizeof(char), 1);
    symbolInfo->MaxNameLen = MAX_SYMBOL_NAME;
    symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    
    // Get line information
    IMAGEHLP_LINE64 lineInfo;
    lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD displacement;
    
    // Format the stack frames
    for (WORD i = 0; i < framesCaptured; i++) {
        std::stringstream frameInfo;
        DWORD64 address = (DWORD64)callstackAddresses[i];
        
        // Get symbol name
        if (SymFromAddr(GetCurrentProcess(), address, NULL, symbolInfo)) {
            frameInfo << symbolInfo->Name << " at 0x" << std::hex << address;
        } else {
            frameInfo << "Unknown function at 0x" << std::hex << address;
        }
        
        // Get file and line information
        if (SymGetLineFromAddr64(GetCurrentProcess(), address, &displacement, &lineInfo)) {
            frameInfo << " in " << lineInfo.FileName << ":" << std::dec << lineInfo.LineNumber;
        }
        
        callstack.push_back(frameInfo.str());
    }
    
    // Clean up
    free(symbolInfo);
    
    return callstack;
}

std::string ComTracker::GetInterfaceTypeFromIID(REFIID riid) {
    // Common DirectX interfaces
    if (riid == __uuidof(IDXGIFactory)) return "IDXGIFactory";
    if (riid == __uuidof(IDXGIFactory1)) return "IDXGIFactory1";
    if (riid == __uuidof(IDXGIFactory2)) return "IDXGIFactory2";
    if (riid == __uuidof(IDXGISwapChain)) return "IDXGISwapChain";
    if (riid == __uuidof(IDXGISwapChain1)) return "IDXGISwapChain1";
    if (riid == __uuidof(IDXGIAdapter)) return "IDXGIAdapter";
    if (riid == __uuidof(IDXGIAdapter1)) return "IDXGIAdapter1";
    if (riid == __uuidof(IDXGIOutput)) return "IDXGIOutput";
    if (riid == __uuidof(IDXGIDevice)) return "IDXGIDevice";
    if (riid == __uuidof(ID3D11Device)) return "ID3D11Device";
    if (riid == __uuidof(ID3D11DeviceContext)) return "ID3D11DeviceContext";
    
    // If not a known interface, return the GUID string
    OLECHAR guidString[64];
    StringFromGUID2(riid, guidString, 64);
    
    char guidStringA[128];
    WideCharToMultiByte(CP_ACP, 0, guidString, -1, guidStringA, sizeof(guidStringA), NULL, NULL);
    
    return std::string("Unknown Interface: ") + guidStringA;
}

bool ComTracker::IsKnownInterface(REFIID riid) {
    // Check if we have a named mapping for this IID
    std::string interfaceType = GetInterfaceTypeFromIID(riid);
    return interfaceType.find("Unknown Interface:") == std::string::npos;
}

} // namespace DXHook
} // namespace UndownUnlock 