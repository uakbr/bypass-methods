#include "../../include/virtual_camera/directshow_filter.h"
#include <dvdmedia.h>
#include <wmcodecdsp.h>
#include <Mfidl.h>
#include <strsafe.h>
#include <shlwapi.h>
#include <algorithm>
#include <iostream>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

namespace UndownUnlock {
namespace VirtualCamera {

// Default buffer size (720p RGB)
const LONG DEFAULT_BUFFER_SIZE = 1280 * 720 * 3;

//
// CVirtualCameraOutputPin implementation
//

CVirtualCameraOutputPin::CVirtualCameraOutputPin(HRESULT* phr, CSource* pFilter, LPCWSTR pinName)
    : CSourceStream(NAME("UndownUnlock Virtual Camera Pin"), phr, pFilter, pinName)
    , m_bNewFrameAvailable(false)
    , m_nWidth(1280)
    , m_nHeight(720)
    , m_nFPS(30)
    , m_rtFrameLength(333333) // 1/30 sec in 100ns units
    , m_rtNextFrameTime(0)
    , m_pFrameBuffer(nullptr)
    , m_lBufferSize(0)
{
    // Initialize critical section for thread safety
    m_cSharedState.Init();

    // Allocate initial frame buffer
    m_pFrameBuffer.reset(new BYTE[DEFAULT_BUFFER_SIZE]);
    m_lBufferSize = DEFAULT_BUFFER_SIZE;

    // Fill with black
    if (m_pFrameBuffer) {
        ZeroMemory(m_pFrameBuffer.get(), m_lBufferSize);
    }
}

CVirtualCameraOutputPin::~CVirtualCameraOutputPin()
{
    m_cSharedState.Term();
}

HRESULT CVirtualCameraOutputPin::GetMediaType(int iPosition, CMediaType* pMediaType)
{
    if (iPosition < 0) {
        return E_INVALIDARG;
    }

    if (iPosition > 3) {
        return VFW_S_NO_MORE_ITEMS;
    }

    // Supported formats (in preference order):
    // 0: RGB24 (most compatible)
    // 1: YUY2
    // 2: UYVY
    // 3: NV12
    
    HRESULT hr = NOERROR;
    
    switch (iPosition) {
    case 0:
        // RGB24
        hr = InitMediaType(pMediaType);
        if (SUCCEEDED(hr)) {
            pMediaType->SetType(&MEDIATYPE_Video);
            pMediaType->SetSubtype(&MEDIASUBTYPE_RGB24);
            pMediaType->SetFormatType(&FORMAT_VideoInfo);
            
            VIDEOINFOHEADER* pvih = (VIDEOINFOHEADER*)pMediaType->Format();
            pvih->bmiHeader.biCompression = BI_RGB;
            pvih->bmiHeader.biBitCount = 24;
        }
        break;
        
    case 1:
        // YUY2
        hr = InitMediaType(pMediaType);
        if (SUCCEEDED(hr)) {
            pMediaType->SetType(&MEDIATYPE_Video);
            pMediaType->SetSubtype(&MEDIASUBTYPE_YUY2);
            pMediaType->SetFormatType(&FORMAT_VideoInfo);
            
            VIDEOINFOHEADER* pvih = (VIDEOINFOHEADER*)pMediaType->Format();
            pvih->bmiHeader.biCompression = MAKEFOURCC('Y', 'U', 'Y', '2');
            pvih->bmiHeader.biBitCount = 16;
        }
        break;
        
    case 2:
        // UYVY
        hr = InitMediaType(pMediaType);
        if (SUCCEEDED(hr)) {
            pMediaType->SetType(&MEDIATYPE_Video);
            pMediaType->SetSubtype(&MEDIASUBTYPE_UYVY);
            pMediaType->SetFormatType(&FORMAT_VideoInfo);
            
            VIDEOINFOHEADER* pvih = (VIDEOINFOHEADER*)pMediaType->Format();
            pvih->bmiHeader.biCompression = MAKEFOURCC('U', 'Y', 'V', 'Y');
            pvih->bmiHeader.biBitCount = 16;
        }
        break;
        
    case 3:
        // NV12
        hr = InitMediaType(pMediaType);
        if (SUCCEEDED(hr)) {
            pMediaType->SetType(&MEDIATYPE_Video);
            pMediaType->SetSubtype(&MEDIASUBTYPE_NV12);
            pMediaType->SetFormatType(&FORMAT_VideoInfo);
            
            VIDEOINFOHEADER* pvih = (VIDEOINFOHEADER*)pMediaType->Format();
            pvih->bmiHeader.biCompression = MAKEFOURCC('N', 'V', '1', '2');
            pvih->bmiHeader.biBitCount = 12;
        }
        break;
    }

    return hr;
}

HRESULT CVirtualCameraOutputPin::CheckMediaType(const CMediaType* pMediaType)
{
    // Check if the media type is acceptable
    if (pMediaType->IsValid() == FALSE) {
        return E_INVALIDARG;
    }

    // Must be video type
    if (*pMediaType->Type() != MEDIATYPE_Video) {
        return E_INVALIDARG;
    }

    // Check format type
    if (*pMediaType->FormatType() != FORMAT_VideoInfo) {
        return E_INVALIDARG;
    }

    // Check if the subtype is one of our supported formats
    const GUID* pSubType = pMediaType->Subtype();
    if (*pSubType != MEDIASUBTYPE_RGB24 &&
        *pSubType != MEDIASUBTYPE_YUY2 &&
        *pSubType != MEDIASUBTYPE_UYVY &&
        *pSubType != MEDIASUBTYPE_NV12) {
        return E_INVALIDARG;
    }

    // Get the format block
    VIDEOINFOHEADER* pvih = (VIDEOINFOHEADER*)pMediaType->Format();
    if (pvih == NULL) {
        return E_INVALIDARG;
    }

    // Check the image dimensions
    if (pvih->bmiHeader.biWidth <= 0 || pvih->bmiHeader.biHeight <= 0) {
        return E_INVALIDARG;
    }

    // Media type is acceptable
    return S_OK;
}

HRESULT CVirtualCameraOutputPin::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());

