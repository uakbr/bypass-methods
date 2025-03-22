#pragma once

#include <Windows.h>
#include <streams.h>
#include <strmif.h>
#include <dshow.h>
#include <initguid.h>
#include <uuids.h>
#include <mutex>
#include <memory>
#include <atomic>

namespace UndownUnlock {
namespace VirtualCamera {

// {5C2CD55D-7DB8-4F65-9AF2-8FE8A7C30F29}
DEFINE_GUID(CLSID_UndownUnlockVirtualCamera, 
    0x5c2cd55d, 0x7db8, 0x4f65, 0x9a, 0xf2, 0x8f, 0xe8, 0xa7, 0xc3, 0xf, 0x29);

// {8AC87D1A-66E5-4F82-9A3F-C1D36E8D154E}
DEFINE_GUID(IID_IUndownUnlockVirtualCamera, 
    0x8ac87d1a, 0x66e5, 0x4f82, 0x9a, 0x3f, 0xc1, 0xd3, 0x6e, 0x8d, 0x15, 0x4e);

/**
 * @brief Custom interface for the virtual camera filter
 */
MIDL_INTERFACE("8AC87D1A-66E5-4F82-9A3F-C1D36E8D154E")
IUndownUnlockVirtualCamera : public IUnknown
{
public:
    // Update the current frame
    STDMETHOD(UpdateFrame)(BYTE* pData, LONG lSize) PURE;

    // Set output format
    STDMETHOD(SetOutputFormat)(int width, int height, int fps) PURE;

    // Set signature spoofing parameters
    STDMETHOD(SetSpoofingParams)(const GUID& deviceGuid, LPCWSTR deviceName, LPCWSTR devicePath) PURE;
};

/**
 * @brief Pin for the virtual camera filter output
 */
class CVirtualCameraOutputPin : public CSourceStream
{
public:
    CVirtualCameraOutputPin(HRESULT* phr, CSource* pFilter, LPCWSTR pinName);
    ~CVirtualCameraOutputPin();

    // IPin methods
    STDMETHODIMP Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt);
    STDMETHODIMP ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt);
    STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE* pmt);
    STDMETHODIMP QueryPinInfo(PIN_INFO* pInfo);
    STDMETHODIMP QueryDirection(PIN_DIRECTION* pPinDir);
    
    // CSourceStream methods
    HRESULT GetMediaType(int iPosition, CMediaType* pMediaType);
    HRESULT CheckMediaType(const CMediaType* pMediaType);
    HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties);
    HRESULT FillBuffer(IMediaSample* pSample);
    HRESULT OnThreadCreate();
    HRESULT OnThreadDestroy();
    HRESULT OnThreadStartPlay();
    
    // Custom methods
    HRESULT UpdateFrame(BYTE* pData, LONG lSize);
    HRESULT SetOutputFormat(int width, int height, int fps);

private:
    CCritSec m_cSharedState;       // Critical section for shared state
    std::atomic<bool> m_bNewFrameAvailable; // Is a new frame available?
    int m_nWidth;                  // Output width
    int m_nHeight;                 // Output height
    int m_nFPS;                    // Output frame rate
    REFERENCE_TIME m_rtFrameLength; // Frame duration
    REFERENCE_TIME m_rtNextFrameTime; // Time for next frame
    
    // Frame buffer management
    std::unique_ptr<BYTE[]> m_pFrameBuffer;
    LONG m_lBufferSize;
    
    // Helper methods
    HRESULT InitMediaType(CMediaType* pMediaType);
    void ApplyCameraEffects(BYTE* pData, LONG lSize);
    REFERENCE_TIME GetCurrentTimestamp();
};

/**
 * @brief Virtual camera filter implementation
 */
class CVirtualCameraFilter : public CSource, public IUndownUnlockVirtualCamera
{
public:
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT* phr);
    DECLARE_IUNKNOWN;
    
    // IUnknown methods
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);
    
    // IUndownUnlockVirtualCamera methods
    STDMETHODIMP UpdateFrame(BYTE* pData, LONG lSize);
    STDMETHODIMP SetOutputFormat(int width, int height, int fps);
    STDMETHODIMP SetSpoofingParams(const GUID& deviceGuid, LPCWSTR deviceName, LPCWSTR devicePath);

private:
    CVirtualCameraFilter(LPUNKNOWN lpunk, HRESULT* phr);
    ~CVirtualCameraFilter();
    
    CVirtualCameraOutputPin* m_pOutputPin;
    CCritSec m_cStateLock;
    
    // Spoofing parameters
    GUID m_deviceGuid;
    WCHAR m_deviceName[MAX_PATH];
    WCHAR m_devicePath[MAX_PATH];
};

/**
 * @brief DirectShow filter registration/unregistration helpers
 */
class DirectShowRegistration {
public:
    /**
     * @brief Register the virtual camera filter with the system
     * @return True if successful
     */
    static bool RegisterFilter();

    /**
     * @brief Unregister the virtual camera filter from the system
     * @return True if successful
     */
    static bool UnregisterFilter();

    /**
     * @brief Check if the filter is registered
     * @return True if registered
     */
    static bool IsFilterRegistered();

private:
    static bool AddFilterToRegistry(
        const GUID& clsid, 
        const WCHAR* wszName,
        const WCHAR* wszDescription,
        const GUID& subType);
        
    static bool RemoveFilterFromRegistry(const GUID& clsid);
};

} // namespace VirtualCamera
} // namespace UndownUnlock 