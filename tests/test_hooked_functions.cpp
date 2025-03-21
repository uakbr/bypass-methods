#include <gtest/gtest.h>
#include <hooks/windows_api_hooks.h>
#include <hooks/hooked_functions.h>
#include <Windows.h>

using namespace UndownUnlock::WindowsHook;

// Test fixture for Windows API hooks
class WindowsAPIHookTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing to set up
    }

    void TearDown() override {
        // Nothing to tear down
    }
};

// Test the clipboard hook functions
TEST_F(WindowsAPIHookTest, TestClipboardFunctions) {
    // Test that HookedEmptyClipboard returns TRUE without actually clearing the clipboard
    EXPECT_TRUE(HookedEmptyClipboard());
    
    // Test that HookedSetClipboardData returns NULL
    HANDLE result = HookedSetClipboardData(CF_TEXT, nullptr);
    EXPECT_EQ(result, nullptr);
}

// Test the process management hooks
TEST_F(WindowsAPIHookTest, TestProcessFunctions) {
    // Test that HookedOpenProcess returns NULL
    HANDLE processHandle = HookedOpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
    EXPECT_EQ(processHandle, nullptr);
    
    // Test that HookedTerminateProcess returns TRUE without actually terminating
    EXPECT_TRUE(HookedTerminateProcess(nullptr, 0));
    
    // Test that HookedK32EnumProcesses sets *pBytesReturned to 0
    DWORD processIds[10];
    DWORD bytesReturned = 1; // Should be set to 0 by the function
    EXPECT_TRUE(HookedK32EnumProcesses(processIds, sizeof(processIds), &bytesReturned));
    EXPECT_EQ(bytesReturned, 0);
}

// Test the window management hooks
TEST_F(WindowsAPIHookTest, TestWindowFunctions) {
    // Test that HookedGetWindowTextW returns an empty string
    WCHAR buffer[100] = L"Not Empty";
    int result = HookedGetWindowTextW(nullptr, buffer, 100);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(buffer[0], L'\0');
    
    // Test that HookedSetWindowPos returns TRUE
    EXPECT_TRUE(HookedSetWindowPos(nullptr, nullptr, 0, 0, 0, 0, 0));
    
    // Test that HookedShowWindow returns TRUE
    EXPECT_TRUE(HookedShowWindow(nullptr));
    
    // Test that HookedGetWindow returns NULL
    EXPECT_EQ(HookedGetWindow(nullptr, 0), nullptr);
    
    // Test that HookedGetForegroundWindow likely returns NULL (since we don't have a main window in tests)
    // This is more of a behavioral test, the actual result might vary
    HWND foregroundWindow = HookedGetForegroundWindow();
    // We won't assert anything here, as the result could be NULL or a real window handle
}

// Main function to run the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 