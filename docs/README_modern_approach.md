# UndownUnlock: Modern Approach

## A Non-Invasive Alternative Using Accessibility APIs

This repository contains a modern implementation of UndownUnlock that uses Windows Accessibility APIs instead of DLL injection. This approach offers several advantages:

- **No DLL Injection Required**: Uses legitimate Windows accessibility APIs
- **More Stable**: Less likely to cause crashes or compatibility issues
- **Less Detectable**: Uses standard Windows interfaces rather than memory manipulation
- **Easier to Maintain**: Cleaner code structure with better separation of concerns

## Components

The modern implementation consists of the following components:

1. **AccessibilityManager**: Interfaces with the Windows UI Automation Framework
2. **AccessibilityController**: Manages keyboard shortcuts and user interaction
3. **NamedPipeManager**: Provides secure inter-process communication
4. **RemoteClient**: Allows control from external processes or applications

## Features

- **Window Focus Management**: Control and cycle through windows
- **Screenshot Capability**: Take screenshots of your screen
- **Window Restoration**: Restore window focus when needed
- **Window Cycling**: Cycle through open windows using keyboard shortcuts
- **Minimize/Restore**: Minimize all windows except the target window, and restore them later
- **Secure IPC**: Communicate securely between processes using Named Pipes
- **Remote Control**: Control the framework from another process or application

## Installation

1. Ensure you have Python 3.6 or higher installed
2. Run the setup script:
   ```
   python setup.py
   ```
3. This will:
   - Set up a virtual environment
   - Install all required dependencies
   - Create launcher scripts for both the controller and client

## Usage

### Running the Application

1. Start the controller:
   ```
   launch_controller.bat
   ```
2. Start the remote client (optional):
   ```
   launch_client.bat
   ```

### Keyboard Shortcuts

When the controller is running, you can use these keyboard shortcuts:

- `Alt+Shift+C`: Cycle to the next window
- `Alt+Shift+X`: Cycle to the previous window
- `Alt+Shift+L`: Focus the LockDown Browser window
- `Alt+Shift+S`: Take a screenshot
- `Alt+Shift+M`: Minimize all windows except LockDown Browser
- `Alt+Shift+R`: Restore all minimized windows

### Remote Client Usage

The remote client can be used in interactive mode or command-line mode:

#### Interactive Mode
```
launch_client.bat
```

#### Command Line Mode
```
client.bat --command [command] [options]
```

Available commands:
- `list`: List all visible windows
- `cycle`: Cycle to next or previous window
- `focus`: Focus a specific window
- `minimize`: Minimize all windows except the specified one
- `restore`: Restore all minimized windows
- `screenshot`: Take a screenshot

## Technical Details

### Phase 1: Accessibility Framework

The first phase implements the core accessibility framework:

1. **AccessibilityManager (accessibility_manager.py)**:
   - Interfaces with Windows UI Automation
   - Manages window focus and state
   - Monitors focus changes
   - Provides window detection and enumeration

2. **AccessibilityController (accessibility_controller.py)**:
   - Manages keyboard shortcuts
   - Handles user interactions
   - Controls screenshot functionality
   - Implements window cycling logic

### Phase 2: Named Pipes for Secure IPC

The second phase adds secure inter-process communication:

1. **NamedPipeManager (named_pipe_manager.py)**:
   - Implements Windows Named Pipes for IPC
   - Provides encryption and authentication
   - Enables secure client-server communication
   - Manages multiple client connections

2. **RemoteClient (remote_client.py)**:
   - Connects to the controller via Named Pipes
   - Provides an interactive and command-line interface
   - Enables remote control of accessibility features

## Advantages Over DLL Injection

1. **Legitimacy**: Uses documented accessibility features for their intended purpose
2. **Stability**: Does not modify memory of other processes
3. **Compatibility**: Works across different Windows versions
4. **Maintainability**: Clear separation of concerns and modular design
5. **Security**: Less likely to be detected as malware or trigger anti-cheat systems

## Customization

You can customize the target application by modifying the `target_app_name` variable in the `accessibility_controller.py` file.

## Troubleshooting

For troubleshooting information, please see the detailed documentation in `README_accessibility.md`.

## Legal and Ethical Considerations

This tool is provided for educational purposes only. Always ensure you have permission to use assistive technology within your environment. The tool is designed to help those with accessibility needs, not to circumvent legitimate security measures. 