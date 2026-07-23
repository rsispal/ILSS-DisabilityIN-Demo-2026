#pragma once

#include "../../protocol/TwinState.h"
#include "../../utils/Logger.h"

class State;
class RGBLED;
class Buzzer;
class Haptics;

/**
 * Maps TwinState to LED / buzzer / haptic hardware with web-matched timings.
 * Also owns link-presence LED modes (unpaired / pairing / connected flash).
 */
class IndicationController {
    const char* TAG = "Indication";

public:
    enum class LinkLedMode : uint8_t {
        Unpaired,   // Blue single-pixel flash every 3s
        Pairing,    // Blue chase while GATT connected but not yet paired
        Connected,  // After pair_ok green flash, then twin state / idle
    };

    IndicationController(Logger* logger, State* state,
                         RGBLED* rgbLed, Buzzer* buzzer, Haptics* haptics);
    ~IndicationController() = default;

    bool begin();
    void process();  // call ~50Hz from led/buzzer tasks or app loop

    /** Apply twin state. Returns false if rejected (mutual exclusion / alert active). */
    bool apply(const ilss::TwinState& desired, ilss::TwinNakReason* reason = nullptr);

    const ilss::TwinState& current() const { return current_; }

    void goIdle();

    /** Hard stand-down: stop LED/buzzer/haptics, clear alerts, unpaired heartbeat. */
    void standDownToUnpaired();

    /** Connection lifecycle LED cues (deferred from NimBLE host via app events). */
    void setLinkLedMode(LinkLedMode mode);
    LinkLedMode linkLedMode() const { return link_mode_; }

    /** One-shot green flash after successful pair, then idle. */
    void showConnectedFlash();

    /**
     * Brief green brightness bump on heartbeat poll (matches web hb-pulse ~550ms).
     * No-op during fire/personal alerts or when not in Connected link mode.
     */
    void showHeartbeatPulse();

private:
    // Physical strip brightness ladder (web twin may report different idle %).
    static constexpr uint8_t kHwIdleGreenPct = 10;   // paired, no alert (physical quiescent)
    static constexpr uint8_t kHwUnpairedPct = 20;    // blue single flash
    static constexpr uint8_t kHwPairingPct = 30;     // blue chase
    static constexpr uint8_t kHwHeartbeatPulsePct = 50;  // gentle alive bump
    static constexpr uint32_t kHeartbeatPulseMs = 550;   // match web hb-pulse
    // Pair flash + fire/personal stay at 100% (attention).

    Logger* logger_;
    State* state_;
    RGBLED* rgbLed_;
    Buzzer* buzzer_;
    Haptics* haptics_;
    ilss::TwinState current_;
    LinkLedMode link_mode_ = LinkLedMode::Unpaired;

    uint32_t last_haptic_ms_ = 0;
    uint32_t haptic_period_ms_ = 0;
    uint32_t connected_flash_until_ms_ = 0;
    /** Defer I2C haptic start off NimBLE GATT/host task onto led_task. */
    bool haptic_start_pending_ = false;

    void driveLed(const ilss::TwinState& s);
    void driveBuzzer(const ilss::TwinState& s);
    void driveHaptic(const ilss::TwinState& s);
    void updateHapticPulse();
    void applyLinkLed();
    void resumeAfterLinkCue();
    static uint32_t nowMs();
};
