#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include "../include/virtual_camera/virtual_camera.h"

using namespace UndownUnlock::VirtualCamera;

// Helper function to create a simple test pattern
void CreateTestPattern(std::vector<BYTE>& buffer, int width, int height) {
    // Allocate buffer for RGB data (24 bits per pixel)
    buffer.resize(width * height * 3);
    
    // Create a colorful test pattern
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int offset = (y * width + x) * 3;
            
            // Calculate normalized coordinates (0.0 to 1.0)
            float nx = static_cast<float>(x) / static_cast<float>(width);
            float ny = static_cast<float>(y) / static_cast<float>(height);
            
            // Create a colorful gradient pattern
            buffer[offset] = static_cast<BYTE>(ny * 255); // B
            buffer[offset + 1] = static_cast<BYTE>((1.0f - nx) * 255); // G
            buffer[offset + 2] = static_cast<BYTE>(nx * 255); // R
            
            // Add some test elements
            
            // Grid pattern
            if (x % 64 == 0 || y % 64 == 0) {
                buffer[offset] = 255; // B
                buffer[offset + 1] = 255; // G
                buffer[offset + 2] = 255; // R
            }
            
            // Center circle
            float centerX = width / 2.0f;
            float centerY = height / 2.0f;
            float dx = x - centerX;
            float dy = y - centerY;
            float distance = sqrtf(dx * dx + dy * dy);
            
            if (distance < width / 6 && distance > width / 7) {
                buffer[offset] = 0; // B
                buffer[offset + 1] = 0; // G
                buffer[offset + 2] = 0; // R
            }
            
            // Timestamp area (reserved for later)
            if (y > height - 40 && y < height - 10 && x > 20 && x < 300) {
                buffer[offset] = 32; // B
                buffer[offset + 1] = 32; // G
                buffer[offset + 2] = 32; // R
            }
        }
    }
}

// Helper function to update timestamp on the frame
void UpdateTimestamp(std::vector<BYTE>& buffer, int width, int height) {
    // Get current time
    time_t now = time(nullptr);
    tm* timeInfo = localtime(&now);
    
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeInfo);
    
    // Draw timestamp at the bottom-left corner
    int startX = 30;
    int startY = height - 30;
    
    // Simple bitmap font rendering (very basic)
    for (size_t i = 0; i < strlen(timestamp); i++) {
        char c = timestamp[i];
        int charX = startX + i * 8;
        
        // Skip if out of bounds
        if (charX >= width - 10) {
            break;
        }
        
        // Draw a simple representation of the character
        for (int y = 0; y < 12; y++) {
            for (int x = 0; x < 6; x++) {
                // Check if the pixel should be on based on the character
                bool isOn = false;
                
                // Very simplified font rendering
                if (c >= '0' && c <= '9') {
                    // For digits
                    if ((x == 0 || x == 5) && (y > 0 && y < 11)) isOn = true; // vertical lines
                    if ((y == 0 || y == 11) && (x > 0 && x < 5)) isOn = true; // horizontal lines
                    if (c == '1' && x < 3) isOn = false;
                    if (c == '2' && y < 6 && x == 0) isOn = false;
                    if (c == '2' && y > 6 && x == 5) isOn = false;
                    // ... additional digit patterns could be added
                } else if (c == ':') {
                    // For colon
                    if ((x == 2 || x == 3) && (y == 3 || y == 4 || y == 8 || y == 9)) isOn = true;
                } else if (c == '-') {
                    // For dash
                    if ((y == 6) && (x >= 1 && x <= 4)) isOn = true;
                } else if (c == ' ') {
                    // For space
                    isOn = false;
                } else {
                    // For other characters (simplified)
                    if ((x == 0 || x == 5) && (y > 0 && y < 11)) isOn = true;
                    if ((y == 0 || y == 11) && (x > 0 && x < 5)) isOn = true;
                }
                
                if (isOn) {
                    int pixelX = charX + x;
                    int pixelY = startY + y;
                    int offset = (pixelY * width + pixelX) * 3;
                    
                    if (offset >= 0 && offset < buffer.size() - 2) {
                        buffer[offset] = 255;     // B
                        buffer[offset + 1] = 255; // G
                        buffer[offset + 2] = 255; // R
                    }
                }
            }
        }
    }
}

// Test the virtual camera system
int main() {
    std::cout << "=== Virtual Camera System Test ===" << std::endl;
    
    // Initialize COM (for Windows components)
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    
    // Create configuration
    VirtualCameraConfig config;
    config.width = 640;
    config.height = 480;
    config.frameRate = 30;
    config.cameraName = L"UndownUnlock Test Camera";
    config.enableFacialTracking = true;
    config.simulateImperfections = true;
    config.enableLightingVariations = true;
    
    // Initialize the virtual camera system
    if (!VirtualCameraSystem::Initialize(config)) {
        std::cerr << "Failed to initialize virtual camera system" << std::endl;
        CoUninitialize();
        return 1;
    }
    
    std::cout << "Virtual camera initialized successfully" << std::endl;
    std::cout << "Generating test pattern..." << std::endl;
    
    // Create a test pattern frame
    std::vector<BYTE> frameBuffer;
    CreateTestPattern(frameBuffer, config.width, config.height);
    
    // Main simulation loop
    bool running = true;
    int frameCount = 0;
    
    std::cout << "Starting camera simulation - press Ctrl+C to exit" << std::endl;
    
    while (running) {
        try {
            // Update timestamp on the frame
            UpdateTimestamp(frameBuffer, config.width, config.height);
            
            // Set the frame as the camera source
            if (!VirtualCameraSystem::SetFrameSource(
                    frameBuffer.data(),
                    frameBuffer.size(),
                    config.width, 
                    config.height,
                    config.width * 3)) {
                std::cerr << "Failed to set frame source" << std::endl;
                break;
            }
            
            // Print status every 30 frames (approximately every second)
            frameCount++;
            if (frameCount % 30 == 0) {
                std::cout << "Frames sent: " << frameCount << std::endl;
            }
            
            // Wait for next frame (approximately 33ms for 30fps)
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            
            // Check for exit condition (could be improved with proper event handling)
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                std::cout << "ESC key pressed, exiting..." << std::endl;
                running = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            running = false;
        }
    }
    
    // Shutdown the virtual camera system
    VirtualCameraSystem::Shutdown();
    
    // Clean up COM
    CoUninitialize();
    
    std::cout << "Test completed" << std::endl;
    return 0;
} 