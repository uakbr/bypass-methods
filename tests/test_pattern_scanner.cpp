#include <gtest/gtest.h>
#include <memory/pattern_scanner.h>
#include <vector>
#include <string>

using namespace UndownUnlock::DXHook;

// Test fixture for PatternScanner
class PatternScannerTest : public ::testing::Test {
protected:
    void SetUp() override {
        scanner = new PatternScanner();
        scanner->Initialize();
    }

    void TearDown() override {
        delete scanner;
    }

    PatternScanner* scanner;
};

// Test ParsePatternString
TEST_F(PatternScannerTest, TestParsePatternString) {
    // Test with a simple pattern
    auto [pattern1, mask1] = PatternScanner::ParsePatternString("48 8B 05 ? ? ? ? 48 85 C0");
    
    ASSERT_EQ(pattern1.size(), 10);
    ASSERT_EQ(mask1.size(), 10);
    
    // Verify the pattern values
    EXPECT_EQ(pattern1[0], 0x48);
    EXPECT_EQ(pattern1[1], 0x8B);
    EXPECT_EQ(pattern1[2], 0x05);
    
    // Verify the mask values
    EXPECT_EQ(mask1[0], 'x');
    EXPECT_EQ(mask1[1], 'x');
    EXPECT_EQ(mask1[2], 'x');
    EXPECT_EQ(mask1[3], '?');
    EXPECT_EQ(mask1[4], '?');
    EXPECT_EQ(mask1[5], '?');
    EXPECT_EQ(mask1[6], '?');
    EXPECT_EQ(mask1[7], 'x');
    EXPECT_EQ(mask1[8], 'x');
    EXPECT_EQ(mask1[9], 'x');
    
    // Test with an empty pattern
    auto [pattern2, mask2] = PatternScanner::ParsePatternString("");
    EXPECT_TRUE(pattern2.empty());
    EXPECT_TRUE(mask2.empty());
    
    // Test with invalid hex values
    auto [pattern3, mask3] = PatternScanner::ParsePatternString("ZZ 8B 05");
    EXPECT_EQ(pattern3.size(), 3);
    EXPECT_EQ(mask3.size(), 3);
    // First byte should be treated as a wildcard
    EXPECT_EQ(mask3[0], '?');
}

// Test the GetModules function
TEST_F(PatternScannerTest, TestGetModules) {
    auto modules = scanner->GetModules();
    
    // Verify that at least some modules were found
    EXPECT_GT(modules.size(), 0);
    
    // Verify that each module has a base address, size, and name
    for (const auto& module : modules) {
        EXPECT_NE(std::get<0>(module), nullptr);
        EXPECT_GT(std::get<1>(module), 0);
        EXPECT_FALSE(std::get<2>(module).empty());
    }
}

// Main function to run the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 