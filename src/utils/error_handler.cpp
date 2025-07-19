#include "include/utils/error_handler.h"
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <thread>
#include <dbghelp.h>

namespace UndownUnlock::Utils {

// Static member initialization
ErrorHandler* ErrorHandler::instance_ = nullptr;
std::mutex ErrorHandler::instance_mutex_;

// FileLogOutput implementation
FileLogOutput::FileLogOutput(const std::string& filename) : filename_(filename), file_(nullptr) {
    file_ = fopen(filename.c_str(), "a");
    if (!file_) {
        // Fallback to stderr if file creation fails
        fprintf(stderr, "Failed to open log file: %s\n", filename.c_str());
    }
}

FileLogOutput::~FileLogOutput() {
    if (file_) {
        fclose(file_);
    }
}

void FileLogOutput::write(const ErrorInfo& error) {
    if (!file_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::string formatted = format_error(error);
    fprintf(file_, "%s\n", formatted.c_str());
    fflush(file_);
}

void FileLogOutput::flush() {
    if (file_) {
        fflush(file_);
    }
}

std::string FileLogOutput::format_error(const ErrorInfo& error) {
    std::ostringstream oss;
    oss << "[" << timestamp_to_string(error.timestamp) << "] "
        << "[" << severity_to_string(error.severity) << "] "
        << "[" << category_to_string(error.category) << "] "
        << error.message;
    
    if (!error.function.empty()) {
        oss << " (Function: " << error.function;
        if (!error.file.empty()) {
            oss << ", File: " << error.file << ":" << error.line;
        }
        oss << ")";
    }
    
    if (error.windows_error != 0) {
        oss << " [Windows Error: " << error.windows_error << " - " 
            << get_windows_error_string(error.windows_error) << "]";
    }
    
    if (!error.context.empty()) {
        oss << " [Context: " << error.context << "]";
    }
    
    if (!error.stack_trace.empty()) {
        oss << "\nStack Trace:\n" << error.stack_trace;
    }
    
    return oss.str();
}

// ConsoleLogOutput implementation
ConsoleLogOutput::ConsoleLogOutput(bool use_colors) : use_colors_(use_colors) {}

void ConsoleLogOutput::write(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (use_colors_) {
        set_console_color(error.severity);
    }
    
    std::string formatted = format_error(error);
    printf("%s\n", formatted.c_str());
    
    if (use_colors_) {
        reset_console_color();
    }
}

void ConsoleLogOutput::flush() {
    fflush(stdout);
}

std::string ConsoleLogOutput::format_error(const ErrorInfo& error) {
    std::ostringstream oss;
    oss << "[" << timestamp_to_string(error.timestamp) << "] "
        << "[" << severity_to_string(error.severity) << "] "
        << "[" << category_to_string(error.category) << "] "
        << error.message;
    
    if (!error.function.empty()) {
        oss << " (Function: " << error.function;
        if (!error.file.empty()) {
            oss << ", File: " << error.file << ":" << error.line;
        }
        oss << ")";
    }
    
    if (error.windows_error != 0) {
        oss << " [Windows Error: " << error.windows_error << " - " 
            << get_windows_error_string(error.windows_error) << "]";
    }
    
    if (!error.context.empty()) {
        oss << " [Context: " << error.context << "]";
    }
    
    return oss.str();
}

void ConsoleLogOutput::set_console_color(ErrorSeverity severity) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return;
    
    WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Default white
    
    switch (severity) {
        case ErrorSeverity::DEBUG:
            color = FOREGROUND_INTENSITY; // Bright white
            break;
        case ErrorSeverity::INFO:
            color = FOREGROUND_GREEN | FOREGROUND_INTENSITY; // Bright green
            break;
        case ErrorSeverity::WARNING:
            color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; // Bright yellow
            break;
        case ErrorSeverity::ERROR:
            color = FOREGROUND_RED | FOREGROUND_INTENSITY; // Bright red
            break;
        case ErrorSeverity::CRITICAL:
        case ErrorSeverity::FATAL:
            color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | BACKGROUND_RED; // Red background
            break;
    }
    
    SetConsoleTextAttribute(hConsole, color);
}

void ConsoleLogOutput::reset_console_color() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }
}

// EventLogOutput implementation
EventLogOutput::EventLogOutput(const std::string& source_name) : event_source_(nullptr) {
    event_source_ = RegisterEventSourceA(nullptr, source_name.c_str());
    if (!event_source_) {
        fprintf(stderr, "Failed to register event source: %s\n", source_name.c_str());
    }
}

EventLogOutput::~EventLogOutput() {
    if (event_source_) {
        DeregisterEventSource(event_source_);
    }
}

