#pragma once

#include "../../utils/Logger.h"
#include "../../state/State.h"
#include "../../lowlevel/LowLevel.h"
#include "../../protocol/TwinState.h"

/**
 * Haptics — feature wrapper over DRV2605L.
 * Callers use TwinHaptic / semantic helpers; waveform IDs stay inside this class.
 */
class Haptics {
    const char* TAG = "Haptics";

public:
    Haptics(State* state, LowLevel* lowLevel);
    ~Haptics() = default;

    bool begin();
    bool isReady() const;

    void stop();

    /** Soft bump (DRV #7) — gentle presence / power-up. */
    void playSoftBump();

    /** Strong buzz (DRV #14) — continuous / test. */
    void playStrongBuzz();

    /** Apply a twin haptic pattern (one-shot or start of a looped pattern). */
    void play(ilss::TwinHaptic pattern);

    /**
     * Re-fire the current looped pattern if due.
     * Call ~50Hz from the indication process loop.
     * @return period_ms for the active pattern (0 = one-shot / off).
     */
    uint32_t loopPeriodMs(ilss::TwinHaptic pattern) const;
    void retrigger(ilss::TwinHaptic pattern);

private:
    State* state_ = nullptr;
    LowLevel* lowLevel_ = nullptr;
    Logger logger_;
    bool initialized_ = false;

    void playWaveform(uint8_t id);
};
