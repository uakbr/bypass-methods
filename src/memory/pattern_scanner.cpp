#include "../../include/memory/pattern_scanner.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <Psapi.h>
#include <future>
#include <thread>

namespace UndownUnlock {
namespace DXHook {

// Cache alignment size for better memory access patterns
constexpr size_t CACHE_LINE_SIZE = 64;

// Constructor now sets the number of threads for parallel scanning
PatternScanner::PatternScanner()
    : m_progressCallback(nullptr)
    , m_numThreads(std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4) {
}

PatternScanner::~PatternScanner() {
}

bool PatternScanner::Initialize() {
    try {
        // Initialize memory regions for scanning
        InitializeMemoryRegions();
        
        // Pre-analyze PE sections to optimize scanning
        AnalyzePESections();
        
        return !m_memoryRegions.empty();
    } catch (const std::exception& e) {
        std::cerr << "Exception in PatternScanner::Initialize: " << e.what() << std::endl;
        return false;
    }
}

void PatternScanner::InitializeMemoryRegions() {
    // Clear existing regions
    m_memoryRegions.clear();
    
    // Get system info for memory range
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    
    // Scan the entire process memory range
    MEMORY_BASIC_INFORMATION memInfo;
    LPVOID address = sysInfo.lpMinimumApplicationAddress;
    
    // Get modules to name memory regions
    std::vector<std::tuple<void*, size_t, std::string>> modules = GetModules();
    
    while (address < sysInfo.lpMaximumApplicationAddress) {
        if (VirtualQuery(address, &memInfo, sizeof(memInfo))) {
            // Only add regions that are committed and have execute or read access
            if ((memInfo.State == MEM_COMMIT) && 
                (memInfo.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READONLY | PAGE_READWRITE))) {
                
                MemoryRegion region;
                region.baseAddress = memInfo.BaseAddress;
                region.size = memInfo.RegionSize;
                region.protection = memInfo.Protect;
                region.isExecutable = (memInfo.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0;
                
                // Try to find a module that contains this address
                for (const auto& module : modules) {
                    void* moduleBase = std::get<0>(module);
                    size_t moduleSize = std::get<1>(module);
                    
                    if (memInfo.BaseAddress >= moduleBase && 
                        reinterpret_cast<uint8_t*>(memInfo.BaseAddress) < reinterpret_cast<uint8_t*>(moduleBase) + moduleSize) {
                        region.name = std::get<2>(module);
                        region.isModuleRegion = true;
                        break;
                    }
                }
                
                m_memoryRegions.push_back(region);
            }
            
            // Move to the next memory region
            address = reinterpret_cast<LPBYTE>(memInfo.BaseAddress) + memInfo.RegionSize;
        } else {
            // If VirtualQuery fails, move to the next page
            address = reinterpret_cast<LPBYTE>(address) + sysInfo.dwPageSize;
        }
    }
    
    std::cout << "Initialized " << m_memoryRegions.size() << " memory regions for scanning" << std::endl;
}

void PatternScanner::AnalyzePESections() {
    // Analyze module sections to identify code sections for targeted scanning
    for (auto& region : m_memoryRegions) {
        if (!region.isModuleRegion || region.name.empty()) {
            continue;
        }
        
        // PE header analysis
        try {
            HMODULE hModule = GetModuleHandleA(region.name.c_str());
            if (!hModule) continue;
            
            const IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(hModule);
            if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) continue;
            
            const IMAGE_NT_HEADERS* ntHeader = reinterpret_cast<const IMAGE_NT_HEADERS*>(
                reinterpret_cast<const uint8_t*>(hModule) + dosHeader->e_lfanew);
            if (ntHeader->Signature != IMAGE_NT_SIGNATURE) continue;
            
            // Mark this as a PE module
            region.isPEModule = true;
            
            // Store PE section information
            const IMAGE_SECTION_HEADER* sectionHeader = IMAGE_FIRST_SECTION(ntHeader);
            for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; i++) {
                if (sectionHeader[i].Characteristics & IMAGE_SCN_CNT_CODE) {
                    // This is a code section, mark it
                    region.codeSection = true;
                    region.codePriority = 2; // Higher priority for code sections
                } else if (sectionHeader[i].Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) {
                    // Initialized data section
                    region.dataPriority = 1; // Medium priority
                }
            }
        } catch (...) {
            // Continue even if PE analysis fails
            continue;
        }
    }
    
