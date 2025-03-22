#include "../../../include/hooks/keyboard_hook.h"
#include "../../../include/hooks/windows_api_hooks.h"

namespace UndownUnlock {
namespace WindowsHook {

// Initialize static variables
HHOOK KeyboardHook::s_keyboardHook = NULL;

void KeyboardHook::Initialize() {
    // Install a low-level keyboard hook that will monitor all keyboard input
    s_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, nullptr, 0);
    
    if (s_keyboardHook == NULL) {
        std::cerr << "Failed to install keyboard hook. Error code: " << GetLastError() << std::endl;
    }
    else {
        std::cout << "Keyboard hook installed successfully." << std::endl;
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
                    WindowsAPIHooks::InstallFocusHooks();
                    std::cout << "Up arrow key pressed, installing focus hooks." << std::endl;
                    break;
                    
                // VK_DOWN is the virtual key code for the Down arrow key
                case VK_DOWN:
                    WindowsAPIHooks::UninstallFocusHooks();
                    
                    // Apply the original functions to restore normal behavior
                    if (WindowsAPIHooks::s_focusHWND != NULL) {
                        SetFocus(WindowsAPIHooks::s_focusHWND);
                    }
                    
                    if (WindowsAPIHooks::s_bringWindowToTopHWND != NULL) {
                        BringWindowToTop(WindowsAPIHooks::s_bringWindowToTopHWND);
                    }
                    
                    if (WindowsAPIHooks::s_setWindowFocusHWND != NULL) {
                        SetWindowPos(
                            WindowsAPIHooks::s_setWindowFocusHWND, 
                            WindowsAPIHooks::s_setWindowFocushWndInsertAfter, 
                            WindowsAPIHooks::s_setWindowFocusX, 
                            WindowsAPIHooks::s_setWindowFocusY, 
                            WindowsAPIHooks::s_setWindowFocuscx, 
                            WindowsAPIHooks::s_setWindowFocuscy, 
                            WindowsAPIHooks::s_setWindowFocusuFlags
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