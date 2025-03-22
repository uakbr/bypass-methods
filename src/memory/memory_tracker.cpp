#include "../../include/memory/memory_tracker.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <dbghelp.h>
#include <algorithm>

#pragma comment(lib, "dbghelp.lib")

namespace UndownUnlock {
namespace Memory {

// Initialize static members
MemoryTracker* MemoryTracker::s_instance = nullptr;

// Global flag to avoid recursion in new/delete operators
thread_local bool g_trackingEnabled = true;

// Original new/delete operators
static void* (*original_new)(size_t) = nullptr;
static void (*original_delete)(void*) = nullptr;

MemoryTracker& MemoryTracker::GetInstance() {
    if (!s_instance) {
        s_instance = new(std::nothrow) MemoryTracker();
    }
    return *s_instance;
}

MemoryTracker::MemoryTracker()
    : m_enableCallstackCapture(false)
    , m_leakThreshold(0) {
    
    // Initialize statistics
    m_stats = MemoryStats();
}

MemoryTracker::~MemoryTracker() {
    Shutdown();
}

bool MemoryTracker::Initialize(bool enableCallstackCapture) {
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
    
    // Store original new/delete operators
    original_new = std::get_new_handler();
    
    std::cout << "Memory Tracker initialized. Callstack capture " 
              << (m_enableCallstackCapture ? "enabled" : "disabled") << std::endl;
    return true;
}

void MemoryTracker::Shutdown() {
    // Report leaks before clearing
    DumpLeakReport(true);
    
    // Clear tracked allocations
    std::lock_guard<std::mutex> lock(m_mutex);
    m_allocations.clear();
    
    // Cleanup symbol handler if it was initialized
    if (m_enableCallstackCapture) {
        SymCleanup(GetCurrentProcess());
    }
    
    std::cout << "Memory Tracker shutdown. Final statistics: " 
              << m_stats.totalAllocations << " allocations, " 
              << m_stats.totalBytes << " bytes allocated, "
              << m_stats.totalFrees << " frees." << std::endl;
    
    if (m_stats.currentAllocations > 0) {
        std::cout << "WARNING: " << m_stats.currentAllocations << " allocations still tracked "
                  << "(" << m_stats.currentBytes << " bytes)" << std::endl;
    }
}

void MemoryTracker::TrackAllocation(void* address, size_t size, const char* file, int line, const std::string& tag) {
    if (!address) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Create a new tracked allocation
    TrackedAllocation allocation;
    allocation.address = address;
    allocation.size = size;
    allocation.allocationTime = std::time(nullptr);
    allocation.threadId = GetCurrentThreadId();
    allocation.file = file;
    allocation.line = line;
    allocation.tag = tag;
    
    // Capture callstack if enabled
    if (m_enableCallstackCapture) {
        allocation.callstack = CaptureCallstack(3); // Skip this function, TRACK_ALLOCATION, and new
    }
    
    // Add to tracking map
    m_allocations[address] = allocation;
    
    // Update statistics
    m_stats.currentAllocations++;
    m_stats.currentBytes += size;
    m_stats.totalAllocations++;
    m_stats.totalBytes += size;
    
    // Update peak statistics
    m_stats.peakAllocations = std::max(m_stats.peakAllocations, m_stats.currentAllocations);
    m_stats.peakBytes = std::max(m_stats.peakBytes, m_stats.currentBytes);
}

bool MemoryTracker::UntrackAllocation(void* address) {
    if (!address) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_allocations.find(address);
    if (it == m_allocations.end()) {
        // This could be an allocation made before tracking was enabled
        return false;
    }
    
    // Update statistics
    m_stats.currentAllocations--;
    m_stats.currentBytes -= it->second.size;
    m_stats.totalFrees++;
    
    // Remove from tracking map
    m_allocations.erase(it);
    
    return true;
}

MemoryStats MemoryTracker::GetStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void MemoryTracker::DumpLeakReport(bool onlyLeaks) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_allocations.empty()) {
        std::cout << "No memory allocations currently being tracked." << std::endl;
        return;
    }
    
    // Collect leaks for reporting
    std::vector<TrackedAllocation> leaks;
    for (const auto& pair : m_allocations) {
        const TrackedAllocation& alloc = pair.second;
        
        // Skip if under threshold
        if (alloc.size < m_leakThreshold) {
            continue;
        }
        
        // Skip if tag is filtered
        if (!ShouldReportTag(alloc.tag)) {
            continue;
        }
        
        leaks.push_back(alloc);
    }
    
    // Sort leaks by size (largest first)
    std::sort(leaks.begin(), leaks.end(), [](const TrackedAllocation& a, const TrackedAllocation& b) {
        return a.size > b.size;
    });
    
    // Print report header
    std::cout << "======================= Memory Leak Report =======================" << std::endl;
    std::cout << std::left << std::setw(18) << "Address" 
              << std::setw(12) << "Size" 
              << std::setw(20) << "Allocation Time"
              << std::setw(12) << "Thread ID"
              << std::setw(30) << "Location"
              << std::setw(20) << "Tag"
              << std::endl;
    std::cout << "--------------------------------------------------------------------" << std::endl;
    