    // Get the input pin's allocator properties
    ALLOCATOR_PROPERTIES Actual;
    
    // Calculate buffer size
    VIDEOINFOHEADER* pvih = (VIDEOINFOHEADER*)m_mt.Format();
    
    // Calculate frame size based on current media type
    LONG lSize = pvih->bmiHeader.biSizeImage;
    if (lSize == 0) {
        // RGB24 = width * height * 3
        // YUY2/UYVY = width * height * 2
        // NV12 = width * height * 3/2
        if (m_mt.subtype == MEDIASUBTYPE_RGB24) {
            lSize = pvih->bmiHeader.biWidth * abs(pvih->bmiHeader.biHeight) * 3;
        } else if (m_mt.subtype == MEDIASUBTYPE_YUY2 || m_mt.subtype == MEDIASUBTYPE_UYVY) {
            lSize = pvih->bmiHeader.biWidth * abs(pvih->bmiHeader.biHeight) * 2;
        } else if (m_mt.subtype == MEDIASUBTYPE_NV12) {
            lSize = pvih->bmiHeader.biWidth * abs(pvih->bmiHeader.biHeight) * 3 / 2;
        } else {
            // Default to RGB24 if unknown
            lSize = pvih->bmiHeader.biWidth * abs(pvih->bmiHeader.biHeight) * 3;
        }
    }

    // Set up the allocator properties
    pProperties->cBuffers = 3;              // Use 3 buffers
    pProperties->cbBuffer = lSize;          // Buffer size
    pProperties->cbAlign = 16;              // Align to 16 bytes
    pProperties->cbPrefix = 0;              // No extra data before the sample

    // Ask the allocator to reserve the memory
    HRESULT hr = pAlloc->SetProperties(pProperties, &Actual);
    if (FAILED(hr)) {
        return hr;
    }

