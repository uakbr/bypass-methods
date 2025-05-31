#include "../../../include/hooks/trampoline_utils.h"
#include <windows.h>
#include <iostream> // For debugging

// =================================================================================================
// WARNING: THIS FILE CONTAINS CRITICAL LIMITATIONS FOR PRODUCTION USE
// =================================================================================================
// The trampoline utilities provided in this file, especially `CalculateMinimumPrologueSize`,
// are simplified and DO NOT USE A DISASSEMBLER. This means they are inherently unsafe for
// general-purpose hooking, particularly in x64 environments or with functions that have
// complex or variable-length prologue instructions.
//
// Using this code without a proper disassembler (e.g., Zydis, Capstone) to accurately
// determine instruction boundaries will likely lead to:
//   - Cutting instructions mid-byte.
//   - Copying incomplete instructions to the trampoline.
//   - Incorrect jump calculations.
//   - Application instability, crashes, or undefined behavior.
//
// The current `CalculateMinimumPrologueSize` includes a compile-time error to prevent
// compilation for x64 due to these safety concerns. The x86 logic is also risky and
// assumes simple function prologues.
//
// PROCEED WITH EXTREME CAUTION AND CONSIDER THIS A PLACEHOLDER FOR A ROBUST SOLUTION.
// =================================================================================================
#include <stdexcept> // For exceptions if needed, though returning bool for now

// Define JMP_REL32_SIZE if not already defined (e.g. in a common header)
#ifndef JMP_REL32_SIZE
#define JMP_REL32_SIZE 5
#endif

// Define JMP_ABS_X64_SIZE for potential future x64 (though current code is x86 focused)
#ifndef JMP_ABS_X64_SIZE
#define JMP_ABS_X64_SIZE 14 // Example: MOV RAX, addr; JMP RAX
#endif


namespace UndownUnlock {
    namespace TrampolineUtils {

        SIZE_T CalculateMinimumPrologueSize(LPCVOID functionAddress, SIZE_T minBytesToCopy) {
            // This function is a critical placeholder and has severe limitations.
            //
            // CRITICAL IMPLEMENTATION NOTE:
            // A proper disassembler (e.g., Zydis, Capstone) is ABSOLUTELY REQUIRED to accurately
            // determine the number of bytes to copy for a function prologue without cutting instructions
            // in half. Simply copying a fixed number of bytes is extremely dangerous.
            //
            // The current approach of using a fixed size (even if slightly larger than a JMP instruction)
            // is inherently unsafe and can lead to application instability or crashes. This is because:
            //   1. Instructions have variable lengths. Copying a fixed size can easily cut an instruction.
            //   2. Functions can have prologues of varying complexity.
            //   3. Optimizations (like hotpatching support) can alter prologues.
            //
            // This is particularly problematic for x64 due to:
            //   - Different instruction encodings and lengths (e.g., REX prefixes).
            //   - The potential need for larger JMP instructions (e.g., 14-byte absolute JMP via RAX)
            //     if the detour target is outside the +/- 2GB range of a 32-bit relative JMP.
            //     The current trampoline and hook logic primarily uses 5-byte relative JMPs.
            //
            // The Hook class currently determines `prologueSize_` (often default 5 bytes) and passes it
            // as `minBytesToCopy`. This function, in its current state, doesn't perform any actual
            // disassembly or intelligent calculation based on `functionAddress`.

            #ifdef _WIN64
            #error "CRITICAL: CalculateMinimumPrologueSize in trampoline_utils.cpp is not implemented safely for x64. It lacks a disassembler to determine correct instruction boundaries and uses x86-specific JMP logic. This will lead to crashes. Compilation halted to prevent runtime errors. A proper disassembler and x64-compatible trampoline logic are required."
            #endif

            // The following logic is for x86 (32-bit) context ONLY and is still risky.
            // It assumes that `minBytesToCopy` (typically 5 bytes, the size of a JMP_REL32 instruction)
            // is sufficient for the prologue of simple functions. This is a fragile assumption.
            // The Hook class relies on this behavior for its `prologueSize_` calculation.
            if (minBytesToCopy < JMP_REL32_SIZE) {
                // If the requested size is less than a 32-bit relative JMP, return at least that much.
                // This primarily ensures that if we overwrite with a JMP, we've copied at least enough
                // bytes that our JMP instruction itself would occupy.
                return JMP_REL32_SIZE;
            }

            // For x86, and with the above caveats, return the passed `minBytesToCopy`.
            // This is still unsafe as it doesn't guarantee instruction integrity.
            // std::cout << "[TrampolineUtils] CalculateMinimumPrologueSize (x86, UNSAFE): Using fixed size: " << minBytesToCopy << std::endl;
            return minBytesToCopy;
        }


