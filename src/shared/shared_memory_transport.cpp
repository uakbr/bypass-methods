#include "../../include/shared_memory_transport.h"
#include "../../include/frame_extractor.h"
#include <iostream>
#include <sstream>

namespace UndownUnlock {
namespace DXHook {

// Constants
constexpr uint32_t SHARED_MEMORY_MAGIC = 0x554E444F;  // "UNDO" in hex
constexpr uint32_t SHARED_MEMORY_VERSION = 1;
constexpr uint32_t DEFAULT_MAX_FRAMES = 4;
constexpr uint32_t HEADER_SIZE = sizeof(SharedMemoryHeader);
constexpr uint32_t SLOT_HEADER_SIZE = sizeof(FrameSlotHeader);

SharedMemoryTransport::SharedMemoryTransport(const std::string& name, size_t initialSize)
    : m_name(name)
    , m_sharedMemoryHandle(nullptr)
    , m_mappedAddress(nullptr)
    , m_header(nullptr)
    , m_initialSize(initialSize)
    , m_newFrameEvent(nullptr) {
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
        m_sharedMemoryHandle = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            (DWORD)m_initialSize,
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
        m_header->frameDataOffset = HEADER_SIZE;
        m_header->producerIndex.store(0);
        m_header->consumerIndex.store(0);
        m_header->maxFrames = DEFAULT_MAX_FRAMES;
        
        // Calculate frame size - allocate enough space for a 1080p frame with some extra
        uint32_t estimatedFrameSize = 1920 * 1080 * 4 + SLOT_HEADER_SIZE;
        m_header->frameSize = estimatedFrameSize;
        
        // Initialize sequence counter
        m_header->sequence.store(0);
        
        // Initialize the slim reader/writer lock
        InitializeSRWLock(&m_header->srwLock);
        
        std::cout << "Created shared memory: " << m_name << ", size: " << m_initialSize 
                 << ", max frames: " << m_header->maxFrames << std::endl;
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
        
        if (m_header->version != SHARED_MEMORY_VERSION) {
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
    uint32_t produceIndex = m_header->producerIndex.load();
    uint32_t consumeIndex = m_header->consumerIndex.load();
    
    // Check if the buffer is full
    if ((produceIndex + 1) % m_header->maxFrames == consumeIndex) {
        // If full, overwrite the oldest frame
        // In a real implementation, we might want to decide on a different policy
        m_header->consumerIndex.store((consumeIndex + 1) % m_header->maxFrames);
    }
    
    return produceIndex;
}

uint32_t SharedMemoryTransport::GetNextFrameToRead() {
    uint32_t produceIndex = m_header->producerIndex.load();
    uint32_t consumeIndex = m_header->consumerIndex.load();
    
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

bool SharedMemoryTransport::WriteFrame(const FrameData& frameData) {
    if (!m_mappedAddress || !m_header) {
        return false;
    }
    
    // Calculate total size needed for this frame
    uint32_t requiredSize = SLOT_HEADER_SIZE + (uint32_t)frameData.data.size();
    
    // Check if the frame is too large for our slots
    if (requiredSize > m_header->frameSize) {
        std::cerr << "Frame too large for shared memory slot: " << requiredSize 
                 << " > " << m_header->frameSize << std::endl;
        
        // In a real implementation, we would resize the shared memory here
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
        slotHeader->dataSize = (uint32_t)frameData.data.size();
        slotHeader->flags = 0;
        
        // Write the frame data
        uint8_t* dataPtr = static_cast<uint8_t*>(slotAddress) + SLOT_HEADER_SIZE;
        memcpy(dataPtr, frameData.data.data(), frameData.data.size());
        
        // Update the producer index
        m_header->producerIndex.store((slotIndex + 1) % m_header->maxFrames);
        
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
        
        // Read the frame data
        uint8_t* dataPtr = static_cast<uint8_t*>(slotAddress) + SLOT_HEADER_SIZE;
        frameData.data.resize(slotHeader->dataSize);
        memcpy(frameData.data.data(), dataPtr, slotHeader->dataSize);
        
        // Update the consumer index
        m_header->consumerIndex.store((slotIndex + 1) % m_header->maxFrames);
        
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

} // namespace DXHook
} // namespace UndownUnlock 