    // Check if the allocated buffers are big enough
    if (Actual.cbBuffer < lSize) {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT CVirtualCameraOutputPin::FillBuffer(IMediaSample* pSample)
{
    CAutoLock cAutoLock(&m_cSharedState);

    // Check parameters
    if (pSample == NULL) {
        return E_POINTER;
    }

    // Set the sample time stamps
    REFERENCE_TIME rtStart, rtStop;
    
    // The current time is the start time
    rtStart = m_rtNextFrameTime;
    
    // Frame length is the duration of the frame
    m_rtNextFrameTime += m_rtFrameLength;
    
    // The end time is the next frame time
    rtStop = m_rtNextFrameTime;
    
    pSample->SetTime(&rtStart, &rtStop);
    pSample->SetSyncPoint(TRUE);
    pSample->SetDiscontinuity(FALSE);
    
    // Get pointer to the sample buffer
    BYTE* pData = NULL;
    HRESULT hr = pSample->GetPointer(&pData);
    if (FAILED(hr)) {
        return hr;
    }

    // Get the size of the buffer
    long lDataLen = pSample->GetSize();
    if (lDataLen == 0) {
        return S_OK;
    }

    // If we have new frame data, copy it to the sample buffer
    if (m_bNewFrameAvailable && m_pFrameBuffer) {
        // Make sure we don't overflow the buffer
        long lCopySize = min(lDataLen, m_lBufferSize);
        memcpy(pData, m_pFrameBuffer.get(), lCopySize);
        
        // Apply simulated camera effects like noise, lighting changes, etc.
        ApplyCameraEffects(pData, lCopySize);
        
        m_bNewFrameAvailable = false;
    } else {
        // No new frame available, fill with black
        memset(pData, 0, lDataLen);
    }

    // Set the actual data length
    pSample->SetActualDataLength(lDataLen);

    return S_OK;
}

HRESULT CVirtualCameraOutputPin::OnThreadCreate()
{
    m_rtNextFrameTime = 0;
    return NOERROR;
}

HRESULT CVirtualCameraOutputPin::OnThreadDestroy()
{
    return NOERROR;
}

HRESULT CVirtualCameraOutputPin::OnThreadStartPlay()
{
    m_rtNextFrameTime = 0;
    return NOERROR;
}

STDMETHODIMP CVirtualCameraOutputPin::Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt)
{
    return CSourceStream::Connect(pReceivePin, pmt);
}

STDMETHODIMP CVirtualCameraOutputPin::ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt)
{
    return CSourceStream::ReceiveConnection(pConnector, pmt);
}

STDMETHODIMP CVirtualCameraOutputPin::QueryAccept(const AM_MEDIA_TYPE* pmt)
{
    return CSourceStream::QueryAccept(pmt);
}

STDMETHODIMP CVirtualCameraOutputPin::QueryPinInfo(PIN_INFO* pInfo)
{
    return CSourceStream::QueryPinInfo(pInfo);
}

STDMETHODIMP CVirtualCameraOutputPin::QueryDirection(PIN_DIRECTION* pPinDir)
{
    return CSourceStream::QueryDirection(pPinDir);
}

HRESULT CVirtualCameraOutputPin::UpdateFrame(BYTE* pData, LONG lSize)
{
    CAutoLock cAutoLock(&m_cSharedState);

    if (pData == NULL || lSize <= 0) {
        return E_INVALIDARG;
    }

    // If the buffer size has changed, reallocate the buffer
    if (lSize > m_lBufferSize) {
        try {
            m_pFrameBuffer.reset(new BYTE[lSize]);
            m_lBufferSize = lSize;
        } catch (std::bad_alloc&) {
            return E_OUTOFMEMORY;
        }
    }

    // Copy the new frame data
    memcpy(m_pFrameBuffer.get(), pData, lSize);
    m_bNewFrameAvailable = true;

    return S_OK;
}