void EventLogOutput::write(const ErrorInfo& error) {
    if (!event_source_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    WORD event_type = get_event_type(error.severity);
    std::string formatted = format_error(error);
    
    const char* strings[] = { formatted.c_str() };
    ReportEventA(event_source_, event_type, 0, 0, nullptr, 1, 0, strings, nullptr);
}

void EventLogOutput::flush() {
    // Event log doesn't need explicit flushing
}

WORD EventLogOutput::get_event_type(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::DEBUG:
        case ErrorSeverity::INFO:
            return EVENTLOG_INFORMATION_TYPE;
        case ErrorSeverity::WARNING:
            return EVENTLOG_WARNING_TYPE;
        case ErrorSeverity::ERROR:
        case ErrorSeverity::CRITICAL:
        case ErrorSeverity::FATAL:
            return EVENTLOG_ERROR_TYPE;
        default:
            return EVENTLOG_INFORMATION_TYPE;
    }
}

std::string EventLogOutput::format_error(const ErrorInfo& error) {
    std::ostringstream oss;
    oss << "[" << severity_to_string(error.severity) << "] "
        << "[" << category_to_string(error.category) << "] "
        << error.message;
    
    if (!error.function.empty()) {
        oss << " (Function: " << error.function << ")";
    }
    
    if (error.windows_error != 0) {
        oss << " [Windows Error: " << error.windows_error << "]";
    }
    
    return oss.str();
}

// ErrorHandler implementation
ErrorHandler::ErrorHandler() : min_severity_(ErrorSeverity::INFO), enabled_(true),
                              error_count_(0), warning_count_(0) {
    last_error_time_ = std::chrono::system_clock::now();
}

ErrorHandler::~ErrorHandler() {
    flush_all_outputs();
}

ErrorHandler& ErrorHandler::get_instance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = new ErrorHandler();
    }
    return *instance_;
}

void ErrorHandler::initialize() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = new ErrorHandler();
    }
}

void ErrorHandler::shutdown() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}

void ErrorHandler::set_min_severity(ErrorSeverity severity) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_severity_ = severity;
}

void ErrorHandler::set_enabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
}

void ErrorHandler::add_output(std::unique_ptr<ILogOutput> output) {
    std::lock_guard<std::mutex> lock(mutex_);
    outputs_.push_back(std::move(output));
}

void ErrorHandler::set_global_callback(std::function<void(const ErrorInfo&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    global_error_callback_ = callback;
}

void ErrorHandler::report_error(ErrorSeverity severity, ErrorCategory category,
                               const std::string& message, const std::string& function,
                               const std::string& file, int line, DWORD windows_error,
                               const std::string& context) {
    if (!enabled_ || severity < min_severity_) {
        return;
    }
    
    ErrorInfo error;
    error.severity = severity;
    error.category = category;
    error.message = message;
    error.function = function;
    error.file = file;
    error.line = line;
    error.windows_error = windows_error;
    error.context = context;
    error.timestamp = std::chrono::system_clock::now();
    error.stack_trace = get_stack_trace();
    
    report_error(error);
}

void ErrorHandler::report_error(const ErrorInfo& error) {
    if (!enabled_ || error.severity < min_severity_) {
        return;
    }
    
    // Update statistics
    if (error.severity >= ErrorSeverity::ERROR) {
        error_count_.fetch_add(1);
    } else if (error.severity == ErrorSeverity::WARNING) {
        warning_count_.fetch_add(1);
    }
    
    last_error_time_ = error.timestamp;
    
    // Write to outputs
    write_to_outputs(error);
    
    // Call global callback
    if (global_error_callback_) {
        global_error_callback_(error);
    }
    
    // Handle recovery strategies
    handle_error(error);
}

void ErrorHandler::debug(const std::string& message, ErrorCategory category,
                        const std::string& function, const std::string& file, int line) {
    report_error(ErrorSeverity::DEBUG, category, message, function, file, line);
}

void ErrorHandler::info(const std::string& message, ErrorCategory category,
                       const std::string& function, const std::string& file, int line) {
    report_error(ErrorSeverity::INFO, category, message, function, file, line);
}

void ErrorHandler::warning(const std::string& message, ErrorCategory category,
                          const std::string& function, const std::string& file, int line) {
    report_error(ErrorSeverity::WARNING, category, message, function, file, line);
}

void ErrorHandler::error(const std::string& message, ErrorCategory category,
                        const std::string& function, const std::string& file, int line,
                        DWORD windows_error) {
    report_error(ErrorSeverity::ERROR, category, message, function, file, line, windows_error);
}

void ErrorHandler::critical(const std::string& message, ErrorCategory category,
                           const std::string& function, const std::string& file, int line,
                           DWORD windows_error) {
    report_error(ErrorSeverity::CRITICAL, category, message, function, file, line, windows_error);
}

void ErrorHandler::fatal(const std::string& message, ErrorCategory category,
                        const std::string& function, const std::string& file, int line,
                        DWORD windows_error) {
    report_error(ErrorSeverity::FATAL, category, message, function, file, line, windows_error);
}

void ErrorHandler::add_recovery_strategy(const RecoveryStrategy& strategy) {
    std::lock_guard<std::mutex> lock(mutex_);
    recovery_strategies_.push_back(strategy);
}

RecoveryAction ErrorHandler::handle_error(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& strategy : recovery_strategies_) {
        if (strategy.condition && strategy.condition(error)) {
            if (strategy.custom_handler) {
                strategy.custom_handler(error);
            }
            return strategy.action;
        }
    }
    
    return RecoveryAction::NONE;
}

