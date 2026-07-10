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

    Buzzer bootBuzzer(&state, &low_level);
    bootBuzzer.begin();

    Haptics bootHaptics(&state, &low_level);
    bootHaptics.begin();

    USBCLI usb_cli(&logger, &low_level, &state);
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
    DigitalTwinApplication app(&logger, &low_level, &state);
    app.begin();
}
