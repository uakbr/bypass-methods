#include <gtest/gtest.h>
#include "../include/com_hooks/com_tracker.h"
#include "../include/com_hooks/com_ptr.h"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

using namespace UndownUnlock::DXHook;

// Test fixture for COM tracking tests
class ComTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize COM
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        EXPECT_TRUE(SUCCEEDED(hr));
        
        // Initialize COM tracker without callstack capture for faster tests
        ComTracker::GetInstance().Initialize(false);
    }

    void TearDown() override {
        // Shutdown COM tracker
        ComTracker::GetInstance().Shutdown();
        
        // Uninitialize COM
        CoUninitialize();
    }
};

// Test basic COM tracking
TEST_F(ComTrackerTest, BasicTracking) {
    // Create a COM object
    IUnknown* pUnknown = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_StockFontPage, NULL, CLSCTX_INPROC_SERVER, 
                                IID_IUnknown, reinterpret_cast<void**>(&pUnknown));
    
    // Only proceed if the COM object was created successfully
    if (SUCCEEDED(hr) && pUnknown) {
        // Track the interface
        ComTracker::GetInstance().TrackInterface(pUnknown, "IUnknown", 1);
        
        // Update ref count (simulate AddRef)
        pUnknown->AddRef();
        ComTracker::GetInstance().UpdateRefCount(pUnknown, 2, true);
        
        // Update ref count (simulate Release)
        pUnknown->Release();
        ComTracker::GetInstance().UpdateRefCount(pUnknown, 1, false);
        
        // Check for issues
        int issues = ComTracker::GetInstance().CheckForIssues();
        EXPECT_EQ(issues, 0);
        
        // Untrack and clean up
        ComTracker::GetInstance().UntrackInterface(pUnknown);
        pUnknown->Release();
    }
}

// Test ComPtr template with COM tracking
TEST_F(ComTrackerTest, ComPtrTracking) {
    // Create a COM object using ComPtr
    ComPtr<IUnknown> spUnknown;
    HRESULT hr = CoCreateInstance(CLSID_StockFontPage, NULL, CLSCTX_INPROC_SERVER, 
                                IID_IUnknown, reinterpret_cast<void**>(spUnknown.GetAddressOf()));
    
    // Only proceed if the COM object was created successfully
    if (SUCCEEDED(hr) && spUnknown) {
        // Track the interface
        ComTracker::GetInstance().TrackInterface(spUnknown.Get(), "IUnknown", 1);
        
        // Create a new scope to test reference counting
        {
            // Create a copy of the ComPtr
            ComPtr<IUnknown> spUnknown2 = spUnknown;
            
            // The copy should increment the ref count
            ULONG refCount = spUnknown->AddRef();
            spUnknown->Release(); // Undo the AddRef we just did
            
            // Update the tracked ref count
            ComTracker::GetInstance().UpdateRefCount(spUnknown.Get(), refCount, true);
            
            // Check tracked count
            ComTracker::GetInstance().DumpTrackedInterfaces(false);
        }
        
        // After the inner scope, spUnknown2 should be released
        ULONG refCount = spUnknown->AddRef();
        spUnknown->Release(); // Undo the AddRef we just did
        
        // Update the tracked ref count
        ComTracker::GetInstance().UpdateRefCount(spUnknown.Get(), refCount, false);
        
        // Dump tracked interfaces
        ComTracker::GetInstance().DumpTrackedInterfaces(false);
        
        // Let ComPtr handle cleanup
        // No need to manually release spUnknown
    }
}

// Test interface type detection
TEST_F(ComTrackerTest, InterfaceTypeDetection) {
    // Test with some common DirectX interfaces
    std::string factoryType = ComTracker::GetInstance().GetInterfaceTypeFromIID(__uuidof(IDXGIFactory));
    EXPECT_EQ(factoryType, "IDXGIFactory");
    
    std::string deviceType = ComTracker::GetInstance().GetInterfaceTypeFromIID(__uuidof(ID3D11Device));
    EXPECT_EQ(deviceType, "ID3D11Device");
    
    std::string unknownType = ComTracker::GetInstance().GetInterfaceTypeFromIID(__uuidof(IUnknown));
    EXPECT_TRUE(unknownType.find("Unknown Interface") != std::string::npos);
}

// Test interface validation
TEST_F(ComTrackerTest, InterfaceValidation) {
    IUnknown* pUnknown = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_StockFontPage, NULL, CLSCTX_INPROC_SERVER, 
                                IID_IUnknown, reinterpret_cast<void**>(&pUnknown));
    
    if (SUCCEEDED(hr) && pUnknown) {
        // Valid interface
        bool valid = ComTracker::GetInstance().IsKnownInterface(IID_IUnknown);
        
        // Create a random IID that shouldn't exist
        IID randomIID = {0x12345678, 0x1234, 0x1234, {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}};
        bool unknownValid = ComTracker::GetInstance().IsKnownInterface(randomIID);
        
        // Clean up
        pUnknown->Release();
    }
}

// Test leak detection
TEST_F(ComTrackerTest, LeakDetection) {
    // Create a COM object
    IUnknown* pUnknown = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_StockFontPage, NULL, CLSCTX_INPROC_SERVER, 
                                IID_IUnknown, reinterpret_cast<void**>(&pUnknown));
    
    if (SUCCEEDED(hr) && pUnknown) {
        // Track the interface
        ComTracker::GetInstance().TrackInterface(pUnknown, "IUnknown", 1);
        
        // Deliberately "leak" it by not releasing but not untracking
        
        // Dump the leak report - should show our "leak"
        ComTracker::GetInstance().DumpTrackedInterfaces(true);
        
        // Clean up to avoid actual leaks in the test
        ComTracker::GetInstance().UntrackInterface(pUnknown);
        pUnknown->Release();
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 