std::string ErrorHandler::get_windows_error_string(DWORD error_code) {
    if (error_code == 0) return "No error";
    
    LPSTR message_buffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);
    
    if (size == 0 || !message_buffer) {
        return "Unknown error code: " + std::to_string(error_code);
    }
    
    std::string message(message_buffer);
    LocalFree(message_buffer);
    
    // Remove trailing newlines
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    
    return message;
}

std::string ErrorHandler::get_stack_trace() {
    // This is a simplified stack trace implementation
    // In a production environment, you'd want to use a more robust solution
    std::ostringstream oss;
    oss << "Stack trace not available in this build";
    return oss.str();
}

void ErrorHandler::flush_all_outputs() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& output : outputs_) {
        output->flush();
    }
}

void ErrorHandler::write_to_outputs(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& output : outputs_) {
        try {
            output->write(error);
        } catch (...) {
            // Don't let output errors propagate
        }
    }
}

std::string ErrorHandler::severity_to_string(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::DEBUG: return "DEBUG";
        case ErrorSeverity::INFO: return "INFO";
        case ErrorSeverity::WARNING: return "WARNING";
        case ErrorSeverity::ERROR: return "ERROR";
        case ErrorSeverity::CRITICAL: return "CRITICAL";
        case ErrorSeverity::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string ErrorHandler::category_to_string(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::GENERAL: return "GENERAL";
        case ErrorCategory::MEMORY: return "MEMORY";
        case ErrorCategory::HOOK: return "HOOK";
        case ErrorCategory::DIRECTX: return "DIRECTX";
        case ErrorCategory::WINDOWS_API: return "WINDOWS_API";
        case ErrorCategory::NETWORK: return "NETWORK";
        case ErrorCategory::FILE_IO: return "FILE_IO";
        case ErrorCategory::THREAD: return "THREAD";
        case ErrorCategory::PROCESS: return "PROCESS";
        case ErrorCategory::SECURITY: return "SECURITY";
        default: return "UNKNOWN";
    }
}

std::string ErrorHandler::timestamp_to_string(const std::chrono::system_clock::time_point& timestamp) {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// ErrorUtils implementation
namespace ErrorUtils {
    std::string format_windows_error(DWORD error_code) {
        return ErrorHandler::get_instance().get_windows_error_string(error_code);
    }
    
    std::string get_last_windows_error_string() {
        return format_windows_error(GetLastError());
    }
    
    DWORD get_last_windows_error() {
        return GetLastError();
    }
    
    std::string get_stack_trace() {
        return ErrorHandler::get_instance().get_stack_trace();
    }
    
    bool is_critical_error(DWORD windows_error) {
        // Define which Windows errors are considered critical
        switch (windows_error) {
            case ERROR_ACCESS_DENIED:
            case ERROR_INVALID_HANDLE:
            case ERROR_OUTOFMEMORY:
            case ERROR_NOT_ENOUGH_MEMORY:
            case ERROR_INVALID_ADDRESS:
            case ERROR_STACK_OVERFLOW:
                return true;
            default:
                return false;
        }
    }
    
    bool should_retry(DWORD windows_error) {
        // Define which Windows errors should trigger a retry
        switch (windows_error) {
            case ERROR_BUSY:
            case ERROR_DEVICE_BUSY:
            case ERROR_SERVICE_BUSY:
            case ERROR_TIMEOUT:
            case ERROR_SEM_TIMEOUT:
            case ERROR_IO_INCOMPLETE:
            case ERROR_IO_PENDING:
                return true;
            default:
                return false;
        }
    }
    
    std::chrono::milliseconds get_retry_delay(DWORD windows_error) {
        // Define retry delays based on error type
        switch (windows_error) {
            case ERROR_BUSY:
            case ERROR_DEVICE_BUSY:
                return std::chrono::milliseconds(100);
            case ERROR_TIMEOUT:
            case ERROR_SEM_TIMEOUT:
                return std::chrono::milliseconds(1000);
            case ERROR_IO_INCOMPLETE:
            case ERROR_IO_PENDING:
                return std::chrono::milliseconds(50);
            default:
                return std::chrono::milliseconds(500);
        }
    }
}

} // namespace UndownUnlock::Utils 