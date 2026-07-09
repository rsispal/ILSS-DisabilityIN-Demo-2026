#include "IndicationController.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/buzzer/Buzzer.h"
#include "../../lowlevel/LowLevel.h"
#include "../../lowlevel/haptics/DRV2605Driver.h"
#include "../../state/State.h"
#include "esp_timer.h"

IndicationController::IndicationController(Logger* logger, State* state, LowLevel* lowLevel,
                                           RGBLED* rgbLed, Buzzer* buzzer)
    : logger_(logger), state_(state), lowLevel_(lowLevel), rgbLed_(rgbLed), buzzer_(buzzer) {
    current_ = ilss::TwinState::idle();
}

bool IndicationController::begin() {
    goIdle();
    return true;
}

uint32_t IndicationController::nowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void IndicationController::goIdle() {
    current_ = ilss::TwinState::idle();
    driveLed(current_);
    if (buzzer_) buzzer_->requestStop();
    if (lowLevel_) lowLevel_->get_haptics().stop();
    haptic_period_ms_ = 0;
}

bool IndicationController::apply(const ilss::TwinState& desired, ilss::TwinNakReason* reason) {
    // Mutual exclusion: cannot switch fire <-> personal without clearing first
    if (current_.alert == ilss::TwinAlert::Fire && desired.alert == ilss::TwinAlert::Personal) {
        if (reason) *reason = ilss::TwinNakReason::MutualExclusion;
        logger_->LOGW(TAG, "Reject personal while fire active");
        return false;
    }
    if (current_.alert == ilss::TwinAlert::Personal && desired.alert == ilss::TwinAlert::Fire) {
        if (reason) *reason = ilss::TwinNakReason::MutualExclusion;
        logger_->LOGW(TAG, "Reject fire while personal active");
        return false;
    }

    // Advanced patterns only when no alert (or when clearing to none / setting alert presets)
    const bool advanced =
        desired.alert == ilss::TwinAlert::None &&
        (desired.flags & ilss::TWIN_FLAG_ADVANCED) != 0;
    if (advanced && current_.alert != ilss::TwinAlert::None) {
        if (reason) *reason = ilss::TwinNakReason::AlertActive;
        logger_->LOGW(TAG, "Reject advanced while alert active");
        return false;
    }

    current_ = desired;
    driveLed(current_);
    driveBuzzer(current_);
    driveHaptic(current_);
    logger_->LOGI(TAG, "Applied twin state alert=%u led=%u buzz=%u hap=%u",
                  static_cast<unsigned>(current_.alert),
                  static_cast<unsigned>(current_.led),
                  static_cast<unsigned>(current_.buzzer),
                  static_cast<unsigned>(current_.haptic));
    return true;
}

void IndicationController::process() {
    updateHapticPulse();
}

void IndicationController::driveLed(const ilss::TwinState& s) {
    if (!rgbLed_ || !state_->getEnableLedIndications()) return;

    LedColor color = LedColor::GREEN;
    switch (s.color) {
        case ilss::TwinColor::Red: color = LedColor::RED; break;
        case ilss::TwinColor::Green: color = LedColor::GREEN; break;
        case ilss::TwinColor::Blue: color = LedColor::BLUE; break;
        case ilss::TwinColor::Teal: color = LedColor::CYAN; break;
        case ilss::TwinColor::Purple: color = LedColor::PURPLE; break;
        case ilss::TwinColor::Yellow: color = LedColor::YELLOW; break;
        case ilss::TwinColor::Orange: color = LedColor::ORANGE; break;
    }

    LedEffect effect = LedEffect::CONTINUOUS;
    switch (s.led) {
        case ilss::TwinLed::Solid: effect = LedEffect::CONTINUOUS; break;
        case ilss::TwinLed::Flash: effect = LedEffect::FLASH_1S; break;  // retuned period in RGBLED for 1.5s web flash
        case ilss::TwinLed::Pulse: effect = LedEffect::PULSE; break;     // 1500ms
        case ilss::TwinLed::Double: effect = LedEffect::DOUBLE_FLASH; break;
        case ilss::TwinLed::Alt: effect = LedEffect::BLINK_ALTERNATE; break;
        case ilss::TwinLed::Half: effect = LedEffect::BLINK_ALTERNATE; break;
        case ilss::TwinLed::Chase: effect = LedEffect::CHASE_FADE; break;
        case ilss::TwinLed::Off: effect = LedEffect::OFF; break;
    }

    if (effect == LedEffect::OFF) {
        rgbLed_->stopEffect();
    } else {
        rgbLed_->queueEffect(effect, color, Brightness::B100, 0);
    }
}

