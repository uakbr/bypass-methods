#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <iostream>
#include <thread>
#include <memory>
#include <functional>
#include "../include/dx_hook_core.h"
#include "../include/hooks/windows_api_hooks.h"
#include "../include/hooks/keyboard_hook.h"

// Global variables
std::unique_ptr<std::thread> g_hookThread;
std::unique_ptr<std::thread> g_keyboardThread;
std::atomic<bool> g_shutdown{false};

// Forward declarations
void DXHookThreadProc();
void KeyboardHookThreadProc();

// Console management class with RAII pattern
class ConsoleManager {
public:
    ConsoleManager() {
        #ifdef _DEBUG
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        std::cout << "Console initialized for debugging" << std::endl;
        #endif
    }
    
    ~ConsoleManager() {
        #ifdef _DEBUG
        std::cout << "Closing console..." << std::endl;
        fclose(stdout);
        fclose(stderr);
        FreeConsole();
        #endif
    }
    
    // Prevent copying
    ConsoleManager(const ConsoleManager&) = delete;
    ConsoleManager& operator=(const ConsoleManager&) = delete;
};

// Thread management class with RAII pattern
class ThreadManager {
public:
    ThreadManager() = default;
    
    ~ThreadManager() {
        Shutdown();
    }
    
    void StartDXHookThread() {
        m_dxHookThread = std::make_unique<std::thread>(&DXHookThreadProc);
    }
    
    void StartKeyboardThread() {
        m_keyboardThread = std::make_unique<std::thread>(&KeyboardHookThreadProc);
    }
    
    void Shutdown() {
        g_shutdown = true;
        
        if (m_dxHookThread && m_dxHookThread->joinable()) {
            m_dxHookThread->join();
        }
        
        if (m_keyboardThread && m_keyboardThread->joinable()) {
            // The keyboard thread runs a message loop, which may not exit on its own
            // We need to post a quit message to the thread
            if (GetThreadId(m_keyboardThread->native_handle())) {
                PostThreadMessage(GetThreadId(m_keyboardThread->native_handle()), WM_QUIT, 0, 0);
                m_keyboardThread->join();
            }
        }
    }
    
private:
    std::unique_ptr<std::thread> m_dxHookThread;
    std::unique_ptr<std::thread> m_keyboardThread;
};

// Singleton manager for the DLL resources
class DLLResourceManager {
public:
    static DLLResourceManager& GetInstance() {
        static DLLResourceManager instance;
        return instance;
    }
    
    bool Initialize(HMODULE hModule) {
        if (m_initialized)
            return true;
            
        m_hModule = hModule;
        m_console = std::make_unique<ConsoleManager>();
        
        std::cout << "UndownUnlock DirectX Hook DLL loaded" << std::endl;
        
        // Initialize Windows API hooks
        if (!UndownUnlock::WindowsHook::WindowsAPIHooks::Initialize()) {
            std::cerr << "Failed to initialize Windows API hooks" << std::endl;
            return false;
        }
        
        // Start the worker threads
        m_threadManager = std::make_unique<ThreadManager>();
        m_threadManager->StartDXHookThread();
        m_threadManager->StartKeyboardThread();
        
        m_initialized = true;
        return true;
    }
    
    void Shutdown() {
        if (!m_initialized)
            return;
            
        std::cout << "Shutting down DLL resource manager..." << std::endl;
        
        // First stop the threads
        if (m_threadManager) {
            m_threadManager->Shutdown();
            m_threadManager.reset();
        }
        
        // Then clean up the hooks
        UndownUnlock::WindowsHook::WindowsAPIHooks::Shutdown();
        UndownUnlock::WindowsHook::KeyboardHook::Shutdown();
        UndownUnlock::DXHook::DXHookCore::Shutdown();
        
        m_initialized = false;
    }
    
    ~DLLResourceManager() {
        Shutdown();
    }
    
    // Prevent copying
    DLLResourceManager(const DLLResourceManager&) = delete;
    DLLResourceManager& operator=(const DLLResourceManager&) = delete;
    
private:
    DLLResourceManager() : m_initialized(false), m_hModule(nullptr) {}
    
    bool m_initialized;
    HMODULE m_hModule;
    std::unique_ptr<ConsoleManager> m_console;
    std::unique_ptr<ThreadManager> m_threadManager;
};

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
        {
            // Disable thread notifications to reduce overhead
            DisableThreadLibraryCalls(hModule);
            
            // Initialize the singleton manager
            if (!DLLResourceManager::GetInstance().Initialize(hModule)) {
                return FALSE;
            }
            
            break;
        }
        
        case DLL_PROCESS_DETACH:
        {
            // Clean up resources
            DLLResourceManager::GetInstance().Shutdown();
            break;
        }
    }
    
    return TRUE;
}

// Thread for DirectX hooking
void DXHookThreadProc() {
    // Wait a bit for the application to initialize DirectX
    Sleep(1000);
    
    // Initialize the hook core with proper error handling
    if (!UndownUnlock::DXHook::DXHookCore::Initialize()) {
        std::cerr << "Failed to initialize DirectX hook core" << std::endl;
        return;
    }
    
    std::cout << "DirectX hook thread started successfully" << std::endl;
    
    // Main loop - keep the thread alive and handle any ongoing tasks
    while (!g_shutdown) {
        // Sleep to avoid high CPU usage
        Sleep(100);
    }
    
    std::cout << "DirectX hook thread shutting down" << std::endl;
}

// Thread for keyboard hook
void KeyboardHookThreadProc() {
    // Initialize and start the keyboard hook
    if (!UndownUnlock::WindowsHook::KeyboardHook::Initialize()) {
        std::cerr << "Failed to initialize keyboard hook" << std::endl;
        return;
    }
    
    std::cout << "Keyboard hook thread started successfully" << std::endl;
    
    // Run the message loop
    UndownUnlock::WindowsHook::KeyboardHook::RunMessageLoop();
    
    std::cout << "Keyboard hook thread shutting down" << std::endl;
}

// Export a simple test function
extern "C" __declspec(dllexport) bool IsHookActive() {
    return true; // We don't have direct access to private member in DXHookCore
} 