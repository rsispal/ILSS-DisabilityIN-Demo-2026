#include "IndicationController.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/buzzer/Buzzer.h"
#include "../../features/haptics/Haptics.h"
#include "../../state/State.h"
#include "esp_timer.h"

namespace {

Brightness pctToBrightness(uint8_t pct) {
    if (pct > 100) pct = 100;
    pct = static_cast<uint8_t>((pct / 10) * 10);
    if (pct == 0) return Brightness::B5;  // caller should stop instead
    const int idx = static_cast<int>(pct / 5) - 1;
    return static_cast<Brightness>(idx < 0 ? 0 : (idx > 19 ? 19 : idx));
}

}  // namespace

IndicationController::IndicationController(Logger* logger, State* state,
                                           RGBLED* rgbLed, Buzzer* buzzer, Haptics* haptics)
    : logger_(logger), state_(state), rgbLed_(rgbLed), buzzer_(buzzer), haptics_(haptics) {
    current_ = ilss::TwinState::idle();
}

bool IndicationController::begin() {
    current_ = ilss::TwinState::idle();
    link_mode_ = LinkLedMode::Unpaired;
    return true;
}

uint32_t IndicationController::nowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void IndicationController::goIdle() {
    current_ = ilss::TwinState::idle();
    if (buzzer_) buzzer_->stop();
    if (haptics_) haptics_->stop();
    haptic_period_ms_ = 0;
    connected_flash_until_ms_ = 0;
    // Only paint idle green when already in Connected mode; otherwise
    // leave Unpaired/Pairing link cues alone (caller switches link mode).
    if (link_mode_ == LinkLedMode::Connected) {
        driveLed(current_);
    }
}

void IndicationController::standDownToUnpaired() {
    logger_->LOGI(TAG, "Stand-down → unpaired (clear alerts, stop indications)");
    current_ = ilss::TwinState::idle();
    haptic_period_ms_ = 0;
    connected_flash_until_ms_ = 0;
    link_mode_ = LinkLedMode::Unpaired;

    if (buzzer_) {
        buzzer_->requestStop();
        buzzer_->stop();
    }
    if (haptics_) haptics_->stop();
    applyLinkLed();
}

void IndicationController::setLinkLedMode(LinkLedMode mode) {
    link_mode_ = mode;
    connected_flash_until_ms_ = 0;
    if (mode == LinkLedMode::Connected) {
        // Keep whatever twin state is current (usually idle at pair_ok).
        resumeAfterLinkCue();
        return;
    }
    applyLinkLed();
}

void IndicationController::showConnectedFlash() {
    if (!rgbLed_ || !state_->getEnableLedIndications()) return;
    link_mode_ = LinkLedMode::Connected;
    connected_flash_until_ms_ = nowMs() + 450;
    // Brief all-green flash, then process() restores idle at 10%.
    rgbLed_->queueEffect(LedEffect::CONTINUOUS, LedColor::GREEN, Brightness::B100, 450);
    logger_->LOGI(TAG, "Connected flash (green)");
}

void IndicationController::applyLinkLed() {
    if (!rgbLed_ || !state_->getEnableLedIndications()) return;
    switch (link_mode_) {
        case LinkLedMode::Unpaired:
            rgbLed_->queueEffect(LedEffect::FLASH_SINGLE_3S, LedColor::BLUE,
                                 pctToBrightness(kHwUnpairedPct), 0);
            break;
        case LinkLedMode::Pairing:
            rgbLed_->queueEffect(LedEffect::CHASE_FADE, LedColor::BLUE,
                                 pctToBrightness(kHwPairingPct), 0);
            break;
        case LinkLedMode::Connected:
            break;
    }
}

