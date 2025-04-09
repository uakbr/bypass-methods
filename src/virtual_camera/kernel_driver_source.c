/**
 * UndownUnlock Virtual Camera Driver
 * 
 * This is a kernel-mode driver that implements a virtual camera device.
 * It creates a device interface that user-mode applications can communicate with
 * to send frame data that will be presented to applications as if it came from
 * a physical camera.
 * 
 * NOTE: This is a simplified example for educational purposes only.
 * A real implementation would require significantly more code to handle
 * all the complexities of a real camera driver.
 */

#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>
#include <windef.h>
#include <guiddef.h>

// Device interface GUID for client applications to find our device
// {5C2CD55D-7DB8-4F65-9AF2-8FE8A7C30F29}
DEFINE_GUID(GUID_DEVINTERFACE_UndownUnlockVCam,
    0x5c2cd55d, 0x7db8, 0x4f65, 0x9a, 0xf2, 0x8f, 0xe8, 0xa7, 0xc3, 0x0f, 0x29);

// IOCTL codes for device communication
#define IOCTL_SET_FRAME          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_SET_DEVICE_PARAMS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_GET_STATUS         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_READ_ACCESS)

// Maximum frame size (4K resolution, RGB24)
#define MAX_FRAME_SIZE (3840 * 2160 * 3)

// Device parameters structure
typedef struct _DEVICE_PARAMS {
    GUID deviceGuid;
    WCHAR deviceName[256];
    WCHAR devicePath[256];
    ULONG flags;
} DEVICE_PARAMS, *PDEVICE_PARAMS;

// Device context structure
typedef struct _DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFQUEUE Queue;
    
    // Frame buffer
    PVOID FrameBuffer;
    SIZE_T FrameSize;
    KSPIN_LOCK FrameLock;
    
    // Device parameters
    DEVICE_PARAMS Params;
    
    // Status flags
    BOOLEAN IsActive;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

// Forward declarations
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD UndownUnlockVCamEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL UndownUnlockVCamEvtIoDeviceControl;
EVT_WDF_OBJECT_CONTEXT_CLEANUP UndownUnlockVCamEvtCleanup;

// WDF driver initialization
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;
    
    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "UndownUnlockVCam: Driver Entry\n"));
    
    // Initialize the WDF driver configuration structure
    WDF_DRIVER_CONFIG_INIT(&config, UndownUnlockVCamEvtDeviceAdd);
    
    // Create the WDF driver object
    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
                 "UndownUnlockVCam: WdfDriverCreate failed with status 0x%x\n", status));
        return status;
    }
    
    return STATUS_SUCCESS;
}

// WDF device add event handler
NTSTATUS
UndownUnlockVCamEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDFDEVICE device;
    WDFQUEUE queue;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    PDEVICE_CONTEXT deviceContext;
    
    UNREFERENCED_PARAMETER(Driver);
    
    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "UndownUnlockVCam: Device Add\n"));
    
    // Allow user-mode applications to access the device
    status = WdfDeviceInitAssignSDDLString(
        DeviceInit,
        &SDDL_DEVOBJ_SYS_ALL_ADM_ALL
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
                 "UndownUnlockVCam: WdfDeviceInitAssignSDDLString failed with status 0x%x\n", status));
        return status;
    }
    
    // Create a device context to store our frame buffer and state
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
    deviceAttributes.EvtCleanupCallback = UndownUnlockVCamEvtCleanup;
    
    // Create the device
    status = WdfDeviceCreate(
        &DeviceInit,
        &deviceAttributes,
        &device
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
                 "UndownUnlockVCam: WdfDeviceCreate failed with status 0x%x\n", status));
        return status;
    }
    
    // Create a device interface so applications can find our device
    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_UndownUnlockVCam,
        NULL
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
                 "UndownUnlockVCam: WdfDeviceCreateDeviceInterface failed with status 0x%x\n", status));
        return status;
    }
    
    // Configure the default queue to handle IO control requests
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &queueConfig,
        WdfIoQueueDispatchSequential
    );
    
    queueConfig.EvtIoDeviceControl = UndownUnlockVCamEvtIoDeviceControl;
    
    // Create the IO queue
    status = WdfIoQueueCreate(
        device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    );
    
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
                 "UndownUnlockVCam: WdfIoQueueCreate failed with status 0x%x\n", status));
        return status;
    }
    
    // Initialize the device context
    deviceContext = WdfObjectGet_DEVICE_CONTEXT(device);
    deviceContext->Device = device;
    deviceContext->Queue = queue;
    deviceContext->FrameBuffer = NULL;
    deviceContext->FrameSize = 0;
    deviceContext->IsActive = FALSE;
    
    // Initialize the frame lock
    KeInitializeSpinLock(&deviceContext->FrameLock);
    
    // Allocate memory for the frame buffer
    deviceContext->FrameBuffer = ExAllocatePoolWithTag(
        NonPagedPool,
        MAX_FRAME_SIZE,
        'maCU'  // UndownUnlock Cam tag
    );
    
    if (deviceContext->FrameBuffer == NULL) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
                 "UndownUnlockVCam: Failed to allocate frame buffer\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    // Initialize the frame buffer to black
    RtlZeroMemory(deviceContext->FrameBuffer, MAX_FRAME_SIZE);
    
    // Initialize default device parameters
    RtlZeroMemory(&deviceContext->Params, sizeof(DEVICE_PARAMS));
    
    deviceContext->Params.deviceGuid = GUID_DEVINTERFACE_UndownUnlockVCam;
    RtlCopyMemory(
        deviceContext->Params.deviceName,
        L"UndownUnlock Virtual Camera",
        sizeof(L"UndownUnlock Virtual Camera")
    );
    
    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, 
             "UndownUnlockVCam: Device initialized successfully\n"));
    
    return STATUS_SUCCESS;
}

