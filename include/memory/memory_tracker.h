#pragma once

#include <Windows.h>
#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>
#include <ctime>
#include <cstdint>
#include <atomic>
#include <functional>

namespace UndownUnlock {
namespace Memory {

/**
 * @brief Structure to track memory allocations
 */
struct TrackedAllocation {
    void* address;               // Memory address
    size_t size;                 // Size of allocation
    std::time_t allocationTime;  // When the memory was allocated
    DWORD threadId;              // Thread ID that made the allocation
    const char* file;            // Source file where allocation was made
    int line;                    // Line number in source file
    std::string tag;             // Optional tag for categorizing allocations
    std::vector<std::string> callstack; // Optional callstack at allocation point
};

/**
 * @brief Statistics about memory usage
 */
struct MemoryStats {
    size_t currentAllocations = 0;     // Number of current allocations
    size_t currentBytes = 0;           // Total bytes currently allocated
    size_t peakAllocations = 0;        // Peak number of allocations
    size_t peakBytes = 0;              // Peak bytes allocated
    size_t totalAllocations = 0;       // Total number of allocations made
    size_t totalBytes = 0;             // Total bytes allocated over time
    size_t totalFrees = 0;             // Total number of free operations
};

/**
 * @brief Memory leak detector and tracker
 */
class MemoryTracker {
public:
    /**
     * @brief Get the singleton instance
     */
    static MemoryTracker& GetInstance();

    /**
     * @brief Initialize memory tracking
     * @param enableCallstackCapture Whether to capture callstacks (impacts performance)
     * @return True if initialization succeeded
     */
    bool Initialize(bool enableCallstackCapture = false);

    /**
     * @brief Shutdown memory tracking and report leaks
     */
    void Shutdown();

    /**
     * @brief Track a memory allocation
     * @param address Memory address that was allocated
     * @param size Size of the allocation in bytes
     * @param file Source file making the allocation (use __FILE__)
     * @param line Line number in source file (use __LINE__)
     * @param tag Optional tag for categorizing the allocation
     */
    void TrackAllocation(void* address, size_t size, const char* file, int line, const std::string& tag = "");

    /**
     * @brief Untrack a memory allocation (when memory is freed)
     * @param address Memory address to untrack
     * @return True if the address was found and untracked
     */
    bool UntrackAllocation(void* address);

    /**
     * @brief Get memory allocation statistics
     */
    MemoryStats GetStats() const;

    /**
     * @brief Dump memory leak report to console
     * @param onlyLeaks Only report leaks (not all tracked allocations)
     */
    void DumpLeakReport(bool onlyLeaks = true);

    /**
     * @brief Set threshold for leak reporting
     * @param thresholdBytes Don't report leaks smaller than this size (default: 0)
     */
    void SetLeakThreshold(size_t thresholdBytes);

    /**
     * @brief Add tag to reporting filter
     * @param tag Tag to include/exclude in reporting
     * @param include True to include, false to exclude
     */
    void AddTagFilter(const std::string& tag, bool include);

    /**
     * @brief Set leak report callback
     * @param callback Function to call with leak information
     */
    void SetLeakReportCallback(std::function<void(const std::vector<TrackedAllocation>&)> callback);

private:
    // Private constructor for singleton
    MemoryTracker();
    ~MemoryTracker();

    // Deleted copy/move constructors and assignments
    MemoryTracker(const MemoryTracker&) = delete;
    MemoryTracker(MemoryTracker&&) = delete;
    MemoryTracker& operator=(const MemoryTracker&) = delete;
    MemoryTracker& operator=(MemoryTracker&&) = delete;

    // Singleton instance
    static MemoryTracker* s_instance;

    // Thread safety
    mutable std::mutex m_mutex;

    // Tracking data
    std::unordered_map<void*, TrackedAllocation> m_allocations;
    
    // Statistics
    MemoryStats m_stats;
    
    // Configuration
    bool m_enableCallstackCapture;
    size_t m_leakThreshold;
    std::vector<std::pair<std::string, bool>> m_tagFilters; // tag, include/exclude
    std::function<void(const std::vector<TrackedAllocation>&)> m_leakReportCallback;
    
    // Helper methods
    std::vector<std::string> CaptureCallstack(int skipFrames = 1, int maxFrames = 32);
    bool ShouldReportTag(const std::string& tag) const;
};

// Helper macros for memory tracking
#define TRACK_ALLOCATION(ptr, size) \
    UndownUnlock::Memory::MemoryTracker::GetInstance().TrackAllocation(ptr, size, __FILE__, __LINE__)

#define TRACK_ALLOCATION_TAGGED(ptr, size, tag) \
    UndownUnlock::Memory::MemoryTracker::GetInstance().TrackAllocation(ptr, size, __FILE__, __LINE__, tag)

#define UNTRACK_ALLOCATION(ptr) \
    UndownUnlock::Memory::MemoryTracker::GetInstance().UntrackAllocation(ptr)

// Overloaded new/delete operators for automatic tracking
void* operator new(size_t size);
void* operator new[](size_t size);
void* operator new(size_t size, const char* file, int line);
void* operator new[](size_t size, const char* file, int line);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete(void* ptr, const char* file, int line) noexcept;
void operator delete[](void* ptr, const char* file, int line) noexcept;

// Macros to replace new/delete with tracked versions
#define new new(__FILE__, __LINE__)

} // namespace Memory
} // namespace UndownUnlock 