# UndownUnlock Accessibility Framework

This implementation of UndownUnlock uses the Windows UI Automation Framework to provide enhanced window management capabilities without requiring DLL injection or API hooking.

## Why UI Automation Framework?

The Windows UI Automation Framework is a legitimate accessibility technology designed by Microsoft to help users with disabilities interact with applications. Using this framework offers several advantages:

1. **No DLL Injection Required**: Instead of invasive DLL injection that could be detected, this implementation works with established accessibility APIs.
2. **Legitimate Use Case**: The implementation leverages accessibility features that are designed for assistive technology.
3. **Better Stealth**: Using supported Microsoft interfaces results in less detection surface compared to API hooking.
4. **More Reliable**: Works across Windows versions without needing to adapt injection techniques.

## Features

- **Window Focus Management**: Switch between application windows seamlessly
- **Screenshot Capability**: Take screenshots of your screen
- **Window Restoration**: Restore window focus when a target application attempts to grab focus
- **Window Cycling**: Cycle through open windows using keyboard shortcuts
- **Minimize/Restore**: Minimize all windows except the target window, and restore them later

## Installation

1. Ensure you have Python 3.6 or higher installed
2. Run the setup script:

```
python setup.py
```

This will:
- Set up a virtual environment
- Install all required dependencies
- Create a launcher script

## Usage

### Starting the Application

On Windows, double-click the `launch.bat` file or run it from the command line.

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+Tab | Cycle to next window |
| Ctrl+Shift+Tab | Cycle to previous window |
| Ctrl+L | Focus LockDown Browser (if running) |
| Ctrl+O | Focus another window |
| Ctrl+Shift+S | Take a screenshot |
| Ctrl+M | Minimize all windows except LockDown Browser |
| Ctrl+R | Restore all previously minimized windows |

### Exiting the Application

Press `Ctrl+C` in the terminal window to exit the application.

## How It Works

The implementation consists of two main components:

1. **AccessibilityManager**: Interfaces with the Windows UI Automation Framework to control window focus and track window state changes.

2. **AccessibilityController**: Provides keyboard shortcuts and handles user interactions.

The system monitors focus changes through event handlers and can detect when a target application attempts to steal focus. When detected, it can restore focus to your preferred window.

## Customization

You can customize the target application by modifying the `target_app_name` variable in the `AccessibilityController` class.

## Troubleshooting

### Common Issues

1. **Keyboard shortcuts not working**: Ensure no other application is capturing the same keyboard shortcuts.

2. **Application not starting**: Verify that your Python version is 3.6 or higher and all dependencies are installed correctly.

3. **Focus management issues**: Some applications with high privilege levels might still be able to force focus. In those cases, try using the keyboard shortcuts to restore focus manually.

### Logs

The application generates log files that can help diagnose issues:
- `accessibility_manager.log` - Logs from the accessibility framework
- `accessibility_controller.log` - Logs from the controller

## Legal and Ethical Considerations

This tool is designed for educational purposes to demonstrate accessibility framework capabilities. Always ensure you have permission to use assistive technology in your environment.

## Technical Details

This implementation uses:
- Python's `comtypes` library to interface with COM objects
- Windows UI Automation interfaces
- Keyboard event monitoring for hotkeys
- PIL for screenshots

## Advantages Over DLL Injection

- No modifications to other processes' memory
- No hooking of system APIs
- Uses documented and supported accessibility interfaces
- More compatible across different Windows versions
- Less likely to be flagged by security software 