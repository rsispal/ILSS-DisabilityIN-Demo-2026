#include "WiFiLanyardApplication.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

WiFiLanyardApplication::WiFiLanyardApplication(Logger* logger, LowLevel* lowLevel, State* state) {
    this->state = state;
    this->logger = logger;
    this->lowLevel = lowLevel;
}

WiFiLanyardApplication::~WiFiLanyardApplication() {
}

void WiFiLanyardApplication::begin() {
    logger->LOGI(TAG, "WiFi Lanyard Application starting...");

    // Load default state
    state->loadDefaultState();

    logger->LOGI(TAG, "Application initialized. Entering main loop...");
    
    // Simple main loop
    while (true) {
        // Placeholder for main application logic
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