        bool AllocateTrampoline(
            LPCVOID targetFunction,
            SIZE_T prologueSize,      // How many bytes of original function to copy
            PBYTE copiedPrologueBytes,// Buffer to store the copied prologue (allocated by caller)
            LPVOID& trampolineAddress // Out: Address of the allocated trampoline
        ) {
            if (!targetFunction || !copiedPrologueBytes || prologueSize == 0) {
                std::cerr << "[TrampolineUtils::AllocateTrampoline] Invalid arguments." << std::endl;
                return false;
            }

            // 1. Read prologueSize bytes from targetFunction into copiedPrologueBytes buffer
            DWORD oldProtectRead;
            if (!VirtualProtect(const_cast<LPVOID>(targetFunction), prologueSize, PAGE_EXECUTE_READ, &oldProtectRead)) {
                 std::cerr << "[TrampolineUtils::AllocateTrampoline] VirtualProtect (read) failed. Error: " << GetLastError() << std::endl;
                return false;
            }
            memcpy(copiedPrologueBytes, targetFunction, prologueSize);
            VirtualProtect(const_cast<LPVOID>(targetFunction), prologueSize, oldProtectRead, &oldProtectRead); // Restore protection


            // 2. Allocate executable memory for the trampoline
            // Trampoline size = prologueSize (original instructions) + JMP_REL32_SIZE (jmp back to original_func + prologueSize)
            SIZE_T trampolineTotalSize = prologueSize + JMP_REL32_SIZE;
            trampolineAddress = VirtualAlloc(NULL, trampolineTotalSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (!trampolineAddress) {
                std::cerr << "[TrampolineUtils::AllocateTrampoline] VirtualAlloc failed. Error: " << GetLastError() << std::endl;
                return false;
            }

            // 3. Write the copiedPrologueBytes to the start of the trampoline
            memcpy(trampolineAddress, copiedPrologueBytes, prologueSize);

            // 4. Append a JMP from the trampoline (trampolineAddress + prologueSize)
            //    back to (targetFunction + prologueSize)
            PBYTE jmpLocationInTrampoline = static_cast<PBYTE>(trampolineAddress) + prologueSize;
            LPCVOID jumpBackTargetAddress = static_cast<LPCBYTE>(targetFunction) + prologueSize;

            // Calculate relative offset for the JMP
            // Offset = Destination - (Source + 5)
            // Destination = jumpBackTargetAddress
            // Source = jmpLocationInTrampoline (address of the 0xE9 byte)
            DWORD relativeOffset = (DWORD)jumpBackTargetAddress - ((DWORD)jmpLocationInTrampoline + JMP_REL32_SIZE);

            jmpLocationInTrampoline[0] = 0xE9; // JMP opcode
            memcpy(&jmpLocationInTrampoline[1], &relativeOffset, sizeof(DWORD));

            // 5. Finalize protection (usually PAGE_EXECUTE_READ)
            DWORD oldProtectTrampoline;
            if (!VirtualProtect(trampolineAddress, trampolineTotalSize, PAGE_EXECUTE_READ, &oldProtectTrampoline)) {
                std::cerr << "[TrampolineUtils::AllocateTrampoline] VirtualProtect (trampoline finalize) failed. Error: " << GetLastError() << std::endl;
                VirtualFree(trampolineAddress, 0, MEM_RELEASE); // Clean up allocated memory
                trampolineAddress = nullptr;
                return false;
            }

            FlushInstructionCache(GetCurrentProcess(), trampolineAddress, trampolineTotalSize);
            std::cout << "[TrampolineUtils::AllocateTrampoline] Trampoline allocated at " << trampolineAddress
                      << " for target " << targetFunction << ". Jumps back to " << jumpBackTargetAddress << std::endl;
            return true;
        }

        bool FreeTrampoline(LPVOID trampolineAddress) {
            if (!trampolineAddress) {
                // std::cout << "[TrampolineUtils::FreeTrampoline] No trampoline address provided or already freed." << std::endl;
                return true; // Nothing to free
            }
            if (VirtualFree(trampolineAddress, 0, MEM_RELEASE)) {
                std::cout << "[TrampolineUtils::FreeTrampoline] Trampoline memory at " << trampolineAddress << " freed." << std::endl;
                return true;
            } else {
                std::cerr << "[TrampolineUtils::FreeTrampoline] VirtualFree failed for address " << trampolineAddress
                          << ". Error: " << GetLastError() << std::endl;
                return false;
            }
        }

        bool WriteAbsoluteJump(
            LPVOID sourceAddress,       // Address to write the JMP instruction (target function)
            LPVOID destinationAddress,  // Address the JMP will go to (detour function)
            PBYTE originalBytesBuffer,  // Buffer to store the overwritten bytes (size = jumpInstructionSize)
            SIZE_T jumpInstructionSize) {

            if (!sourceAddress || !destinationAddress || !originalBytesBuffer || jumpInstructionSize < JMP_REL32_SIZE) {
                 std::cerr << "[TrampolineUtils::WriteAbsoluteJump] Invalid arguments." << std::endl;
                return false;
            }

            DWORD oldProtect;
            if (!VirtualProtect(sourceAddress, jumpInstructionSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                std::cerr << "[TrampolineUtils::WriteAbsoluteJump] VirtualProtect (Phase 1) failed. Error: " << GetLastError() << std::endl;
                return false;
            }

            // Save original bytes before overwriting
            memcpy(originalBytesBuffer, sourceAddress, jumpInstructionSize);

            // Write JMP instruction (relative jump for x86, 0xE9)
            // Offset = Destination - (Source + 5)
            PBYTE jmpInstructionTarget = static_cast<PBYTE>(sourceAddress);
            DWORD relativeOffset = (DWORD)destinationAddress - ((DWORD)sourceAddress + JMP_REL32_SIZE);

            jmpInstructionTarget[0] = 0xE9; // JMP opcode
            memcpy(&jmpInstructionTarget[1], &relativeOffset, sizeof(DWORD));

            // Fill remaining bytes with NOPs if jumpInstructionSize > JMP_REL32_SIZE (though typically it's exactly 5)
            for (SIZE_T i = JMP_REL32_SIZE; i < jumpInstructionSize; ++i) {
                jmpInstructionTarget[i] = 0x90; // NOP
            }

            if (!VirtualProtect(sourceAddress, jumpInstructionSize, oldProtect, &oldProtect)) {
                std::cerr << "[TrampolineUtils::WriteAbsoluteJump] VirtualProtect (Phase 2) failed. Error: " << GetLastError() << std::endl;
                // Attempt to restore original bytes if VP fails, though this is a bad state
                memcpy(sourceAddress, originalBytesBuffer, jumpInstructionSize);
                return false;
            }

            FlushInstructionCache(GetCurrentProcess(), sourceAddress, jumpInstructionSize);
            std::cout << "[TrampolineUtils::WriteAbsoluteJump] JMP written from " << sourceAddress << " to " << destinationAddress << std::endl;
            return true;
        }

        bool RestoreOriginalBytes(
            LPVOID targetAddress,
            PBYTE bytesBuffer,
            SIZE_T instructionSize) {

            if (!targetAddress || !bytesBuffer || instructionSize == 0) {
                std::cerr << "[TrampolineUtils::RestoreOriginalBytes] Invalid arguments." << std::endl;
                return false;
            }

            DWORD oldProtect;
            if (!VirtualProtect(targetAddress, instructionSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
                 std::cerr << "[TrampolineUtils::RestoreOriginalBytes] VirtualProtect (Phase 1) failed. Error: " << GetLastError() << std::endl;
                return false;
            }

            memcpy(targetAddress, bytesBuffer, instructionSize);

            if (!VirtualProtect(targetAddress, instructionSize, oldProtect, &oldProtect)) {
                std::cerr << "[TrampolineUtils::RestoreOriginalBytes] VirtualProtect (Phase 2) failed. Error: " << GetLastError() << std::endl;
                return false;
            }

            FlushInstructionCache(GetCurrentProcess(), targetAddress, instructionSize);
            std::cout << "[TrampolineUtils::RestoreOriginalBytes] Bytes restored at " << targetAddress << std::endl;
            return true;
        }

    } // namespace TrampolineUtils
} // namespace UndownUnlock
