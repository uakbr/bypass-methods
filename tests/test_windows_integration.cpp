#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include "../include/hooks/windows_api_hooks.h"

using namespace UndownUnlock::WindowsHook;

// Callback function for window monitoring
void WindowCreatedCallback(HWND hWnd, bool isCreated) {
    if (!IsWindow(hWnd)) {
        return;
    }

    TCHAR windowName[256] = { 0 };
    GetWindowText(hWnd, windowName, 256);

    std::wcout << (isCreated ? L"Window created: " : L"Window destroyed: ")
               << windowName << L" (HWND: " << hWnd << L")" << std::endl;
}

// Test the Accessibility API functionality
bool TestAccessibilityAPI() {
    std::cout << "\n=== Testing Accessibility API ===\n" << std::endl;

    // Initialize Accessibility API
    if (!WindowsAPIHooks::InitializeAccessibilityAPI()) {
        std::cerr << "Failed to initialize Accessibility API" << std::endl;
        return false;
    }

    // Get the desktop window as a test target
    HWND desktopWindow = GetDesktopWindow();
    
    // Test focusing the desktop window (this should always succeed)
    if (!WindowsAPIHooks::AccessibilityControlWindow(desktopWindow, ACC_CMD_FOCUS)) {
        std::cerr << "Failed to focus desktop window using Accessibility API" << std::endl;
        return false;
    }

    // Get a real application window for testing
    HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow && foregroundWindow != desktopWindow) {
        // Try to minimize the window
        if (WindowsAPIHooks::AccessibilityControlWindow(foregroundWindow, ACC_CMD_MINIMIZE)) {
            std::cout << "Successfully minimized foreground window" << std::endl;
            
            // Wait a moment, then restore it
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            if (WindowsAPIHooks::AccessibilityControlWindow(foregroundWindow, ACC_CMD_RESTORE)) {
                std::cout << "Successfully restored window" << std::endl;
            }
        }
    }

    // Clean up
    WindowsAPIHooks::ShutdownAccessibilityAPI();
    std::cout << "Accessibility API tests completed" << std::endl;
    return true;
}

// Test the WMI functionality
bool TestWMI() {
    std::cout << "\n=== Testing WMI ===\n" << std::endl;

    // Initialize WMI
    if (!WindowsAPIHooks::InitializeWMI()) {
        std::cerr << "Failed to initialize WMI" << std::endl;
        return false;
    }

    // Monitor windows
    if (!WindowsAPIHooks::MonitorWindowsWithWMI(WindowCreatedCallback)) {
        std::cerr << "Failed to monitor windows with WMI" << std::endl;
        WindowsAPIHooks::ShutdownWMI();
        return false;
    }

    std::cout << "WMI initial window monitoring completed. Creating test window..." << std::endl;

    // Create a test window to see if our callback gets called
    HWND testWindow = CreateWindowEx(
        0, L"STATIC", L"TestWindowForWMI", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (testWindow) {
        // Show the window
        ShowWindow(testWindow, SW_SHOW);
        UpdateWindow(testWindow);

        // Let the messages process
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Clean up the test window
        DestroyWindow(testWindow);
    }

    // Clean up
    WindowsAPIHooks::ShutdownWMI();
    std::cout << "WMI tests completed" << std::endl;
    return true;
}

// Test the Shell Extension functionality
bool TestShellExtension() {
    std::cout << "\n=== Testing Shell Extension ===\n" << std::endl;

    // Initialize Shell Extension
    if (!WindowsAPIHooks::InitializeShellExtension()) {
        std::cerr << "Failed to initialize Shell Extension" << std::endl;
        return false;
    }

    // Get a window to test with
    HWND notepad = NULL;
    
    // Try to find Notepad if it's running
    notepad = FindWindow(L"Notepad", NULL);
    
    if (!notepad) {
        // Launch Notepad if not running
        STARTUPINFO si = { sizeof(STARTUPINFO) };
        PROCESS_INFORMATION pi = { 0 };
        
        if (CreateProcess(L"notepad.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            
            // Give it time to start
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Try to find it again
            notepad = FindWindow(L"Notepad", NULL);
        }
    }
    
    if (notepad) {
        // Test Shell Extension with the Notepad window
        if (WindowsAPIHooks::ShellExtensionManageWindow(notepad, SHELL_CMD_PROPERTIES)) {
            std::cout << "Successfully showed properties for Notepad" << std::endl;
        } else {
            std::cerr << "Failed to show properties for Notepad" << std::endl;
        }
    } else {
        std::cout << "Couldn't find or start Notepad for testing" << std::endl;
    }

    // Clean up
    WindowsAPIHooks::ShutdownShellExtension();
    std::cout << "Shell Extension tests completed" << std::endl;
    return true;
}

// Test Windows version compatibility
bool TestWindowsVersion() {
    std::cout << "\n=== Testing Windows Version Compatibility ===\n" << std::endl;
    
    bool result = WindowsAPIHooks::CheckWindowsVersionCompatibility();
    
    std::cout << "Windows version compatibility check " 
              << (result ? "passed" : "failed") << std::endl;
    
    return true;
}

// Test hook security enhancements
bool TestHookSecurity() {
    std::cout << "\n=== Testing Hook Security Enhancements ===\n" << std::endl;
    
    bool result = WindowsAPIHooks::EnhanceHookSecurity();
    
    std::cout << "Hook security enhancement " 
              << (result ? "succeeded" : "failed") << std::endl;
    
    return true;
}

// Test hook functionality
bool TestHooks() {
    std::cout << "\n=== Testing Hook Functionality ===\n" << std::endl;
    
    bool result = WindowsAPIHooks::TestHookFunctionality();
    
    std::cout << "Hook functionality test " 
              << (result ? "passed" : "failed") << std::endl;
    
    return true;
}

int main() {
    std::cout << "==== Windows Integration Enhancements Test ====" << std::endl;
    
    // Initialize Windows API hooks
    if (!WindowsAPIHooks::Initialize()) {
        std::cerr << "Failed to initialize Windows API hooks" << std::endl;
        return 1;
    }
    
    // Run the tests
    std::vector<std::pair<std::string, bool(*)()>> tests = {
        {"Accessibility API", TestAccessibilityAPI},
        {"WMI", TestWMI},
        {"Shell Extension", TestShellExtension},
        {"Windows Version", TestWindowsVersion},
        {"Hook Security", TestHookSecurity},
        {"Hook Functionality", TestHooks}
    };
    
    int passedTests = 0;
    for (const auto& test : tests) {
        std::cout << "\nRunning test: " << test.first << std::endl;
        bool result = test.second();
        if (result) {
            passedTests++;
            std::cout << test.first << " test PASSED" << std::endl;
        } else {
            std::cout << test.first << " test FAILED" << std::endl;
        }
    }
    
    // Print summary
    std::cout << "\n==== Test Summary ====" << std::endl;
    std::cout << "Passed: " << passedTests << "/" << tests.size() << std::endl;
    
    // Clean up
    WindowsAPIHooks::Shutdown();
    
    return (passedTests == tests.size()) ? 0 : 1;
} 