// Device cleanup handler
VOID
UndownUnlockVCamEvtCleanup(
    _In_ WDFOBJECT Object
)
{
    PDEVICE_CONTEXT deviceContext;
    
    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "UndownUnlockVCam: Device Cleanup\n"));
    
    deviceContext = WdfObjectGet_DEVICE_CONTEXT(Object);
    
    // Free the frame buffer
    if (deviceContext->FrameBuffer != NULL) {
        ExFreePoolWithTag(deviceContext->FrameBuffer, 'maCU');
        deviceContext->FrameBuffer = NULL;
    }
}

// IO control request handler
VOID
UndownUnlockVCamEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    PVOID inputBuffer = NULL;
    PVOID outputBuffer = NULL;
    size_t bytesTransferred = 0;
    
    UNREFERENCED_PARAMETER(OutputBufferLength);
    
    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, 
             "UndownUnlockVCam: IO Control 0x%x\n", IoControlCode));
    
    device = WdfIoQueueGetDevice(Queue);
    deviceContext = WdfObjectGet_DEVICE_CONTEXT(device);
    
    switch (IoControlCode) {
    case IOCTL_SET_FRAME:
        // Set a new frame in the buffer
        if (InputBufferLength > MAX_FRAME_SIZE) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, 
                     "UndownUnlockVCam: Frame too large (%zd bytes), max is %d\n", 
                     InputBufferLength, MAX_FRAME_SIZE));
            status = STATUS_BUFFER_OVERFLOW;
            break;
        }
        
        // Get the input buffer
        status = WdfRequestRetrieveInputBuffer(
            Request,
            InputBufferLength,
            &inputBuffer,
            NULL
        );
        
        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
                     "UndownUnlockVCam: WdfRequestRetrieveInputBuffer failed with status 0x%x\n", status));
            break;
        }
        
        // Copy the frame data to our buffer
        {
            KIRQL oldIrql;
            
            KeAcquireSpinLock(&deviceContext->FrameLock, &oldIrql);
            
            RtlCopyMemory(
                deviceContext->FrameBuffer,
                inputBuffer,
                InputBufferLength
            );
            
            deviceContext->FrameSize = InputBufferLength;
            deviceContext->IsActive = TRUE;
            
            KeReleaseSpinLock(&deviceContext->FrameLock, oldIrql);
        }
        
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, 
                 "UndownUnlockVCam: Frame updated (%zd bytes)\n", InputBufferLength));
        
        bytesTransferred = InputBufferLength;
        break;
        
    case IOCTL_SET_DEVICE_PARAMS:
        // Set device parameters
        if (InputBufferLength < sizeof(DEVICE_PARAMS)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, 
                     "UndownUnlockVCam: Invalid device params buffer size\n"));
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        
        // Get the input buffer
        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(DEVICE_PARAMS),
            &inputBuffer,
            NULL
        );
        
        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
                     "UndownUnlockVCam: WdfRequestRetrieveInputBuffer failed with status 0x%x\n", status));
            break;
        }
        
        // Copy the device parameters
        RtlCopyMemory(
            &deviceContext->Params,
            inputBuffer,
            sizeof(DEVICE_PARAMS)
        );
        
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, 
                 "UndownUnlockVCam: Device parameters updated\n"));
        
        bytesTransferred = sizeof(DEVICE_PARAMS);
        break;
        
    case IOCTL_GET_STATUS:
        // Get current status
        if (OutputBufferLength < sizeof(BOOLEAN)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, 
                     "UndownUnlockVCam: Invalid status buffer size\n"));
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }
        
        // Get the output buffer
        status = WdfRequestRetrieveOutputBuffer(
            Request,
            sizeof(BOOLEAN),
            &outputBuffer,
            NULL
        );
        
        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, 
                     "UndownUnlockVCam: WdfRequestRetrieveOutputBuffer failed with status 0x%x\n", status));
            break;
        }
        
        // Copy the status
        *((PBOOLEAN)outputBuffer) = deviceContext->IsActive;
        
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, 
                 "UndownUnlockVCam: Status requested, IsActive=%d\n", deviceContext->IsActive));
        
        bytesTransferred = sizeof(BOOLEAN);
        break;
        
    default:
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, 
                 "UndownUnlockVCam: Unknown IOCTL 0x%x\n", IoControlCode));
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }
    
    // Complete the request
    WdfRequestCompleteWithInformation(Request, status, bytesTransferred);
} 