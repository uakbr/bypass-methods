#include <gtest/gtest.h>
#include "../include/memory/memory_tracker.h"
#include <thread>
#include <vector>

// Disable the memory tracking macro for tests to avoid interference
#undef new

using namespace UndownUnlock::Memory;

// Test fixture for memory tracking tests
class MemoryTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory tracker without callstack capture for faster tests
        MemoryTracker::GetInstance().Initialize(false);
    }

    void TearDown() override {
        // Shutdown memory tracker
        MemoryTracker::GetInstance().Shutdown();
    }
};

// Test basic allocation tracking
TEST_F(MemoryTrackerTest, BasicAllocationTracking) {
    // Get initial stats
    MemoryStats initialStats = MemoryTracker::GetInstance().GetStats();
    
    // Allocate some memory and track it
    void* ptr = malloc(1024);
    TRACK_ALLOCATION(ptr, 1024);
    
    // Check that stats were updated
    MemoryStats stats = MemoryTracker::GetInstance().GetStats();
    EXPECT_EQ(stats.currentAllocations, initialStats.currentAllocations + 1);
    EXPECT_EQ(stats.currentBytes, initialStats.currentBytes + 1024);
    
    // Free the memory and untrack it
    UNTRACK_ALLOCATION(ptr);
    free(ptr);
    
    // Check that stats were updated
    stats = MemoryTracker::GetInstance().GetStats();
    EXPECT_EQ(stats.currentAllocations, initialStats.currentAllocations);
    EXPECT_EQ(stats.currentBytes, initialStats.currentBytes);
}

// Test leak detection
TEST_F(MemoryTrackerTest, LeakDetection) {
    // Get initial stats
    MemoryStats initialStats = MemoryTracker::GetInstance().GetStats();
    
    // Allocate some memory and track it
    void* ptr = malloc(1024);
    TRACK_ALLOCATION(ptr, 1024);
    
    // Don't free the memory - this is a deliberate leak
    
    // Dump the leak report - should show our allocation
    MemoryTracker::GetInstance().DumpLeakReport();
    
    // Check that the allocation is still tracked
    MemoryStats stats = MemoryTracker::GetInstance().GetStats();
    EXPECT_EQ(stats.currentAllocations, initialStats.currentAllocations + 1);
    
    // Clean up to avoid actual memory leaks in the test
    UNTRACK_ALLOCATION(ptr);
    free(ptr);
}

// Test tagged allocations
TEST_F(MemoryTrackerTest, TaggedAllocations) {
    // Add a filter to only show allocations with a specific tag
    MemoryTracker::GetInstance().AddTagFilter("TEST_TAG", true);
    
    // Allocate some memory with different tags
    void* ptr1 = malloc(1024);
    TRACK_ALLOCATION_TAGGED(ptr1, 1024, "TEST_TAG");
    
    void* ptr2 = malloc(1024);
    TRACK_ALLOCATION_TAGGED(ptr2, 1024, "OTHER_TAG");
    
    // Dump the leak report - should only show allocations with TEST_TAG
    MemoryTracker::GetInstance().DumpLeakReport();
    
    // Clean up
    UNTRACK_ALLOCATION(ptr1);
    UNTRACK_ALLOCATION(ptr2);
    free(ptr1);
    free(ptr2);
}

// Test multithreaded allocations
TEST_F(MemoryTrackerTest, MultithreadedAllocations) {
    constexpr int NUM_THREADS = 4;
    constexpr int ALLOCS_PER_THREAD = 100;
    
    std::vector<std::thread> threads;
    
    // Get initial stats
    MemoryStats initialStats = MemoryTracker::GetInstance().GetStats();
    
    // Create threads that allocate and free memory
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([i]() {
            std::vector<void*> ptrs;
            
            // Allocate some memory
            for (int j = 0; j < ALLOCS_PER_THREAD; j++) {
                void* ptr = malloc(64);
                TRACK_ALLOCATION_TAGGED(ptr, 64, "THREAD_" + std::to_string(i));
                ptrs.push_back(ptr);
            }
            
            // Free half the memory
            for (int j = 0; j < ALLOCS_PER_THREAD / 2; j++) {
                UNTRACK_ALLOCATION(ptrs[j]);
                free(ptrs[j]);
            }
            
            // The other half will be "leaked" but we'll track them for cleanup
        });
    }
    
    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Check that we have the expected number of allocations
    MemoryStats stats = MemoryTracker::GetInstance().GetStats();
    EXPECT_EQ(stats.currentAllocations, initialStats.currentAllocations + (NUM_THREADS * ALLOCS_PER_THREAD / 2));
    
    // Dump the leak report - should show our deliberate "leaks"
    MemoryTracker::GetInstance().DumpLeakReport();
}

// Test threshold settings
TEST_F(MemoryTrackerTest, LeakThreshold) {
    // Set a leak threshold of 100 bytes
    MemoryTracker::GetInstance().SetLeakThreshold(100);
    
    // Allocate memory below and above the threshold
    void* ptr1 = malloc(50);  // Below threshold
    TRACK_ALLOCATION(ptr1, 50);
    
    void* ptr2 = malloc(200); // Above threshold
    TRACK_ALLOCATION(ptr2, 200);
    
    // Dump the leak report - should only show allocations above the threshold
    MemoryTracker::GetInstance().DumpLeakReport();
    
    // Clean up
    UNTRACK_ALLOCATION(ptr1);
    UNTRACK_ALLOCATION(ptr2);
    free(ptr1);
    free(ptr2);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 