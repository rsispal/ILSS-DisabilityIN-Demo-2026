#include "BootSequence.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/buzzer/Buzzer.h"
#include "../../features/haptics/Haptics.h"
#include "../../lowlevel/LowLevel.h"
#include "../../lowlevel/usb/UsbLowLevelDriver.h"
#include "../../state/State.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

BootSequence::BootSequence(Logger* logger, State* state, LowLevel* lowLevel,
                           RGBLED* rgbLed, Buzzer* buzzer, Haptics* haptics)
    : logger_(logger), state_(state), lowLevel_(lowLevel),
      rgbLed_(rgbLed), buzzer_(buzzer), haptics_(haptics) {}

bool BootSequence::waitForConfigKey(int timeout_ms) {
    if (!lowLevel_) return false;

    const bool led_ok = rgbLed_ && rgbLed_->isReady();
    if (led_ok) {
        rgbLed_->queueEffect(LedEffect::FLASH_SINGLE_3S, LedColor::BLUE, Brightness::B20, 0);
        rgbLed_->process();
    }

    lowLevel_->get_usb().writeLine("\r\nPress any key within 2s for configuration CLI...");

    int elapsed = 0;
    const int step_ms = 50;
    while (elapsed < timeout_ms) {
        if (led_ok) rgbLed_->process();
        uint8_t ch;
        int r = lowLevel_->get_usb().read(&ch, step_ms);
        if (r > 0) {
            lowLevel_->get_usb().writeLine("\r\nConfiguration mode triggered.");
            return true;
        }
        elapsed += step_ms;
    }

    lowLevel_->get_usb().writeLine("No key pressed; starting digital twin.");
    return false;
}

void BootSequence::playPowerUpCue() {
    const bool led_ok = rgbLed_ && rgbLed_->isReady();
    auto pump = [this, led_ok]() {
        if (led_ok) rgbLed_->process();
    };

    if (led_ok) {
        rgbLed_->queueEffect(LedEffect::TWINKLE, LedColor::WHITE, Brightness::B80, 1800);
    }

    if (buzzer_ && !buzzer_->isReady()) {
        logger_->LOGW(TAG, "Power-up: buzzer not ready — attempting begin()");
        if (!buzzer_->begin()) {
            logger_->LOGE(TAG, "Power-up: buzzer begin failed — no beep");
        }
    }

    if (haptics_) haptics_->playSoftBump();

    const struct { uint32_t freq; uint32_t on_ms; uint32_t gap_ms; } notes[] = {
        { 2000, 140, 100 },
        { 2700, 140, 100 },
        { 3500, 320, 0 },
    };
    for (size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); ++i) {
        const auto& n = notes[i];
        if (buzzer_) {
            buzzer_->playToneKeepAlive(n.freq, n.on_ms, pump);
            logger_->LOGI(TAG, "Power-up beep freq=%uHz %ums", n.freq, n.on_ms);
            if (n.gap_ms) buzzer_->silenceKeepAlive(n.gap_ms, pump);
        } else {
            for (uint32_t t = 0; t < n.on_ms + n.gap_ms; t += 20) {
                pump();
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
        if (i == 0 && haptics_) haptics_->playSoftBump();
    }

    for (int i = 0; i < 30; ++i) {
        pump();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (led_ok) rgbLed_->stopEffect();
    if (buzzer_) buzzer_->stop();
    if (haptics_) haptics_->stop();
}
