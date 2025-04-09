#include "../../include/virtual_camera/kernel_driver.h"
#include "../../include/virtual_camera/virtual_camera.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <Windows.h>

using namespace UndownUnlock;
using namespace UndownUnlock::VirtualCamera;

// Function to generate a test frame
std::vector<BYTE> GenerateTestFrame(int width, int height, int frameNumber) {
    // Create a colored test pattern
    std::vector<BYTE> frameData(width * height * 3); // RGB24 format
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = (y * width + x) * 3;
            
            // Create a moving pattern based on the frame number
            BYTE r = static_cast<BYTE>((x + frameNumber) % 255);
            BYTE g = static_cast<BYTE>((y + frameNumber) % 255);
            BYTE b = static_cast<BYTE>((x + y + frameNumber) % 255);
            
            frameData[index] = r;
            frameData[index + 1] = g;
            frameData[index + 2] = b;
        }
    }
    
    return frameData;
}

// Function to load a test image
bool LoadImageFile(const std::string& filename, std::vector<BYTE>& imageData, int& width, int& height) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open image file: " << filename << std::endl;
        return false;
    }
    
    // This is a very simplified BMP reader - a real implementation would be more robust
    // Skip BMP header (14 bytes)
    file.seekg(14);
    
    // Read DIB header size
    DWORD dibHeaderSize;
    file.read(reinterpret_cast<char*>(&dibHeaderSize), sizeof(DWORD));
    
    // Seek back to start of DIB header
    file.seekg(14);
    
    // Read DIB header
    std::vector<BYTE> dibHeader(dibHeaderSize);
    file.read(reinterpret_cast<char*>(dibHeader.data()), dibHeaderSize);
    
    // Extract width and height
    if (dibHeaderSize >= 20) { // BITMAPCOREHEADER or larger
        width = *reinterpret_cast<int*>(&dibHeader[4]);
        height = *reinterpret_cast<int*>(&dibHeader[8]);
        
        // Handle negative height (top-down DIB)
        if (height < 0) {
            height = -height;
        }
    } else {
        std::cerr << "Invalid BMP format" << std::endl;
        return false;
    }
    
    // Skip to the pixel data
    DWORD pixelDataOffset;
    file.seekg(10);
    file.read(reinterpret_cast<char*>(&pixelDataOffset), sizeof(DWORD));
    file.seekg(pixelDataOffset);
    
    // Calculate row size (BMP rows are padded to multiple of 4 bytes)
    int rowSize = ((width * 3 + 3) / 4) * 4;
    
    // Read the pixel data
    std::vector<BYTE> paddedData(rowSize * height);
    file.read(reinterpret_cast<char*>(paddedData.data()), paddedData.size());
    
    // Convert BGR to RGB and remove padding
    imageData.resize(width * height * 3);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // BMPs are stored bottom-up, so flip the Y coordinate
            int srcY = height - 1 - y;
            int srcIndex = srcY * rowSize + x * 3;
            int dstIndex = (y * width + x) * 3;
            
            // BMP stores as BGR, we want RGB
            imageData[dstIndex] = paddedData[srcIndex + 2]; // R
            imageData[dstIndex + 1] = paddedData[srcIndex + 1]; // G
            imageData[dstIndex + 2] = paddedData[srcIndex]; // B
        }
    }
    
    return true;
}

