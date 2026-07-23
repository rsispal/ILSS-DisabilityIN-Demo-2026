#include "USBCLI.h"
#include "../../utils/Logger.h"
#include "../../lowlevel/LowLevel.h"
#include "../../lowlevel/usb/UsbLowLevelDriver.h"
#include "../../lowlevel/nvs/NVSLowLevelDriver.h"
#include "../../lowlevel/i2c/I2CLowLevelDriver.h"
#include "../../lowlevel/haptics/DRV2605Driver.h"
#include "../../application/Hardware.h"
#include "../../state/State.h"
#include "nvs_flash.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/buzzer/Buzzer.h"
#include "../../features/crypto/Crypto.h"
#include "../../lowlevel/crypto/ATECC608BDriver.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

USBCLI::USBCLI(Logger* logger, LowLevel* lowLevel, State* state, RGBLED* rgbLed)
    : logger_(logger), lowLevel_(lowLevel), state_(state), rgbLed_(rgbLed) {}

void USBCLI::begin() {
    logger_->LOGI(TAG, "USB CLI ready");
}

void USBCLI::write(const char* s) {
    if (!s) return;
    const size_t n = strlen(s);
    if (n == 0) return; // usb_serial_jtag rejects zero-length writes
    lowLevel_->get_usb().writeBytes(reinterpret_cast<const uint8_t*>(s), n);
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
        if (ch == 0x1b) {  // ESC cancels line / exits menus
            if (echo) write("\r\n");
            return "e";
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

void USBCLI::clearPendingInput() {
    uint8_t dummy;
    while (readByte(&dummy, 10) > 0) {}
}

bool USBCLI::waitForExitKey() {
    uint8_t ch;
    int r = readByte(&ch, 50);
    if (r > 0) {
        if (ch == 'e' || ch == 'E' || ch == 0x1b) {
            return true;
        }
    }
    return false;
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
    writeln("Serial is programmed via tools/provision-lanyard.sh into ble_prov.");
    writeln("Options: 1=show status  2=wipe NVS bonds/prefs  e=exit");
    for (;;) {
        write("> ");
        auto line = readline();
        if (line == "e" || line == "exit" || line == "E") break;
        if (line == "1") showStatus();
        else if (line == "2") runFactoryReset();
    }
}

void USBCLI::runI2cScan() {
    char buf[96];
    writeln("\r\n=== I2C Bus Scan ===");
    snprintf(buf, sizeof(buf), "Bus: port %d  SDA=GPIO%d  SCL=GPIO%d  %lu Hz",
             static_cast<int>(HARDWARE_I2C_NUM),
             static_cast<int>(HARDWARE_I2C_SDA_PIN),
             static_cast<int>(HARDWARE_I2C_SCL_PIN),
             static_cast<unsigned long>(HARDWARE_I2C_CLOCK_SPEED));
    writeln(buf);
    snprintf(buf, sizeof(buf), "Expected: DRV2605 @ 0x%02X, ATECC608B @ 0x%02X",
             HARDWARE_DRV2605_I2C_ADDR, HARDWARE_ATECC608B_I2C_ADDR);
    writeln(buf);
#ifdef HARDWARE_MCP17048_I2C_ADDR
    snprintf(buf, sizeof(buf), "Expected: MCP17048 @ 0x%02X", HARDWARE_MCP17048_I2C_ADDR);
    writeln(buf);
#endif

    auto& i2c = lowLevel_->get_i2c();
    if (!i2c.is_ready()) {
        writeln("I2C bus not ready — scan aborted.");
        return;
    }

    writeln("Probing 0x08..0x77...");
    uint8_t found[16] = {};
    size_t count = i2c.scan(found, sizeof(found));

    if (count == 0) {
        writeln("No devices responded.");
        return;
    }

    snprintf(buf, sizeof(buf), "Found %zu device(s):", count);
    writeln(buf);
    const size_t listed = count < sizeof(found) ? count : sizeof(found);
    for (size_t i = 0; i < listed; ++i) {
        snprintf(buf, sizeof(buf), "  0x%02X", found[i]);
        writeln(buf);
    }
    if (count > sizeof(found)) {
        writeln("  (additional devices truncated from list)");
    }
}

// ---------------------------------------------------------------------------
// Buzzer test menu (numeric patterns, e/esc to cancel)
// ---------------------------------------------------------------------------

struct BuzzerTaskParams {
    Buzzer* buzzer;
    int pattern_type;
    volatile bool* running;
    volatile bool* taskDone;
};

static void buzzerTaskFunc(void* params) {
    BuzzerTaskParams* p = static_cast<BuzzerTaskParams*>(params);
    switch (p->pattern_type) {
        case 6: p->buzzer->playSiren(2700, 3500, 0); break;
        case 7: p->buzzer->playMediumSweep(800, 970, 0); break;
        case 8: p->buzzer->playAlternating(800, 970, 0); break;
        case 9: p->buzzer->playLFBuzz(600, 650, 0); break;
        default: break;
    }
    *(p->taskDone) = true;
    vTaskDelete(NULL);
}

static void personalAlertBuzzerTask(void* params) {
    BuzzerTaskParams* p = static_cast<BuzzerTaskParams*>(params);
    p->buzzer->playCode3Siren(2700, 3500, 0);
    *(p->taskDone) = true;
    vTaskDelete(NULL);
}

static void fireAlarmBuzzerTask(void* params) {
    BuzzerTaskParams* p = static_cast<BuzzerTaskParams*>(params);
    p->buzzer->playCode3Sweep(2700, 3500, 0);
    *(p->taskDone) = true;
    vTaskDelete(NULL);
}

void USBCLI::testBuzzerPattern(const char* name, int pattern_type) {
    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n=== Testing: %s ===", name);
    writeln(msg);
    writeln("Press 'e' or ESC to stop");

    clearPendingInput();

    Buzzer buzzer(state_, lowLevel_);
    if (!buzzer.begin()) {
        writeln("ERROR: Failed to initialize buzzer");
        return;
    }

    if (pattern_type >= 6 && pattern_type <= 9) {
        volatile bool taskRunning = true;
        volatile bool taskDone = false;
        BuzzerTaskParams taskParams = { &buzzer, pattern_type, &taskRunning, &taskDone };
        TaskHandle_t buzzerTask = NULL;

        xTaskCreatePinnedToCore(
            buzzerTaskFunc, "buzzer_test", 4096, &taskParams, 5, &buzzerTask, 0);

        while (!waitForExitKey()) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        buzzer.requestStop();
        int timeout = 50;
        while (!taskDone && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout--;
        }
    } else {
        bool exitRequested = false;
        do {
            switch (pattern_type) {
                case 1:
                    buzzer.tick(3000);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    break;
                case 2:
                    buzzer.beep(2000, 1000);
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    break;
                case 3:
                    buzzer.queueCode3Temporal(3000, 0);
                    buzzer.processPendingBuzzer();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
                case 4:
                    buzzer.queueCode3Sweep(2700, 3500, 0);
                    buzzer.processPendingBuzzer();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
                case 5:
                    buzzer.queueCode3Siren(2700, 3500, 0);
                    buzzer.processPendingBuzzer();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
                default:
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
            }
            exitRequested = waitForExitKey();
        } while (!exitRequested);
    }

    buzzer.stop();
    writeln("\r\nTest stopped.");
}

void USBCLI::runAllBuzzerPatterns() {
    writeln("\r\n=== Running ALL Buzzer Patterns ===");

    Buzzer buzzer(state_, lowLevel_);
    if (!buzzer.begin()) {
        writeln("ERROR: Failed to initialize buzzer");
        return;
    }

    struct PatternInfo { const char* name; int pattern_type; };
    PatternInfo patterns[] = {
        {"1. Tick", 1},
        {"2. Beep", 2},
        {"3. Code-3 Temporal", 3},
        {"4. Code-3 Sweep", 4},
        {"5. Code-3 Siren", 5},
        {"6. Siren", 6},
        {"7. Medium Sweep", 7},
        {"8. Alternating", 8},
        {"9. LF Buzz", 9},
    };

    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "\r\nPlaying: %s", patterns[i].name);
        writeln(msg);

        switch (patterns[i].pattern_type) {
            case 1: buzzer.tick(3000); vTaskDelay(pdMS_TO_TICKS(500)); break;
            case 2: buzzer.beep(2000, 1000); vTaskDelay(pdMS_TO_TICKS(1500)); break;
            case 3:
                buzzer.queueCode3Temporal(3000, 1);
                buzzer.processPendingBuzzer();
                vTaskDelay(pdMS_TO_TICKS(4500));
                break;
            case 4:
                buzzer.queueCode3Sweep(2700, 3500, 1);
                buzzer.processPendingBuzzer();
                vTaskDelay(pdMS_TO_TICKS(4500));
                break;
            case 5:
                buzzer.queueCode3Siren(2700, 3500, 1);
                buzzer.processPendingBuzzer();
                vTaskDelay(pdMS_TO_TICKS(4500));
                break;
            case 6:
                buzzer.queueSiren(2700, 3500, 5);
                buzzer.processPendingBuzzer();
                break;
            case 7:
                buzzer.queueMediumSweep(800, 970, 2);
                buzzer.processPendingBuzzer();
                break;
            case 8:
                buzzer.queueAlternating(800, 970, 4);
                buzzer.processPendingBuzzer();
                break;
            case 9:
                buzzer.queueLFBuzz(600, 650, 2);
                buzzer.processPendingBuzzer();
                break;
        }
        buzzer.stop();
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    buzzer.stop();
    writeln("\r\nAll patterns complete.");
}

void USBCLI::testSpeechPlayback() {
    writeln("\r\nSpeech playback is not available in this digital-twin build.");
}

void USBCLI::runBuzzerTestMenu() {
    while (true) {
        writeln("\r\n=== Buzzer Test Sub-Menu ===");
        writeln("0 = Run ALL patterns");
        writeln("1 = Tick (short beep)");
        writeln("2 = Beep (1 second tone)");
        writeln("3 = Code-3 Temporal (constant tone)");
        writeln("4 = Code-3 Sweep (3x ramp UP)");
        writeln("5 = Code-3 Siren (3x smooth siren)");
        writeln("6 = Siren (continuous up/down)");
        writeln("7 = Medium Sweep");
        writeln("8 = Alternating (two-tone)");
        writeln("9 = Fire Horn Buzz");
        writeln("e = Exit (or ESC)");
        write("\r\nChoice: ");

        std::string choice = readline();
        if (choice == "e" || choice == "E" || choice == "exit") {
            writeln("Returning to test menu...");
            return;
        } else if (choice == "0") {
            runAllBuzzerPatterns();
        } else if (choice == "1") {
            testBuzzerPattern("Tick", 1);
        } else if (choice == "2") {
            testBuzzerPattern("Beep", 2);
        } else if (choice == "3") {
            testBuzzerPattern("Code-3 Temporal", 3);
        } else if (choice == "4") {
            testBuzzerPattern("Code-3 Sweep", 4);
        } else if (choice == "5") {
            testBuzzerPattern("Code-3 Siren", 5);
        } else if (choice == "6") {
            testBuzzerPattern("Siren", 6);
        } else if (choice == "7") {
            testBuzzerPattern("Medium Sweep", 7);
        } else if (choice == "8") {
            testBuzzerPattern("Alternating", 8);
        } else if (choice == "9") {
            testBuzzerPattern("LF Buzz", 9);
        } else if (choice == "s" || choice == "S") {
            testSpeechPlayback();
        } else {
            writeln("Invalid choice.");
        }
    }
}

// ---------------------------------------------------------------------------
// LED pattern test (effect + color numeric selection, e/esc to cancel)
// ---------------------------------------------------------------------------

void USBCLI::runLEDPatternTest() {
    writeln("\r\n=== LED Pattern Test ===");
    writeln("Choose an LED effect:");
    writeln("0  = OFF");
    writeln("1  = PULSE (smooth sine wave, 2s cycle)");
    writeln("2  = RAPID_PULSE (rapid sine wave, 500ms cycle)");
    writeln("3  = BLINK_ALTERNATE (on/off toggle, 1s cycle)");
    writeln("4  = FLASH_1S (quick flash every 1s)");
    writeln("5  = FLASH_2S (quick flash every 2s)");
    writeln("6  = CHASE_FADE (chasing LED with fade trail)");
    writeln("7  = CONTINUOUS (solid color)");
    writeln("8  = DOUBLE_FLASH (double flash pattern)");
    writeln("9  = WATER_DROP (water drop effect)");
    write("\r\nEffect choice (0-9): ");

    std::string effect_str = readline();
    int effect_num = -1;
    if (!effect_str.empty()) {
        char* endptr = nullptr;
        long effect_long = std::strtol(effect_str.c_str(), &endptr, 10);
        if (endptr != nullptr && *endptr == '\0' && effect_long >= 0 && effect_long <= 9) {
            effect_num = static_cast<int>(effect_long);
        }
    }
    if (effect_num < 0 || effect_num > 9) {
        writeln("Invalid effect choice.");
        return;
    }

    writeln("\r\nChoose a color:");
    writeln("0 = RED");
    writeln("1 = GREEN");
    writeln("2 = BLUE");
    writeln("3 = PURPLE");
    writeln("4 = YELLOW");
    writeln("5 = CYAN");
    writeln("6 = WHITE");
    writeln("7 = ORANGE");
    write("\r\nColor choice (0-7): ");

    std::string color_str = readline();
    int color_num = -1;
    if (!color_str.empty()) {
        char* endptr = nullptr;
        long color_long = std::strtol(color_str.c_str(), &endptr, 10);
        if (endptr != nullptr && *endptr == '\0' && color_long >= 0 && color_long <= 7) {
            color_num = static_cast<int>(color_long);
        }
    }
    if (color_num < 0 || color_num > 7) {
        writeln("Invalid color choice.");
        return;
    }

    LedEffect effect = LedEffect::OFF;
    const char* effect_name = "OFF";
    switch (effect_num) {
        case 0: effect = LedEffect::OFF; effect_name = "OFF"; break;
        case 1: effect = LedEffect::PULSE; effect_name = "PULSE"; break;
        case 2: effect = LedEffect::RAPID_PULSE; effect_name = "RAPID_PULSE"; break;
        case 3: effect = LedEffect::BLINK_ALTERNATE; effect_name = "BLINK_ALTERNATE"; break;
        case 4: effect = LedEffect::FLASH_1S; effect_name = "FLASH_1S"; break;
        case 5: effect = LedEffect::FLASH_2S; effect_name = "FLASH_2S"; break;
        case 6: effect = LedEffect::CHASE_FADE; effect_name = "CHASE_FADE"; break;
        case 7: effect = LedEffect::CONTINUOUS; effect_name = "CONTINUOUS"; break;
        case 8: effect = LedEffect::DOUBLE_FLASH; effect_name = "DOUBLE_FLASH"; break;
        case 9: effect = LedEffect::WATER_DROP; effect_name = "WATER_DROP"; break;
    }

    LedColor color = LedColor::RED;
    const char* color_name = "RED";
    switch (color_num) {
        case 0: color = LedColor::RED; color_name = "RED"; break;
        case 1: color = LedColor::GREEN; color_name = "GREEN"; break;
        case 2: color = LedColor::BLUE; color_name = "BLUE"; break;
        case 3: color = LedColor::PURPLE; color_name = "PURPLE"; break;
        case 4: color = LedColor::YELLOW; color_name = "YELLOW"; break;
        case 5: color = LedColor::CYAN; color_name = "CYAN"; break;
        case 6: color = LedColor::WHITE; color_name = "WHITE"; break;
        case 7: color = LedColor::ORANGE; color_name = "ORANGE"; break;
    }

    if (!rgbLed_ || !rgbLed_->isReady()) {
        writeln("ERROR: Boot LED strip not ready");
        return;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "\r\nPlaying: %s effect with %s color", effect_name, color_name);
    writeln(msg);
    writeln("Press 'e' or ESC to stop");

    clearPendingInput();
    rgbLed_->queueEffect(effect, color, Brightness::B100, 0);

    while (!waitForExitKey()) {
        rgbLed_->process();
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    rgbLed_->stopEffect();
    writeln("\r\nTest stopped.");
}

// ---------------------------------------------------------------------------
// Haptic test menu (numeric TwinHaptic / DRV patterns, e/esc to cancel)
// ---------------------------------------------------------------------------

void USBCLI::testHapticPattern(uint8_t waveform, const char* name, uint32_t loop_period_ms) {
    char msg[96];
    snprintf(msg, sizeof(msg), "\r\n=== Haptic: %s (DRV #%u) ===", name, waveform);
    writeln(msg);
    writeln("Press 'e' or ESC to stop");

    clearPendingInput();

    auto& drv = lowLevel_->get_haptics();
    if (!drv.isReady()) {
        writeln("ERROR: Haptics not ready");
        return;
    }

    if (drv.play_pattern(waveform) < 0) {
        writeln("ERROR: play_pattern failed");
        return;
    }

    uint32_t last = xTaskGetTickCount() * portTICK_PERIOD_MS;
    while (!waitForExitKey()) {
        if (loop_period_ms > 0) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last >= loop_period_ms) {
                drv.play_pattern(waveform);
                last = now;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    drv.stop();
    writeln("\r\nTest stopped.");
}

void USBCLI::testHapticDemo() {
    writeln("\r\n=== Haptic Patterns Demo ===");
    writeln("Playing sequence of haptic effects...");
    writeln("Press 'e' or ESC to abort");

    clearPendingInput();

    if (!lowLevel_->get_haptics().isReady()) {
        writeln("ERROR: Haptics not ready (may need device restart)");
        return;
    }

    struct HapticDemo {
        uint8_t pattern;
        const char* name;
        uint32_t delay_after_ms;
        bool needs_stop;
    };

    HapticDemo demos[] = {
        {52, "Buzz", 800, false},
        {48, "Pattern 48", 1000, false},
        {1, "Strong Click", 800, false},
        {4, "Sharp Click", 800, false},
        {118, "Long Buzz (118)", 2000, true},
        {63, "Ramp Up", 1000, false},
        {58, "Ramp Down", 1000, false},
    };

    for (size_t i = 0; i < sizeof(demos) / sizeof(demos[0]); i++) {
        if (waitForExitKey()) break;

        char msg[48];
        snprintf(msg, sizeof(msg), "  %s (pattern %d)", demos[i].name, demos[i].pattern);
        writeln(msg);

        int ret = lowLevel_->get_haptics().play_pattern(demos[i].pattern);
        if (ret < 0) {
            writeln("    (failed)");
            continue;
        }

        uint32_t waited = 0;
        while (waited < demos[i].delay_after_ms) {
            if (waitForExitKey()) {
                lowLevel_->get_haptics().stop();
                writeln("\r\nDemo aborted.");
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            waited += 50;
        }

        if (demos[i].needs_stop) {
            lowLevel_->get_haptics().stop();
            writeln("    (stopped programmatically)");
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    writeln("Done.");
}

void USBCLI::runHapticTestMenu() {
    while (true) {
        writeln("\r\n=== Haptic Test Sub-Menu ===");
        writeln("0 = All DRV demo sequence");
        writeln("1 = Click (Strong Click #1)");
        writeln("2 = ShortPulse (Sharp Click #4)");
        writeln("3 = LongPulse (Buzz 1 #47)");
        writeln("4 = ShortPulses (Double Click #10, looped)");
        writeln("5 = LongPulses (Alert #16, looped)");
        writeln("6 = Continuous (Strong Buzz #14, looped)");
        writeln("7 = Ramp (Ramp Up Long #82, looped)");
        writeln("8 = Soft Bump (#7)");
        writeln("9 = Long Buzz continuous (#118)");
        writeln("d = Diagnose (status + recover + click)");
        writeln("e = Exit (or ESC)");
        write("\r\nChoice: ");

        std::string choice = readline();
        if (choice == "e" || choice == "E" || choice == "exit") {
            writeln("Returning to test menu...");
            return;
        } else if (choice == "0") {
            testHapticDemo();
        } else if (choice == "1") {
            testHapticPattern(1, "Click", 0);
        } else if (choice == "2") {
            testHapticPattern(4, "ShortPulse", 0);
        } else if (choice == "3") {
            testHapticPattern(47, "LongPulse", 0);
        } else if (choice == "4") {
            testHapticPattern(10, "ShortPulses", 700);
        } else if (choice == "5") {
            testHapticPattern(16, "LongPulses", 1400);
        } else if (choice == "6") {
            testHapticPattern(14, "Continuous", 60);
        } else if (choice == "7") {
            testHapticPattern(82, "Ramp", 1600);
        } else if (choice == "8") {
            testHapticPattern(7, "Soft Bump", 0);
        } else if (choice == "9") {
            testHapticPattern(118, "Long Buzz 118", 0);  // continuous until stop
        } else if (choice == "d" || choice == "D") {
            auto& drv = lowLevel_->get_haptics();
            if (!drv.isReady()) {
                writeln("ERROR: Haptics not ready");
                continue;
            }
            writeln("\r\n=== Haptic Diagnose ===");
            drv.recover();
            char buf[64];
            uint8_t st = 0;
            if (drv.read_status(&st) == 0) {
                snprintf(buf, sizeof(buf), "STATUS after recover: 0x%02X", st);
                writeln(buf);
            }
            int pret = drv.play_pattern(1);
            snprintf(buf, sizeof(buf), "play_pattern(1) -> %d", pret);
            writeln(buf);
            if (drv.read_status(&st) == 0) {
                snprintf(buf, sizeof(buf), "STATUS after GO: 0x%02X", st);
                writeln(buf);
                if (st & 0x01) {
                    writeln("OC_DETECT still set — check ERM on J4 MOTOR+/-, EN jumper, shorts.");
                } else if (pret == 0) {
                    writeln("No OC — if still silent, actuator may be missing/DNP.");
                }
            }
        } else {
            writeln("Invalid choice.");
        }
    }
}

// ---------------------------------------------------------------------------
// Combined alert sims (e/esc to cancel)
// ---------------------------------------------------------------------------

void USBCLI::testPersonalAlert() {
    writeln("\r\n=== Personal Alert Simulator ===");
    writeln("Simulating personal alert (LED, buzzer, haptics)...");
    writeln("Press 'e' or ESC to stop");

    clearPendingInput();

    Buzzer buzzer(state_, lowLevel_);
    buzzer.begin();

    if (!buzzer.isReady()) writeln("WARNING: Buzzer not ready");
    if (!rgbLed_ || !rgbLed_->isReady()) writeln("WARNING: LED not ready");
    if (!lowLevel_->get_haptics().isReady()) writeln("WARNING: Haptics not ready");

    if (rgbLed_ && rgbLed_->isReady()) {
        rgbLed_->queueEffect(LedEffect::RAPID_PULSE, LedColor::PURPLE, Brightness::B100, 0);
        rgbLed_->process();
    }

    volatile bool taskDone = false;
    BuzzerTaskParams taskParams = { &buzzer, 5, nullptr, &taskDone };
    if (buzzer.isReady()) {
        xTaskCreatePinnedToCore(
            personalAlertBuzzerTask, "pa_buzzer", 4096, &taskParams, 5, nullptr, 0);
    }

    uint32_t lastHapticTime = 0;
    const uint32_t hapticInterval = 3000;
    if (lowLevel_->get_haptics().isReady()) {
        lowLevel_->get_haptics().play_pattern(118);
        lastHapticTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }

    while (!waitForExitKey()) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (rgbLed_ && rgbLed_->isReady()) rgbLed_->process();
        if (lowLevel_->get_haptics().isReady() && (now - lastHapticTime >= hapticInterval)) {
            lowLevel_->get_haptics().play_pattern(118);
            lastHapticTime = now;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    if (buzzer.isReady()) {
        buzzer.requestStop();
        int timeout = 50;
        while (!taskDone && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout--;
        }
        buzzer.stop();
    }
    if (rgbLed_ && rgbLed_->isReady()) rgbLed_->stopEffect();
    if (lowLevel_->get_haptics().isReady()) lowLevel_->get_haptics().stop();

    writeln("\r\nTest stopped.");
}

void USBCLI::testFireAlarm() {
    writeln("\r\n=== Fire Alarm Simulator ===");
    writeln("Simulating fire alarm (LED, buzzer, haptics)...");
    writeln("Press 'e' or ESC to stop");

    clearPendingInput();

    Buzzer buzzer(state_, lowLevel_);
    buzzer.begin();

    if (!buzzer.isReady()) writeln("WARNING: Buzzer not ready");
    if (!rgbLed_ || !rgbLed_->isReady()) writeln("WARNING: LED not ready");
    if (!lowLevel_->get_haptics().isReady()) writeln("WARNING: Haptics not ready");

    if (rgbLed_ && rgbLed_->isReady()) {
        rgbLed_->queueEffect(LedEffect::DOUBLE_FLASH, LedColor::RED, Brightness::B100, 0);
        rgbLed_->process();
    }

    volatile bool taskDone = false;
    BuzzerTaskParams taskParams = { &buzzer, 4, nullptr, &taskDone };
    if (buzzer.isReady()) {
        xTaskCreatePinnedToCore(
            fireAlarmBuzzerTask, "fa_buzzer", 4096, &taskParams, 5, nullptr, 0);
    }

    uint32_t lastHapticTime = 0;
    const uint32_t hapticInterval = 3000;
    if (lowLevel_->get_haptics().isReady()) {
        lowLevel_->get_haptics().play_pattern(118);
        lastHapticTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }

    while (!waitForExitKey()) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (rgbLed_ && rgbLed_->isReady()) rgbLed_->process();
        if (lowLevel_->get_haptics().isReady() && (now - lastHapticTime >= hapticInterval)) {
            lowLevel_->get_haptics().play_pattern(118);
            lastHapticTime = now;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    if (buzzer.isReady()) {
        buzzer.requestStop();
        int timeout = 50;
        while (!taskDone && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout--;
        }
        buzzer.stop();
    }
    if (rgbLed_ && rgbLed_->isReady()) rgbLed_->stopEffect();
    if (lowLevel_->get_haptics().isReady()) lowLevel_->get_haptics().stop();

    writeln("\r\nTest stopped.");
}

// ---------------------------------------------------------------------------
// Test mode entry
// ---------------------------------------------------------------------------

void USBCLI::runCryptoTestMenu() {
    Crypto crypto(state_, lowLevel_);
    if (!crypto.begin()) {
        writeln("ATECC608B not ready. Run 'i2c' to confirm 0x60 is present, then reboot.");
        // Still allow low-level wake probe via driver if the bus device attached.
    }

    char buf[128];
    while (true) {
        writeln("\r\n=== Crypto (ATECC608B) ===");
        snprintf(buf, sizeof(buf), "I2C addr 0x%02X  ready=%s",
                 HARDWARE_ATECC608B_I2C_ADDR, crypto.isReady() ? "yes" : "no");
        writeln(buf);
        writeln("0 / info  = Wake + Info revision");
        writeln("1 / sn    = Read 9-byte serial number");
        writeln("2 / rand  = Hardware random (32 bytes)");
        writeln("3 / wake  = Wake → Idle → Sleep cycle");
        writeln("4 / probe = I2C probe 0x60");
        writeln("e         = Exit");
        write("\r\nChoice: ");

        auto line = readline();
        if (line == "e" || line == "E" || line == "exit") return;

        auto& drv = lowLevel_->get_crypto();

        if (line == "0" || line == "info" || line == "INFO") {
            uint8_t rev[ATECC608BDriver::kRevisionLen] = {};
            if (!drv.readRevision(rev)) {
                writeln("FAIL: Info/Revision");
                continue;
            }
            snprintf(buf, sizeof(buf), "OK  %s  rev=%02X %02X %02X %02X",
                     ATECC608BDriver::revisionName(rev), rev[0], rev[1], rev[2], rev[3]);
            writeln(buf);
        } else if (line == "1" || line == "sn" || line == "SN") {
            uint8_t sn[ATECC608BDriver::kSerialLen] = {};
            if (!drv.readSerial(sn)) {
                writeln("FAIL: Read serial");
                continue;
            }
            char hex[(ATECC608BDriver::kSerialLen * 2) + 16];
            size_t pos = 0;
            pos += static_cast<size_t>(snprintf(hex + pos, sizeof(hex) - pos, "OK  SN="));
            for (size_t i = 0; i < ATECC608BDriver::kSerialLen && pos + 2 < sizeof(hex); ++i) {
                pos += static_cast<size_t>(snprintf(hex + pos, sizeof(hex) - pos, "%02X", sn[i]));
            }
            writeln(hex);
        } else if (line == "2" || line == "rand" || line == "RAND") {
            uint8_t rnd[ATECC608BDriver::kRandomLen] = {};
            if (!drv.random(rnd)) {
                writeln("FAIL: Random");
                continue;
            }
            // Unlocked config zone returns fixed 0xFFFF0000 repeating (Microchip).
            const bool fixed_pattern =
                rnd[0] == 0xFF && rnd[1] == 0xFF && rnd[2] == 0x00 && rnd[3] == 0x00;
            char hex[(ATECC608BDriver::kRandomLen * 2) + 48];
            size_t pos = 0;
            pos += static_cast<size_t>(snprintf(hex + pos, sizeof(hex) - pos, "OK  RAND="));
            for (size_t i = 0; i < ATECC608BDriver::kRandomLen && pos + 2 < sizeof(hex); ++i) {
                pos += static_cast<size_t>(snprintf(hex + pos, sizeof(hex) - pos, "%02X", rnd[i]));
            }
            writeln(hex);
            if (fixed_pattern) {
                writeln("(note: FFFF0000 pattern = config zone unlocked; real RNG after lock)");
            }
        } else if (line == "3" || line == "wake" || line == "WAKE") {
            const bool w = drv.wake();
            const bool i = drv.idle();
            const bool s = drv.sleep();
            snprintf(buf, sizeof(buf), "%s  wake=%s idle=%s sleep=%s",
                     (w && i && s) ? "OK" : "FAIL",
                     w ? "ok" : "fail", i ? "ok" : "fail", s ? "ok" : "fail");
            writeln(buf);
        } else if (line == "4" || line == "probe" || line == "PROBE") {
            uint8_t found[16] = {};
            const size_t n = lowLevel_->get_i2c().scan(found, sizeof(found));
            bool hit = false;
            for (size_t i = 0; i < n && i < sizeof(found); ++i) {
                if (found[i] == HARDWARE_ATECC608B_I2C_ADDR) hit = true;
            }
            snprintf(buf, sizeof(buf), "%s  0x%02X %s (scan found %zu device(s))",
                     hit ? "OK" : "FAIL",
                     HARDWARE_ATECC608B_I2C_ADDR,
                     hit ? "present" : "missing",
                     n);
            writeln(buf);
        } else {
            writeln("Invalid choice.");
        }
    }
}

void USBCLI::runTestMode() {
    while (true) {
        writeln("\r\n=== Hardware Test ===");
        writeln("b / buzz = Buzzer pattern menu");
        writeln("led      = LED pattern test (effect + color)");
        writeln("hap      = Haptic pattern menu");
        writeln("crypto   = ATECC608B secure element diagnostics");
        writeln("i2c      = I2C bus scan");
        writeln("pa       = Personal Alert sim");
        writeln("fa       = Fire Alarm sim");
        writeln("e        = Exit (or ESC)");
        write("\r\nChoice: ");

        auto line = readline();
        if (line == "e" || line == "E" || line == "exit") {
            writeln("Returning to main menu...");
            return;
        } else if (line == "b" || line == "B" || line == "buzz" || line == "BUZZ") {
            runBuzzerTestMenu();
        } else if (line == "led" || line == "LED" || line == "ledp" || line == "LEDP") {
            runLEDPatternTest();
        } else if (line == "hap" || line == "HAP") {
            runHapticTestMenu();
        } else if (line == "crypto" || line == "CRYPTO" || line == "atecc" || line == "ATECC") {
            runCryptoTestMenu();
        } else if (line == "i2c" || line == "I2C") {
            runI2cScan();
        } else if (line == "pa" || line == "PA") {
            testPersonalAlert();
        } else if (line == "fa" || line == "FA") {
            testFireAlarm();
        } else {
            writeln("Invalid choice.");
        }
    }
}

void USBCLI::runConfigurationMode() {
    auto showHelp = [this]() {
        writeln("\r\n=== ILSS Digital Twin CLI ===");
        writeln("h / help / ? = show this help");
        writeln("c = configure prefs");
        writeln("t = test hardware");
        writeln("f = factory / identity");
        writeln("s = status");
        writeln("reset = erase NVS + reboot");
        writeln("e = exit to application");
    };

    showHelp();

    for (;;) {
        write("cli> ");
        auto line = readline();
        if (line == "e" || line == "exit") {
            // Leave the shared boot strip quiet for playPowerUpCue / app handoff.
            if (rgbLed_ && rgbLed_->isReady()) rgbLed_->stopEffect();
            if (lowLevel_->get_haptics().isReady()) lowLevel_->get_haptics().stop();
            break;
        }
        if (line == "h" || line == "H" || line == "help" || line == "HELP" || line == "?") {
            showHelp();
        } else if (line == "s" || line == "status") showStatus();
        else if (line == "t" || line == "test") runTestMode();
        else if (line == "f" || line == "factory") runFactoryMode();
        else if (line == "reset") runFactoryReset();
        else if (line == "c" || line == "configure") {
            writeln("PA hold delay ms (current shown, blank=keep):");
            char buf[64];
            snprintf(buf, sizeof(buf), "[%d] ", state_->getPersonalAlertButtonTriggerDelayMs());
            write(buf);
            auto v = readline();
            if (!v.empty() && v != "e") {
                int ms = atoi(v.c_str());
                if (ms > 0) state_->setPersonalAlertButtonTriggerDelayMs(ms);
            }
            writeln("Done.");
        } else {
            writeln("Unknown command — type h or help");
        }
    }
}
