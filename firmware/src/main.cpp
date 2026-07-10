#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <cstring>
#include "lowlevel/LowLevel.h"
#include "lowlevel/usb/UsbLowLevelDriver.h"
#include "lowlevel/buzzer/BuzzerLowLevelDriver.h"
#include "lowlevel/haptics/DRV2605Driver.h"
#include "state/State.h"
#include "utils/Logger.h"
#include "application/DigitalTwinApplication/DigitalTwinApplication.h"
#include "features/usb/USBCLI.h"
#include "features/rgb-led/RGBLED.h"

Logger logger;

/** Subtle power-up: white/red star twinkle + soft haptic taps + ascending beep-beep-BEEP. */
static void playPowerUpCue(RGBLED& led, LowLevel& low_level, bool led_ok)
{
    if (led_ok) {
        led.queueEffect(LedEffect::TWINKLE, LedColor::WHITE, Brightness::B80, 1800);
    }

    auto& hap = low_level.get_haptics();
    auto& buzz = low_level.get_buzzer();

    if (!buzz.isReady()) {
        logger.LOGW("main", "Power-up: buzzer not ready — attempting begin()");
        if (!buzz.begin()) {
            logger.LOGE("main", "Power-up: buzzer begin failed — no beep");
        }
    }

    // Soft bump (DRV #7) — gentle presence, not an alert.
    hap.play_pattern(7);

    // Piezo is loudest around 2–3.5 kHz (same band as alert tones).
    const struct { uint32_t freq; uint32_t on_ms; uint32_t gap_ms; } notes[] = {
        { 2000, 140, 100 },
        { 2700, 140, 100 },
        { 3500, 320, 0 },
    };
    for (size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); ++i) {
        const auto& n = notes[i];
        if (!buzz.isReady() || !buzz.setPWM(n.freq, 50)) {
            logger.LOGW("main", "Power-up beep failed freq=%u ready=%d",
                        n.freq, buzz.isReady() ? 1 : 0);
        } else {
            logger.LOGI("main", "Power-up beep freq=%uHz %ums", n.freq, n.on_ms);
        }
        for (uint32_t t = 0; t < n.on_ms; t += 20) {
            if (led_ok) led.process();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        // Soft silence between notes — keep LEDC channel alive (duty 0, not ledc_stop).
        if (buzz.isReady()) buzz.setDutyOnly(0);
        for (uint32_t t = 0; t < n.gap_ms; t += 20) {
            if (led_ok) led.process();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (i == 0) hap.play_pattern(7);
    }

    // Let the twinkle finish (~remaining of 1.8s).
    for (int i = 0; i < 30; ++i) {
        if (led_ok) led.process();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (led_ok) led.stopEffect();
    if (buzz.isReady()) buzz.stop();
    hap.stop();
}

extern "C" void app_main(void)
{
    logger.setLogLevel(static_cast<LogLevel>(CONFIG_ILSS_LOG_LEVEL));

    State state(&logger);
    LowLevel low_level(&logger);

    if (!low_level.begin()) {
        logger.LOGE("main", "LowLevel initialization failed");
        return;
    }

    state.setNVSDriver(&low_level.get_nvs());
    state.loadDefaultState();

    RGBLED bootLed(&state, &low_level);
    bool led_ok = bootLed.begin();

    USBCLI usb_cli(&logger, &low_level, &state);
    usb_cli.begin();

    bool config_mode = false;
    int elapsed = 0;
    const int timeout_ms = 2000;
    const int step_ms = 50;

    // Quiet ready cue during the short CLI window (no white pulse).
    if (led_ok) {
        bootLed.queueEffect(LedEffect::FLASH_SINGLE_3S, LedColor::BLUE, Brightness::B40, 0);
        bootLed.process();
    }

    low_level.get_usb().writeLine("\r\nPress any key within 2s for configuration CLI...");

    while (elapsed < timeout_ms) {
        if (led_ok) bootLed.process();
        uint8_t ch;
        int r = low_level.get_usb().read(&ch, step_ms);
        if (r > 0) {
            low_level.get_usb().writeLine("\r\nConfiguration mode triggered.");
            config_mode = true;
            break;
        }
        elapsed += step_ms;
    }

    if (!config_mode) {
        low_level.get_usb().writeLine("No key pressed; starting digital twin.");
    }

    if (config_mode) {
        if (led_ok) bootLed.stopEffect();
        logger.LOGI("main", "Entering configuration mode");
        usb_cli.runConfigurationMode();
        logger.LOGI("main", "Configuration mode exited");
    }

    // Power-up flourish before the twin app takes over the strip.
    playPowerUpCue(bootLed, low_level, led_ok);

    logger.LOGI("main", "Booting DigitalTwinApplication");
    DigitalTwinApplication app(&logger, &low_level, &state);
    app.begin();
}