    // Print each leak
    size_t totalLeakBytes = 0;
    for (const auto& leak : leaks) {
        // Format time
        char timeBuffer[64];
        std::tm* timeinfo = std::localtime(&leak.allocationTime);
        std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        
        // Format location
        std::stringstream location;
        location << leak.file << ":" << leak.line;
        
        // Print leak info
        std::cout << std::left 
                  << "0x" << std::hex << std::setw(16) << (uintptr_t)leak.address << std::dec << " "
                  << std::setw(12) << leak.size
                  << std::setw(20) << timeBuffer
                  << std::setw(12) << leak.threadId
                  << std::setw(30) << location.str()
                  << std::setw(20) << leak.tag
                  << std::endl;
        
        // Print callstack if available
        if (!leak.callstack.empty()) {
            std::cout << "    Callstack:" << std::endl;
            for (const auto& frame : leak.callstack) {
                std::cout << "        " << frame << std::endl;
            }
            std::cout << std::endl;
        }
        
        totalLeakBytes += leak.size;
    }
    
    // Print summary
    std::cout << "--------------------------------------------------------------------" << std::endl;
    std::cout << "Total: " << leaks.size() << " leaks, " << totalLeakBytes << " bytes" << std::endl;
    std::cout << "All allocations: " << m_allocations.size() << " allocations, " << m_stats.currentBytes << " bytes" << std::endl;
    std::cout << "=====================================================================" << std::endl;
    
    // Call leak report callback if set
    if (m_leakReportCallback && !leaks.empty()) {
        m_leakReportCallback(leaks);
    }
}

void MemoryTracker::SetLeakThreshold(size_t thresholdBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_leakThreshold = thresholdBytes;
}

void MemoryTracker::AddTagFilter(const std::string& tag, bool include) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Remove existing filter for this tag
    m_tagFilters.erase(
        std::remove_if(m_tagFilters.begin(), m_tagFilters.end(),
                     [&tag](const std::pair<std::string, bool>& filter) { return filter.first == tag; }),
        m_tagFilters.end());
    
    // Add new filter
    m_tagFilters.push_back(std::make_pair(tag, include));
}

void MemoryTracker::SetLeakReportCallback(std::function<void(const std::vector<TrackedAllocation>&)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_leakReportCallback = callback;
}

std::vector<std::string> MemoryTracker::CaptureCallstack(int skipFrames, int maxFrames) {
    std::vector<std::string> callstack;
    
    if (!m_enableCallstackCapture) {
        return callstack;
    }
    
    // Capture the raw callstack
    void* callstackAddresses[64];
    WORD framesCaptured = CaptureStackBackTrace(skipFrames, min(maxFrames, 64), callstackAddresses, NULL);
    
    // Allocate symbol info buffer
    constexpr size_t MAX_SYMBOL_NAME = 256;
    SYMBOL_INFO* symbolInfo = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + MAX_SYMBOL_NAME * sizeof(char));
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

bool MemoryTracker::ShouldReportTag(const std::string& tag) const {
    // If no filters, report everything
    if (m_tagFilters.empty()) {
        return true;
    }
    
    // Check if tag matches any filters
    for (const auto& filter : m_tagFilters) {
        if (filter.first == tag) {
            return filter.second; // Return include/exclude flag
        }
    }
    
    // Default behavior: include if no specific filter
    return true;
}

// Global overloaded new/delete operators

void* operator new(size_t size) {
    // Skip tracking for internal allocations to avoid recursion
    bool wasEnabled = g_trackingEnabled;
    g_trackingEnabled = false;
    
    // Allocate memory
    void* ptr = malloc(size);
    
    // Track if enabled
    if (wasEnabled && ptr) {
        MemoryTracker::GetInstance().TrackAllocation(ptr, size, "unknown", 0);
    }
    
    g_trackingEnabled = wasEnabled;
    return ptr;
}

void* operator new[](size_t size) {
    return operator new(size);
}

void* operator new(size_t size, const char* file, int line) {
    // Skip tracking for internal allocations
    bool wasEnabled = g_trackingEnabled;
    g_trackingEnabled = false;
    
    // Allocate memory
    void* ptr = malloc(size);
    
    // Track with file/line info
    if (wasEnabled && ptr) {
        MemoryTracker::GetInstance().TrackAllocation(ptr, size, file, line);
    }
    
    g_trackingEnabled = wasEnabled;
    return ptr;
}

void* operator new[](size_t size, const char* file, int line) {
    return operator new(size, file, line);
}

void operator delete(void* ptr) noexcept {
    // Skip tracking for internal deallocations
    bool wasEnabled = g_trackingEnabled;
    g_trackingEnabled = false;
    
    // Untrack the allocation
    if (wasEnabled && ptr) {
        MemoryTracker::GetInstance().UntrackAllocation(ptr);
    }
    
    // Free the memory
    free(ptr);
    
    g_trackingEnabled = wasEnabled;
}

void operator delete[](void* ptr) noexcept {
    operator delete(ptr);
}

void operator delete(void* ptr, const char* file, int line) noexcept {
    operator delete(ptr);
}

void operator delete[](void* ptr, const char* file, int line) noexcept {
    operator delete(ptr);
}

} // namespace Memory
} // namespace UndownUnlock 