#include "USBCLI.h"
#include "../../utils/Logger.h"
#include "../../lowlevel/LowLevel.h"
#include "../../lowlevel/usb/UsbLowLevelDriver.h"
#include "../../lowlevel/nvs/NVSLowLevelDriver.h"
#include "../../state/State.h"
#include "nvs_flash.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/buzzer/Buzzer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstring>

USBCLI::USBCLI(Logger* logger, LowLevel* lowLevel, State* state)
    : logger_(logger), lowLevel_(lowLevel), state_(state) {}

void USBCLI::begin() {
    logger_->LOGI(TAG, "USB CLI ready");
}

void USBCLI::write(const char* s) {
    lowLevel_->get_usb().writeBytes(reinterpret_cast<const uint8_t*>(s), strlen(s));
}

void USBCLI::writeln(const char* s) {
    write(s);
    write("\r\n");
}

int USBCLI::readByte(uint8_t* out, int timeout_ms) {
    return lowLevel_->get_usb().read(out, timeout_ms);
}

std::string USBCLI::readline(int maxlen, bool echo) {
    std::string line;
    while (static_cast<int>(line.size()) < maxlen) {
        uint8_t ch = 0;
        int r = readByte(&ch, 60000);
        if (r <= 0) continue;
        if (ch == '\r' || ch == '\n') {
            if (echo) write("\r\n");
            break;
        }
        if (ch == 0x08 || ch == 0x7f) {
            if (!line.empty()) {
                line.pop_back();
                if (echo) write("\b \b");
            }
            continue;
        }
        line.push_back(static_cast<char>(ch));
        if (echo) {
            char c[2] = {static_cast<char>(ch), 0};
            write(c);
        }
    }
    return line;
}

void USBCLI::showStatus() {
    char buf[128];
    writeln("\r\n=== Device Status ===");
    snprintf(buf, sizeof(buf), "Device ID: %s", state_->getDeviceId().c_str());
    writeln(buf);
    snprintf(buf, sizeof(buf), "Battery: %u%%", state_->getBatteryLevel());
    writeln(buf);
    snprintf(buf, sizeof(buf), "PA hold ms: %d", state_->getPersonalAlertButtonTriggerDelayMs());
    writeln(buf);
    snprintf(buf, sizeof(buf), "Haptics: %s  Buzzer: %s  LED: %s",
             state_->getEnableHaptics() ? "on" : "off",
             state_->getEnableBuzzer() ? "on" : "off",
             state_->getEnableLedIndications() ? "on" : "off");
    writeln(buf);
}

void USBCLI::runFactoryReset() {
    writeln("Erasing NVS and rebooting...");
    nvs_flash_erase();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

void USBCLI::runFactoryMode() {
    writeln("\r\n=== Factory / Identity ===");
    writeln("Serial is programmed via tools/provision_lanyard.py into ble_prov.");
    writeln("Options: 1=show status  2=wipe NVS bonds/prefs  e=exit");
    for (;;) {
        write("> ");
        auto line = readline();
        if (line == "e" || line == "exit") break;
        if (line == "1") showStatus();
        else if (line == "2") runFactoryReset();
    }
}

void USBCLI::runTestMode() {
    writeln("\r\n=== Hardware Test ===");
    writeln("led | buzz | hap | fa | pa | e");
    RGBLED led(state_, lowLevel_);
    led.begin();
    Buzzer buzzer(state_, lowLevel_);
    buzzer.begin();

    for (;;) {
        write("test> ");
        auto line = readline();
        if (line == "e" || line == "exit") {
            led.stopEffect();
            buzzer.requestStop();
            break;
        }
        if (line == "led") {
            led.test();
        } else if (line == "buzz") {
            buzzer.queueCode3Sweep(2700, 3500, 1);
            for (int i = 0; i < 50; ++i) {
                buzzer.processPendingBuzzer();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            buzzer.requestStop();
        } else if (line == "hap") {
            lowLevel_->get_haptics().play_pattern(14);
            vTaskDelay(pdMS_TO_TICKS(500));
            lowLevel_->get_haptics().stop();
        } else if (line == "fa") {
            led.queueEffect(LedEffect::DOUBLE_FLASH, LedColor::RED, Brightness::B100, 5000);
            buzzer.queueCode3Sweep(2700, 3500, 1);
            for (int i = 0; i < 50; ++i) {
                led.process();
                buzzer.processPendingBuzzer();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            led.stopEffect();
            buzzer.requestStop();
        } else if (line == "pa") {
            led.queueEffect(LedEffect::PULSE, LedColor::PURPLE, Brightness::B100, 5000);
            buzzer.queueCode3Siren(2700, 3500, 1);
            for (int i = 0; i < 50; ++i) {
                led.process();
                buzzer.processPendingBuzzer();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            led.stopEffect();
            buzzer.requestStop();
        }
    }
}

void USBCLI::runConfigurationMode() {
    writeln("\r\n=== ILSS Digital Twin CLI ===");
    writeln("c = configure prefs");
    writeln("t = test hardware");
    writeln("f = factory / identity");
    writeln("s = status");
    writeln("reset = erase NVS + reboot");
    writeln("e = exit to application");

    for (;;) {
        write("cli> ");
        auto line = readline();
        if (line == "e" || line == "exit") break;
        if (line == "s" || line == "status") showStatus();
        else if (line == "t" || line == "test") runTestMode();
        else if (line == "f" || line == "factory") runFactoryMode();
        else if (line == "reset") runFactoryReset();
        else if (line == "c" || line == "configure") {
            writeln("PA hold delay ms (current shown, blank=keep):");
            char buf[64];
            snprintf(buf, sizeof(buf), "[%d] ", state_->getPersonalAlertButtonTriggerDelayMs());
            write(buf);
            auto v = readline();
            if (!v.empty()) {
                int ms = atoi(v.c_str());
                if (ms > 0) state_->setPersonalAlertButtonTriggerDelayMs(ms);
            }
            writeln("Done.");
        } else {
            writeln("Unknown command");
        }
    }
}
