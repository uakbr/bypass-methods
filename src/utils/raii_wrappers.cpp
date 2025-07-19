#include "include/utils/raii_wrappers.h"
#include "include/utils/error_handler.h"
#include <sstream>
#include <iomanip>

namespace UndownUnlock::Utils {

// Implementation of RAII wrapper classes
// Most functionality is already implemented in the header file as inline functions
// This file contains additional implementation details and utility functions

namespace {
    // Helper function to format handle information for logging
    std::string format_handle_info(HANDLE handle, const std::string& operation) {
        std::ostringstream oss;
        oss << "Handle " << std::hex << std::setw(8) << std::setfill('0') 
            << reinterpret_cast<uintptr_t>(handle) << " " << operation;
        return oss.str();
    }

    // Helper function to check if handle is valid
    bool is_valid_handle(HANDLE handle) {
        return handle != INVALID_HANDLE_VALUE && handle != nullptr;
    }

    // Helper function to safely close handle
    void safe_close_handle(HANDLE handle) {
        if (is_valid_handle(handle)) {
            if (!CloseHandle(handle)) {
                DWORD error = GetLastError();
                LOG_WARNING("Failed to close handle", ErrorCategory::WINDOWS_API);
                LOG_WINDOWS_ERROR("CloseHandle failed", ErrorCategory::WINDOWS_API);
            }
        }
    }
}

// Additional utility functions for RAII wrappers

ScopedHandle create_scoped_handle(HANDLE handle) {
    return ScopedHandle(handle);
}

ScopedHandle create_scoped_handle_from_process(DWORD process_id, DWORD desired_access, 
                                              bool inherit_handle, DWORD flags) {
    HANDLE handle = OpenProcess(desired_access, inherit_handle, process_id);
    if (!is_valid_handle(handle)) {
        LOG_ERROR("Failed to open process", ErrorCategory::PROCESS);
        LOG_WINDOWS_ERROR("OpenProcess failed", ErrorCategory::PROCESS);
        return ScopedHandle();
    }
    return ScopedHandle(handle);
}

ScopedHandle create_scoped_handle_from_thread(DWORD thread_id, DWORD desired_access, 
                                             bool inherit_handle, DWORD flags) {
    HANDLE handle = OpenThread(desired_access, inherit_handle, thread_id);
    if (!is_valid_handle(handle)) {
        LOG_ERROR("Failed to open thread", ErrorCategory::THREAD);
        LOG_WINDOWS_ERROR("OpenThread failed", ErrorCategory::THREAD);
        return ScopedHandle();
    }
    return ScopedHandle(handle);
}

ScopedHandle create_scoped_handle_from_file(const std::string& filename, DWORD desired_access,
                                           DWORD share_mode, LPSECURITY_ATTRIBUTES security_attributes,
                                           DWORD creation_disposition, DWORD flags_and_attributes,
                                           HANDLE template_file) {
    HANDLE handle = CreateFileA(filename.c_str(), desired_access, share_mode, security_attributes,
                               creation_disposition, flags_and_attributes, template_file);
    if (!is_valid_handle(handle)) {
        LOG_ERROR("Failed to create/open file: " + filename, ErrorCategory::FILE_IO);
        LOG_WINDOWS_ERROR("CreateFile failed", ErrorCategory::FILE_IO);
        return ScopedHandle();
    }
    return ScopedHandle(handle);
}

ScopedHandle create_scoped_handle_from_mutex(const std::string& name, bool initial_owner,
                                            LPSECURITY_ATTRIBUTES security_attributes) {
    HANDLE handle = CreateMutexA(security_attributes, initial_owner, name.c_str());
    if (!is_valid_handle(handle)) {
        LOG_ERROR("Failed to create mutex: " + name, ErrorCategory::THREAD);
        LOG_WINDOWS_ERROR("CreateMutex failed", ErrorCategory::THREAD);
        return ScopedHandle();
    }
    return ScopedHandle(handle);
}

ScopedHandle create_scoped_handle_from_event(const std::string& name, bool manual_reset,
                                            bool initial_state, LPSECURITY_ATTRIBUTES security_attributes) {
    HANDLE handle = CreateEventA(security_attributes, manual_reset, initial_state, name.c_str());
    if (!is_valid_handle(handle)) {
        LOG_ERROR("Failed to create event: " + name, ErrorCategory::THREAD);
        LOG_WINDOWS_ERROR("CreateEvent failed", ErrorCategory::THREAD);
        return ScopedHandle();
    }
    return ScopedHandle(handle);
}

ScopedHandle create_scoped_handle_from_semaphore(const std::string& name, LONG initial_count,
                                                LONG maximum_count, LPSECURITY_ATTRIBUTES security_attributes) {
    HANDLE handle = CreateSemaphoreA(security_attributes, initial_count, maximum_count, name.c_str());
    if (!is_valid_handle(handle)) {
        LOG_ERROR("Failed to create semaphore: " + name, ErrorCategory::THREAD);
        LOG_WINDOWS_ERROR("CreateSemaphore failed", ErrorCategory::THREAD);
        return ScopedHandle();
    }
    return ScopedHandle(handle);
}

ScopedModuleHandle create_scoped_module_handle(const std::string& module_name) {
    HMODULE handle = LoadLibraryA(module_name.c_str());
    if (handle == nullptr) {
        LOG_ERROR("Failed to load module: " + module_name, ErrorCategory::WINDOWS_API);
        LOG_WINDOWS_ERROR("LoadLibrary failed", ErrorCategory::WINDOWS_API);
        return ScopedModuleHandle();
    }
    return ScopedModuleHandle(handle);
}

ScopedModuleHandle create_scoped_module_handle_from_address(LPVOID address) {
    HMODULE handle = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(address), &handle)) {
        LOG_ERROR("Failed to get module handle from address", ErrorCategory::WINDOWS_API);
        LOG_WINDOWS_ERROR("GetModuleHandleEx failed", ErrorCategory::WINDOWS_API);
        return ScopedModuleHandle();
    }
    return ScopedModuleHandle(handle);
}

