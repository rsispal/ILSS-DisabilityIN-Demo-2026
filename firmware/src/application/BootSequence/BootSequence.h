#pragma once

#include "../../utils/Logger.h"

class State;
class LowLevel;
class RGBLED;
class Buzzer;
class Haptics;

/**
 * BootSequence — CLI wait gate + Honeywell power-up flourish.
 * Keeps main.cpp free of lowlevel driver calls.
 */
class BootSequence {
    const char* TAG = "BootSequence";

public:
    BootSequence(Logger* logger, State* state, LowLevel* lowLevel,
                 RGBLED* rgbLed, Buzzer* buzzer, Haptics* haptics);

    /**
     * Quiet 2s USB CLI window. Returns true if a key was pressed (enter config).
     * Shows blue single-flash while waiting.
     */
    bool waitForConfigKey(int timeout_ms = 2000);

    /** White/red star twinkle + soft haptics + ascending beep-beep-BEEP (~1.8s). */
    void playPowerUpCue();

private:
    Logger* logger_;
    State* state_;
    LowLevel* lowLevel_;
    RGBLED* rgbLed_;
    Buzzer* buzzer_;
    Haptics* haptics_;
};
