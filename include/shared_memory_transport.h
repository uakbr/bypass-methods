#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace UndownUnlock {
namespace DXHook {

// Forward declarations
struct FrameData;

// Constants for cache alignment
constexpr size_t CACHE_LINE_SIZE = 64;  // Standard cache line size on modern CPUs

// Compression types
enum class CompressionType : uint32_t {
    None = 0,        // No compression
    RLE = 1,         // Run-length encoding (basic)
    LZ4 = 2,         // LZ4 compression (fast)
    ZSTD = 3         // Zstandard compression (better ratio)
};

/**
 * @brief Cache-aligned atomic counter for optimal performance
 */
struct alignas(CACHE_LINE_SIZE) CacheAlignedCounter {
    std::atomic<uint32_t> value;
    
    // Padding to ensure counter takes up a full cache line
    uint8_t padding[CACHE_LINE_SIZE - sizeof(std::atomic<uint32_t>)];
    
    CacheAlignedCounter() : value(0) {}
    
    operator uint32_t() const { return value.load(std::memory_order_acquire); }
    uint32_t operator=(uint32_t desired) { 
        value.store(desired, std::memory_order_release); 
        return desired;
    }
    
    uint32_t fetch_add(uint32_t increment) {
        return value.fetch_add(increment, std::memory_order_acq_rel);
    }
};

/**
 * @brief Shared memory frame buffer header
 */
struct SharedMemoryHeader {
    uint32_t magic;                      // Magic number to identify the shared memory
    uint32_t version;                    // Version of the shared memory format
    uint32_t bufferSize;                 // Total size of the buffer
    uint32_t frameDataOffset;            // Offset to the start of frame data
    CacheAlignedCounter producerIndex;   // Current producer position (cache-aligned)
    CacheAlignedCounter consumerIndex;   // Current consumer position (cache-aligned)
    CacheAlignedCounter sequence;        // Frame sequence counter (cache-aligned)
    uint32_t maxFrames;                  // Maximum number of frames in the ring buffer
    uint32_t frameSize;                  // Size of each frame slot
    uint32_t compressionFlags;           // Supported compression flags
    SRWLOCK srwLock;                     // Slim reader/writer lock for synchronization
};

/**
 * @brief Frame slot header in shared memory
 */
struct FrameSlotHeader {
    uint32_t sequence;                   // Frame sequence number
    uint32_t width;                      // Frame width
    uint32_t height;                     // Frame height
    uint32_t stride;                     // Bytes per row
    uint32_t format;                     // Pixel format (DXGI_FORMAT)
    uint64_t timestamp;                  // Capture timestamp
    uint32_t dataSize;                   // Size of frame data
    uint32_t originalSize;               // Original size before compression
    CompressionType compression;         // Compression type used
    uint32_t flags;                      // Additional flags
};

/**
 * @brief Class for transporting frame data via shared memory
 */
class SharedMemoryTransport {
public:
    /**
     * @brief Constructor
     * @param name Name of the shared memory object
     * @param initialSize Initial size of the shared memory buffer
     */
    SharedMemoryTransport(const std::string& name, size_t initialSize = 64 * 1024 * 1024);
    ~SharedMemoryTransport();
    
    /**
     * @brief Initialize the shared memory transport
     * @return True if initialization succeeded
     */
    bool Initialize();
    
    /**
     * @brief Write frame data to shared memory
     * @param frameData The frame data to write
     * @return True if write succeeded
     */
    bool WriteFrame(const FrameData& frameData);
    
    /**
     * @brief Read frame data from shared memory
     * @param frameData Output frame data
     * @return True if read succeeded
     */
    bool ReadFrame(FrameData& frameData);
    
    /**
     * @brief Wait for a new frame to be available
     * @param timeoutMs Timeout in milliseconds, 0 for no wait, INFINITE for infinite wait
     * @return True if a new frame is available
     */
    bool WaitForFrame(DWORD timeoutMs = 1000);
    
    /**
     * @brief Resize the shared memory buffer if needed
     * @param newSize New size for the buffer
     * @return True if resize succeeded
     */
    bool ResizeBuffer(size_t newSize);
    
    /**
     * @brief Set compression type for frame data
     * @param type Compression type to use
     * @return True if compression type is supported
     */
    bool SetCompressionType(CompressionType type);
    
    /**
     * @brief Get current compression type
     * @return Current compression type
     */
    CompressionType GetCompressionType() const;
    
private:
    std::string m_name;                // Name of the shared memory object
    HANDLE m_sharedMemoryHandle;       // Handle to the shared memory
    void* m_mappedAddress;             // Mapped address of the shared memory
    SharedMemoryHeader* m_header;      // Pointer to the shared memory header
    size_t m_initialSize;              // Initial size of the shared memory buffer
    CompressionType m_compressionType; // Current compression type
    
    // Event for signaling new frames
    HANDLE m_newFrameEvent;
    
    // Helper functions
    void* GetFrameSlotAddress(uint32_t index);
    bool AcquireWriteLock();
    void ReleaseWriteLock();
    bool AcquireReadLock();
    void ReleaseReadLock();
    
    // Frame management
    uint32_t GetAvailableFrameSlot();
    uint32_t GetNextFrameToRead();
    
    // Compression functions
    bool CompressFrameData(const std::vector<uint8_t>& inputData, 
                         std::vector<uint8_t>& outputData,
                         CompressionType type);
    bool DecompressFrameData(const std::vector<uint8_t>& inputData, 
                           std::vector<uint8_t>& outputData,
                           uint32_t originalSize,
                           CompressionType type);
};

} // namespace DXHook
} // namespace UndownUnlock 