ScopedDC create_scoped_dc_from_window(HWND window) {
    if (window == nullptr) {
        LOG_WARNING("Invalid window handle for DC creation", ErrorCategory::WINDOWS_API);
        return ScopedDC();
    }
    return ScopedDC(window);
}

ScopedDC create_scoped_dc_from_desktop() {
    HDC dc = GetDC(nullptr);
    if (dc == nullptr) {
        LOG_ERROR("Failed to get desktop DC", ErrorCategory::WINDOWS_API);
        LOG_WINDOWS_ERROR("GetDC failed", ErrorCategory::WINDOWS_API);
        return ScopedDC();
    }
    return ScopedDC(dc);
}

ScopedVirtualMemory create_scoped_virtual_memory(SIZE_T size, DWORD allocation_type,
                                                DWORD protection) {
    LPVOID address = VirtualAlloc(nullptr, size, allocation_type, protection);
    if (address == nullptr) {
        LOG_ERROR("Failed to allocate virtual memory", ErrorCategory::MEMORY);
        LOG_WINDOWS_ERROR("VirtualAlloc failed", ErrorCategory::MEMORY);
        return ScopedVirtualMemory();
    }
    return ScopedVirtualMemory(address, size);
}

ScopedVirtualMemory create_scoped_virtual_memory_at(LPVOID address, SIZE_T size,
                                                   DWORD allocation_type, DWORD protection) {
    LPVOID allocated_address = VirtualAlloc(address, size, allocation_type, protection);
    if (allocated_address == nullptr) {
        LOG_ERROR("Failed to allocate virtual memory at specified address", ErrorCategory::MEMORY);
        LOG_WINDOWS_ERROR("VirtualAlloc failed", ErrorCategory::MEMORY);
        return ScopedVirtualMemory();
    }
    return ScopedVirtualMemory(allocated_address, size);
}

// Utility functions for handle operations

bool duplicate_handle(HANDLE source_handle, HANDLE source_process, HANDLE target_process,
                     LPHANDLE target_handle, DWORD desired_access, bool inherit_handle,
                     DWORD options) {
    if (!DuplicateHandle(source_process, source_handle, target_process, target_handle,
                        desired_access, inherit_handle, options)) {
        LOG_ERROR("Failed to duplicate handle", ErrorCategory::WINDOWS_API);
        LOG_WINDOWS_ERROR("DuplicateHandle failed", ErrorCategory::WINDOWS_API);
        return false;
    }
    return true;
}

