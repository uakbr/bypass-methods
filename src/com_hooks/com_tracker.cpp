#include "../../include/com_hooks/com_tracker.h"
#include <Windows.h>
#include <DbgHelp.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <ctime>

#pragma comment(lib, "DbgHelp.lib")

namespace UndownUnlock {
namespace DXHook {

// Initialize the static instance
ComTracker* ComTracker::s_instance = nullptr;

// Helper function to create a GUID from a string (for testing)
GUID GuidFromString(const std::string& str) {
    GUID guid;
    if (str.length() >= 38) { // Format: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
        sscanf_s(str.c_str(), 
            "{%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}", 
            &guid.Data1, &guid.Data2, &guid.Data3,
            &guid.Data4[0], &guid.Data4[1],
            &guid.Data4[2], &guid.Data4[3], &guid.Data4[4],
            &guid.Data4[5], &guid.Data4[6], &guid.Data4[7]
        );
    }
    return guid;
}

ComTracker::ComTracker()
    : m_enableCallstackCapture(false)
    , m_totalInterfacesCreated(0)
    , m_totalInterfacesDestroyed(0) {
    // Initialize symbol handler for callstack capture
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
}

ComTracker::~ComTracker() {
    // Clean up symbol handler
    SymCleanup(GetCurrentProcess());
}

ComTracker& ComTracker::GetInstance() {
    if (!s_instance) {
        s_instance = new ComTracker();
    }
    return *s_instance;
}

bool ComTracker::Initialize(bool enableCallstackCapture) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_enableCallstackCapture = enableCallstackCapture;
    
    // Initialize the interface version information
    InitializeVersionInfo();
    
    // Initialize statistics
    m_totalInterfacesCreated = 0;
    m_totalInterfacesDestroyed = 0;
    
    std::cout << "COM Tracker initialized. Callstack capture " 
              << (m_enableCallstackCapture ? "enabled" : "disabled") << std::endl;
    
