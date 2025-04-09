/**
 * UndownUnlock Virtual Camera Driver Structures
 * 
 * This file contains structure definitions and constants used by the kernel-mode driver
 * for implementing a virtual camera device with AVStream integration.
 */

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <ksmedia.h>
#include <ks.h>
#include <usbdi.h>

// Global driver object
PDRIVER_OBJECT g_DriverObject = NULL;

// Forward declaration for KS dispatch function
DRIVER_DISPATCH KsDriverDispatch;

// Video pixel formats
typedef enum _PIXEL_FORMAT {
    PIXEL_FORMAT_RGB24 = 0,   // RGB24 format
    PIXEL_FORMAT_RGB32,       // RGB32 format
    PIXEL_FORMAT_YUY2,        // YUY2 format (4:2:2)
    PIXEL_FORMAT_MJPG,        // MJPEG compressed format
    PIXEL_FORMAT_NV12,        // NV12 format (4:2:0)
    PIXEL_FORMAT_I420,        // I420 format (4:2:0)
    PIXEL_FORMAT_UYVY,        // UYVY format
    PIXEL_FORMAT_MAX          // Max enum value
} PIXEL_FORMAT;

// Video format descriptor
typedef struct _VIDEO_FORMAT_DESCRIPTOR {
    ULONG width;               // Frame width
    ULONG height;              // Frame height
    PIXEL_FORMAT pixelFormat;  // Pixel format
    ULONG frameRateNumerator;  // Frame rate numerator
    ULONG frameRateDenominator; // Frame rate denominator
} VIDEO_FORMAT_DESCRIPTOR, *PVIDEO_FORMAT_DESCRIPTOR;

// Current video format
typedef struct _VIDEO_FORMAT {
    ULONG width;               // Frame width
    ULONG height;              // Frame height
    PIXEL_FORMAT pixelFormat;  // Pixel format
    ULONG frameRateNumerator;  // Frame rate numerator
    ULONG frameRateDenominator; // Frame rate denominator
} VIDEO_FORMAT, *PVIDEO_FORMAT;

// Device parameters structure for communication with the driver
typedef struct _DEVICE_PARAMS {
    GUID deviceGuid;           // Device GUID
    WCHAR deviceName[256];     // Device name
    WCHAR devicePath[256];     // Device path
    ULONG flags;               // Flags
} DEVICE_PARAMS, *PDEVICE_PARAMS;

// Camera control parameters
typedef struct _CAMERA_CONTROL {
    LONG brightness;           // Brightness control (-100 to 100)
    LONG contrast;             // Contrast control (-100 to 100)
    LONG saturation;           // Saturation control (-100 to 100)
    LONG sharpness;            // Sharpness control (-100 to 100)
    LONG whiteBalance;         // White balance control (in Kelvin)
    LONG focus;                // Focus control (0-100)
    BOOLEAN autoFocus;         // Auto focus enabled
    BOOLEAN autoWhiteBalance;  // Auto white balance enabled
    BOOLEAN autoExposure;      // Auto exposure enabled
} CAMERA_CONTROL, *PCAMERA_CONTROL;

// Security parameters for device signature spoofing
typedef struct _SECURITY_PARAMS {
    ULONG flags;                // Security flags
    USHORT vendorId;            // Spoofed USB vendor ID
    USHORT productId;           // Spoofed USB product ID
    WCHAR serialNumber[64];     // Spoofed serial number
    WCHAR manufacturer[128];    // Spoofed manufacturer
    WCHAR product[128];         // Spoofed product name
} SECURITY_PARAMS, *PSECURITY_PARAMS;

// AVStream specific structs
typedef struct _STREAM_POINTER_CONTEXT {
    PVOID FrameBuffer;          // Pointer to frame buffer
    SIZE_T FrameSize;           // Size of frame
} STREAM_POINTER_CONTEXT, *PSTREAM_POINTER_CONTEXT;

// Pin specific context
typedef struct _PIN_CONTEXT {
    struct _DEVICE_CONTEXT* DeviceContext; // Back-pointer to device context
    BOOLEAN IsActive;           // Whether pin is active
    KSPIN_LOCK PinLock;         // Synchronization lock
    VIDEO_FORMAT Format;        // Current format
} PIN_CONTEXT, *PPIN_CONTEXT;

// Main device context structure
typedef struct _DEVICE_CONTEXT {
    // WDF Handles
    WDFDEVICE Device;          // WDF device handle
    WDFQUEUE Queue;            // Default I/O queue
    
    // Frame buffers (double buffering)
    PVOID FrameBuffers[2];     // Two frame buffers for double buffering
    SIZE_T FrameSizes[2];      // Sizes of the frame buffers
    ULONG ActiveBufferIndex;   // Index of the active buffer (0 or 1)
    KSPIN_LOCK FrameLock;      // Spinlock for frame buffer access
    
    // Video format
    VIDEO_FORMAT CurrentFormat; // Current video format
    PVIDEO_FORMAT_DESCRIPTOR FormatDescriptors; // Supported formats
    ULONG FormatCount;         // Number of supported formats
    
    // Device parameters
    DEVICE_PARAMS Params;      // Device parameters
    
    // Camera control
    CAMERA_CONTROL Controls;   // Camera control parameters
    
    // Security parameters
    SECURITY_PARAMS Security;  // Security parameters
    ULONG SecurityFlags;       // Security flags
    
    // USB device simulation
    USB_DEVICE_DESCRIPTOR UsbDeviceDescriptor; // Fake USB device descriptor
    
    // KS/AVStream integration
    PKSDEVICE KsDevice;        // KS device object
    KMUTEX AvStreamMutex;      // Mutex for AVStream operations
    
    // MDL for direct memory mapping
    PMDL FrameMdl;             // MDL for frame data
    
    // Status flags
    BOOLEAN IsActive;          // Whether device is active
    BOOLEAN EnableCameraEffects; // Whether to enable camera effects
    BOOLEAN IsStreaming;       // Whether streaming is active
    
    // Statistics
    ULONGLONG FrameCount;      // Number of frames processed
    ULONGLONG BytesProcessed;  // Number of bytes processed
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

// Get device context from WDF object
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, WdfObjectGet_DEVICE_CONTEXT)

// Camera property flags
#define PROPERTY_FLAG_AUTO     0x00000001
#define PROPERTY_FLAG_MANUAL   0x00000002
#define PROPERTY_FLAG_ABSOLUTE 0x00000004
#define PROPERTY_FLAG_RELATIVE 0x00000008

// Video format calculation macros
#define BYTES_PER_PIXEL_RGB24  3
#define BYTES_PER_PIXEL_RGB32  4
#define BYTES_PER_PIXEL_YUY2   2
#define BYTES_PER_PIXEL_NV12   1.5
#define BYTES_PER_PIXEL_I420   1.5
#define BYTES_PER_LINE_RGB24(width) ((width) * BYTES_PER_PIXEL_RGB24)
#define BYTES_PER_LINE_RGB32(width) ((width) * BYTES_PER_PIXEL_RGB32)
#define BYTES_PER_LINE_YUY2(width)  ((width) * BYTES_PER_PIXEL_YUY2)

// IOCTL checking helpers
#define IS_KS_IOCTL(code)      (((code) >= IOCTL_KS_PROPERTY) && ((code) <= IOCTL_KS_WRITE_STREAM))
#define IS_KS_PROPERTY_IOCTL(code) ((code) == IOCTL_KS_PROPERTY) 