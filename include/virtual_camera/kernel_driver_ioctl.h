#pragma once

#include <Windows.h>
#include <winioctl.h>

namespace UndownUnlock {
namespace VirtualCamera {

// Device interface GUID for client applications to find our device
// {5C2CD55D-7DB8-4F65-9AF2-8FE8A7C30F29}
DEFINE_GUID(GUID_DEVINTERFACE_UndownUnlockVCam,
    0x5c2cd55d, 0x7db8, 0x4f65, 0x9a, 0xf2, 0x8f, 0xe8, 0xa7, 0xc3, 0x0f, 0x29);

// USB webcam GUID - used for device class registration
// {E5323777-F976-4f5b-9B55-B94699C46E44}
DEFINE_GUID(GUID_USB_VIDEO_DEVICE,
    0xe5323777, 0xf976, 0x4f5b, 0x9b, 0x55, 0xb9, 0x46, 0x99, 0xc4, 0x6e, 0x44);

// IOCTL codes for device communication
#define IOCTL_SET_FRAME          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SET_DEVICE_PARAMS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_GET_STATUS         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SPOOF_SIGNATURE    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SET_FORMAT         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_GET_FORMATS        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SET_FRAME_MDL      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_OUT_DIRECT, FILE_WRITE_ACCESS)

// Video pixel formats (must match kernel definitions)
enum class PixelFormat {
    RGB24 = 0,   // RGB24 format
    RGB32,       // RGB32 format
    YUY2,        // YUY2 format (4:2:2)
    MJPG,        // MJPEG compressed format
    NV12,        // NV12 format (4:2:0)
    I420,        // I420 format (4:2:0)
    UYVY,        // UYVY format
    MAX          // Max enum value
};

// Security flags
#define SECURITY_FLAG_SPOOF_VID_PID       0x00000001
#define SECURITY_FLAG_HIDE_FROM_ENUM      0x00000002
#define SECURITY_FLAG_BYPASS_INTEGRITY    0x00000004
#define SECURITY_FLAG_FAKE_SERIAL         0x00000008
#define SECURITY_FLAG_TRUSTED_DEVICE      0x00000010

// Define structures for IOCTL communication (matching kernel structures)

// Video format descriptor
struct VideoFormatDescriptor {
    ULONG width;                    // Frame width
    ULONG height;                   // Frame height
    PixelFormat pixelFormat;        // Pixel format
    ULONG frameRateNumerator;       // Frame rate numerator
    ULONG frameRateDenominator;     // Frame rate denominator
};

// Current video format
struct VideoFormat {
    ULONG width;                    // Frame width
    ULONG height;                   // Frame height
    PixelFormat pixelFormat;        // Pixel format
    ULONG frameRateNumerator;       // Frame rate numerator
    ULONG frameRateDenominator;     // Frame rate denominator
};

// Device parameters structure
struct DeviceParams {
    GUID deviceGuid;                // Device GUID
    WCHAR deviceName[256];          // Device name
    WCHAR devicePath[256];          // Device path
    ULONG flags;                    // Flags
};

// Camera control parameters
struct CameraControl {
    LONG brightness;                // Brightness control (-100 to 100)
    LONG contrast;                  // Contrast control (-100 to 100)
    LONG saturation;                // Saturation control (-100 to 100)
    LONG sharpness;                 // Sharpness control (-100 to 100)
    LONG whiteBalance;              // White balance control (in Kelvin)
    LONG focus;                     // Focus control (0-100)
    BOOLEAN autoFocus;              // Auto focus enabled
    BOOLEAN autoWhiteBalance;       // Auto white balance enabled
    BOOLEAN autoExposure;           // Auto exposure enabled
};

// Security parameters for device signature spoofing
struct SecurityParams {
    ULONG flags;                    // Security flags
    USHORT vendorId;                // Spoofed USB vendor ID
    USHORT productId;               // Spoofed USB product ID
    WCHAR serialNumber[64];         // Spoofed serial number
    WCHAR manufacturer[128];        // Spoofed manufacturer
    WCHAR product[128];             // Spoofed product name
};

// Get formats response structure
struct GetFormatsResponse {
    ULONG formatCount;              // Number of formats
    VideoFormatDescriptor formats[16]; // Format array (up to 16 formats)
};

// Device status structure
struct DeviceStatus {
    BOOLEAN isActive;               // Whether device is active
    BOOLEAN isStreaming;            // Whether streaming is active
    ULONGLONG frameCount;           // Number of frames processed
    ULONGLONG bytesProcessed;       // Number of bytes processed
    VideoFormat currentFormat;      // Current format
};

// Maximum frame size (4K resolution, RGB32)
#define MAX_FRAME_SIZE (3840 * 2160 * 4)

} // namespace VirtualCamera
} // namespace UndownUnlock 