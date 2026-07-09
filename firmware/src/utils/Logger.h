#pragma once

#include <string>
#include <cstdint>
#include <cstdarg>
#include "../constants/Constants.h"

/**
 * Logger - Utility class for logging
 * 
 * ESP-IDF logging wrapper providing a consistent interface with Zephyr codebase.
 * Supports both instance methods and static methods (prefixed with 's').
 */
class Logger {
public:
    Logger();
    ~Logger();

    void setLogLevel(LogLevel logLevel);
    LogLevel getLogLevel() const { return logLevel_; }

    // Instance logging methods
    void LOGI(const char* tag, const char* fmt, ...);
    void LOGD(const char* tag, const char* fmt, ...);
    void LOGW(const char* tag, const char* fmt, ...);
    void LOGE(const char* tag, const char* fmt, ...);

    // Instance binary data logging
    void LOGIB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    void LOGDB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    void LOGWB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    void LOGEB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);

    // Static logging methods (always log, no level checking)
    static void sLOGI(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    static void sLOGD(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    static void sLOGW(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    static void sLOGE(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    
    // Static binary logging methods
    static void sLOGIB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    static void sLOGDB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    static void sLOGWB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    static void sLOGEB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);

private:
    LogLevel logLevel_;
    
    bool shouldLog(LogLevel level) const;
    void logMessage(const char* tag, LogLevel level, const char* fmt, va_list args) const;
    void logBinary(const char* tag, LogLevel level, const uint8_t* data, const size_t size, const char* fmt, va_list args) const;
};
