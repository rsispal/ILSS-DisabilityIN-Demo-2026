#pragma once

#include "../../protocol/TwinState.h"
#include "../../utils/Logger.h"

class State;
class LowLevel;
class RGBLED;
class Buzzer;

/**
 * Maps TwinState to LED / buzzer / haptic hardware with web-matched timings.
 */
class IndicationController {
    const char* TAG = "Indication";

public:
    IndicationController(Logger* logger, State* state, LowLevel* lowLevel,
                         RGBLED* rgbLed, Buzzer* buzzer);
    ~IndicationController() = default;

    bool begin();
    void process();  // call ~50Hz from led/buzzer tasks or app loop

    /** Apply twin state. Returns false if rejected (mutual exclusion / alert active). */
    bool apply(const ilss::TwinState& desired, ilss::TwinNakReason* reason = nullptr);

    const ilss::TwinState& current() const { return current_; }

    void goIdle();

private:
    Logger* logger_;
    State* state_;
    LowLevel* lowLevel_;
    RGBLED* rgbLed_;
    Buzzer* buzzer_;
    ilss::TwinState current_;

    uint32_t last_haptic_ms_ = 0;
    uint32_t haptic_period_ms_ = 0;

    void driveLed(const ilss::TwinState& s);
    void driveBuzzer(const ilss::TwinState& s);
    void driveHaptic(const ilss::TwinState& s);
    void updateHapticPulse();
    static uint32_t nowMs();
};
