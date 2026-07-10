#include "Haptics.h"
#include "../../lowlevel/haptics/DRV2605Driver.h"

Haptics::Haptics(State* state, LowLevel* lowLevel)
    : state_(state), lowLevel_(lowLevel) {
    logger_.setLogLevel(LogLevel::INFO);
}

bool Haptics::begin() {
    if (!lowLevel_) return false;
    // Driver is started by LowLevel::begin(); just verify readiness.
    initialized_ = lowLevel_->get_haptics().isReady();
    if (!initialized_) {
        logger_.LOGW(TAG, "DRV2605 not ready");
    }
    return initialized_;
}

bool Haptics::isReady() const {
    return initialized_ && lowLevel_ && lowLevel_->get_haptics().isReady();
}

void Haptics::stop() {
    if (!lowLevel_) return;
    lowLevel_->get_haptics().stop();
}

void Haptics::playWaveform(uint8_t id) {
    if (!lowLevel_) return;
    if (state_ && !state_->getEnableHaptics()) return;
    lowLevel_->get_haptics().play_pattern(id);
}

void Haptics::playSoftBump() {
    playWaveform(7);  // Soft Bump 100%
}

void Haptics::playStrongBuzz() {
    playWaveform(14);  // Strong Buzz 100%
}

uint32_t Haptics::loopPeriodMs(ilss::TwinHaptic pattern) const {
    switch (pattern) {
        case ilss::TwinHaptic::ShortPulses: return 700;
        case ilss::TwinHaptic::LongPulses:  return 1400;
        case ilss::TwinHaptic::Continuous:  return 60;
        case ilss::TwinHaptic::Ramp:        return 1600;
        default: return 0;  // one-shot / off
    }
}

void Haptics::play(ilss::TwinHaptic pattern) {
    if (!lowLevel_) return;
    stop();
    switch (pattern) {
        case ilss::TwinHaptic::Off:
            break;
        case ilss::TwinHaptic::Click:
            playWaveform(1);   // Strong Click
            break;
        case ilss::TwinHaptic::ShortPulse:
            playWaveform(4);   // Sharp Click
            break;
        case ilss::TwinHaptic::LongPulse:
            playWaveform(47);  // Buzz 1
            break;
        case ilss::TwinHaptic::ShortPulses:
            playWaveform(10);  // Double Click
            break;
        case ilss::TwinHaptic::LongPulses:
            playWaveform(16);  // Alert 1000ms
            break;
        case ilss::TwinHaptic::Continuous:
            playWaveform(14);  // Strong Buzz
            break;
        case ilss::TwinHaptic::Ramp:
            playWaveform(82);  // Ramp Up Long Smooth
            break;
    }
}

void Haptics::retrigger(ilss::TwinHaptic pattern) {
    switch (pattern) {
        case ilss::TwinHaptic::ShortPulses:
            playWaveform(10);
            break;
        case ilss::TwinHaptic::LongPulses:
            playWaveform(16);
            break;
        case ilss::TwinHaptic::Continuous:
            playWaveform(14);
            break;
        case ilss::TwinHaptic::Ramp:
            playWaveform(82);
            break;
        default:
            break;
    }
}
