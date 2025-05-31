#pragma once

#include <Windows.h>
#include <iostream>
#include <thread> // Added for std::thread

namespace UndownUnlock {
namespace WindowsHook {

/**
 * @brief Class for managing low-level keyboard hooks
 */
class KeyboardHook {
public:
    /**
     * @brief Initialize the keyboard hook system
     */
    static void Initialize();

    /**
     * @brief Shutdown and cleanup the keyboard hook
     */
    static void Shutdown();

    /**
     * @brief Run the message loop to keep the hook alive
     */
    static void RunMessageLoop();

private:
    // Low-level keyboard hook handle
    static HHOOK s_keyboardHook;
    // Thread for running the message loop
    static std::thread s_messageLoopThread;

    // Keyboard hook callback procedure
    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
};

} // namespace WindowsHook
} // namespace UndownUnlock 