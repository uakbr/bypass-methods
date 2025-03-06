# UndownUnlock Accessibility Framework

## Introduction

UndownUnlock Accessibility Framework is an advanced implementation that leverages the Windows UI Automation Framework to provide enhanced window management capabilities. This implementation uses legitimate accessibility APIs rather than DLL injection, making it more stable, less intrusive, and less likely to be detected as malicious software.

## Phase 2: Named Pipes for Secure Inter-Process Communication

In Phase 2, we've enhanced our framework with secure inter-process communication using Windows Named Pipes. This allows multiple processes to communicate with the main accessibility controller, enabling remote control and integration with other applications.

## Phase 3: Enhanced Screen Capture

In Phase 3, we've implemented advanced screen capture techniques that can bypass `SetWindowDisplayAffinity` protection used by secure browsers like LockDown Browser. Our multi-layered approach uses legitimate accessibility APIs and other Windows features to capture window content even when standard methods fail.

## Why UI Automation Framework?

- **No DLL Injection**: Uses legitimate Windows accessibility APIs rather than injecting code
- **Legitimate Use Case**: Designed as an accessibility tool for users with disabilities
- **Better Stealth**: Less likely to be flagged by security software
- **Reliability**: Works across different Windows versions with consistent behavior

## Features

- **Window Focus Management**: Programmatically control window focus
- **Enhanced Screenshot Capability**: Take screenshots even of protected windows
- **Window Restoration**: Restore minimized windows
- **Window Cycling**: Cycle through open windows
- **Minimize/Restore Functions**: Minimize all windows except a target application
- **Secure IPC**: Communicate securely between processes using Named Pipes
- **Remote Control**: Control the framework from another process or application
- **DRM Bypass**: Capture content from windows with anti-screen capture protections

## Installation

1. Ensure you have Python 3.6 or higher installed
2. Run the setup script:
   ```
   python setup.py
   ```
3. This will:
   - Install all required dependencies
   - Set up a virtual environment
   - Create launch scripts for the controller and client

## Usage

### Running the Accessibility Controller

1. Start the controller by running:
   ```
   launch_controller.bat
   ```
2. The controller will start in the background and listen for keyboard shortcuts and Named Pipe connections.

### Using Keyboard Shortcuts

When the controller is running, you can use these keyboard shortcuts:

- `Alt+Shift+C`: Cycle to the next window
- `Alt+Shift+X`: Cycle to the previous window
- `Alt+Shift+L`: Focus the LockDown Browser window
- `Alt+Shift+S`: Take a screenshot
- `Alt+Shift+M`: Minimize all windows except LockDown Browser
- `Alt+Shift+R`: Restore all minimized windows

### Using the Remote Client

The remote client allows you to control the accessibility features from another process.

#### Interactive Mode

1. Start the remote client in interactive mode:
   ```
   launch_client.bat
   ```
2. Follow the on-screen menu to perform various actions.

#### Command Line Mode

You can also use the client in command-line mode for scripting:

```
client.bat --command [command] [options]
```

Available commands:
- `list`: List all visible windows
- `cycle`: Cycle to next or previous window (`--direction next|previous`)
- `focus`: Focus a specific window (`--window "Window Name"`)
- `minimize`: Minimize all windows except the specified one
- `restore`: Restore all minimized windows
- `screenshot`: Take a screenshot (can target specific window with `--window`)

Examples:
```
client.bat --command list
client.bat --command focus --window "LockDown Browser"
client.bat --command cycle --direction previous
client.bat --command screenshot --window "LockDown Browser"
```

For help:
```
client.bat --help
```

## How It Works

### Main Components

1. **AccessibilityManager**: Core component that interfaces with the Windows UI Automation Framework
2. **AccessibilityController**: Manages keyboard shortcuts and handles user interactions
3. **NamedPipeManager**: Provides secure inter-process communication using Windows Named Pipes
4. **RemoteClient**: Client application that communicates with the controller over Named Pipes
5. **EnhancedScreenCapture**: Advanced screen capture module that bypasses DRM protections

### Enhanced Screen Capture

Our multi-method approach to screen capture attempts several techniques to bypass `SetWindowDisplayAffinity` protection:

1. **GDI Capture**: Standard Windows GDI API approach
2. **UI Automation**: Uses accessibility interfaces
3. **Direct Memory Access**: Uses PrintWindow API
4. **Temporary Affinity Modification**: Attempts to temporarily change window settings
5. **Magnification API**: Uses the Windows Magnification API designed for accessibility tools

The system tries each method in sequence until one succeeds. For more details, see the [Enhanced Screen Capture Documentation](README_capture.md).

### Security Features

- **Encrypted Communication**: All messages between client and server are encrypted
- **Message Authentication**: Messages are signed to prevent tampering
- **Access Control**: Only processes on the same computer can connect to the Named Pipe

## Customization

You can customize the target application by modifying the `target_app_name` variable in the `accessibility_controller.py` file.

## Troubleshooting

### Common Issues

- **Controller won't start**: Ensure you have administrator privileges and the required dependencies are installed.
- **Keyboard shortcuts not working**: Make sure no other application is capturing the same shortcuts.
- **Client can't connect**: Verify the controller is running and check firewall settings.

### Logs

Log files are created in the application directory:
- `accessibility_manager.log`: Logs from the accessibility manager
- `accessibility_controller.log`: Logs from the controller
- `named_pipe_manager.log`: Logs from the Named Pipe communication
- `remote_client.log`: Logs from the remote client

These logs can help diagnose issues with the application.

## Legal and Ethical Considerations

This tool is provided for educational purposes only. Always ensure you have permission to use assistive technology within your environment. The tool is designed to help those with accessibility needs, not to circumvent legitimate security measures.

## Additional Tools

- **Enhanced Screen Capture Demo**: Test different screen capture methods and see which ones work on your system by running `launch_capture_demo.bat`

## Technical Details

- **Windows UI Automation**: Microsoft's accessibility framework for programmatically accessing UI elements
- **Named Pipes**: Windows IPC mechanism with built-in security features
- **SetWindowDisplayAffinity Bypass**: Techniques to work around anti-screenshot DRM protections
- **Magnification API**: Accessibility interface specifically designed for screen readers and magnifiers
- **Python Libraries**: 
  - `comtypes`: For COM interface access
  - `win32pipe` and `win32file`: For Named Pipes functionality
  - `cryptography`: For secure message encryption
  - `keyboard`: For global keyboard hook management
  - `win32ui` and `win32gui`: For advanced window management
  - `numpy`: For image processing of captured screenshots

## Advantages Over DLL Injection

- **No Memory Modifications**: Does not modify the memory of other processes
- **No API Hooking**: Does not hook Windows APIs
- **Legitimacy**: Uses documented accessibility features for their intended purpose
- **Compatibility**: Works across different Windows versions without modification
- **Maintainability**: Easier to understand and maintain
- **Security**: Less likely to be detected as malware 