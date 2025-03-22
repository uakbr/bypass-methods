#include "../../include/shared_memory_transport.h"
#include "../../include/frame_extractor.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

// For compression support
#ifdef USE_LZ4
#include <lz4.h>
#endif

#ifdef USE_ZSTD
#include <zstd.h>
#endif

namespace UndownUnlock {
namespace DXHook {

// Constants
constexpr uint32_t SHARED_MEMORY_MAGIC = 0x554E444F;  // "UNDO" in hex
constexpr uint32_t SHARED_MEMORY_VERSION = 2;         // Updated version for cache alignment
constexpr uint32_t DEFAULT_MAX_FRAMES = 4;
constexpr uint32_t HEADER_SIZE = sizeof(SharedMemoryHeader);
constexpr uint32_t SLOT_HEADER_SIZE = sizeof(FrameSlotHeader);

// Simple RLE compression for when external libraries aren't available
namespace {
    // RLE compress - basic implementation for when no compression libraries are available
    size_t RleCompress(const uint8_t* src, size_t srcSize, uint8_t* dst, size_t dstCapacity) {
        if (srcSize == 0 || dstCapacity < 2) return 0;
        
        size_t di = 0;
        size_t si = 0;
        
        while (si < srcSize && di + 2 <= dstCapacity) {
            uint8_t currentByte = src[si++];
            uint8_t count = 1;
            
            // Count repeating bytes
            while (si < srcSize && src[si] == currentByte && count < 255) {
                count++;
                si++;
            }
            
            // Write count and byte
            dst[di++] = count;
            dst[di++] = currentByte;
        }
        
        return (si == srcSize) ? di : 0; // Return compressed size or 0 if failed
    }
    
    // RLE decompress
    size_t RleDecompress(const uint8_t* src, size_t srcSize, uint8_t* dst, size_t dstCapacity) {
        if (srcSize == 0 || dstCapacity == 0) return 0;
        
        size_t di = 0;
        size_t si = 0;
        
        while (si + 2 <= srcSize && di < dstCapacity) {
            uint8_t count = src[si++];
            uint8_t byte = src[si++];
            
            for (uint8_t i = 0; i < count && di < dstCapacity; i++) {
                dst[di++] = byte;
            }
        }
        
        return di; // Return decompressed size
    }
}

SharedMemoryTransport::SharedMemoryTransport(const std::string& name, size_t initialSize)
    : m_name(name)
    , m_sharedMemoryHandle(nullptr)
    , m_mappedAddress(nullptr)
    , m_header(nullptr)
    , m_initialSize(initialSize)
    , m_newFrameEvent(nullptr)
    , m_compressionType(CompressionType::None) {
}

SharedMemoryTransport::~SharedMemoryTransport() {
    // Clean up resources
    if (m_mappedAddress) {
        UnmapViewOfFile(m_mappedAddress);
        m_mappedAddress = nullptr;
    }
    
    if (m_sharedMemoryHandle) {
        CloseHandle(m_sharedMemoryHandle);
        m_sharedMemoryHandle = nullptr;
    }
    
    if (m_newFrameEvent) {
        CloseHandle(m_newFrameEvent);
        m_newFrameEvent = nullptr;
    }
}

bool SharedMemoryTransport::Initialize() {
    // Create event for signaling new frames
    std::string eventName = m_name + "_Event";
    m_newFrameEvent = CreateEventA(nullptr, FALSE, FALSE, eventName.c_str());
    if (!m_newFrameEvent) {
        std::cerr << "Failed to create event for shared memory: " << GetLastError() << std::endl;
        return false;
    }
    
    // Try to open existing shared memory first
    m_sharedMemoryHandle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, m_name.c_str());
    
    // If it doesn't exist, create a new one
    if (!m_sharedMemoryHandle) {
        // Ensure initial size is a multiple of the page size for better performance
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        size_t pageSize = sysInfo.dwPageSize;
        m_initialSize = ((m_initialSize + pageSize - 1) / pageSize) * pageSize;
        
        m_sharedMemoryHandle = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            (DWORD)(m_initialSize >> 32),
            (DWORD)(m_initialSize & 0xFFFFFFFF),
            m_name.c_str()
        );
        
