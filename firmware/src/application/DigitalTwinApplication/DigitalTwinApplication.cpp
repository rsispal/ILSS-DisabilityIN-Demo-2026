#include "DigitalTwinApplication.h"
#include "../IndicationController/IndicationController.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/buzzer/Buzzer.h"
#include "../../features/haptics/Haptics.h"
#include "../../features/side-buttons/SideButtons.h"
#include "../../features/ble-twin/BleTwin.h"
#include "../../lowlevel/LowLevel.h"
#include "../../state/State.h"
#include "../../protocol/TwinState.h"
#include "../../application/Hardware.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cstdio>
#include <cstring>

DigitalTwinApplication::DigitalTwinApplication(Logger* logger, LowLevel* lowLevel, State* state,
                                               RGBLED* rgbLed)
    : logger_(logger), lowLevel_(lowLevel), state_(state),
      rgbLed_(rgbLed), owns_rgb_led_(rgbLed == nullptr) {}

DigitalTwinApplication::~DigitalTwinApplication() {
    delete bleTwin_;
    delete indications_;
    delete sideButtons_;
    delete haptics_;
    delete buzzer_;
    if (owns_rgb_led_) delete rgbLed_;
    if (event_queue_) vQueueDelete(event_queue_);
}

void DigitalTwinApplication::ledTaskTrampoline(void* arg) {
    static_cast<DigitalTwinApplication*>(arg)->ledTask();
}

void DigitalTwinApplication::buzzerTaskTrampoline(void* arg) {
    static_cast<DigitalTwinApplication*>(arg)->buzzerTask();
}

void DigitalTwinApplication::ledTask() {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        if (rgbLed_) rgbLed_->process();
        if (indications_) indications_->process();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
    }
}

