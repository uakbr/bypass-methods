#pragma once

#include "com_ptr.h"
#include <Windows.h>
#include <dxgi.h>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>
#include <vector>
#include <ctime>

namespace UndownUnlock {
namespace DXHook {

/**
 * @brief Structure to hold information about a tracked COM interface
 */
struct TrackedComInterface {
    void* pInterface;                   // Raw pointer to interface
    std::string interfaceType;          // Human-readable interface type
    std::atomic<ULONG> refCount;        // Current reference count
    DWORD creationThreadId;             // Thread ID that created the interface
    std::time_t creationTime;           // Time when interface was created
    std::vector<std::string> callstack;  // Optional callstack at creation time
};

/**
 * @brief Class to track COM interface creation and reference counting
 * 
 * This singleton class tracks the lifecycle of COM interfaces and helps
 * detect reference counting issues and memory leaks.
 */
class ComTracker {
public:
    /**
     * @brief Get the singleton instance
     */
    static ComTracker& GetInstance();

    /**
     * @brief Initialize the COM tracker
     * @param enableCallstackCapture Whether to capture callstacks (performance impact)
     * @return True if initialization was successful
     */
    bool Initialize(bool enableCallstackCapture = false);

    /**
     * @brief Shutdown and report any leaked interfaces
     */
    void Shutdown();

    /**
     * @brief Track a new COM interface
     * @param pInterface Pointer to the interface
     * @param interfaceType Type string for the interface
     * @param initialRefCount Initial reference count (usually 1)
     * @return True if tracking was successful
     */
    bool TrackInterface(void* pInterface, const std::string& interfaceType, ULONG initialRefCount = 1);

    /**
     * @brief Update the reference count for a tracked interface
     * @param pInterface Pointer to the interface
     * @param newRefCount New reference count
     * @param isAddRef True if this is from AddRef, false for Release
     * @return True if the interface was found and updated
     */
    bool UpdateRefCount(void* pInterface, ULONG newRefCount, bool isAddRef);

    /**
     * @brief Untrack an interface when its reference count reaches 0
     * @param pInterface Pointer to the interface
     * @return True if the interface was found and untracked
     */
    bool UntrackInterface(void* pInterface);

    /**
     * @brief Check for reference counting issues
     * @return Number of issues detected
     */
    int CheckForIssues();

    /**
     * @brief Dump all tracked interfaces to log
     * @param onlyLeaked Only dump interfaces with refCount > 0
     */
    void DumpTrackedInterfaces(bool onlyLeaked = true);

    /**
     * @brief Get total number of tracked interfaces
     */
    size_t GetTrackedInterfaceCount() const;

    /**
     * @brief Get number of potential leaked interfaces
     */
    size_t GetPotentialLeakCount() const;

private:
    // Private constructor for singleton
    ComTracker();
    ~ComTracker();

    // Deleted copy/move constructors and assignments
    ComTracker(const ComTracker&) = delete;
    ComTracker(ComTracker&&) = delete;
    ComTracker& operator=(const ComTracker&) = delete;
    ComTracker& operator=(ComTracker&&) = delete;

    // Singleton instance
    static ComTracker* s_instance;

    // Thread safety
    mutable std::mutex m_mutex;

    // Tracking data
    std::unordered_map<void*, TrackedComInterface> m_trackedInterfaces;
    
    // Configuration
    bool m_enableCallstackCapture;
    
    // Statistics
    std::atomic<size_t> m_totalInterfacesCreated;
    std::atomic<size_t> m_totalInterfacesDestroyed;
    
    // Helper methods
    std::vector<std::string> CaptureCallstack(int skipFrames = 1, int maxFrames = 32);
    std::string GetInterfaceTypeFromIID(REFIID riid);
    bool IsKnownInterface(REFIID riid);
};

// Helper macros for COM tracking
#define TRACK_COM_CREATION(ptr, type) \
    UndownUnlock::DXHook::ComTracker::GetInstance().TrackInterface(ptr, type)

#define TRACK_COM_ADDREF(ptr, count) \
    UndownUnlock::DXHook::ComTracker::GetInstance().UpdateRefCount(ptr, count, true)

#define TRACK_COM_RELEASE(ptr, count) \
    UndownUnlock::DXHook::ComTracker::GetInstance().UpdateRefCount(ptr, count, false)

} // namespace DXHook
} // namespace UndownUnlock 