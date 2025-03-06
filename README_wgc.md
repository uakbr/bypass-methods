# Windows Graphics Capture Implementation

## Overview

This document details the Windows Graphics Capture implementation in the `windows_graphics_capture.py` module. This advanced capture technique directly utilizes the Windows.Graphics.Capture API introduced in Windows 10 version 1809 (build 17763) to capture screenshots, even from windows that have enabled SetWindowDisplayAffinity protection.

## Key Features

- **Bypasses SetWindowDisplayAffinity**: Can capture protected windows that block standard capture methods
- **Uses Modern Windows APIs**: Leverages official Windows 10+ screen capture infrastructure
- **High-Quality Captures**: Captures at full resolution with hardware acceleration
- **Legitimate Access Method**: Uses the same API that Windows uses for its own screen recorder
- **Direct Integration**: Interfaces with Windows Runtime (WinRT) APIs via Python.NET

## Technical Implementation

The implementation uses a sophisticated approach to access WinRT APIs from Python:

1. **Python.NET Interop Layer**: Uses the `pythonnet` (clr) package to access .NET and WinRT APIs
2. **C# Intermediary Code**: Dynamically compiles a C# interop library to bridge Python and WinRT
3. **COM/WinRT Activation**: Directly activates the GraphicsCaptureItem via COM interfaces
4. **Direct3D Integration**: Works with Direct3D surfaces to access captured frame data
5. **Bitmap Conversion**: Converts hardware textures to accessible bitmap data

## How It Bypasses SetWindowDisplayAffinity

The Windows Graphics Capture API was specifically designed by Microsoft to support legitimate screen capture scenarios, including capturing the output of protected applications. It operates at a privileged level in the Windows graphics stack:

1. **Privileged API Access**: Windows Graphics Capture runs with special permissions granted by the OS
2. **System Compositor Integration**: Interfaces directly with the Desktop Window Manager (DWM)
3. **Hardware Composition**: Uses GPU-accelerated composition that occurs after display affinity checks
4. **WinRT Privilege Level**: Windows Runtime components often have elevated privileges for legitimate system operations

Unlike traditional GDI or DirectX capture methods that operate at the application level, the Windows Graphics Capture API is integrated directly into the Windows composition engine at a level that is after the SetWindowDisplayAffinity protections are applied.

## Prerequisites

- Windows 10 version 1809 (build 17763) or later
- Python 3.6 or later
- Python.NET package (`pythonnet`)
- .NET Framework 4.7.2 or later installed on the system
- A C# compiler (csc.exe) accessible in the PATH or in standard installation locations

## Dynamic C# Interop Assembly

The implementation dynamically creates a C# interop assembly (`WinRTCaptureInterop.dll`) that provides the necessary bridge between Python and the Windows Graphics Capture API. This DLL implements:

1. **COM Interface Definitions**: Provides P/Invoke declarations for the necessary COM interfaces
2. **GraphicsCaptureItem Creation**: Creates the capture item for a specific window or monitor
3. **Frame Pool Management**: Manages the capture frame pool and event handling
4. **Direct3D Surface Access**: Provides access to the captured frame's surface data
5. **Bitmap Conversion**: Converts the Direct3D surface data to a Bitmap for saving

## Usage

### Basic Usage

```python
from windows_graphics_capture import WindowsGraphicsCapture

# Create the capture object
capture = WindowsGraphicsCapture("screenshots")

# Check if the API is supported on this system
if not capture.is_supported:
    print("Windows Graphics Capture API is not supported on this system")
    
# Capture a specific window by name
screenshot_path = capture.capture_window(window_name="LockDown Browser")

# Or capture a specific window using its handle
import win32gui
hwnd = win32gui.FindWindow(None, "LockDown Browser")
screenshot_path = capture.capture_window(hwnd=hwnd)

# Or capture the current foreground window
screenshot_path = capture.capture_window()
```

### Running the Demo

1. Run the setup script:
   ```
   python setup.py
   ```

2. Launch the Windows Graphics Capture test:
   ```
   launch_wgc_capture.bat
   ```

3. The test will:
   - Check if Windows Graphics Capture API is supported on your system
   - List available windows if no window name is provided
   - Attempt to capture the specified window or the foreground window
   - Show the affinity status of the target window
   - Save and (optionally) display the captured screenshot

## Integration with Advanced Capture

The Windows Graphics Capture implementation is integrated into the Advanced Capture system as the highest-priority capture method. The ScreenCaptureProxy class in `advanced_capture.py` will:

1. First attempt to use Windows Graphics Capture directly
2. Then fall back to other advanced methods if that fails
3. Finally try the enhanced methods as a last resort

## Technical Details

### Windows Graphics Capture Architecture

The Windows Graphics Capture API has a layered architecture:

1. **GraphicsCaptureItem**: Represents a window or monitor to be captured
2. **Direct3D11CaptureFramePool**: Manages a pool of frames for capturing
3. **GraphicsCaptureSession**: Controls the capture session lifecycle
4. **Direct3D11CaptureFrame**: Represents a single captured frame
5. **IDirect3DSurface**: Provides access to the captured pixel data

### C# Interop Implementation

The generated C# code defines necessary COM interfaces:

```csharp
[ComImport]
[Guid("3628E81B-3CAC-4C60-B7F4-23CE0E0C3356")]
[InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IGraphicsCaptureItemInterop
{
    IntPtr CreateForWindow([In] IntPtr window, [In] ref Guid riid);
    IntPtr CreateForMonitor([In] IntPtr monitor, [In] ref Guid riid);
}
```

And creates the capture session:

```csharp
// Create the GraphicsCaptureItem for the window
GraphicsCaptureItem item = CreateCaptureItemForWindow(hwnd);

// Create a frame pool
Direct3D11CaptureFramePool framePool = Direct3D11CaptureFramePool.Create(
    _d3dDevice,
    DirectXPixelFormat.B8G8R8A8UIntNormalized,
    2,  // Frame count
    size);

// Create a capture session
GraphicsCaptureSession session = framePool.CreateCaptureSession(item);

// Start capture
session.StartCapture();
```

## Troubleshooting

If you encounter issues with Windows Graphics Capture:

1. **Check Windows Version**: Ensure you're running Windows 10 version 1809 (build 17763) or later
2. **Verify .NET Installation**: Make sure .NET Framework 4.7.2 or later is installed
3. **Check C# Compiler**: Ensure csc.exe is available on your system
4. **Review Logs**: Check the `windows_graphics_capture.log` file for detailed error information
5. **Try Administrator Mode**: Some capture operations may require elevated privileges

## Limitations

- Only works on Windows 10 version 1809 or later
- Not all graphics drivers fully support the required DirectX features
- Requires administrator rights in some scenarios
- Some applications with extreme protection might still block this method

## Legal and Ethical Considerations

This implementation uses documented and legitimate Windows APIs for the purpose of accessibility and legal screen capture. It:

1. Does not circumvent security protections through undocumented means
2. Uses the same APIs that Windows uses for its built-in screen recorder
3. Operates within the security boundaries established by Microsoft
4. Is designed for legitimate use cases like accessibility, education, and documentation

As with all screen capture technology, ensure you have appropriate authorization before capturing content from applications. 