    // Sort regions by priority (code sections first, then data, then others)
    std::sort(m_memoryRegions.begin(), m_memoryRegions.end(), 
        [](const MemoryRegion& a, const MemoryRegion& b) {
            if (a.codePriority != b.codePriority) {
                return a.codePriority > b.codePriority; // Higher priority first
            }
            if (a.dataPriority != b.dataPriority) {
                return a.dataPriority > b.dataPriority;
            }
            return a.isExecutable && !b.isExecutable; // Executable regions first
        });
}

std::vector<std::tuple<void*, size_t, std::string>> PatternScanner::GetModules() {
    std::vector<std::tuple<void*, size_t, std::string>> modules;
    
    // Enumerate all modules in the process
    HMODULE moduleHandles[1024];
    DWORD cbNeeded;
    
    if (EnumProcessModules(GetCurrentProcess(), moduleHandles, sizeof(moduleHandles), &cbNeeded)) {
        int moduleCount = cbNeeded / sizeof(HMODULE);
        
        for (int i = 0; i < moduleCount; i++) {
            MODULEINFO moduleInfo;
            if (GetModuleInformation(GetCurrentProcess(), moduleHandles[i], &moduleInfo, sizeof(moduleInfo))) {
                char moduleName[MAX_PATH];
                if (GetModuleFileNameExA(GetCurrentProcess(), moduleHandles[i], moduleName, MAX_PATH)) {
                    // Extract just the filename
                    std::string fullPath(moduleName);
                    size_t lastSlash = fullPath.find_last_of("/\\");
                    std::string fileName = (lastSlash != std::string::npos) ? 
                        fullPath.substr(lastSlash + 1) : fullPath;
                    
                    modules.emplace_back(moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage, fileName);
                }
            }
        }
    }
    
    return modules;
}

void PatternScanner::AddMemoryRegion(const MemoryRegion& region) {
    m_memoryRegions.push_back(region);
}

const std::vector<MemoryRegion>& PatternScanner::GetMemoryRegions() const {
    return m_memoryRegions;
}

void PatternScanner::SetProgressCallback(std::function<void(int)> callback) {
    m_progressCallback = callback;
}

void PatternScanner::SetNumThreads(unsigned int numThreads) {
    m_numThreads = numThreads > 0 ? numThreads : 1;
}

std::pair<std::vector<uint8_t>, std::string> PatternScanner::ParsePatternString(const std::string& patternString) {
    std::vector<uint8_t> pattern;
    std::string mask;
    
    std::istringstream stream(patternString);
    std::string token;
    
    while (stream >> token) {
        if (token == "?" || token == "??") {
            pattern.push_back(0);
            mask.push_back('?');
        } else {
            // Convert hex string to byte
            try {
                uint8_t byte = static_cast<uint8_t>(std::stoi(token, nullptr, 16));
                pattern.push_back(byte);
                mask.push_back('x');
            } catch (const std::exception& e) {
                std::cerr << "Error parsing pattern token: " << token << " - " << e.what() << std::endl;
                // Use wildcard for invalid tokens
                pattern.push_back(0);
                mask.push_back('?');
            }
        }
    }
    
    return {pattern, mask};
}