bool get_handle_information(HANDLE handle, PDWORD flags) {
    if (!GetHandleInformation(handle, flags)) {
        LOG_ERROR("Failed to get handle information", ErrorCategory::WINDOWS_API);
        LOG_WINDOWS_ERROR("GetHandleInformation failed", ErrorCategory::WINDOWS_API);
        return false;
    }
    return true;
}

bool set_handle_information(HANDLE handle, DWORD mask, DWORD flags) {
    if (!SetHandleInformation(handle, mask, flags)) {
        LOG_ERROR("Failed to set handle information", ErrorCategory::WINDOWS_API);
        LOG_WINDOWS_ERROR("SetHandleInformation failed", ErrorCategory::WINDOWS_API);
        return false;
    }
    return true;
}

// Utility functions for module operations

FARPROC get_proc_address(HMODULE module, const std::string& proc_name) {
    FARPROC proc = GetProcAddress(module, proc_name.c_str());
    if (proc == nullptr) {
        LOG_ERROR("Failed to get procedure address: " + proc_name, ErrorCategory::WINDOWS_API);
        LOG_WINDOWS_ERROR("GetProcAddress failed", ErrorCategory::WINDOWS_API);
    }
    return proc;
}

FARPROC get_proc_address_by_ordinal(HMODULE module, DWORD ordinal) {
    FARPROC proc = GetProcAddress(module, reinterpret_cast<LPCSTR>(ordinal));
    if (proc == nullptr) {
        LOG_ERROR("Failed to get procedure address by ordinal: " + std::to_string(ordinal), 
                  ErrorCategory::WINDOWS_API);
        LOG_WINDOWS_ERROR("GetProcAddress failed", ErrorCategory::WINDOWS_API);
    }
    return proc;
}

// Utility functions for DC operations

bool bit_blt(HDC dest_dc, int dest_x, int dest_y, int dest_width, int dest_height,
             HDC source_dc, int source_x, int source_y, DWORD raster_operation) {
    if (!BitBlt(dest_dc, dest_x, dest_y, dest_width, dest_height,
                source_dc, source_x, source_y, raster_operation)) {
        LOG_ERROR("Failed to perform BitBlt operation", ErrorCategory::WINDOWS_API);
        LOG_WINDOWS_ERROR("BitBlt failed", ErrorCategory::WINDOWS_API);
        return false;
    }
    return true;
}

bool stretch_blt(HDC dest_dc, int dest_x, int dest_y, int dest_width, int dest_height,
                HDC source_dc, int source_x, int source_y, int source_width, int source_height,
                DWORD raster_operation) {
    if (!StretchBlt(dest_dc, dest_x, dest_y, dest_width, dest_height,
                    source_dc, source_x, source_y, source_width, source_height,
                    raster_operation)) {
        LOG_ERROR("Failed to perform StretchBlt operation", ErrorCategory::WINDOWS_API);
        LOG_WINDOWS_ERROR("StretchBlt failed", ErrorCategory::WINDOWS_API);
        return false;
    }
    return true;
}

// Utility functions for virtual memory operations

bool virtual_protect(LPVOID address, SIZE_T size, DWORD new_protection, PDWORD old_protection) {
    if (!VirtualProtect(address, size, new_protection, old_protection)) {
        LOG_ERROR("Failed to change virtual memory protection", ErrorCategory::MEMORY);
        LOG_WINDOWS_ERROR("VirtualProtect failed", ErrorCategory::MEMORY);
        return false;
    }
    return true;
}

bool virtual_query(LPVOID address, PMEMORY_BASIC_INFORMATION buffer, SIZE_T length) {
    SIZE_T result = VirtualQuery(address, buffer, length);
    if (result == 0) {
        LOG_ERROR("Failed to query virtual memory information", ErrorCategory::MEMORY);
        LOG_WINDOWS_ERROR("VirtualQuery failed", ErrorCategory::MEMORY);
        return false;
    }
    return true;
}

bool virtual_lock(LPVOID address, SIZE_T size) {
    if (!VirtualLock(address, size)) {
        LOG_ERROR("Failed to lock virtual memory", ErrorCategory::MEMORY);
        LOG_WINDOWS_ERROR("VirtualLock failed", ErrorCategory::MEMORY);
        return false;
    }
    return true;
}