        if (!m_sharedMemoryHandle) {
            std::cerr << "Failed to create shared memory: " << GetLastError() << std::endl;
            return false;
        }
        
        // Map the shared memory
        m_mappedAddress = MapViewOfFile(
            m_sharedMemoryHandle,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            m_initialSize
        );
        
        if (!m_mappedAddress) {
            std::cerr << "Failed to map shared memory: " << GetLastError() << std::endl;
            CloseHandle(m_sharedMemoryHandle);
            m_sharedMemoryHandle = nullptr;
            return false;
        }
        
        // Initialize the header
        m_header = static_cast<SharedMemoryHeader*>(m_mappedAddress);
        m_header->magic = SHARED_MEMORY_MAGIC;
        m_header->version = SHARED_MEMORY_VERSION;
        m_header->bufferSize = (uint32_t)m_initialSize;
        
        // Ensure frame data offset is aligned to cache line boundary for better performance
        m_header->frameDataOffset = ((HEADER_SIZE + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;
        
        // Initialize cache-aligned counters
        m_header->producerIndex = 0;
        m_header->consumerIndex = 0;
        m_header->sequence = 0;
        
        m_header->maxFrames = DEFAULT_MAX_FRAMES;
        
        // Calculate frame size - allocate enough space for a 1080p frame with some extra
        // and ensure it's aligned to a cache line boundary
        uint32_t estimatedFrameSize = 1920 * 1080 * 4 + SLOT_HEADER_SIZE;
        m_header->frameSize = ((estimatedFrameSize + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;
        
        // Set supported compression flags
        m_header->compressionFlags = (1 << static_cast<uint32_t>(CompressionType::None)) | 
                                    (1 << static_cast<uint32_t>(CompressionType::RLE));
                                    
        #ifdef USE_LZ4
        m_header->compressionFlags |= (1 << static_cast<uint32_t>(CompressionType::LZ4));
        #endif
        
        #ifdef USE_ZSTD
        m_header->compressionFlags |= (1 << static_cast<uint32_t>(CompressionType::ZSTD));
        #endif
        
        // Initialize the slim reader/writer lock
        InitializeSRWLock(&m_header->srwLock);
        
        std::cout << "Created shared memory: " << m_name 
                 << ", size: " << m_initialSize 
                 << ", max frames: " << m_header->maxFrames 
                 << ", frame size: " << m_header->frameSize << std::endl;
    }
    else {
        // Map the existing shared memory
        m_mappedAddress = MapViewOfFile(
            m_sharedMemoryHandle,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            0  // Map the entire file
        );
        
        if (!m_mappedAddress) {
            std::cerr << "Failed to map existing shared memory: " << GetLastError() << std::endl;
            CloseHandle(m_sharedMemoryHandle);
            m_sharedMemoryHandle = nullptr;
            return false;
        }
        
        // Get the header
        m_header = static_cast<SharedMemoryHeader*>(m_mappedAddress);
        
        // Verify magic and version
        if (m_header->magic != SHARED_MEMORY_MAGIC) {
            std::cerr << "Invalid shared memory magic number" << std::endl;
            UnmapViewOfFile(m_mappedAddress);
            CloseHandle(m_sharedMemoryHandle);
            m_mappedAddress = nullptr;
            m_sharedMemoryHandle = nullptr;
            return false;
        }
        
        if (m_header->version > SHARED_MEMORY_VERSION) {
            std::cerr << "Incompatible shared memory version" << std::endl;
            UnmapViewOfFile(m_mappedAddress);
            CloseHandle(m_sharedMemoryHandle);
            m_mappedAddress = nullptr;
            m_sharedMemoryHandle = nullptr;
            return false;
        }
        
        std::cout << "Connected to existing shared memory: " << m_name 
                 << ", size: " << m_header->bufferSize
                 << ", max frames: " << m_header->maxFrames << std::endl;
    }
    
    return true;
}

bool SharedMemoryTransport::AcquireWriteLock() {
    AcquireSRWLockExclusive(&m_header->srwLock);
    return true;
}

void SharedMemoryTransport::ReleaseWriteLock() {
    ReleaseSRWLockExclusive(&m_header->srwLock);
}

bool SharedMemoryTransport::AcquireReadLock() {
    AcquireSRWLockShared(&m_header->srwLock);
    return true;
}

void SharedMemoryTransport::ReleaseReadLock() {
    ReleaseSRWLockShared(&m_header->srwLock);
}

uint32_t SharedMemoryTransport::GetAvailableFrameSlot() {
    uint32_t produceIndex = m_header->producerIndex;
    uint32_t consumeIndex = m_header->consumerIndex;
    
    // Check if the buffer is full
    if ((produceIndex + 1) % m_header->maxFrames == consumeIndex) {
        // If full, overwrite the oldest frame
        m_header->consumerIndex = (consumeIndex + 1) % m_header->maxFrames;
    }
    
    return produceIndex;
}

uint32_t SharedMemoryTransport::GetNextFrameToRead() {
    uint32_t produceIndex = m_header->producerIndex;
    uint32_t consumeIndex = m_header->consumerIndex;
    
    // Check if the buffer is empty
    if (produceIndex == consumeIndex) {
        return UINT32_MAX;  // No frames available
    }
    
    return consumeIndex;
}

void* SharedMemoryTransport::GetFrameSlotAddress(uint32_t index) {
    if (!m_mappedAddress || !m_header) {
        return nullptr;
    }
    
    // Calculate the offset of the frame slot
    uint32_t offset = m_header->frameDataOffset + (index * m_header->frameSize);
    
    // Ensure we don't go past the end of the buffer
    if (offset + m_header->frameSize > m_header->bufferSize) {
        return nullptr;
    }
    
    // Return the address of the frame slot
    return static_cast<uint8_t*>(m_mappedAddress) + offset;
}

bool SharedMemoryTransport::CompressFrameData(const std::vector<uint8_t>& inputData, 
                                            std::vector<uint8_t>& outputData,
                                            CompressionType type) {
    // If no compression, just copy the data
    if (type == CompressionType::None) {
        outputData = inputData;
        return true;
    }
    
    // Allocate a maximum possible size for the compressed data
    size_t maxCompressSize = 0;
    
    switch (type) {
        case CompressionType::RLE:
            // RLE worst case is 2x the original size (if no compression achieved)
            maxCompressSize = inputData.size() * 2;
            break;
            
        case CompressionType::LZ4:
            #ifdef USE_LZ4
            maxCompressSize = LZ4_compressBound(static_cast<int>(inputData.size()));
            #else
            std::cerr << "LZ4 compression not supported in this build" << std::endl;
            return false;
            #endif
            break;
            
        case CompressionType::ZSTD:
            #ifdef USE_ZSTD
            maxCompressSize = ZSTD_compressBound(inputData.size());
            #else
            std::cerr << "ZSTD compression not supported in this build" << std::endl;
            return false;
            #endif
            break;
            
        default:
            std::cerr << "Unknown compression type: " << static_cast<int>(type) << std::endl;
            return false;
    }
    
    outputData.resize(maxCompressSize);
    size_t compressedSize = 0;
    
    // Compress based on selected algorithm
    switch (type) {
        case CompressionType::RLE:
            compressedSize = RleCompress(
                inputData.data(), 
                inputData.size(), 
                outputData.data(), 
                outputData.size()
            );
            break;
            
        case CompressionType::LZ4:
            #ifdef USE_LZ4
            compressedSize = LZ4_compress_default(
                reinterpret_cast<const char*>(inputData.data()),
                reinterpret_cast<char*>(outputData.data()),
                static_cast<int>(inputData.size()),
                static_cast<int>(outputData.size())
            );
            #endif
            break;
            
        case CompressionType::ZSTD:
            #ifdef USE_ZSTD
            compressedSize = ZSTD_compress(
                outputData.data(),
                outputData.size(),
                inputData.data(),
                inputData.size(),
                1  // Compression level
            );
            #endif
            break;
    }
    
    if (compressedSize == 0) {
        // Compression failed or produced larger output, use uncompressed data
        outputData = inputData;
        return false;
    }
    
    // Resize output to actual compressed size
    outputData.resize(compressedSize);
    return true;
}

bool SharedMemoryTransport::DecompressFrameData(const std::vector<uint8_t>& inputData, 
                                              std::vector<uint8_t>& outputData,
                                              uint32_t originalSize,
                                              CompressionType type) {
    // If no compression, just copy the data
    if (type == CompressionType::None) {
        outputData = inputData;
        return true;
    }
    
    // Prepare output buffer with original size
    outputData.resize(originalSize);
    bool success = false;
    
    switch (type) {
        case CompressionType::RLE:
            success = RleDecompress(
                inputData.data(), 
                inputData.size(), 
                outputData.data(), 
                outputData.size()
            ) == originalSize;
            break;
            
        case CompressionType::LZ4:
            #ifdef USE_LZ4
            success = LZ4_decompress_safe(
                reinterpret_cast<const char*>(inputData.data()),
                reinterpret_cast<char*>(outputData.data()),
                static_cast<int>(inputData.size()),
                static_cast<int>(outputData.size())
            ) == static_cast<int>(originalSize);
            #else
            std::cerr << "LZ4 decompression not supported in this build" << std::endl;
            success = false;
            #endif
            break;
            
        case CompressionType::ZSTD:
            #ifdef USE_ZSTD
            success = ZSTD_decompress(
                outputData.data(),
                outputData.size(),
                inputData.data(),
                inputData.size()
            ) == originalSize;
            #else
            std::cerr << "ZSTD decompression not supported in this build" << std::endl;
            success = false;
            #endif
            break;
            
        default:
            std::cerr << "Unknown compression type: " << static_cast<int>(type) << std::endl;
            success = false;
    }
    
    if (!success) {
        // If decompression failed, fallback to input data
        outputData = inputData;
    }
    
    return success;
}

bool SharedMemoryTransport::WriteFrame(const FrameData& frameData) {
    if (!m_mappedAddress || !m_header) {
        return false;
    }
    
    // Prepare frame data for writing
    std::vector<uint8_t> compressedData;
    bool compressionSuccessful = false;
    
    // Attempt compression if compression is enabled
    if (m_compressionType != CompressionType::None) {
        compressionSuccessful = CompressFrameData(frameData.data, compressedData, m_compressionType);
    }
    
    // If compression failed or is not enabled, use original data
    const std::vector<uint8_t>& dataToWrite = 
        (compressionSuccessful && compressedData.size() < frameData.data.size()) ? 
        compressedData : frameData.data;
    
    CompressionType effectiveCompression = 
        (compressionSuccessful && compressedData.size() < frameData.data.size()) ? 
        m_compressionType : CompressionType::None;
    
    // Calculate total size needed for this frame
    uint32_t requiredSize = SLOT_HEADER_SIZE + (uint32_t)dataToWrite.size();
    
    // Check if the frame is too large for our slots
    if (requiredSize > m_header->frameSize) {
        std::cerr << "Frame too large for shared memory slot: " << requiredSize 
                 << " > " << m_header->frameSize << std::endl;
        
        // ToDo: Implement dynamic resizing
        return false;
    }
    
    // Acquire write lock
    AcquireWriteLock();
    
    try {
        // Get the slot to write to
        uint32_t slotIndex = GetAvailableFrameSlot();
        void* slotAddress = GetFrameSlotAddress(slotIndex);
        
        if (!slotAddress) {
            std::cerr << "Failed to get frame slot address" << std::endl;
            ReleaseWriteLock();
            return false;
        }
        
        // Write the frame header
        FrameSlotHeader* slotHeader = static_cast<FrameSlotHeader*>(slotAddress);
        slotHeader->sequence = m_header->sequence.fetch_add(1);
        slotHeader->width = frameData.width;
        slotHeader->height = frameData.height;
        slotHeader->stride = frameData.stride;
        slotHeader->format = static_cast<uint32_t>(frameData.format);
        slotHeader->timestamp = frameData.timestamp;
        slotHeader->dataSize = (uint32_t)dataToWrite.size();
        slotHeader->originalSize = (uint32_t)frameData.data.size();
        slotHeader->compression = effectiveCompression;
        slotHeader->flags = 0;
        
        // Write the frame data
        uint8_t* dataPtr = static_cast<uint8_t*>(slotAddress) + SLOT_HEADER_SIZE;
        memcpy(dataPtr, dataToWrite.data(), dataToWrite.size());
        
        // Update the producer index
        m_header->producerIndex = (slotIndex + 1) % m_header->maxFrames;
        
        // Release the lock before signaling
        ReleaseWriteLock();
        
        // Signal that a new frame is available
        SetEvent(m_newFrameEvent);
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in WriteFrame: " << e.what() << std::endl;
        ReleaseWriteLock();
        return false;
    }
}

bool SharedMemoryTransport::ReadFrame(FrameData& frameData) {
    if (!m_mappedAddress || !m_header) {
        return false;
    }
    
    // Acquire read lock
    AcquireReadLock();
    
    try {
        // Get the slot to read from
        uint32_t slotIndex = GetNextFrameToRead();
        
        // No frames available
        if (slotIndex == UINT32_MAX) {
            ReleaseReadLock();
            return false;
        }
        
        void* slotAddress = GetFrameSlotAddress(slotIndex);
        
        if (!slotAddress) {
            std::cerr << "Failed to get frame slot address for reading" << std::endl;
            ReleaseReadLock();
            return false;
        }
        
        // Read the frame header
        FrameSlotHeader* slotHeader = static_cast<FrameSlotHeader*>(slotAddress);
        frameData.width = slotHeader->width;
        frameData.height = slotHeader->height;
        frameData.stride = slotHeader->stride;
        frameData.format = static_cast<DXGI_FORMAT>(slotHeader->format);
        frameData.timestamp = slotHeader->timestamp;
        frameData.sequence = slotHeader->sequence;
        
        // Read the compressed frame data
        uint8_t* dataPtr = static_cast<uint8_t*>(slotAddress) + SLOT_HEADER_SIZE;
        std::vector<uint8_t> compressedData(dataPtr, dataPtr + slotHeader->dataSize);
        
        // Decompress if necessary
        if (slotHeader->compression != CompressionType::None) {
            DecompressFrameData(
                compressedData, 
                frameData.data, 
                slotHeader->originalSize, 
                slotHeader->compression
            );
        } else {
            frameData.data = compressedData;
        }
        
        // Update the consumer index
        m_header->consumerIndex = (slotIndex + 1) % m_header->maxFrames;
        
        // Release the lock
        ReleaseReadLock();
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in ReadFrame: " << e.what() << std::endl;
        ReleaseReadLock();
        return false;
    }
}

bool SharedMemoryTransport::WaitForFrame(DWORD timeoutMs) {
    if (!m_newFrameEvent) {
        return false;
    }
    
    // Wait for the event
    DWORD result = WaitForSingleObject(m_newFrameEvent, timeoutMs);
    return result == WAIT_OBJECT_0;
}

bool SharedMemoryTransport::ResizeBuffer(size_t newSize) {
    // Not implemented yet - would allow dynamic resizing of the shared memory
    // to accommodate larger frames
    std::cerr << "SharedMemoryTransport::ResizeBuffer not implemented yet" << std::endl;
    return false;
}

bool SharedMemoryTransport::SetCompressionType(CompressionType type) {
    // Check if the compression type is supported
    if (m_header && (m_header->compressionFlags & (1 << static_cast<uint32_t>(type)))) {
        m_compressionType = type;
        return true;
    }
    
    return false;
}

CompressionType SharedMemoryTransport::GetCompressionType() const {
    return m_compressionType;
}

} // namespace DXHook
} // namespace UndownUnlock 