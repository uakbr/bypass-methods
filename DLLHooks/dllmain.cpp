#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <iostream>
#include <thread>
#include "../include/dx_hook_core.h"
#include "../include/hooks/windows_api_hooks.h"
#include "../include/hooks/keyboard_hook.h"

// Global variables
HMODULE g_hModule = nullptr;
std::thread g_hookThread;
std::thread g_keyboardThread;
bool g_shutdown = false;

// Forward declarations
DWORD WINAPI DXHookThreadProc(LPVOID lpParam);
DWORD WINAPI KeyboardHookThreadProc(LPVOID lpParam);

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
        {
            // Store the module handle for later use
            g_hModule = hModule;
            
            // Disable thread notifications to reduce overhead
            DisableThreadLibraryCalls(hModule);
            
            // Create a console for debugging if needed
            #ifdef _DEBUG
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONOUT$", "w", stderr);
            #endif
            
            std::cout << "UndownUnlock DirectX Hook DLL loaded" << std::endl;
            
            // Install Windows API hooks
            UndownUnlock::WindowsHook::WindowsAPIHooks::Initialize();
            
            // Start the DirectX hook thread
            g_hookThread = std::thread(DXHookThreadProc, nullptr);
            g_hookThread.detach();
            
            // Start the keyboard hook thread
            g_keyboardThread = std::thread(KeyboardHookThreadProc, nullptr);
            g_keyboardThread.detach();
            
            break;
        }
        
        case DLL_PROCESS_DETACH:
        {
            // Signal threads to exit
            g_shutdown = true;
            
            // Clean up
            UndownUnlock::WindowsHook::WindowsAPIHooks::Shutdown();
            UndownUnlock::WindowsHook::KeyboardHook::Shutdown();
            UndownUnlock::DXHook::DXHookCore::Shutdown();
            
            // Clean up console if we created one
            #ifdef _DEBUG
            FreeConsole();
            #endif
            
            break;
        }
    }
    
    return TRUE;
}

// Thread for DirectX hooking
DWORD WINAPI DXHookThreadProc(LPVOID lpParam) {
    // Wait a bit for the application to initialize DirectX
    Sleep(1000);
    
    // Initialize the hook core
    if (!UndownUnlock::DXHook::DXHookCore::Initialize()) {
        std::cerr << "Failed to initialize DirectX hook core" << std::endl;
        return 1;
    }
    
    // Main loop - keep the thread alive and handle any ongoing tasks
    while (!g_shutdown) {
        // Sleep to avoid high CPU usage
        Sleep(100);
    }
    
    return 0;
}

// Thread for keyboard hook
DWORD WINAPI KeyboardHookThreadProc(LPVOID lpParam) {
    // Initialize and start the keyboard hook
    UndownUnlock::WindowsHook::KeyboardHook::Initialize();
    UndownUnlock::WindowsHook::KeyboardHook::RunMessageLoop();
    
    return 0;
}

// Export a simple test function
extern "C" __declspec(dllexport) bool IsHookActive() {
    // Check if DXHookCore instance exists and is initialized
    return UndownUnlock::DXHook::DXHookCore::s_instance != nullptr && UndownUnlock::DXHook::DXHookCore::s_instance->m_initialized;
}