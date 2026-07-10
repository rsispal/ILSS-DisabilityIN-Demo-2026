#pragma once

#include <cstdint>
#include <cstring>

namespace ilss {

enum class TwinAlert : uint8_t { None = 0, Personal = 1, Fire = 2 };

enum class TwinColor : uint8_t {
    Red = 0, Green = 1, Blue = 2, Teal = 3, Purple = 4, Yellow = 5, Orange = 6
};

enum class TwinLed : uint8_t {
    Solid = 0, Flash = 1, Pulse = 2, Double = 3, Alt = 4, Half = 5, Chase = 6, Off = 7
};

enum class TwinHaptic : uint8_t {
    Off = 0,
    Click = 1,          // Strong Click 100% (DRV #1) — one-shot
    ShortPulse = 2,     // Sharp Click 100% (DRV #4) — one-shot
    LongPulse = 3,      // Buzz 1 100% (DRV #47) — one-shot
    ShortPulses = 4,    // Double Click 100% (DRV #10), firmware-looped
    LongPulses = 5,     // Alert 1000ms (DRV #16), looped — fire + personal
    Continuous = 6,     // Strong Buzz 100% (DRV #14), looped
    Ramp = 7,           // Ramp Up Long Smooth (DRV #82), looped
};

enum class TwinBuzzer : uint8_t {
    Alternating = 0,
    Silent = 1,
    BsSweep = 2,
    BsFastSweep = 3,
    LfBuzz = 4,
    Siren = 5,
    Code3Beep = 6,
    Code3Sweep = 7,
    Code3Siren = 8,
    Off = 9
};

static constexpr uint8_t TWIN_FLAG_ADVANCED = 0x01;

struct TwinState {
    TwinAlert alert = TwinAlert::None;
    TwinColor color = TwinColor::Green;
    TwinLed led = TwinLed::Solid;
    TwinHaptic haptic = TwinHaptic::Off;
    TwinBuzzer buzzer = TwinBuzzer::Silent;
    uint8_t flags = 0;
    /** LED brightness 0–100 in steps of 10. 0 = off (pattern still selected). */
    uint8_t brightness = 100;

    static constexpr size_t kSize = 7;

    void pack(uint8_t out[kSize]) const {
        out[0] = static_cast<uint8_t>(alert);
        out[1] = static_cast<uint8_t>(color);
        out[2] = static_cast<uint8_t>(led);
        out[3] = static_cast<uint8_t>(haptic);
        out[4] = static_cast<uint8_t>(buzzer);
        out[5] = flags;
        out[6] = brightness;
    }

    static TwinState unpack(const uint8_t* in, size_t len) {
        TwinState s;
        if (!in || len < 6) return s;
        s.alert = static_cast<TwinAlert>(in[0]);
        s.color = static_cast<TwinColor>(in[1]);
        s.led = static_cast<TwinLed>(in[2]);
        s.haptic = static_cast<TwinHaptic>(in[3]);
        s.buzzer = static_cast<TwinBuzzer>(in[4]);
        s.flags = in[5];
        // Backward compatible: 6-byte payloads default to 100%.
        s.brightness = (len >= 7) ? in[6] : 100;
        if (s.brightness > 100) s.brightness = 100;
        s.brightness = static_cast<uint8_t>((s.brightness / 10) * 10);
        return s;
    }

    static TwinState idle() {
        TwinState s;
        s.alert = TwinAlert::None;
        s.color = TwinColor::Green;
        s.led = TwinLed::Solid;
        s.haptic = TwinHaptic::Off;
        s.buzzer = TwinBuzzer::Silent;
        s.flags = 0;
        s.brightness = 40;
        return s;
    }

    static TwinState fire() {
        TwinState s;
        s.alert = TwinAlert::Fire;
        s.color = TwinColor::Red;
        s.led = TwinLed::Double;
        s.haptic = TwinHaptic::LongPulses;
        s.buzzer = TwinBuzzer::Code3Sweep;
        s.brightness = 100;
        return s;
    }

    static TwinState personal() {
        TwinState s;
        s.alert = TwinAlert::Personal;
        s.color = TwinColor::Purple;
        s.led = TwinLed::Pulse;
        s.haptic = TwinHaptic::LongPulses;
        s.buzzer = TwinBuzzer::Code3Siren;
        s.brightness = 100;
        return s;
    }

    bool equals(const TwinState& o) const {
        return alert == o.alert && color == o.color && led == o.led &&
               haptic == o.haptic && buzzer == o.buzzer && flags == o.flags &&
               brightness == o.brightness;
    }
};

enum class TwinNakReason : uint8_t {
    None = 0,
    MutualExclusion = 1,
    AlertActive = 2,
    NotPaired = 3,
    InvalidPayload = 4,
};

// Application codes
static constexpr uint8_t APP_CODE_TWIN_STATE = 0x01;
static constexpr uint8_t APP_CODE_HEARTBEAT = 0x02;
/** Side-button UI cue: Data[0]=0 left / 1 right, Data[1]=0 press (momentary). */
static constexpr uint8_t APP_CODE_BUTTON = 0x10;
static constexpr uint8_t APP_CODE_PAIRING = 0x40;

static constexpr uint8_t BUTTON_SIDE_LEFT = 0;
static constexpr uint8_t BUTTON_SIDE_RIGHT = 1;
static constexpr uint8_t BUTTON_ACTION_PRESS = 0;

}  // namespace ilss