void DigitalTwinApplication::buzzerTask() {
    for (;;) {
        if (buzzer_) buzzer_->processPendingBuzzer();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void DigitalTwinApplication::initTasks() {
    xTaskCreatePinnedToCore(ledTaskTrampoline, "led_task", 4096, this, 5, &led_task_, 1);
    xTaskCreatePinnedToCore(buzzerTaskTrampoline, "buzzer_task", 4096, this, 5, &buzzer_task_, 1);
}

void DigitalTwinApplication::onBothHold() {
    if (!indications_ || !bleTwin_) return;
    if (indications_->current().alert == ilss::TwinAlert::Fire) {
        logger_->LOGW(TAG, "Ignore BOTH_HOLD during fire");
        return;
    }
    // Debounce here (not in SideButtons) so button momentary path stays simple.
    const TickType_t now = xTaskGetTickCount();
    constexpr TickType_t kBothHoldDebounce = pdMS_TO_TICKS(3000);
    if (last_both_hold_tick_ != 0 && (now - last_both_hold_tick_) < kBothHoldDebounce) {
        logger_->LOGW(TAG, "Ignore BOTH_HOLD (debounce)");
        return;
    }
    last_both_hold_tick_ = now;

    if (indications_->current().alert == ilss::TwinAlert::Personal) {
        // Toggle clear when already in personal
        auto idle = ilss::TwinState::idle();
        if (indications_->apply(idle)) {
            bleTwin_->emitTwinEvent(idle);
        }
        return;
    }
    auto pa = ilss::TwinState::personal();
    ilss::TwinNakReason reason;
    if (indications_->apply(pa, &reason)) {
        bleTwin_->emitTwinEvent(pa);
        logger_->LOGI(TAG, "Personal alert raised from buttons");
    }
}

void DigitalTwinApplication::onSidePress(uint8_t side) {
    if (!bleTwin_) return;
    bleTwin_->emitButtonEvent(side, ilss::BUTTON_ACTION_PRESS);
    logger_->LOGI(TAG, "Side button press → UI cue (%s)",
                  side == ilss::BUTTON_SIDE_LEFT ? "left" : "right");
}

void DigitalTwinApplication::onBleDisconnected() {
    logger_->LOGI(TAG, "BLE disconnected — stand down to unpaired ready");
    if (indications_) {
        indications_->standDownToUnpaired();
    }
    if (bleTwin_) bleTwin_->publishStatus(ilss::TwinState::idle());
}

void DigitalTwinApplication::onBleConnecting() {
    logger_->LOGI(TAG, "BLE connected — pairing chase");
    if (indications_) {
        indications_->setLinkLedMode(IndicationController::LinkLedMode::Pairing);
    }
}

void DigitalTwinApplication::onBlePaired() {
    logger_->LOGI(TAG, "BLE paired — connected flash then idle");
    if (indications_) {
        indications_->goIdle();
        indications_->showConnectedFlash();
    }
    if (bleTwin_) bleTwin_->publishStatus(ilss::TwinState::idle());
}

void DigitalTwinApplication::eventLoop() {
    AppEvent ev{};
    TickType_t last_hb = xTaskGetTickCount();
    for (;;) {
        if (xQueueReceive(event_queue_, &ev, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (ev.type) {
                case AppEventType::ButtonBothHold:
                    onBothHold();
                    break;
                case AppEventType::ButtonSidePress:
                    onSidePress(ev.side);
                    break;
                case AppEventType::BleDisconnected:
                    onBleDisconnected();
                    break;
                case AppEventType::BleConnecting:
                    onBleConnecting();
                    break;
                case AppEventType::BlePaired:
                    onBlePaired();
                    break;
                default:
                    break;
            }
        }
        if (bleTwin_) bleTwin_->process();

        // Soft heartbeat log every 30s
        if ((xTaskGetTickCount() - last_hb) > pdMS_TO_TICKS(30000)) {
            last_hb = xTaskGetTickCount();
            logger_->LOGI(TAG, "Alive alert=%u connected=%d",
                          static_cast<unsigned>(indications_ ? indications_->current().alert : ilss::TwinAlert::None),
                          bleTwin_ && bleTwin_->isConnected());
        }
    }
}

void DigitalTwinApplication::begin() {
    logger_->LOGI(TAG, "Starting digital twin application");
    event_queue_ = xQueueCreate(16, sizeof(AppEvent));

    // Prefer the boot strip (already owns RMT on the LED GPIO). Re-begin()
    // here used to log "GPIO is not usable, maybe conflict with others".
    if (!rgbLed_) {
        rgbLed_ = new RGBLED(state_);
        owns_rgb_led_ = true;
        if (!rgbLed_->begin()) {
            logger_->LOGE(TAG, "RGBLED begin failed");
        }
    } else if (!rgbLed_->isReady()) {
        if (!rgbLed_->begin()) {
            logger_->LOGE(TAG, "RGBLED begin failed");
        }
    } else {
        rgbLed_->stopEffect();
        logger_->LOGI(TAG, "Reusing boot RGBLED strip");
    }

    buzzer_ = new Buzzer(state_, lowLevel_);
    buzzer_->begin();

    haptics_ = new Haptics(state_, lowLevel_);
    haptics_->begin();

    indications_ = new IndicationController(logger_, state_, rgbLed_, buzzer_, haptics_);
    indications_->begin();

    sideButtons_ = new SideButtons(state_, HARDWARE_BUTTON_LEFT_PIN, HARDWARE_BUTTON_RIGHT_PIN);
    sideButtons_->begin();
    sideButtons_->setEventCallback([this](ButtonEvent event) {
        if (event == ButtonEvent::BOTH_HOLD) {
            AppEvent ev{AppEventType::ButtonBothHold, 0};
            if (event_queue_) xQueueSend(event_queue_, &ev, 0);
            return;
        }
        // Momentary tap → web twin illuminates the matching side button.
        if (event == ButtonEvent::LEFT_PRESS) {
            AppEvent ev{AppEventType::ButtonSidePress, ilss::BUTTON_SIDE_LEFT};
            if (event_queue_) xQueueSend(event_queue_, &ev, 0);
        } else if (event == ButtonEvent::RIGHT_PRESS) {
            AppEvent ev{AppEventType::ButtonSidePress, ilss::BUTTON_SIDE_RIGHT};
            if (event_queue_) xQueueSend(event_queue_, &ev, 0);
        }
    });

    bleTwin_ = new BleTwin(logger_, lowLevel_, indications_, state_);
    bleTwin_->setTwinStateHandler([this](const ilss::TwinState& desired, ilss::TwinNakReason* reason) {
        return indications_->apply(desired, reason);
    });
    bleTwin_->setDisconnectHandler([this]() {
        AppEvent ev{AppEventType::BleDisconnected};
        if (event_queue_) xQueueSend(event_queue_, &ev, 0);
    });
    bleTwin_->setConnectingHandler([this]() {
        AppEvent ev{AppEventType::BleConnecting};
        if (event_queue_) xQueueSend(event_queue_, &ev, 0);
    });
    bleTwin_->setPairedHandler([this]() {
        AppEvent ev{AppEventType::BlePaired};
        if (event_queue_) xQueueSend(event_queue_, &ev, 0);
    });

    logger_->setBleLogSink([this](const char* line) {
        if (bleTwin_) bleTwin_->notifyLogLine(line);
    });

    char adv[32];
    // Prefer a short stable suffix from the end of the device UUID (more unique than
    // the time-based UUIDv7 prefix). Example: ILSS-LY-ac6f
    const std::string id = state_->getDeviceId();
    std::string suffix = "0000";
    if (id.size() >= 4) {
        suffix = id.substr(id.size() - 4);
    }
    snprintf(adv, sizeof(adv), "ILSS-LY-%s", suffix.c_str());
    bleTwin_->begin(adv);

    // Unpaired ready after power-up (goIdle alone would leave link mode).
    indications_->goIdle();
    indications_->setLinkLedMode(IndicationController::LinkLedMode::Unpaired);
    bleTwin_->publishStatus(indications_->current());

    initTasks();
    logger_->LOGI(TAG, "Digital twin running");
    eventLoop();
}
