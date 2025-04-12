#include <iostream>
#include <Windows.h>
#include <string>
#include <thread>
#include <chrono>

#include "virtual_camera/kernel_driver.h"
#include "virtual_camera/virtual_camera.h"

using namespace UndownUnlock::VirtualCamera;

// Test image size and format
const int TEST_WIDTH = 1280;
const int TEST_HEIGHT = 720;
const PixelFormat TEST_FORMAT = PixelFormat::RGB24;
const int TEST_FRAME_RATE = 30;

// Test colors
const BYTE RED_COLOR[] = {0, 0, 255};    // BGR format
const BYTE GREEN_COLOR[] = {0, 255, 0};
const BYTE BLUE_COLOR[] = {255, 0, 0};

// Helper function to create a test frame
bool GenerateTestFrame(BYTE* buffer, int width, int height, PixelFormat format, const BYTE* color) {
    if (!buffer || width <= 0 || height <= 0) {
        return false;
    }
    
    int bytesPerPixel = 0;
    switch (format) {
        case PixelFormat::RGB24:
            bytesPerPixel = 3;
            break;
        case PixelFormat::RGB32:
            bytesPerPixel = 4;
            break;
        default:
            std::cerr << "Unsupported format for test" << std::endl;
            return false;
    }
    
    int stride = width * bytesPerPixel;
    int totalSize = stride * height;
    
    // Fill the frame with the specified color
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int offset = y * stride + x * bytesPerPixel;
            
            // Copy the color values
            for (int c = 0; c < bytesPerPixel; c++) {
                buffer[offset + c] = color[c % 3];
            }
            
            // Set alpha to 255 if using RGBA
            if (bytesPerPixel == 4) {
                buffer[offset + 3] = 255;
            }
        }
    }
    
    // Add a test pattern (diagonal lines)
    for (int i = 0; i < height && i < width; i++) {
        int offset = i * stride + i * bytesPerPixel;
        for (int c = 0; c < bytesPerPixel; c++) {
            buffer[offset + c] = 255 - color[c % 3];
        }
    }
    
    return true;
}

// Test integration between driver patching and virtual camera
int main() {
    std::cout << "=== UndownUnlock Virtual Camera Driver Patching Test ===" << std::endl;
    
    // 1. Initialize the kernel driver with patching enabled
    KernelDriverConfig config;
    config.allowDriverPatching = true;
    config.bypassSecurityChecks = true;
    
    if (!KernelDriver::Initialize(config)) {
        std::cerr << "Failed to initialize kernel driver" << std::endl;
        return 1;
    }
    
    // 2. Test driver signature patching
    std::cout << "Testing driver signature patching..." << std::endl;
    if (!KernelDriver::TestDriverSignaturePatching()) {
        std::cerr << "Driver signature patching test failed" << std::endl;
        return 1;
    }
    
    // 3. Install the driver
    std::cout << "Installing driver..." << std::endl;
    if (!KernelDriver::Install()) {
        std::cerr << "Failed to install driver" << std::endl;
        return 1;
    }
    
    // 4. Start the driver service
    std::cout << "Starting driver service..." << std::endl;
    if (!KernelDriver::Start()) {
        std::cerr << "Failed to start driver service" << std::endl;
        KernelDriver::Uninstall();
        return 1;
    }
    
    // 5. Add anti-detection measures
    std::cout << "Adding anti-detection measures..." << std::endl;
    if (!KernelDriver::AddAntiDetectionMeasures()) {
        std::cerr << "Warning: Failed to add anti-detection measures" << std::endl;
        // Continue anyway, as this is not critical
    }
    
    // 6. Initialize the virtual camera
    std::cout << "Initializing virtual camera..." << std::endl;
    VirtualCameraConfig vcConfig;
    vcConfig.width = TEST_WIDTH;
    vcConfig.height = TEST_HEIGHT;
    vcConfig.pixelFormat = TEST_FORMAT;
    vcConfig.frameRate = TEST_FRAME_RATE;
    vcConfig.enableFacialTracking = true;
    
    if (!VirtualCamera::Initialize(vcConfig)) {
        std::cerr << "Failed to initialize virtual camera" << std::endl;
        KernelDriver::Stop();
        KernelDriver::Uninstall();
        return 1;
    }
    
    // 7. Set video format
    std::cout << "Setting video format..." << std::endl;
    if (!KernelDriver::SetVideoFormat(TEST_WIDTH, TEST_HEIGHT, TEST_FORMAT, TEST_FRAME_RATE)) {
        std::cerr << "Failed to set video format" << std::endl;
        VirtualCamera::Shutdown();
        KernelDriver::Stop();
        KernelDriver::Uninstall();
        return 1;
    }
    
    // 8. Create a test frame
    std::cout << "Creating test frames..." << std::endl;
    int bytesPerPixel = TEST_FORMAT == PixelFormat::RGB24 ? 3 : 4;
    int frameSize = TEST_WIDTH * TEST_HEIGHT * bytesPerPixel;
    BYTE* frameBuffer = new BYTE[frameSize];
    
    // 9. Test frame transmission
    std::cout << "Sending test frames..." << std::endl;
    for (int i = 0; i < 100; i++) {
        // Alternate between colors
        const BYTE* color = NULL;
        switch (i % 3) {
            case 0: color = RED_COLOR; break;
            case 1: color = GREEN_COLOR; break;
            case 2: color = BLUE_COLOR; break;
        }
        
        // Generate and send a frame
        if (GenerateTestFrame(frameBuffer, TEST_WIDTH, TEST_HEIGHT, TEST_FORMAT, color)) {
            if (!KernelDriver::SetFrame(frameBuffer, frameSize)) {
                std::cerr << "Warning: Failed to send frame " << i << std::endl;
            }
        }
        
        // Brief delay between frames
        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30fps
    }
    
    // 10. Clean up
    std::cout << "Cleaning up..." << std::endl;
    delete[] frameBuffer;
    
    VirtualCamera::Shutdown();
    KernelDriver::Stop();
    
    // 11. Restore original driver signature (optional)
    std::cout << "Restoring original driver signature..." << std::endl;
    if (!KernelDriver::RestoreOriginalDriverSignature()) {
        std::cerr << "Warning: Failed to restore original driver signature" << std::endl;
    }
    
    // 12. Uninstall the driver
    std::cout << "Uninstalling driver..." << std::endl;
    if (!KernelDriver::Uninstall()) {
        std::cerr << "Warning: Failed to uninstall driver completely" << std::endl;
    }
    
    KernelDriver::Shutdown();
    
    std::cout << "=== Test completed successfully ===" << std::endl;
    return 0;
} 