// Main test function
int main(int argc, char* argv[]) {
    std::cout << "UndownUnlock Virtual Camera Test Application" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    // Initialize the kernel driver with default configuration
    KernelDriverConfig config;
    
    // Set additional security bypass options
    config.bypassSecurityChecks = true;
    config.registerAsTrusted = true;
    config.allowDriverPatching = true;
    config.allowAdvancedBypass = true;
    
    std::cout << "Initializing kernel driver..." << std::endl;
    if (!KernelDriver::Initialize(config)) {
        std::cerr << "Failed to initialize kernel driver" << std::endl;
        return 1;
    }
    
    // Check if the driver is already installed
    if (!KernelDriver::IsInstalled()) {
        std::cout << "Installing kernel driver..." << std::endl;
        if (!KernelDriver::Install()) {
            std::cerr << "Failed to install kernel driver" << std::endl;
            KernelDriver::Shutdown();
            return 1;
        }
    } else {
        std::cout << "Kernel driver is already installed" << std::endl;
    }
    
    // Start the driver if not already running
    if (!KernelDriver::IsRunning()) {
        std::cout << "Starting kernel driver..." << std::endl;
        if (!KernelDriver::Start()) {
            std::cerr << "Failed to start kernel driver" << std::endl;
            KernelDriver::Shutdown();
            return 1;
        }
    } else {
        std::cout << "Kernel driver is already running" << std::endl;
    }
    
    // Apply device signature spoofing
    std::cout << "Spoofing device signature..." << std::endl;
    if (!KernelDriver::SpoofDeviceSignature()) {
        std::cerr << "Warning: Failed to spoof device signature" << std::endl;
        // Continue anyway
    }
    
    // Set video format (720p MJPG)
    std::cout << "Setting video format to 720p MJPG..." << std::endl;
    if (!KernelDriver::SetVideoFormat(1280, 720, PixelFormat::MJPG, 30)) {
        std::cerr << "Warning: Failed to set video format" << std::endl;
        // Continue anyway
    }
    
    // Initialize the virtual camera system
    std::cout << "Initializing virtual camera system..." << std::endl;
    VirtualCameraConfig cameraConfig;
    cameraConfig.width = 1280;
    cameraConfig.height = 720;
    cameraConfig.frameRate = 30;
    cameraConfig.simulateImperfections = true;
    cameraConfig.enableLightingVariations = true;
    
    if (!VirtualCameraSystem::Initialize(cameraConfig)) {
        std::cerr << "Failed to initialize virtual camera system" << std::endl;
        KernelDriver::Stop();
        KernelDriver::Shutdown();
        return 1;
    }
    
    // Check for a test image
    std::vector<BYTE> imageData;
    int imageWidth = 0, imageHeight = 0;
    bool haveImageFile = false;
    
    if (argc > 1) {
        std::cout << "Loading image file: " << argv[1] << std::endl;
        haveImageFile = LoadImageFile(argv[1], imageData, imageWidth, imageHeight);
        if (haveImageFile) {
            std::cout << "Loaded image: " << imageWidth << "x" << imageHeight << std::endl;
        }
    }
    
    // Start streaming test frames
    std::cout << "Streaming test frames. Press Ctrl+C to stop." << std::endl;
    
    try {
        for (int frameNumber = 0; frameNumber < 1000; frameNumber++) {
            // Generate or use the test frame
            std::vector<BYTE> frameData;
            
            if (haveImageFile) {
                // Use the loaded image
                frameData = imageData;
            } else {
                // Generate a test pattern
                frameData = GenerateTestFrame(1280, 720, frameNumber);
            }
            
            // Send the frame to the virtual camera
            if (!KernelDriver::SetFrame(frameData.data(), frameData.size())) {
                std::cerr << "Warning: Failed to send frame " << frameNumber << std::endl;
            } else {
                std::cout << "Sent frame " << frameNumber << " (" << frameData.size() << " bytes)" << std::endl;
            }
            
            // Get device status
            DeviceStatus status = KernelDriver::GetDeviceStatus();
            std::cout << "Device status: Active=" << (status.isActive ? "Yes" : "No") 
                      << ", Streaming=" << (status.isStreaming ? "Yes" : "No")
                      << ", Frame count=" << status.frameCount << std::endl;
            
            // Sleep for the next frame (30 fps = ~33ms per frame)
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during streaming: " << e.what() << std::endl;
    }
    
    // Clean up
    std::cout << "Cleaning up..." << std::endl;
    
    VirtualCameraSystem::Shutdown();
    
    if (KernelDriver::IsRunning()) {
        std::cout << "Stopping kernel driver..." << std::endl;
        if (!KernelDriver::Stop()) {
            std::cerr << "Warning: Failed to stop kernel driver" << std::endl;
        }
    }
    
    KernelDriver::Shutdown();
    
    std::cout << "Test completed successfully" << std::endl;
    
    return 0;
} 