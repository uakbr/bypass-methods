#include "../../include/frame_extractor.h"
#include "../../include/shared_memory_transport.h"
#include "../../include/hooks/com_interface_wrapper.h"
#include <chrono>
#include <iostream>

namespace UndownUnlock {
namespace DXHook {

FrameExtractor::FrameExtractor()
    : m_device(nullptr)
    , m_deviceContext(nullptr)
    , m_currentWidth(0)
    , m_currentHeight(0)
    , m_currentFormat(DXGI_FORMAT_UNKNOWN)
    , m_sharedMemory(nullptr)
    , m_frameSequence(0) {
}

FrameExtractor::~FrameExtractor() {
    // RAII wrapper automatically releases the staging texture when it goes out of scope
    m_stagingTextureWrapper.Release();
    
    // We don't release the device or context as we don't own them
}

bool FrameExtractor::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!device || !context) {
        std::cerr << "Invalid device or context" << std::endl;
        return false;
    }
    
    m_device = device;
    m_deviceContext = context;
    m_frameSequence = 0;
    
    std::cout << "Frame extractor initialized" << std::endl;
    return true;
}

bool FrameExtractor::CreateOrResizeStagingTexture(uint32_t width, uint32_t height, DXGI_FORMAT format) {
    // If we already have a staging texture with the right size and format, reuse it
    if (m_stagingTextureWrapper && m_currentWidth == width && m_currentHeight == height && m_currentFormat == format) {
        return true;
    }
    
    // Release the old texture if it exists
    m_stagingTextureWrapper.Release();
    
    // Create a new staging texture
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    
    ID3D11Texture2D* stagingTexture = nullptr;
    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        std::cerr << "Failed to create staging texture: " << std::hex << hr << std::endl;
        return false;
    }
    
    // Wrap the texture with RAII
    m_stagingTextureWrapper.Reset(stagingTexture, true);
    
    // Store the new dimensions and format
    m_currentWidth = width;
    m_currentHeight = height;
    m_currentFormat = format;
    
    std::cout << "Created staging texture: " << width << "x" << height << ", format: " << format << std::endl;
    return true;
}

bool FrameExtractor::ExtractFrame(IDXGISwapChain* pSwapChain) {
    if (!pSwapChain || !m_device || !m_deviceContext) {
        return false;
    }
    
    try {
        // Get the back buffer from the swap chain
        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));
        
        if (FAILED(hr) || !backBuffer) {
            std::cerr << "Failed to get back buffer from swap chain: " << std::hex << hr << std::endl;
            return false;
        }
        
        // Get the backbuffer description
        D3D11_TEXTURE2D_DESC backBufferDesc;
        backBuffer->GetDesc(&backBufferDesc);
        
        // Create or resize the staging texture if needed
        if (!CreateOrResizeStagingTexture(backBufferDesc.Width, backBufferDesc.Height, backBufferDesc.Format)) {
            backBuffer->Release();
            return false;
        }
        
        // Copy the back buffer to the staging texture
        m_deviceContext->CopyResource(m_stagingTextureWrapper.Get(), backBuffer);
        
        // We're done with the back buffer
        backBuffer->Release();
        
        // Map the staging texture to get access to its data
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        hr = m_deviceContext->Map(m_stagingTextureWrapper.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
        
        if (FAILED(hr)) {
            std::cerr << "Failed to map staging texture: " << std::hex << hr << std::endl;
            return false;
        }
        
        // Create the frame data structure
        FrameData frameData;
        frameData.width = m_currentWidth;
        frameData.height = m_currentHeight;
        frameData.stride = mappedResource.RowPitch;
        frameData.format = m_currentFormat;
        frameData.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        frameData.sequence = m_frameSequence++;
        
        // Copy the data
        const uint8_t* src = static_cast<const uint8_t*>(mappedResource.pData);
        size_t totalSize = frameData.height * frameData.stride;
        frameData.data.resize(totalSize);
        memcpy(frameData.data.data(), src, totalSize);
        
        // Unmap the texture
        m_deviceContext->Unmap(m_stagingTextureWrapper.Get(), 0);
        
        // Convert the frame format if needed
        FrameData convertedData;
        bool needsConversion = ConvertFrameFormat(frameData, convertedData);
        
        // Use the callback if set
        if (m_frameCallback) {
            m_frameCallback(needsConversion ? convertedData : frameData);
        }
        
        // Write to shared memory if available
        if (m_sharedMemory) {
            m_sharedMemory->WriteFrame(needsConversion ? convertedData : frameData);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in ExtractFrame: " << e.what() << std::endl;
        return false;
    }
}

bool FrameExtractor::ConvertFrameFormat(const FrameData& sourceData, FrameData& convertedData) {
    // Currently, we only support direct pass-through
    // Future implementations will add format conversion for non-standard formats
    
    // For now, we'll just check if the format is one we can easily handle
    switch (sourceData.format) {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            // These formats are already compatible, no conversion needed
            return false;
        
        default:
            // For unsupported formats, we'd implement conversion here
            // For now, just return false to indicate no conversion was done
            std::cerr << "Unsupported frame format: " << sourceData.format << std::endl;
            return false;
    }
}

void FrameExtractor::SetFrameCallback(std::function<void(const FrameData&)> callback) {
    m_frameCallback = callback;
}

void FrameExtractor::SetSharedMemoryTransport(SharedMemoryTransport* sharedMemory) {
    m_sharedMemory = sharedMemory;
}

} // namespace DXHook
} // namespace UndownUnlock 