HRESULT CVirtualCameraOutputPin::SetOutputFormat(int width, int height, int fps)
{
    CAutoLock cAutoLock(&m_cSharedState);

    if (width <= 0 || height <= 0 || fps <= 0) {
        return E_INVALIDARG;
    }

    m_nWidth = width;
    m_nHeight = height;
    m_nFPS = fps;
    m_rtFrameLength = REFERENCE_TIME(10000000.0 / fps);  // Convert FPS to 100ns units

    return S_OK;
}

HRESULT CVirtualCameraOutputPin::InitMediaType(CMediaType* pMediaType)
{
    // Create and initialize the VIDEOINFOHEADER
    VIDEOINFOHEADER* pvih = (VIDEOINFOHEADER*)pMediaType->AllocFormatBuffer(sizeof(VIDEOINFOHEADER));
    if (pvih == NULL) {
        return E_OUTOFMEMORY;
    }

    ZeroMemory(pvih, sizeof(VIDEOINFOHEADER));

    // Initialize the BITMAPINFOHEADER part
    pvih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvih->bmiHeader.biWidth = m_nWidth;
    pvih->bmiHeader.biHeight = m_nHeight;  // Negative height for top-down DIB
    pvih->bmiHeader.biPlanes = 1;
    pvih->bmiHeader.biSizeImage = 0;  // Will be calculated based on the format
    pvih->bmiHeader.biXPelsPerMeter = 0;
    pvih->bmiHeader.biYPelsPerMeter = 0;
    pvih->bmiHeader.biClrUsed = 0;
    pvih->bmiHeader.biClrImportant = 0;

    // Set the frame rate
    pvih->AvgTimePerFrame = m_rtFrameLength;

    // Set the source rectangle
    SetRectEmpty(&(pvih->rcSource));

    // Set the target rectangle
    SetRectEmpty(&(pvih->rcTarget));

    // Set media type properties
    pMediaType->SetFormatType(&FORMAT_VideoInfo);
    pMediaType->SetTemporalCompression(FALSE);

    return S_OK;
}

void CVirtualCameraOutputPin::ApplyCameraEffects(BYTE* pData, LONG lSize)
{
    // No effects for now, this will be implemented in CameraSimulator class
}

REFERENCE_TIME CVirtualCameraOutputPin::GetCurrentTimestamp()
{
    // Get the current time in 100ns units
    REFERENCE_TIME rtCurrentTime = 0;
    
    // Use timeGetTime as a fallback
    DWORD dwCurrentTime = timeGetTime();
    rtCurrentTime = (REFERENCE_TIME)dwCurrentTime * 10000;  // Convert ms to 100ns units
    
    return rtCurrentTime;
}

//
// CVirtualCameraFilter implementation
//

CUnknown* WINAPI CVirtualCameraFilter::CreateInstance(LPUNKNOWN lpunk, HRESULT* phr)
{
    ASSERT(phr);
    
    CUnknown* punk = new CVirtualCameraFilter(lpunk, phr);
    if (punk == NULL) {
        *phr = E_OUTOFMEMORY;
    }
    
    return punk;
}

CVirtualCameraFilter::CVirtualCameraFilter(LPUNKNOWN lpunk, HRESULT* phr)
    : CSource(NAME("UndownUnlock Virtual Camera"), lpunk, CLSID_UndownUnlockVirtualCamera)
    , m_pOutputPin(NULL)
{
    ASSERT(phr);
    
    // Create the output pin
    m_pOutputPin = new CVirtualCameraOutputPin(phr, this, L"Output");
    if (m_pOutputPin == NULL) {
        *phr = E_OUTOFMEMORY;
        return;
    }
    
    // Initialize default device GUID and name
    m_deviceGuid = CLSID_UndownUnlockVirtualCamera;
    StringCchCopy(m_deviceName, MAX_PATH, L"UndownUnlock Virtual Camera");
    StringCchCopy(m_devicePath, MAX_PATH, L"");
}

CVirtualCameraFilter::~CVirtualCameraFilter()
{
    // Nothing to do - CBaseFilter will release the pins
}

STDMETHODIMP CVirtualCameraFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUndownUnlockVirtualCamera) {
        return GetInterface((IUndownUnlockVirtualCamera*)this, ppv);
    }
    
    return CSource::NonDelegatingQueryInterface(riid, ppv);
}