    return true;
}

void ComTracker::InitializeVersionInfo() {
    // DXGI Interfaces
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::DXGI_1_0;
        info.versionString = "DXGI 1.0";
        m_versionInfo["IDXGIFactory"] = info;
        m_versionInfo["IDXGIAdapter"] = info;
        m_versionInfo["IDXGIOutput"] = info;
        m_versionInfo["IDXGIDevice"] = info;
        m_versionInfo["IDXGISwapChain"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::DXGI_1_1;
        info.versionString = "DXGI 1.1";
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory));
        m_versionInfo["IDXGIFactory1"] = info;
        m_versionInfo["IDXGIAdapter1"] = info;
        m_versionInfo["IDXGIDevice1"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::DXGI_1_2;
        info.versionString = "DXGI 1.2";
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory1));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory));
        m_versionInfo["IDXGIFactory2"] = info;
        m_versionInfo["IDXGIAdapter2"] = info;
        m_versionInfo["IDXGIDevice2"] = info;
        m_versionInfo["IDXGISwapChain1"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::DXGI_1_3;
        info.versionString = "DXGI 1.3";
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory2));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory1));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory));
        m_versionInfo["IDXGIFactory3"] = info;
        m_versionInfo["IDXGIAdapter3"] = info;
        m_versionInfo["IDXGIDevice3"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::DXGI_1_4;
        info.versionString = "DXGI 1.4";
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory3));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory2));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory1));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory));
        m_versionInfo["IDXGIFactory4"] = info;
        m_versionInfo["IDXGISwapChain3"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::DXGI_1_5;
        info.versionString = "DXGI 1.5";
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory4));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory3));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory2));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory1));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory));
        m_versionInfo["IDXGIFactory5"] = info;
        m_versionInfo["IDXGISwapChain4"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::DXGI_1_6;
        info.versionString = "DXGI 1.6";
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory5));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory4));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory3));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory2));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory1));
        info.compatibleIIDs.push_back(__uuidof(IDXGIFactory));
        m_versionInfo["IDXGIFactory6"] = info;
        m_versionInfo["IDXGIAdapter4"] = info;
    }
    
    // D3D11 Interfaces
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::D3D11_0;
        info.versionString = "D3D11.0";
        m_versionInfo["ID3D11Device"] = info;
        m_versionInfo["ID3D11DeviceContext"] = info;
        m_versionInfo["ID3D11Resource"] = info;
        m_versionInfo["ID3D11Texture2D"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::D3D11_1;
        info.versionString = "D3D11.1";
        info.compatibleIIDs.push_back(__uuidof(ID3D11Device));
        m_versionInfo["ID3D11Device1"] = info;
        m_versionInfo["ID3D11DeviceContext1"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::D3D11_2;
        info.versionString = "D3D11.2";
        info.compatibleIIDs.push_back(__uuidof(ID3D11Device1));
        info.compatibleIIDs.push_back(__uuidof(ID3D11Device));
        m_versionInfo["ID3D11Device2"] = info;
        m_versionInfo["ID3D11DeviceContext2"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::D3D11_3;
        info.versionString = "D3D11.3";
        info.compatibleIIDs.push_back(__uuidof(ID3D11Device2));
        info.compatibleIIDs.push_back(__uuidof(ID3D11Device1));
        info.compatibleIIDs.push_back(__uuidof(ID3D11Device));
        m_versionInfo["ID3D11Device3"] = info;
        m_versionInfo["ID3D11DeviceContext3"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::D3D11_4;
        info.versionString = "D3D11.4";
        info.compatibleIIDs.push_back(__uuidof(ID3D11Device3));
        info.compatibleIIDs.push_back(__uuidof(ID3D11Device2));
        info.compatibleIIDs.push_back(__uuidof(ID3D11Device1));
        info.compatibleIIDs.push_back(__uuidof(ID3D11Device));
        m_versionInfo["ID3D11Device4"] = info;
        m_versionInfo["ID3D11DeviceContext4"] = info;
    }
    
    // D3D12 Interfaces
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::D3D12_0;
        info.versionString = "D3D12.0";
        m_versionInfo["ID3D12Device"] = info;
        m_versionInfo["ID3D12CommandQueue"] = info;
        m_versionInfo["ID3D12CommandList"] = info;
        m_versionInfo["ID3D12Resource"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::D3D12_1;
        info.versionString = "D3D12.1";
        info.compatibleIIDs.push_back(__uuidof(ID3D12Device));
        m_versionInfo["ID3D12Device1"] = info;
    }
    
    {
        InterfaceVersionInfo info;
        info.version = DXInterfaceVersion::D3D12_2;
        info.versionString = "D3D12.2";
        info.compatibleIIDs.push_back(__uuidof(ID3D12Device1));
        info.compatibleIIDs.push_back(__uuidof(ID3D12Device));
        m_versionInfo["ID3D12Device2"] = info;
    }
}

void ComTracker::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Log tracked interface statistics
    std::cout << "=== COM Tracker Shutdown Summary ===" << std::endl;
    std::cout << "Total interfaces created: " << m_totalInterfacesCreated << std::endl;
    std::cout << "Total interfaces destroyed: " << m_totalInterfacesDestroyed << std::endl;
    std::cout << "Interfaces still tracked: " << m_trackedInterfaces.size() << std::endl;
    
    // Dump any potentially leaked interfaces
    if (!m_trackedInterfaces.empty()) {
        std::cout << "Potential leaked interfaces:" << std::endl;
        DumpTrackedInterfaces(true);
    }
    
    // Clear tracked interfaces
    m_trackedInterfaces.clear();
}

bool ComTracker::TrackInterface(void* pInterface, const std::string& interfaceType, ULONG initialRefCount) {
    if (!pInterface) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if already tracked
    if (m_trackedInterfaces.find(pInterface) != m_trackedInterfaces.end()) {
        return false;
    }
    
    // Create a new tracked interface record
    TrackedComInterface interfaceInfo;
    interfaceInfo.pInterface = pInterface;
    interfaceInfo.interfaceType = interfaceType;
    interfaceInfo.refCount = initialRefCount;
    interfaceInfo.creationThreadId = GetCurrentThreadId();
    interfaceInfo.creationTime = std::time(nullptr);
    interfaceInfo.version = DXInterfaceVersion::Unknown;
    interfaceInfo.isCustomImplementation = false;
    
    // Look up the version information
    auto versionInfoIt = m_versionInfo.find(interfaceType);
    if (versionInfoIt != m_versionInfo.end()) {
        interfaceInfo.version = versionInfoIt->second.version;
    }
    
    // Check for custom implementation if it's an IUnknown
    if (pInterface) {
        IUnknown* pUnknown = static_cast<IUnknown*>(pInterface);
        interfaceInfo.isCustomImplementation = DetectCustomImplementation(pUnknown);
    }
    
    // Capture callstack if enabled
    if (m_enableCallstackCapture) {
        interfaceInfo.callstack = CaptureCallstack(2);  // Skip this function and the caller
    }
    
    // Add to tracked interfaces
    m_trackedInterfaces[pInterface] = interfaceInfo;
    
    // Update statistics
    m_totalInterfacesCreated++;
    
    return true;
}

bool ComTracker::UpdateRefCount(void* pInterface, ULONG newRefCount, bool isAddRef) {
    if (!pInterface) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Find the interface in our tracking map
    auto it = m_trackedInterfaces.find(pInterface);
    if (it == m_trackedInterfaces.end()) {
        // Not tracked, perhaps created before we started tracking
        if (isAddRef) {
            // If this is an AddRef, we might want to start tracking it
            return TrackInterface(pInterface, "Unknown (late tracked)", newRefCount);
        }
        return false;
    }
    
    // Update the reference count
    it->second.refCount = newRefCount;
    
    // If reference count is 0 and this is a Release, remove from tracking
    if (newRefCount == 0 && !isAddRef) {
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
    
    // Find the interface in our tracking map
    auto it = m_trackedInterfaces.find(pInterface);
    if (it == m_trackedInterfaces.end()) {
        return false;
    }
    
    // Remove from tracking
    m_trackedInterfaces.erase(it);
    m_totalInterfacesDestroyed++;
    
    return true;
}

int ComTracker::CheckForIssues() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int issueCount = 0;
    
    // Check for high reference counts that may indicate leaks
    for (const auto& pair : m_trackedInterfaces) {
        if (pair.second.refCount > 100) {
            std::cout << "Suspiciously high ref count: " << pair.second.interfaceType 
                     << " (0x" << std::hex << (uintptr_t)pair.second.pInterface << std::dec 
                     << ") has " << pair.second.refCount << " references" << std::endl;
            issueCount++;
        }
    }
    
    return issueCount;
}

void ComTracker::DumpTrackedInterfaces(bool onlyLeaked) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t count = 0;
    
    std::cout << "===========================================================================" << std::endl;
    std::cout << "COM Interface Tracking Dump" << (onlyLeaked ? " (Potentially Leaked Only)" : "") << std::endl;
    std::cout << "===========================================================================" << std::endl;
    std::cout << std::left 
              << std::setw(18) << "Address" 
              << std::setw(30) << "Interface Type" 
              << std::setw(12) << "Ref Count" 
              << std::setw(12) << "Thread ID"
              << std::setw(20) << "Creation Time"
              << std::setw(15) << "Version"
              << std::setw(10) << "Custom"
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
        
        // Get version string
        std::string versionStr = "Unknown";
        auto versionInfoIt = m_versionInfo.find(interface.interfaceType);
        if (versionInfoIt != m_versionInfo.end()) {
            versionStr = versionInfoIt->second.versionString;
        }
        
        // Print interface info
        std::cout << std::left 
                  << "0x" << std::hex << std::setw(16) << (uintptr_t)interface.pInterface << std::dec << " "
                  << std::setw(30) << interface.interfaceType 
                  << std::setw(12) << interface.refCount
                  << std::setw(12) << interface.creationThreadId
                  << std::setw(20) << timeBuffer
                  << std::setw(15) << versionStr
                  << std::setw(10) << (interface.isCustomImplementation ? "Yes" : "No")
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

DXInterfaceVersion ComTracker::GetInterfaceVersion(IUnknown* pInterface, REFIID riid) {
    std::string interfaceType = GetInterfaceTypeFromIID(riid);
    
    // Look up version info based on interface type
    auto versionInfoIt = m_versionInfo.find(interfaceType);
    if (versionInfoIt != m_versionInfo.end()) {
        return versionInfoIt->second.version;
    }
    
    return DXInterfaceVersion::Unknown;
}

bool ComTracker::GetFallbackInterface(IUnknown* pInterface, REFIID requestedIID, void** ppvObject) {
    if (!pInterface || !ppvObject) {
        return false;
    }
    
    // Start by logging the attempt
    std::string requestedInterfaceType = GetInterfaceTypeFromIID(requestedIID);
    std::cout << "Attempting fallback for " << requestedInterfaceType << std::endl;
    
    // If we don't have version info for this interface, we can't find a fallback
    auto versionInfoIt = m_versionInfo.find(requestedInterfaceType);
    if (versionInfoIt == m_versionInfo.end()) {
        return false;
    }
    
    // Try each compatible interface in the fallback chain
    for (const GUID& fallbackIID : versionInfoIt->second.compatibleIIDs) {
        std::string fallbackType = GetInterfaceTypeFromIID(fallbackIID);
        std::cout << "  Trying fallback to " << fallbackType << std::endl;
        
        // Try to get the fallback interface
        HRESULT hr = pInterface->QueryInterface(fallbackIID, ppvObject);
        if (SUCCEEDED(hr)) {
            std::cout << "  Fallback successful to " << fallbackType << std::endl;
            return true;
        }
    }
    
    std::cout << "  No fallback available for " << requestedInterfaceType << std::endl;
    return false;
}

bool ComTracker::IsCustomImplementation(IUnknown* pInterface) {
    if (!pInterface) {
        return false;
    }
    
    return DetectCustomImplementation(pInterface);
}

bool ComTracker::DetectCustomImplementation(IUnknown* pInterface) {
    if (!pInterface) {
        return false;
    }
    
    // Get the vtable pointer
    void** vtable = *reinterpret_cast<void***>(pInterface);
    if (!vtable) {
        return false;
    }
    
    // Check if the vtable is in a module address range
    HMODULE modules[1024];
    DWORD cbNeeded;
    if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &cbNeeded)) {
        int numModules = cbNeeded / sizeof(HMODULE);
        
        for (int i = 0; i < numModules; i++) {
            MODULEINFO moduleInfo;
            if (GetModuleInformation(GetCurrentProcess(), modules[i], &moduleInfo, sizeof(MODULEINFO))) {
                // Check if vtable is within this module's address range
                uintptr_t moduleStart = (uintptr_t)moduleInfo.lpBaseOfDll;
                uintptr_t moduleEnd = moduleStart + moduleInfo.SizeOfImage;
                uintptr_t vtableAddr = (uintptr_t)vtable;
                
                if (vtableAddr >= moduleStart && vtableAddr < moduleEnd) {
                    // Get the module name
                    char moduleName[MAX_PATH];
                    if (GetModuleFileNameExA(GetCurrentProcess(), modules[i], moduleName, MAX_PATH)) {
                        // If this is a standard DirectX module, it's not a custom implementation
                        std::string moduleNameStr = moduleName;
                        std::transform(moduleNameStr.begin(), moduleNameStr.end(), moduleNameStr.begin(), ::tolower);
                        
                        if (moduleNameStr.find("d3d11.dll") != std::string::npos ||
                            moduleNameStr.find("d3d12.dll") != std::string::npos ||
                            moduleNameStr.find("dxgi.dll") != std::string::npos) {
                            return false;
                        }
                    }
                }
            }
        }
    }
    
    // If we get here, we couldn't determine for sure, so assume it's custom
    return true;
}

