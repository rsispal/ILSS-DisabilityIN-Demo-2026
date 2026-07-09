#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <cstring>
#include "lowlevel/LowLevel.h"
#include "state/State.h"
#include "utils/Logger.h"
#include "application/DigitalTwinApplication/DigitalTwinApplication.h"
#include "features/usb/USBCLI.h"
#include "features/rgb-led/RGBLED.h"

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

    RGBLED bootLed(&state, &low_level);
    bool led_ok = bootLed.begin();
    if (led_ok) {
        bootLed.queueEffect(LedEffect::PULSE, LedColor::WHITE, Brightness::B100, 0);
        bootLed.process();
    }

    USBCLI usb_cli(&logger, &low_level, &state);
    usb_cli.begin();

    bool config_mode = false;
    int elapsed = 0;
    const int timeout_ms = 5000;
    const int step_ms = 50;

    low_level.get_usb().writeLine("\r\nPress any key within 5s for configuration CLI...");

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
        logger.LOGI("main", "Entering configuration mode");
        usb_cli.runConfigurationMode();
        logger.LOGI("main", "Configuration mode exited");
    }

    if (led_ok) {
        bootLed.stopEffect();
    }

    logger.LOGI("main", "Booting DigitalTwinApplication");
    DigitalTwinApplication app(&logger, &low_level, &state);
    app.begin();
}
