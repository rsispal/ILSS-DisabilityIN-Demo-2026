#pragma once

#include <string>

class Logger;
class LowLevel;
class State;

/**
 * Slim USB CLI for digital-twin boot configuration and hardware tests.
 */
class USBCLI {
    const char* TAG = "USBCLI";

public:
    USBCLI(Logger* logger, LowLevel* lowLevel, State* state);
    ~USBCLI() = default;

    void begin();
    void runConfigurationMode();

private:
    Logger* logger_;
    LowLevel* lowLevel_;
    State* state_;

    void write(const char* s);
    void writeln(const char* s);
    int readByte(uint8_t* out, int timeout_ms);
    std::string readline(int maxlen = 128, bool echo = true);

    void showStatus();
    void runTestMode();
    void runFactoryMode();
    void runFactoryReset();
};