bool virtual_unlock(LPVOID address, SIZE_T size) {
    if (!VirtualUnlock(address, size)) {
        LOG_ERROR("Failed to unlock virtual memory", ErrorCategory::MEMORY);
        LOG_WINDOWS_ERROR("VirtualUnlock failed", ErrorCategory::MEMORY);
        return false;
    }
    return true;
}

// Utility functions for critical section operations

void initialize_critical_section_ex(CRITICAL_SECTION* critical_section, DWORD spin_count,
                                   DWORD flags) {
    if (!InitializeCriticalSectionEx(critical_section, spin_count, flags)) {
        LOG_ERROR("Failed to initialize critical section", ErrorCategory::THREAD);
        LOG_WINDOWS_ERROR("InitializeCriticalSectionEx failed", ErrorCategory::THREAD);
    }
}

bool try_enter_critical_section(CRITICAL_SECTION* critical_section) {
    return TryEnterCriticalSection(critical_section) != 0;
}

// Utility functions for synchronization objects

DWORD wait_for_single_object(HANDLE handle, DWORD timeout) {
    DWORD result = WaitForSingleObject(handle, timeout);
    if (result == WAIT_FAILED) {
        LOG_ERROR("Failed to wait for single object", ErrorCategory::THREAD);
        LOG_WINDOWS_ERROR("WaitForSingleObject failed", ErrorCategory::THREAD);
    }
    return result;
}

DWORD wait_for_multiple_objects(DWORD count, const HANDLE* handles, bool wait_all, DWORD timeout) {
    DWORD result = WaitForMultipleObjects(count, handles, wait_all, timeout);
    if (result == WAIT_FAILED) {
        LOG_ERROR("Failed to wait for multiple objects", ErrorCategory::THREAD);
        LOG_WINDOWS_ERROR("WaitForMultipleObjects failed", ErrorCategory::THREAD);
    }
    return result;
}

// Utility functions for handle validation

bool is_process_handle(HANDLE handle) {
    DWORD flags = 0;
    if (!GetHandleInformation(handle, &flags)) {
        return false;
    }
    return (flags & HANDLE_FLAG_PROTECT_FROM_CLOSE) == 0;
}

bool is_thread_handle(HANDLE handle) {
    DWORD flags = 0;
    if (!GetHandleInformation(handle, &flags)) {
        return false;
    }
    return (flags & HANDLE_FLAG_PROTECT_FROM_CLOSE) == 0;
}

bool is_file_handle(HANDLE handle) {
    DWORD file_type = GetFileType(handle);
    return file_type == FILE_TYPE_DISK || file_type == FILE_TYPE_CHAR || file_type == FILE_TYPE_PIPE;
}

// Utility functions for handle information

std::string get_handle_type_string(HANDLE handle) {
    DWORD file_type = GetFileType(handle);
    switch (file_type) {
        case FILE_TYPE_DISK:
            return "FILE_TYPE_DISK";
        case FILE_TYPE_CHAR:
            return "FILE_TYPE_CHAR";
        case FILE_TYPE_PIPE:
            return "FILE_TYPE_PIPE";
        case FILE_TYPE_REMOTE:
            return "FILE_TYPE_REMOTE";
        case FILE_TYPE_UNKNOWN:
            return "FILE_TYPE_UNKNOWN";
        default:
            return "UNKNOWN_TYPE";
    }
}

DWORD get_handle_flags(HANDLE handle) {
    DWORD flags = 0;
    if (!GetHandleInformation(handle, &flags)) {
        return 0;
    }
    return flags;
}

// Utility functions for handle debugging

void log_handle_operation(HANDLE handle, const std::string& operation) {
    if (is_valid_handle(handle)) {
        std::string handle_info = format_handle_info(handle, operation);
        LOG_INFO(handle_info, ErrorCategory::WINDOWS_API);
    } else {
        LOG_WARNING("Invalid handle for operation: " + operation, ErrorCategory::WINDOWS_API);
    }
}

void log_handle_creation(HANDLE handle, const std::string& creation_method) {
    log_handle_operation(handle, "created via " + creation_method);
}

void log_handle_cleanup(HANDLE handle, const std::string& cleanup_method) {
    log_handle_operation(handle, "cleaned up via " + cleanup_method);
}

} // namespace UndownUnlock::Utils 