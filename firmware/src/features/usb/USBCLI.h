#pragma once

#include <string>

class Logger;
class LowLevel;
class State;
class RGBLED;

/**
 * Slim USB CLI for digital-twin boot configuration and hardware tests.
 */
class USBCLI {
    const char* TAG = "USBCLI";

public:
    /** rgbLed must be the boot strip already owning the LED RMT GPIO — do not create a second RGBLED. */
    USBCLI(Logger* logger, LowLevel* lowLevel, State* state, RGBLED* rgbLed);
    ~USBCLI() = default;

    void begin();
    void runConfigurationMode();

private:
    Logger* logger_;
    LowLevel* lowLevel_;
    State* state_;
    RGBLED* rgbLed_;

    void write(const char* s);
    void writeln(const char* s);
    int readByte(uint8_t* out, int timeout_ms);
    std::string readline(int maxlen = 128, bool echo = true);

    void clearPendingInput();
    bool waitForExitKey();

    void showStatus();
    void runTestMode();
    void runI2cScan();
    void runFactoryMode();
    void runFactoryReset();

    void runBuzzerTestMenu();
    void runAllBuzzerPatterns();
    void testBuzzerPattern(const char* name, int pattern_type);
    void testSpeechPlayback();

    void runLEDPatternTest();

    void runHapticTestMenu();
    void testHapticPattern(uint8_t waveform, const char* name, uint32_t loop_period_ms);
    void testHapticDemo();

    void runCryptoTestMenu();

    void testPersonalAlert();
    void testFireAlarm();
};
