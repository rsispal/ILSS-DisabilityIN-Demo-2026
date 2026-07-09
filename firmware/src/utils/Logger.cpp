#include "Logger.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

Logger::Logger() : logLevel_(LogLevel::INFO) {
}

Logger::~Logger() {
}

void Logger::setLogLevel(LogLevel logLevel) {
    logLevel_ = logLevel;
}

bool Logger::shouldLog(LogLevel level) const {
    return static_cast<uint8_t>(logLevel_) >= static_cast<uint8_t>(level);
}

void Logger::logMessage(const char* tag, LogLevel level, const char* fmt, va_list args) const {
    if (!shouldLog(level)) return;
    
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    
    switch (level) {
        case LogLevel::DEBUG:
            ESP_LOGD(tag, "%s", buffer);
            break;
        case LogLevel::INFO:
            ESP_LOGI(tag, "%s", buffer);
            break;
        case LogLevel::WARNING:
            ESP_LOGW(tag, "%s", buffer);
            break;
        case LogLevel::ERROR:
            ESP_LOGE(tag, "%s", buffer);
            break;
        default:
            break;
    }
}

void Logger::logBinary(const char* tag, LogLevel level, const uint8_t* data, const size_t size, const char* fmt, va_list args) const {
    if (!shouldLog(level)) return;
    
    char msg_buffer[256];
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    
    ESP_LOGI(tag, "%s [%zu bytes]", msg_buffer, size);
    ESP_LOG_BUFFER_HEX(tag, data, size);
}

// Instance logging methods
void Logger::LOGI(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logMessage(tag, LogLevel::INFO, fmt, args);
    va_end(args);
}

void Logger::LOGD(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logMessage(tag, LogLevel::DEBUG, fmt, args);
    va_end(args);
}

void Logger::LOGW(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logMessage(tag, LogLevel::WARNING, fmt, args);
    va_end(args);
}

void Logger::LOGE(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logMessage(tag, LogLevel::ERROR, fmt, args);
    va_end(args);
}

void Logger::LOGIB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logBinary(tag, LogLevel::INFO, data, size, fmt, args);
    va_end(args);
}

void Logger::LOGDB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logBinary(tag, LogLevel::DEBUG, data, size, fmt, args);
    va_end(args);
}

void Logger::LOGWB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logBinary(tag, LogLevel::WARNING, data, size, fmt, args);
    va_end(args);
}

void Logger::LOGEB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logBinary(tag, LogLevel::ERROR, data, size, fmt, args);
    va_end(args);
}

// Static logging methods (always log, no level checking)
void Logger::sLOGI(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    ESP_LOGI(tag, "%s", buffer);
    va_end(args);
}

void Logger::sLOGD(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    ESP_LOGD(tag, "%s", buffer);
    va_end(args);
}

void Logger::sLOGW(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    ESP_LOGW(tag, "%s", buffer);
    va_end(args);
}

void Logger::sLOGE(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    ESP_LOGE(tag, "%s", buffer);
    va_end(args);
}

void Logger::sLOGIB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msg_buffer[256];
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    ESP_LOGI(tag, "%s [%zu bytes]", msg_buffer, size);
    ESP_LOG_BUFFER_HEX(tag, data, size);
    va_end(args);
}

void Logger::sLOGDB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msg_buffer[256];
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    ESP_LOGD(tag, "%s [%zu bytes]", msg_buffer, size);
    ESP_LOG_BUFFER_HEX(tag, data, size);
    va_end(args);
}

void Logger::sLOGWB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msg_buffer[256];
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    ESP_LOGW(tag, "%s [%zu bytes]", msg_buffer, size);
    ESP_LOG_BUFFER_HEX(tag, data, size);
    va_end(args);
}

void Logger::sLOGEB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msg_buffer[256];
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    ESP_LOGE(tag, "%s [%zu bytes]", msg_buffer, size);
    ESP_LOG_BUFFER_HEX(tag, data, size);
    va_end(args);
}