std::vector<PatternScanResult> PatternScanner::ScanForPattern(
    const std::vector<uint8_t>& pattern,
    const std::string& mask,
    const std::string& name,
    const std::string& moduleName) {
    
    std::vector<PatternScanResult> results;
    
    if (pattern.empty() || mask.empty() || pattern.size() != mask.size()) {
        return results;
    }
    
    // Filter regions by module name if specified
    std::vector<MemoryRegion> regionsToScan;
    for (const auto& region : m_memoryRegions) {
        if (moduleName.empty() || region.name == moduleName) {
            regionsToScan.push_back(region);
        }
    }
    
    // Divide regions into chunks for parallel processing
    std::vector<std::vector<MemoryRegion>> regionChunks;
    regionChunks.resize(m_numThreads);
    
    // Distribute regions to threads
    for (size_t i = 0; i < regionsToScan.size(); i++) {
        regionChunks[i % m_numThreads].push_back(regionsToScan[i]);
    }
    
    // Create futures for parallel scanning
    std::vector<std::future<std::vector<PatternScanResult>>> futures;
    
    for (size_t i = 0; i < m_numThreads; i++) {
        if (regionChunks[i].empty()) continue;
        
        futures.push_back(std::async(std::launch::async, [this, &pattern, &mask, &name, i, &regionChunks]() {
            std::vector<PatternScanResult> threadResults;
            
            // Process all regions assigned to this thread
            for (const auto& region : regionChunks[i]) {
                try {
                    // Skip if region is not accessible
                    if (IsBadReadPtr(region.baseAddress, region.size)) {
                        continue;
                    }
                    
                    // Scan this region
                    std::vector<void*> addresses = BoyerMooreHorspoolSearch(
                        static_cast<const uint8_t*>(region.baseAddress),
                        region.size,
                        pattern,
                        mask
                    );
                    
                    // Convert to results
                    for (void* addr : addresses) {
                        PatternScanResult result;
                        result.address = addr;
                        result.patternName = name;
                        result.confidence = 100; // Exact match
                        threadResults.push_back(result);
                    }
                } catch (...) {
                    // Just skip this region on error
                }
            }
            
            return threadResults;
        }));
    }
    
    // Collect and merge results from all threads
    int completedTasks = 0;
    for (auto& future : futures) {
        try {
            auto threadResults = future.get();
            results.insert(results.end(), threadResults.begin(), threadResults.end());
            
            // Update progress
            completedTasks++;
            if (m_progressCallback) {
                m_progressCallback((completedTasks * 100) / futures.size());
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in pattern scanning task: " << e.what() << std::endl;
        }
    }
    
    // Final progress update
    if (m_progressCallback) {
        m_progressCallback(100);
    }
    
    return results;
}

std::vector<PatternScanResult> PatternScanner::ScanForPatternString(
    const std::string& patternString,
    const std::string& name,
    const std::string& moduleName) {
    
    auto [pattern, mask] = ParsePatternString(patternString);
    return ScanForPattern(pattern, mask, name, moduleName);
}

std::unordered_map<std::string, std::vector<PatternScanResult>> PatternScanner::ScanForPatterns(
    const std::vector<std::tuple<std::vector<uint8_t>, std::string, std::string>>& patterns,
    const std::string& moduleName) {
    
    std::unordered_map<std::string, std::vector<PatternScanResult>> results;
    
    if (patterns.empty()) {
        return results;
    }
    
    // Collect all regions to scan based on module name
    std::vector<MemoryRegion> regionsToScan;
    for (const auto& region : m_memoryRegions) {
        if (moduleName.empty() || region.name == moduleName) {
            regionsToScan.push_back(region);
        }
    }
    
    // Track progress
    size_t totalOperations = patterns.size() * regionsToScan.size();
    size_t completedOperations = 0;
    
    // Multi-pattern scanning - scan all patterns in a single pass through memory
    for (const auto& region : regionsToScan) {
        try {
            // Skip if region is not accessible
            if (IsBadReadPtr(region.baseAddress, region.size)) {
                completedOperations += patterns.size();
                continue;
            }
            
            const uint8_t* data = static_cast<const uint8_t*>(region.baseAddress);
            
            // Create a cache-aligned copy of the data for better performance
            std::vector<uint8_t> alignedData;
            alignedData.reserve(region.size + CACHE_LINE_SIZE);
            
            // Ensure the data is aligned to cache line boundary
            size_t alignOffset = reinterpret_cast<uintptr_t>(data) % CACHE_LINE_SIZE;
            if (alignOffset != 0) {
                size_t paddingSize = CACHE_LINE_SIZE - alignOffset;
                alignedData.insert(alignedData.begin(), paddingSize, 0);
            }
            
            // Copy the data
            alignedData.insert(alignedData.end(), data, data + region.size);
            
            // Scan for each pattern
            for (const auto& [pattern, mask, name] : patterns) {
                // Skip if pattern is empty or invalid
                if (pattern.empty() || mask.empty() || pattern.size() != mask.size()) {
                    completedOperations++;
                    continue;
                }
                
                std::vector<void*> addresses = BoyerMooreHorspoolSearch(
                    data,
                    region.size,
                    pattern,
                    mask
                );
                
                // Convert addresses to results
                for (void* addr : addresses) {
                    PatternScanResult result;
                    result.address = addr;
                    result.patternName = name;
                    result.confidence = 100; // Exact match
                    results[name].push_back(result);
                }
                
                completedOperations++;
                if (m_progressCallback) {
                    m_progressCallback(static_cast<int>((completedOperations * 100) / totalOperations));
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception scanning region at " << region.baseAddress
                      << ": " << e.what() << std::endl;
            // Skip to the next region
            completedOperations += patterns.size();
        }
    }
    
    // Final progress update
    if (m_progressCallback) {
        m_progressCallback(100);
    }
    
    return results;
}

// Optimized Boyer-Moore-Horspool search algorithm
std::vector<void*> PatternScanner::BoyerMooreHorspoolSearch(
    const uint8_t* data,
    size_t dataSize,
    const std::vector<uint8_t>& pattern,
    const std::string& mask) {
    
    std::vector<void*> results;
    
    if (!data || dataSize == 0 || pattern.empty() || mask.empty() || pattern.size() != mask.size()) {
        return results;
    }
    
    const size_t patternSize = pattern.size();
    
    // Skip the search for patterns that are too large for the data
    if (patternSize > dataSize) {
        return results;
    }
    
    // Create the bad character table - precompute skips for all possible bytes
    // This is a key optimization for Boyer-Moore-Horspool
    alignas(CACHE_LINE_SIZE) size_t badChar[256];
    std::fill_n(badChar, 256, patternSize);
    
    // Create a bitmask for wildcards to avoid branching in the inner loop
    alignas(CACHE_LINE_SIZE) bool wildcards[patternSize];
    std::fill_n(wildcards, patternSize, false);
    
    // Precompute the bad character shifts and wildcard mask
    for (size_t i = 0; i < patternSize - 1; i++) {
        wildcards[i] = (mask[i] == '?');
        if (!wildcards[i]) {
            badChar[pattern[i]] = patternSize - 1 - i;
        }
    }
    wildcards[patternSize - 1] = (mask[patternSize - 1] == '?');
    
    // Boyer-Moore-Horspool search with optimizations:
    // 1. Avoid branching in inner loop by using precomputed wildcard mask
    // 2. Use cache-aligned data structures for better memory access
    // 3. Prefetch data to reduce cache misses
    size_t i = 0;
    while (i <= dataSize - patternSize) {
        // Prefetch next potential match location
        // This helps reduce cache misses on larger patterns
        if (patternSize > 16) {
            __builtin_prefetch(data + i + patternSize);
        }
        
        size_t j = 0;
        // Match pattern from left to right (reversed from original BM-Horspool)
        // This is more cache-friendly for modern CPUs
        for (j = 0; j < patternSize; j++) {
            if (!wildcards[j] && pattern[j] != data[i + j]) {
                break;
            }
        }
        
        // Pattern found if we checked all bytes
        if (j == patternSize) {
            results.push_back(const_cast<uint8_t*>(data + i));
        }
        
        // Shift based on bad character rule
        // Always safe because we check dataSize - patternSize above
        i += badChar[data[i + patternSize - 1]];
    }
    
    return results;
}

std::vector<std::pair<void*, int>> PatternScanner::FuzzyPatternMatch(
    const uint8_t* data,
    size_t dataSize,
    const std::vector<uint8_t>& pattern,
    int maxDistance) {
    
    std::vector<std::pair<void*, int>> results;
    
    if (!data || dataSize == 0 || pattern.empty() || maxDistance < 0) {
        return results;
    }
    
    const size_t patternSize = pattern.size();
    
    // Skip if pattern is too large
    if (patternSize > dataSize) {
        return results;
    }
    
    // For each position in the data, calculate similarity score
    for (size_t i = 0; i <= dataSize - patternSize; i++) {
        int matchingBytes = 0;
        
        // Count matching bytes
        for (size_t j = 0; j < patternSize; j++) {
            if (data[i + j] == pattern[j]) {
                matchingBytes++;
            }
        }
        
        // Calculate similarity score (0-100)
        int similarity = (matchingBytes * 100) / patternSize;
        
        // Add to results if similarity is high enough
        // 100 - maxDistance is the minimum required similarity
        if (similarity >= (100 - maxDistance)) {
            results.emplace_back(const_cast<uint8_t*>(data + i), similarity);
        }
    }
    
    // Sort by similarity (highest first)
    std::sort(results.begin(), results.end(), 
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return results;
}

} // namespace DXHook
} // namespace UndownUnlock 