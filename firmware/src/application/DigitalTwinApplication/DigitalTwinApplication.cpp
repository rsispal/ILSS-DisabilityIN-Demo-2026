#include "DigitalTwinApplication.h"
#include "../IndicationController/IndicationController.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/buzzer/Buzzer.h"
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

DigitalTwinApplication::DigitalTwinApplication(Logger* logger, LowLevel* lowLevel, State* state)
    : logger_(logger), lowLevel_(lowLevel), state_(state) {}

DigitalTwinApplication::~DigitalTwinApplication() {
    delete bleTwin_;
    delete indications_;
    delete sideButtons_;
    delete buzzer_;
    delete rgbLed_;
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

void DigitalTwinApplication::onBleDisconnected() {
    logger_->LOGI(TAG, "BLE disconnected — returning to idle");
    if (indications_) indications_->goIdle();
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
                case AppEventType::BleDisconnected:
                    onBleDisconnected();
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

    rgbLed_ = new RGBLED(state_, lowLevel_);
    rgbLed_->begin();
    rgbLed_->queueEffect(LedEffect::CHASE_FADE, LedColor::WHITE, Brightness::B50, 2000);

    buzzer_ = new Buzzer(state_, lowLevel_);
    buzzer_->begin();

    indications_ = new IndicationController(logger_, state_, lowLevel_, rgbLed_, buzzer_);
    indications_->begin();

    sideButtons_ = new SideButtons(state_, lowLevel_, HARDWARE_BUTTON_LEFT_PIN, HARDWARE_BUTTON_RIGHT_PIN);
    sideButtons_->begin();
    sideButtons_->setEventCallback([this](ButtonEvent event) {
        if (event == ButtonEvent::BOTH_HOLD) {
            AppEvent ev{AppEventType::ButtonBothHold};
            if (event_queue_) xQueueSend(event_queue_, &ev, 0);
        }
    });

    bleTwin_ = new BleTwin(logger_, &lowLevel_->get_bluetooth(), indications_, state_);
    bleTwin_->setTwinStateHandler([this](const ilss::TwinState& desired, ilss::TwinNakReason* reason) {
        return indications_->apply(desired, reason);
    });
    bleTwin_->setDisconnectHandler([this]() {
        AppEvent ev{AppEventType::BleDisconnected};
        if (event_queue_) xQueueSend(event_queue_, &ev, 0);
    });

    logger_->setBleLogSink([this](const char* line) {
        if (bleTwin_) bleTwin_->notifyLogLine(line);
    });

    char adv[32];
    snprintf(adv, sizeof(adv), "ILSS-LY-%.4s", state_->getDeviceId().c_str());
    bleTwin_->begin(adv);

    // Idle green after boot chase
    vTaskDelay(pdMS_TO_TICKS(2100));
    indications_->goIdle();
    bleTwin_->publishStatus(indications_->current());

    initTasks();
    logger_->LOGI(TAG, "Digital twin running");
    eventLoop();
}