bool ComTracker::CheckVTableSignature(void** vtable, size_t methodCount, const std::vector<uint8_t>& signature) {
    if (!vtable || signature.empty()) {
        return false;
    }
    
    // Check the first few bytes of each method against the signature
    for (size_t i = 0; i < methodCount && i * 4 < signature.size(); i++) {
        uint8_t* methodAddr = (uint8_t*)vtable[i];
        bool match = true;
        
        for (size_t j = 0; j < 4 && i * 4 + j < signature.size(); j++) {
            if (methodAddr[j] != signature[i * 4 + j]) {
                match = false;
                break;
            }
        }
        
        if (!match) {
            return false;
        }
    }
    
    return true;
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
    if (riid == __uuidof(IDXGIFactory3)) return "IDXGIFactory3";
    if (riid == __uuidof(IDXGIFactory4)) return "IDXGIFactory4";
    if (riid == __uuidof(IDXGIFactory5)) return "IDXGIFactory5";
    if (riid == __uuidof(IDXGIFactory6)) return "IDXGIFactory6";
    
    if (riid == __uuidof(IDXGISwapChain)) return "IDXGISwapChain";
    if (riid == __uuidof(IDXGISwapChain1)) return "IDXGISwapChain1";
    if (riid == __uuidof(IDXGISwapChain2)) return "IDXGISwapChain2";
    if (riid == __uuidof(IDXGISwapChain3)) return "IDXGISwapChain3";
    if (riid == __uuidof(IDXGISwapChain4)) return "IDXGISwapChain4";
    
    if (riid == __uuidof(IDXGIAdapter)) return "IDXGIAdapter";
    if (riid == __uuidof(IDXGIAdapter1)) return "IDXGIAdapter1";
    if (riid == __uuidof(IDXGIAdapter2)) return "IDXGIAdapter2";
    if (riid == __uuidof(IDXGIAdapter3)) return "IDXGIAdapter3";
    if (riid == __uuidof(IDXGIAdapter4)) return "IDXGIAdapter4";
    
    if (riid == __uuidof(IDXGIOutput)) return "IDXGIOutput";
    if (riid == __uuidof(IDXGIOutput1)) return "IDXGIOutput1";
    if (riid == __uuidof(IDXGIOutput2)) return "IDXGIOutput2";
    if (riid == __uuidof(IDXGIOutput3)) return "IDXGIOutput3";
    if (riid == __uuidof(IDXGIOutput4)) return "IDXGIOutput4";
    if (riid == __uuidof(IDXGIOutput5)) return "IDXGIOutput5";
    if (riid == __uuidof(IDXGIOutput6)) return "IDXGIOutput6";
    
    if (riid == __uuidof(IDXGIDevice)) return "IDXGIDevice";
    if (riid == __uuidof(IDXGIDevice1)) return "IDXGIDevice1";
    if (riid == __uuidof(IDXGIDevice2)) return "IDXGIDevice2";
    if (riid == __uuidof(IDXGIDevice3)) return "IDXGIDevice3";
    
    // D3D11 interfaces
    if (riid == __uuidof(ID3D11Device)) return "ID3D11Device";
    if (riid == __uuidof(ID3D11Device1)) return "ID3D11Device1";
    if (riid == __uuidof(ID3D11Device2)) return "ID3D11Device2";
    if (riid == __uuidof(ID3D11Device3)) return "ID3D11Device3";
    if (riid == __uuidof(ID3D11Device4)) return "ID3D11Device4";
    if (riid == __uuidof(ID3D11Device5)) return "ID3D11Device5";
    
    if (riid == __uuidof(ID3D11DeviceContext)) return "ID3D11DeviceContext";
    if (riid == __uuidof(ID3D11DeviceContext1)) return "ID3D11DeviceContext1";
    if (riid == __uuidof(ID3D11DeviceContext2)) return "ID3D11DeviceContext2";
    if (riid == __uuidof(ID3D11DeviceContext3)) return "ID3D11DeviceContext3";
    if (riid == __uuidof(ID3D11DeviceContext4)) return "ID3D11DeviceContext4";
    
    // D3D12 interfaces
    if (riid == __uuidof(ID3D12Device)) return "ID3D12Device";
    if (riid == __uuidof(ID3D12Device1)) return "ID3D12Device1";
    if (riid == __uuidof(ID3D12Device2)) return "ID3D12Device2";
    if (riid == __uuidof(ID3D12Device3)) return "ID3D12Device3";
    if (riid == __uuidof(ID3D12Device4)) return "ID3D12Device4";
    if (riid == __uuidof(ID3D12Device5)) return "ID3D12Device5";
    if (riid == __uuidof(ID3D12Device6)) return "ID3D12Device6";
    if (riid == __uuidof(ID3D12Device7)) return "ID3D12Device7";
    if (riid == __uuidof(ID3D12Device8)) return "ID3D12Device8";
    if (riid == __uuidof(ID3D12Device9)) return "ID3D12Device9";
    
    if (riid == __uuidof(ID3D12CommandQueue)) return "ID3D12CommandQueue";
    if (riid == __uuidof(ID3D12CommandList)) return "ID3D12CommandList";
    if (riid == __uuidof(ID3D12GraphicsCommandList)) return "ID3D12GraphicsCommandList";
    if (riid == __uuidof(ID3D12GraphicsCommandList1)) return "ID3D12GraphicsCommandList1";
    if (riid == __uuidof(ID3D12GraphicsCommandList2)) return "ID3D12GraphicsCommandList2";
    if (riid == __uuidof(ID3D12Resource)) return "ID3D12Resource";
    
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