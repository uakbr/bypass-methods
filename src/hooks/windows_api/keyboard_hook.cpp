#include "../../../include/hooks/keyboard_hook.h"
// #include "../../../include/hooks/windows_api_hooks.h" // Replaced by new_hook_system.h
#include "../../../include/new_hook_system.h" // Added for WindowsApiHookManager
#include <thread> // Added for std::thread

namespace UndownUnlock {
namespace WindowsHook {

// Initialize static variables
HHOOK KeyboardHook::s_keyboardHook = NULL;
std::thread KeyboardHook::s_messageLoopThread;

void KeyboardHook::Initialize() {
    // Install a low-level keyboard hook that will monitor all keyboard input
    s_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, nullptr, 0);
    
    if (s_keyboardHook == NULL) {
        std::cerr << "Failed to install keyboard hook. Error code: " << GetLastError() << std::endl;
    }
    else {
        std::cout << "Keyboard hook installed successfully." << std::endl;
        // Create a new detached thread for RunMessageLoop()
        s_messageLoopThread = std::thread(RunMessageLoop);
        s_messageLoopThread.detach();
    }
}

void KeyboardHook::Shutdown() {
    if (s_keyboardHook != NULL) {
        // Remove the keyboard hook
        if (UnhookWindowsHookEx(s_keyboardHook)) {
            std::cout << "Keyboard hook removed successfully." << std::endl;
        }
        else {
            std::cerr << "Failed to remove keyboard hook. Error code: " << GetLastError() << std::endl;
        }
        
        s_keyboardHook = NULL;
    }
}

void KeyboardHook::RunMessageLoop() {
    // Message loop to keep the hook alive and process messages
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK KeyboardHook::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // Check if we need to process this message
    if (nCode == HC_ACTION) {
        PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
        
        // Only process key down events
        if (wParam == WM_KEYDOWN) {
            switch (p->vkCode) {
                // VK_UP is the virtual key code for the Up arrow key
                case VK_UP:
                    WindowsApiHookManager::GetInstance().InstallWindowManagementHooks();
                    std::cout << "Up arrow key pressed, installing focus hooks." << std::endl;
                    break;
                    
                // VK_DOWN is the virtual key code for the Down arrow key
                case VK_DOWN:
                    WindowsApiHookManager::GetInstance().UninstallWindowManagementHooks();
                    
                    // Apply the original functions to restore normal behavior
                    if (WindowsApiHookManager::g_focusHWND != NULL) {
                        SetFocus(WindowsApiHookManager::g_focusHWND);
                    }
                    
                    if (WindowsApiHookManager::g_bringWindowToTopHWND != NULL) {
                        BringWindowToTop(WindowsApiHookManager::g_bringWindowToTopHWND);
                    }
                    
                    if (WindowsApiHookManager::g_setWindowPosParams.hwnd != NULL) {
                        SetWindowPos(
                            WindowsApiHookManager::g_setWindowPosParams.hwnd,
                            WindowsApiHookManager::g_setWindowPosParams.hwndInsertAfter,
                            WindowsApiHookManager::g_setWindowPosParams.x,
                            WindowsApiHookManager::g_setWindowPosParams.y,
                            WindowsApiHookManager::g_setWindowPosParams.cx,
                            WindowsApiHookManager::g_setWindowPosParams.cy,
                            WindowsApiHookManager::g_setWindowPosParams.flags
                        );
                    }
                    
                    std::cout << "Down arrow key pressed, uninstalling focus hooks." << std::endl;
                    break;
            }
        }
    }
    
    // Call the next hook in the chain
    return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
}

} // namespace WindowsHook
} // namespace UndownUnlock 