void IndicationController::resumeAfterLinkCue() {
    driveLed(current_);
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
    connected_flash_until_ms_ = 0;
    // Twin commands imply a live session — show the commanded indications.
    if (link_mode_ != LinkLedMode::Connected) {
        link_mode_ = LinkLedMode::Connected;
    }
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
    if (connected_flash_until_ms_ != 0 && nowMs() >= connected_flash_until_ms_) {
        connected_flash_until_ms_ = 0;
        resumeAfterLinkCue();
    }
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
        case ilss::TwinLed::Flash: effect = LedEffect::FLASH_1S; break;
        case ilss::TwinLed::Pulse: effect = LedEffect::PULSE; break;
        case ilss::TwinLed::Double: effect = LedEffect::DOUBLE_FLASH; break;
        case ilss::TwinLed::Alt: effect = LedEffect::BLINK_ALTERNATE; break;
        case ilss::TwinLed::Half: effect = LedEffect::HALF_HALF; break;
        case ilss::TwinLed::Chase: effect = LedEffect::CHASE_FADE; break;
        case ilss::TwinLed::Off: effect = LedEffect::OFF; break;
    }

    // Prefer twin brightness (0–100 /10). Connected idle green is forced dim
    // on the physical strip only (web twin may still report 40%).
    uint8_t pct = s.brightness;
    if (pct > 100) pct = 100;
    pct = static_cast<uint8_t>((pct / 10) * 10);

    const bool idle_green =
        s.alert == ilss::TwinAlert::None &&
        s.color == ilss::TwinColor::Green &&
        s.led == ilss::TwinLed::Solid &&
        s.haptic == ilss::TwinHaptic::Off &&
        (s.buzzer == ilss::TwinBuzzer::Silent || s.buzzer == ilss::TwinBuzzer::Off) &&
        (s.flags & ilss::TWIN_FLAG_ADVANCED) == 0;

    if (idle_green) pct = kHwIdleGreenPct;
    if (pct == 0 || effect == LedEffect::OFF || s.led == ilss::TwinLed::Off) {
        rgbLed_->stopEffect();
        return;
    }

    rgbLed_->queueEffect(effect, color, pctToBrightness(pct), 0);
}

void IndicationController::driveBuzzer(const ilss::TwinState& s) {
    if (!buzzer_ || !state_->getEnableBuzzer()) return;

    switch (s.buzzer) {
        case ilss::TwinBuzzer::Silent:
        case ilss::TwinBuzzer::Off:
            // Hard stop: clear pending queues + silence PWM (requestStop alone is not enough).
            buzzer_->stop();
            break;
        case ilss::TwinBuzzer::Code3Sweep:
            // Abort any blocking pattern, then queue. Queued play* clears shouldStop
            // and replaces the FreeRTOS pattern timer — do not clearPending here.
            buzzer_->requestStop();
            buzzer_->queueCode3Sweep(2700, 3500, 0);
            break;
        case ilss::TwinBuzzer::Code3Siren:
            buzzer_->requestStop();
            buzzer_->queueCode3Siren(2700, 3500, 0);
            break;
        case ilss::TwinBuzzer::Code3Beep:
            buzzer_->requestStop();
            buzzer_->queueCode3Temporal(3000, 0);
            break;
        case ilss::TwinBuzzer::Alternating:
            // Match web ilssAudio: 1000 Hz → 740 Hz, 250 ms each (500 ms cycle).
            buzzer_->requestStop();
            buzzer_->queueAlternating(1000, 740, 0);
            break;
        case ilss::TwinBuzzer::BsSweep:
            buzzer_->requestStop();
            buzzer_->queueMediumSweep(800, 970, 4);
            break;
        case ilss::TwinBuzzer::BsFastSweep:
            buzzer_->requestStop();
            buzzer_->queueMediumSweep(800, 970, 2);
            break;
        case ilss::TwinBuzzer::LfBuzz:
            buzzer_->requestStop();
            buzzer_->queueLFBuzz(800, 970, 100);
            break;
        case ilss::TwinBuzzer::Siren:
            buzzer_->requestStop();
            buzzer_->queueSiren(2700, 3500, 0);
            break;
    }
}

void IndicationController::driveHaptic(const ilss::TwinState& s) {
    if (!haptics_ || !state_->getEnableHaptics()) return;
    last_haptic_ms_ = nowMs();
    haptic_period_ms_ = haptics_->loopPeriodMs(s.haptic);
    haptics_->play(s.haptic);
}

void IndicationController::updateHapticPulse() {
    if (haptic_period_ms_ == 0 || !haptics_ || !state_->getEnableHaptics()) return;
    const uint32_t now = nowMs();
    if ((now - last_haptic_ms_) < haptic_period_ms_) return;
    last_haptic_ms_ = now;
    haptics_->stop();
    haptics_->retrigger(current_.haptic);
}