STDMETHODIMP CVirtualCameraFilter::UpdateFrame(BYTE* pData, LONG lSize)
{
    CheckPointer(m_pOutputPin, E_POINTER);
    return m_pOutputPin->UpdateFrame(pData, lSize);
}

STDMETHODIMP CVirtualCameraFilter::SetOutputFormat(int width, int height, int fps)
{
    CheckPointer(m_pOutputPin, E_POINTER);
    return m_pOutputPin->SetOutputFormat(width, height, fps);
}

STDMETHODIMP CVirtualCameraFilter::SetSpoofingParams(const GUID& deviceGuid, LPCWSTR deviceName, LPCWSTR devicePath)
{
    CAutoLock lock(&m_cStateLock);
    
    m_deviceGuid = deviceGuid;
    
    if (deviceName) {
        StringCchCopy(m_deviceName, MAX_PATH, deviceName);
    }
    
    if (devicePath) {
        StringCchCopy(m_devicePath, MAX_PATH, devicePath);
    }
    
    return S_OK;
}

//
// DirectShowRegistration implementation
//

bool DirectShowRegistration::RegisterFilter()
{
    std::cout << "Registering DirectShow filter..." << std::endl;
    
    // Add the filter to the registry
    bool result = AddFilterToRegistry(
        CLSID_UndownUnlockVirtualCamera,
        L"UndownUnlock Virtual Camera",
        L"Virtual Camera Device for UndownUnlock",
        MEDIASUBTYPE_Video
    );
    
    return result;
}

bool DirectShowRegistration::UnregisterFilter()
{
    std::cout << "Unregistering DirectShow filter..." << std::endl;
    
    // Remove the filter from the registry
    return RemoveFilterFromRegistry(CLSID_UndownUnlockVirtualCamera);
}

bool DirectShowRegistration::IsFilterRegistered()
{
    HKEY hKey = NULL;
    WCHAR keyName[MAX_PATH];
    
    // Format the registry key name
    StringCchPrintf(
        keyName,
        MAX_PATH,
        L"CLSID\\{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        CLSID_UndownUnlockVirtualCamera.Data1,
        CLSID_UndownUnlockVirtualCamera.Data2,
        CLSID_UndownUnlockVirtualCamera.Data3,
        CLSID_UndownUnlockVirtualCamera.Data4[0],
        CLSID_UndownUnlockVirtualCamera.Data4[1],
        CLSID_UndownUnlockVirtualCamera.Data4[2],
        CLSID_UndownUnlockVirtualCamera.Data4[3],
        CLSID_UndownUnlockVirtualCamera.Data4[4],
        CLSID_UndownUnlockVirtualCamera.Data4[5],
        CLSID_UndownUnlockVirtualCamera.Data4[6],
        CLSID_UndownUnlockVirtualCamera.Data4[7]
    );
    
    // Try to open the key
    LONG lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, keyName, 0, KEY_READ, &hKey);
    
    if (lResult == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    
    return false;
}

