#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "lowlevel/LowLevel.h"
#include "state/State.h"
#include "utils/Logger.h"
#include "application/DigitalTwinApplication/DigitalTwinApplication.h"
#include "application/BootSequence/BootSequence.h"
#include "features/usb/USBCLI.h"
#include "features/rgb-led/RGBLED.h"
#include "features/buzzer/Buzzer.h"
#include "features/haptics/Haptics.h"
#include "application/Hardware.h"
#ifdef ILSS_TEMP_BUTTON_BRINGUP_TEST
#include "application/ButtonBringupTest/ButtonBringupTest.h"
#endif

Logger logger;

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

    RGBLED bootLed(&state);
    bool led_ok = bootLed.begin();
    (void)led_ok;

#ifdef ILSS_TEMP_RGB_RGB_FLASH_BOOT
    logger.LOGW("main", "ILSS_TEMP_RGB_RGB_FLASH_BOOT: cycling R/G/B forever");
    const LedColor colors[] = { LedColor::RED, LedColor::GREEN, LedColor::BLUE };
    size_t color_idx = 0;
    uint32_t last_switch_ms = 0;
    bootLed.queueEffect(LedEffect::CONTINUOUS, colors[0], Brightness::B100, 0);
    for (;;) {
        const uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_switch_ms >= 500) {
            last_switch_ms = now;
            color_idx = (color_idx + 1) % 3;
            bootLed.queueEffect(LedEffect::CONTINUOUS, colors[color_idx], Brightness::B100, 0);
        }
        bootLed.process();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
#endif

#ifdef ILSS_TEMP_BUTTON_BRINGUP_TEST
    logger.LOGW("main", "ILSS_TEMP_BUTTON_BRINGUP_TEST: entering button matrix (app blocked)");
    ilss_button_bringup_test_run();
#endif

    Buzzer bootBuzzer(&state, &low_level);
    bootBuzzer.begin();

    Haptics bootHaptics(&state, &low_level);
    bootHaptics.begin();

    USBCLI usb_cli(&logger, &low_level, &state, &bootLed);
    usb_cli.begin();

    BootSequence boot(&logger, &state, &low_level, &bootLed, &bootBuzzer, &bootHaptics);

    if (boot.waitForConfigKey(2000)) {
        bootLed.stopEffect();
        logger.LOGI("main", "Entering configuration mode");
        usb_cli.runConfigurationMode();
        logger.LOGI("main", "Configuration mode exited");
    }

    boot.playPowerUpCue();

    logger.LOGI("main", "Booting DigitalTwinApplication");
    // Hand off the boot strip so the app does not re-claim the same RMT GPIO.
    DigitalTwinApplication app(&logger, &low_level, &state, &bootLed);
    app.begin();
}
