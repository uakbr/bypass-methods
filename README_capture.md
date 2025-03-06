# Enhanced Screen Capture Module

## Overview

The Enhanced Screen Capture Module (`enhanced_capture.py`) is designed to work around the `SetWindowDisplayAffinity` Windows API function that is commonly used by secure applications (like LockDown Browser) to prevent screenshots. This module provides multiple screenshot methods that attempt to bypass these restrictions legitimately through the use of accessibility APIs and advanced capture techniques.

## The Problem: SetWindowDisplayAffinity

Applications that want to prevent screen capture can use the Windows API function `SetWindowDisplayAffinity` with the flag `WDA_EXCLUDEFROMCAPTURE` (value `0x00000011`). When this flag is set on a window:

- Standard screenshot methods (like PrintScreen or `ImageGrab.grab()`) will show a black region where the protected window is
- Most screen recording software will also display a black region
- This is a DRM protection technique used by applications handling sensitive content or exam software

## Solution: Multi-Method Approach

Our enhanced capture module implements several different methods to try and work around these restrictions:

1. **GDI (Graphics Device Interface) Capture**: The standard approach using BitBlt API
2. **UI Automation Accessibility**: Uses Windows UI Automation interfaces designed for accessibility tools
3. **Direct Memory Capture**: Uses PrintWindow instead of BitBlt for memory access
4. **Temporary Affinity Modification**: Attempts to temporarily change the window's display affinity
5. **Magnification API**: Uses the Windows Magnification API which is designed for accessibility software

When you call the `capture_screenshot()` method, the module will try each method in sequence until one succeeds. If all fail, it falls back to full-screen capture as a last resort.

## Why This Works

This approach uses legitimate accessibility interfaces rather than DLL injection or hooking techniques. Some key benefits:

- **Accessibility First**: Windows prioritizes accessibility tools over anti-screen capture mechanisms
- **Multiple Methods**: If one approach fails, others may still work, creating redundancy
- **Legitimacy**: Uses documented Windows APIs rather than low-level hacks
- **Magnification API**: Specifically designed for screen magnifiers and screen readers

## Usage

### Basic Usage

```python
from enhanced_capture import EnhancedScreenCapture

# Initialize with directory to save screenshots
capture = EnhancedScreenCapture("screenshots")

# Capture the entire screen
screenshot_path = capture.capture_full_screen()

# Capture a specific window by handle
import win32gui
hwnd = win32gui.FindWindow(None, "Window Title")
screenshot_path = capture.capture_screenshot(hwnd)

# Capture a specific window by name
from enhanced_capture import get_window_handle_by_name
hwnd = get_window_handle_by_name("Window Title")
screenshot_path = capture.capture_screenshot(hwnd)
```

### Running the Demo

To test the different screen capture methods and see which ones work on your system:

1. Run the setup script if you haven't already: `python setup.py`
2. Execute the demo batch file: `launch_capture_demo.bat`
3. Follow the on-screen instructions to test different capture methods

## Technical Details

### Screen Capture Methods

#### 1. GDI Capture
Uses standard Windows GDI functions to capture window content:
```python
hwnd_dc = win32gui.GetWindowDC(hwnd)
mfc_dc = win32ui.CreateDCFromHandle(hwnd_dc)
save_dc = mfc_dc.CreateCompatibleDC()
save_bitmap = win32ui.CreateBitmap()
save_dc.BitBlt((0, 0), (width, height), mfc_dc, (0, 0), win32con.SRCCOPY)
```

#### 2. Accessibility Capture
Uses UI Automation interfaces designed for accessibility tools:
```python
UIAutomation = client.GetModule(('{944DE083-8FB8-45CF-BCB7-C477ACB2F897}', 1, 0))
client_object = client.CreateObject('{FF48DBA4-60EF-4201-AA87-54103EEF594E}')
element = client_object.ElementFromHandle(hwnd)
```

#### 3. Direct Memory Capture
Uses `PrintWindow` function that's meant for printing window contents:
```python
win32gui.PrintWindow(hwnd, compatible_dc, 0)
```

#### 4. BitBlt with Temporary Affinity
Temporarily changes the window's display affinity to allow capture:
```python
current_affinity = wintypes.DWORD()
GetWindowDisplayAffinity(hwnd, ctypes.byref(current_affinity))
SetWindowDisplayAffinity(hwnd, WDA_NONE)
# Perform capture
SetWindowDisplayAffinity(hwnd, current_affinity.value)  # Restore original
```

#### 5. Magnification API
Uses Windows Magnification API designed for accessibility tools:
```python
mag_dll = ctypes.WinDLL("Magnification.dll")
mag_dll.MagInitialize()
# Set up magnification window and transform
```

## Integration with Accessibility Framework

The Enhanced Screen Capture module integrates with the UndownUnlock Accessibility Framework by replacing the standard screenshot functionality with our advanced techniques. The `AccessibilityController` now uses the `EnhancedScreenCapture` class for all screenshot operations, allowing it to capture content from protected windows.

## Troubleshooting

If you're having issues with screen capture:

1. **Check Window Affinity**: Use the demo to check if the window has the `WDA_EXCLUDEFROMCAPTURE` affinity set
2. **Try Different Methods**: Some methods work better on certain systems than others
3. **Run as Administrator**: Some methods require elevated privileges
4. **Check Logs**: Detailed logs are stored in `enhanced_capture.log`
5. **Window Focus**: Ensure the target window is visible and not minimized

## Legal Considerations

This module is designed for legitimate accessibility purposes only. It uses documented Windows APIs and should only be used on applications where you have permission to capture the screen content. It's particularly useful for:

- Accessibility tools for users with disabilities
- Educational software that needs to interact with protected content
- Documentation and support tools

Always ensure you have appropriate permissions before using this tool.

## Dependencies

- PIL/Pillow: For image processing
- pywin32: For Windows API access
- comtypes: For COM interface access
- numpy: For image data processing 