bool DirectShowRegistration::AddFilterToRegistry(
    const GUID& clsid,
    const WCHAR* wszName,
    const WCHAR* wszDescription,
    const GUID& subType)
{
    // Convert the CLSID to a string
    WCHAR wszClsid[MAX_PATH];
    StringFromGUID2(clsid, wszClsid, MAX_PATH);
    
    // Convert the subtype to a string
    WCHAR wszSubType[MAX_PATH];
    StringFromGUID2(subType, wszSubType, MAX_PATH);
    
    // Create the registry keys
    
    // CLSID\{clsid}
    WCHAR keyName[MAX_PATH];
    StringCchPrintf(keyName, MAX_PATH, L"CLSID\\%s", wszClsid);
    
    HKEY hKey = NULL;
    LONG lResult = RegCreateKeyEx(
        HKEY_CLASSES_ROOT,
        keyName,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        NULL,
        &hKey,
        NULL
    );
    
    if (lResult != ERROR_SUCCESS) {
        return false;
    }
    
    // Set the filter name as the default value
    lResult = RegSetValueEx(
        hKey,
        NULL,
        0,
        REG_SZ,
        (BYTE*)wszName,
        (DWORD)((wcslen(wszName) + 1) * sizeof(WCHAR))
    );
    
    RegCloseKey(hKey);
    
    if (lResult != ERROR_SUCCESS) {
        return false;
    }
    
    // CLSID\{clsid}\InprocServer32
    StringCchPrintf(keyName, MAX_PATH, L"CLSID\\%s\\InprocServer32", wszClsid);
    
    lResult = RegCreateKeyEx(
        HKEY_CLASSES_ROOT,
        keyName,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        NULL,
        &hKey,
        NULL
    );
    
    if (lResult != ERROR_SUCCESS) {
        return false;
    }
    
    // Get the path to our DLL
    WCHAR wszModule[MAX_PATH];
    GetModuleFileName(GetModuleHandle(NULL), wszModule, MAX_PATH);
    
    // Set the DLL path as the default value
    lResult = RegSetValueEx(
        hKey,
        NULL,
        0,
        REG_SZ,
        (BYTE*)wszModule,
        (DWORD)((wcslen(wszModule) + 1) * sizeof(WCHAR))
    );
    
    // Set the threading model
    const WCHAR* wszThreadingModel = L"Both";
    lResult = RegSetValueEx(
        hKey,
        L"ThreadingModel",
        0,
        REG_SZ,
        (BYTE*)wszThreadingModel,
        (DWORD)((wcslen(wszThreadingModel) + 1) * sizeof(WCHAR))
    );
    
    RegCloseKey(hKey);
    
    if (lResult != ERROR_SUCCESS) {
        return false;
    }
    
    // Register as a Video Capture Source
    // CLSID\{CLSID_VideoInputDeviceCategory}\{clsid}
    WCHAR wszCategoryClsid[MAX_PATH];
    StringFromGUID2(CLSID_VideoInputDeviceCategory, wszCategoryClsid, MAX_PATH);
    
    StringCchPrintf(keyName, MAX_PATH, L"CLSID\\%s\\%s", wszCategoryClsid, wszClsid);
    
    lResult = RegCreateKeyEx(
        HKEY_CLASSES_ROOT,
        keyName,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_WRITE,
        NULL,
        &hKey,
        NULL
    );
    
    if (lResult != ERROR_SUCCESS) {
        return false;
    }
    
    // Set the filter name as the default value
    lResult = RegSetValueEx(
        hKey,
        NULL,
        0,
        REG_SZ,
        (BYTE*)wszName,
        (DWORD)((wcslen(wszName) + 1) * sizeof(WCHAR))
    );
    
    RegCloseKey(hKey);
    
    return (lResult == ERROR_SUCCESS);
}

bool DirectShowRegistration::RemoveFilterFromRegistry(const GUID& clsid)
{
    // Convert the CLSID to a string
    WCHAR wszClsid[MAX_PATH];
    StringFromGUID2(clsid, wszClsid, MAX_PATH);
    
    // Delete the CLSID key
    WCHAR keyName[MAX_PATH];
    StringCchPrintf(keyName, MAX_PATH, L"CLSID\\%s", wszClsid);
    
    LONG lResult = RegDeleteTree(HKEY_CLASSES_ROOT, keyName);
    
    // Delete the entry from the Video Capture Source category
    WCHAR wszCategoryClsid[MAX_PATH];
    StringFromGUID2(CLSID_VideoInputDeviceCategory, wszCategoryClsid, MAX_PATH);
    
    StringCchPrintf(keyName, MAX_PATH, L"CLSID\\%s\\%s", wszCategoryClsid, wszClsid);
    
    RegDeleteTree(HKEY_CLASSES_ROOT, keyName);
    
    return (lResult == ERROR_SUCCESS || lResult == ERROR_FILE_NOT_FOUND);
}

} // namespace VirtualCamera
} // namespace UndownUnlock 