void IndicationController::driveBuzzer(const ilss::TwinState& s) {
    if (!buzzer_ || !state_->getEnableBuzzer()) return;
    buzzer_->requestStop();

    switch (s.buzzer) {
        case ilss::TwinBuzzer::Silent:
        case ilss::TwinBuzzer::Off:
            break;
        case ilss::TwinBuzzer::Code3Sweep:
            // Web: 4s macro with 3x 0.5s bursts — cycles=1 plays one Code-3 group; re-queue via process if needed
            buzzer_->queueCode3Sweep(2700, 3500, 0);
            break;
        case ilss::TwinBuzzer::Code3Siren:
            buzzer_->queueCode3Siren(2700, 3500, 0);
            break;
        case ilss::TwinBuzzer::Code3Beep:
            buzzer_->queueCode3Temporal(3000, 0);
            break;
        case ilss::TwinBuzzer::Alternating:
            buzzer_->queueAlternating(800, 970, 0);
            break;
        case ilss::TwinBuzzer::BsSweep:
            buzzer_->queueMediumSweep(800, 970, 4);
            break;
        case ilss::TwinBuzzer::BsFastSweep:
            buzzer_->queueMediumSweep(800, 970, 2);
            break;
        case ilss::TwinBuzzer::LfBuzz:
            buzzer_->queueLFBuzz(800, 970, 100);
            break;
        case ilss::TwinBuzzer::Siren:
            buzzer_->queueSiren(2700, 3500, 0);
            break;
    }
}

void IndicationController::driveHaptic(const ilss::TwinState& s) {
    if (!lowLevel_ || !state_->getEnableHaptics()) return;
    auto& hap = lowLevel_->get_haptics();
    hap.stop();
    last_haptic_ms_ = nowMs();

    switch (s.haptic) {
        case ilss::TwinHaptic::Off:
            haptic_period_ms_ = 0;
            break;
        case ilss::TwinHaptic::Continuous:
            // Fire: frequent short buzz
            hap.play_pattern(14);
            haptic_period_ms_ = 50;
            break;
        case ilss::TwinHaptic::Pulse2:
            // Personal: ~1.4s double pulse feel
            hap.play_pattern(118);
            haptic_period_ms_ = 1400;
            break;
        case ilss::TwinHaptic::Pulse1:
            hap.play_pattern(118);
            haptic_period_ms_ = 1400;
            break;
        case ilss::TwinHaptic::Solid:
            hap.play_pattern(14);
            haptic_period_ms_ = 70;
            break;
        case ilss::TwinHaptic::Click:
            hap.play_pattern(1);
            haptic_period_ms_ = 1600;
            break;
    }
}

void IndicationController::updateHapticPulse() {
    if (haptic_period_ms_ == 0 || !lowLevel_ || !state_->getEnableHaptics()) return;
    const uint32_t now = nowMs();
    if ((now - last_haptic_ms_) < haptic_period_ms_) return;
    last_haptic_ms_ = now;
    auto& hap = lowLevel_->get_haptics();
    hap.stop();
    switch (current_.haptic) {
        case ilss::TwinHaptic::Continuous:
        case ilss::TwinHaptic::Solid:
            hap.play_pattern(14);
            break;
        case ilss::TwinHaptic::Pulse1:
        case ilss::TwinHaptic::Pulse2:
            hap.play_pattern(118);
            break;
        case ilss::TwinHaptic::Click:
            hap.play_pattern(1);
            break;
        default:
            break;
    }
}
