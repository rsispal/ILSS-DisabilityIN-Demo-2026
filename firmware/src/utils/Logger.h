#pragma once

#include <string>
#include <cstdint>
#include <cstdarg>
#include <functional>
#include "../constants/Constants.h"

/**
 * Logger - USB serial (ESP_LOG) plus optional BLE notify fan-out.
 */
class Logger {
public:
    using BleLogSink = std::function<void(const char* line)>;

    Logger();
    ~Logger();

    void setLogLevel(LogLevel logLevel);
    LogLevel getLogLevel() const { return logLevel_; }

    /** Non-blocking sink for GATT Log characteristic (may drop under load). */
    void setBleLogSink(BleLogSink sink) { ble_sink_ = std::move(sink); }
    void clearBleLogSink() { ble_sink_ = nullptr; }

    void LOGI(const char* tag, const char* fmt, ...);
    void LOGD(const char* tag, const char* fmt, ...);
    void LOGW(const char* tag, const char* fmt, ...);
    void LOGE(const char* tag, const char* fmt, ...);

    void LOGIB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    void LOGDB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    void LOGWB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    void LOGEB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);

    static void sLOGI(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    static void sLOGD(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    static void sLOGW(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    static void sLOGE(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    static void sLOGIB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    static void sLOGDB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    static void sLOGWB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);
    static void sLOGEB(const char* tag, const uint8_t* data, const size_t size, const char* fmt, ...);

private:
    LogLevel logLevel_;
    BleLogSink ble_sink_;

    bool shouldLog(LogLevel level) const;
    void logMessage(const char* tag, LogLevel level, const char* fmt, va_list args) const;
    void logBinary(const char* tag, LogLevel level, const uint8_t* data, const size_t size, const char* fmt, va_list args) const;
    void fanOutBle(char level_ch, const char* tag, const char* msg) const;
};
