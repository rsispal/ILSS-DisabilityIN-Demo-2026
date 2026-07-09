#include "ILSSLanyardApplication.h"
#include "../../features/azure-iot/AzureIoT.h"
#include "../../features/bluetooth/Bluetooth.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/side-buttons/SideButtons.h"
#include "../../features/buzzer/Buzzer.h"
#include "../../lowlevel/wifi/WiFiLowLevelDriver.h"
#include "../../lowlevel/haptics/DRV2605Driver.h"
#include "../../application/Hardware.h"
#include "../../utils/Utils.h"
#include "../../layers/ble-beacon/BLEBeacon.h"
#include "../../constants/Constants.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <sstream>

ILSSLanyardApplication::ILSSLanyardApplication(Logger* logger, LowLevel* lowLevel, State* state)
    : state(state), logger(logger), lowLevel(lowLevel),
      azureIoT(nullptr), bluetooth(nullptr), rgbLed(nullptr), 
      sideButtons(nullptr), buzzer(nullptr),
      currentMode(AppMode::BOOTUP),
      lastBeaconScanTime(0), lastLocationUpdateTime(0),
      lastFireAlertTime(0), lastPersonalAlertTime(0),
      eventIdCounter(1),
      scanInProgress(false), scanStartTime(0),
      buzzer_task_handle(nullptr), led_task_handle(nullptr),
      buzzer_queue(nullptr), led_queue(nullptr),
      scan_interval_timer(nullptr),
      fire_alert_timer(nullptr), personal_alert_timer(nullptr),
      fire_haptic_timer(nullptr), personal_haptic_timer(nullptr) {
}

ILSSLanyardApplication::~ILSSLanyardApplication() {
    // Deinitialize dual-core tasks
    deinitDualCoreTasks();
    
    // Stop and delete timers
    if (scan_interval_timer) {
        esp_timer_stop(scan_interval_timer);
        esp_timer_delete(scan_interval_timer);
    }
    if (fire_alert_timer) {
        esp_timer_stop(fire_alert_timer);
        esp_timer_delete(fire_alert_timer);
    }
    if (personal_alert_timer) {
        esp_timer_stop(personal_alert_timer);
        esp_timer_delete(personal_alert_timer);
    }
    if (fire_haptic_timer) {
        esp_timer_stop(fire_haptic_timer);
        esp_timer_delete(fire_haptic_timer);
    }
    if (personal_haptic_timer) {
        esp_timer_stop(personal_haptic_timer);
        esp_timer_delete(personal_haptic_timer);
    }
    
    // Delete feature instances
    if (buzzer) delete buzzer;
    if (sideButtons) delete sideButtons;
    if (rgbLed) delete rgbLed;
    if (bluetooth) delete bluetooth;
    if (azureIoT) delete azureIoT;
}

void ILSSLanyardApplication::begin() {
    logger->LOGI(TAG, "ILSS Lanyard Application starting...");
    
    // Initialize dual-core tasks (buzzer and LED on core 1)
    initDualCoreTasks();
    
    // Initialize RGB LED early so it can be used in bootup sequence
    rgbLed = new RGBLED(state, lowLevel);
    rgbLed->begin();
    
    // Bootup sequence
    bootupSequence();
    
    // Initialize features
    bluetooth = new Bluetooth(state, lowLevel);
    bluetooth->begin();
    
    // Set up Bluetooth scan complete callback
    bluetooth->setScanCompleteCallback([this]() {
        onScanComplete();
    });
    
    buzzer = new Buzzer(state, lowLevel);
    buzzer->begin();
    
    sideButtons = new SideButtons(state, lowLevel, HARDWARE_BUTTON_LEFT_PIN, HARDWARE_BUTTON_RIGHT_PIN);
    sideButtons->begin();
    
    // Set up button callback once
    if (sideButtons) {
        sideButtons->setEventCallback([this](ButtonEvent event) {
            // Handle button press down events - provide tactile feedback
            if (event == ButtonEvent::LEFT_PRESS_DOWN || event == ButtonEvent::RIGHT_PRESS_DOWN) {
                // Flash LED purple once (using DOUBLE_FLASH - duration handled by LED task)
                if (state->getEnableLedIndications()) {
                    sendLEDCommand(LEDCommand::SET_EFFECT,
                                   static_cast<uint32_t>(LedEffect::DOUBLE_FLASH),
                                   static_cast<uint32_t>(LedColor::PURPLE),
                                   static_cast<uint32_t>(Brightness::B100));
                }
                
                // Play haptic pattern 14
                if (state->getEnableHaptics() && lowLevel->get_haptics().isReady()) {
                    lowLevel->get_haptics().play_pattern(14);
                }
                
                // Play buzzer beep
                if (state->getEnableBuzzer()) {
                    sendBuzzerCommand(BuzzerCommand::BEEP, 2000, 100, 0);  // 2000Hz, 100ms beep
                }
                
                logger->LOGD(TAG, "Button press feedback: %s", 
                             event == ButtonEvent::LEFT_PRESS_DOWN ? "LEFT" : "RIGHT");
            }
            
            // Handle personal alert (both buttons held) - highest priority
            if (event == ButtonEvent::BOTH_HOLD) {
                if (currentMode == AppMode::QUIESCENT || currentMode == AppMode::FIRE_EVENT) {
                    logger->LOGI(TAG, "Personal Alert triggered!");
                    // Set mode flag - the current loop will detect the change and transition
                    // Don't call enterPersonalAlertMode() directly as it's blocking and we're in a callback context
                    currentMode = AppMode::PERSONAL_ALERT;
                    logger->LOGI(TAG, "Personal alert mode flag set, will transition on next loop iteration");
                }
            }
            
            // Handle mode button (left button held) to enter provisioning
            // Note: LEFT_HOLD currently fires after 5 seconds, specification requires 10 seconds
            // This may need adjustment in the button driver
            if (event == ButtonEvent::LEFT_HOLD && 
                currentMode != AppMode::FIRE_EVENT && 
                currentMode != AppMode::PERSONAL_ALERT) {
                logger->LOGI(TAG, "Mode button held, entering Provisioning Mode");
                
                // Clear state and reset to defaults
                state->loadDefaultState();
                state->setIsProvisioned(false);
                
                // Disconnect WiFi and Azure IoT
                if (azureIoT && azureIoT->isConnected()) {
                    azureIoT->disconnect();
                }
                lowLevel->get_wifi().disconnect();
                
                // Set mode flag - the current loop will detect the change and transition
                // Don't call enterProvisioningMode() directly as it's blocking and we're in a callback context
                currentMode = AppMode::PROVISIONING;
                logger->LOGI(TAG, "Provisioning mode flag set, will transition on next loop iteration");
            }
        });
    }
    
    azureIoT = new AzureIoT(state);
    azureIoT->begin();
    setupAzureMessageCallback();
    
    // Check if provisioned
    if (state->getIsProvisioned()) {
        // Check for WiFi credentials - if missing, enter error state
        if (state->getWifiSsid().empty() || state->getWifiPassword().empty()) {
            logger->LOGE(TAG, "Device is provisioned but WiFi credentials are missing!");
            logger->LOGE(TAG, "WiFi SSID: %s, Password: %s", 
                         state->getWifiSsid().empty() ? "MISSING" : "present",
                         state->getWifiPassword().empty() ? "MISSING" : "present");
            enterErrorMode();
            return;
        }
        
        logger->LOGI(TAG, "Device is provisioned, entering Quiescent Mode");
        
        // Connect WiFi if credentials are available
        if (!state->getWifiSsid().empty()) {
            // Only connect if not already connected
            if (!lowLevel->get_wifi().isConnected()) {
                logger->LOGI(TAG, "Connecting to WiFi: %s", state->getWifiSsid().c_str());
                lowLevel->get_wifi().connect(state->getWifiSsid(), state->getWifiPassword());
                
                // Wait for connection with timeout (up to 15 seconds)
                int timeout_ms = 15000;
                int elapsed = 0;
                while (!lowLevel->get_wifi().isConnected() && elapsed < timeout_ms) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    elapsed += 500;
                }
                
                if (lowLevel->get_wifi().isConnected()) {
                    logger->LOGI(TAG, "WiFi connected successfully");
                } else {
                    logger->LOGE(TAG, "WiFi connection failed or timed out");
                }
            } else {
                logger->LOGI(TAG, "WiFi already connected");
            }
        } else {
            logger->LOGW(TAG, "No WiFi SSID configured");
        }
        
        // Connect to Azure IoT if WiFi is connected
        if (lowLevel->get_wifi().isConnected()) {
            logger->LOGI(TAG, "Connecting to Azure IoT Hub...");
            azureIoT->connect();
            
            // Wait for connection with timeout (up to 10 seconds)
            int timeout_ms = 10000;
            int elapsed = 0;
            while (!azureIoT->isConnected() && elapsed < timeout_ms) {
                azureIoT->process(); // Process MQTT events
                vTaskDelay(pdMS_TO_TICKS(500));
                elapsed += 500;
            }
            
            if (azureIoT->isConnected()) {
                logger->LOGI(TAG, "Azure IoT Hub connected successfully");
            } else {
                logger->LOGE(TAG, "Azure IoT Hub connection failed or timed out");
            }
        } else {
            logger->LOGW(TAG, "WiFi not connected, skipping Azure IoT connection");
        }
        
        enterQuiescentMode();
    } else {
        logger->LOGI(TAG, "Device is not provisioned, entering Provisioning Mode");
        // Show pulsing blue LED - send to LED task
        sendLEDCommand(LEDCommand::SET_EFFECT,
                       static_cast<uint32_t>(LedEffect::PULSE),
                       static_cast<uint32_t>(LedColor::BLUE),
                       static_cast<uint32_t>(Brightness::B50));
        enterProvisioningMode();
    }
}

void ILSSLanyardApplication::bootupSequence() {
    logger->LOGI(TAG, "Bootup sequence starting...");
    
    // Show chasing white LED pattern for 3 seconds
    showBootupLEDPattern();
    
    logger->LOGI(TAG, "Bootup sequence complete");
}

void ILSSLanyardApplication::showBootupLEDPattern() {
    if (!rgbLed || !rgbLed->isReady()) {
        logger->LOGW(TAG, "RGB LED not ready for bootup pattern");
        return;
    }
    
    // Show chasing white LED pattern
    rgbLed->queueEffect(LedEffect::CHASE_FADE, LedColor::WHITE, Brightness::B50, 3000);
    
    // Process LED for 3 seconds
    uint32_t startTime = Utils::getCurrentTimestamp();
    while (Utils::getCurrentTimestamp() - startTime < 3000) {
        rgbLed->process();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void ILSSLanyardApplication::enterProvisioningMode() {
    logger->LOGI(TAG, "Entering Provisioning Mode");
    currentMode = AppMode::PROVISIONING;
    
    // Show chasing blue LED pattern - send to LED task
    sendLEDCommand(LEDCommand::SET_EFFECT,
                   static_cast<uint32_t>(LedEffect::CHASE_FADE),
                   static_cast<uint32_t>(LedColor::BLUE),
                   static_cast<uint32_t>(Brightness::B50));
    
    // Enter provisioning loop
    provisioningModeLoop();
}

void ILSSLanyardApplication::provisioningModeLoop() {
    logger->LOGI(TAG, "Provisioning Mode loop started");
    logger->LOGI(TAG, "Provisioning Mode: USB CLI JSON commands are available for configuration");
    
    // Disable ALL logging during USB protocol provisioning to prevent interference with JSON responses
    // ESP logs go over the same USB serial port as JSON protocol responses, so any log output
    // would interleave with and corrupt the JSON responses sent to the client.
    // This disables ESP-IDF logs for all components.
    esp_log_level_set("*", ESP_LOG_NONE);
    
    // Create USBCLI instance for JSON protocol processing
    USBCLI usbCli(logger, lowLevel, state);
    usbCli.begin();
    
    // LED and buzzer are now handled by dedicated tasks on core 1
    // No need to process them here
    
    while (currentMode == AppMode::PROVISIONING) {
        // Process JSON commands from USB
        usbCli.processJSONCommands();
        
        // Check for unexpected mode transitions (shouldn't happen, but safety check)
        if (currentMode != AppMode::PROVISIONING) {
            logger->LOGW(TAG, "Unexpected mode change detected in provisioning loop: %d", static_cast<int>(currentMode));
            break;  // Exit provisioning loop
        }
        
        // Check if provisioning is complete
        if (checkProvisioningComplete()) {
            logger->LOGI(TAG, "Provisioning complete, entering Quiescent Mode");
            logger->LOGI(TAG, "Provisioning details: SessionID=%s, FirstName=%s, LastName=%s, Persona=%s, SSID=%s",
                         state->getSessionId().c_str(),
                         state->getUserFirstName().c_str(),
                         state->getUserLastName().c_str(),
                         state->getPersona().c_str(),
                         state->getWifiSsid().c_str());
            
            // Connect WiFi (if not already connected from JSON command 4)
            if (!state->getWifiSsid().empty() && !lowLevel->get_wifi().isConnected()) {
                logger->LOGI(TAG, "Connecting to WiFi: %s", state->getWifiSsid().c_str());
                lowLevel->get_wifi().connect(state->getWifiSsid(), state->getWifiPassword());
                
                // Wait for WiFi connection (up to 10 seconds)
                int wifi_timeout_ms = 10000;
                int wifi_elapsed = 0;
                while (!lowLevel->get_wifi().isConnected() && wifi_elapsed < wifi_timeout_ms) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    wifi_elapsed += 500;
                }
                
                if (lowLevel->get_wifi().isConnected()) {
                    logger->LOGI(TAG, "WiFi connected successfully");
                } else {
                    logger->LOGE(TAG, "WiFi connection failed or timed out");
                }
            } else if (lowLevel->get_wifi().isConnected()) {
                logger->LOGI(TAG, "WiFi already connected");
            }
            
            // Connect to Azure IoT if WiFi is connected
            if (lowLevel->get_wifi().isConnected() && azureIoT) {
                logger->LOGI(TAG, "Connecting to Azure IoT Hub...");
                azureIoT->connect();
                
                // Wait for Azure IoT connection with timeout (up to 15 seconds)
                int azure_timeout_ms = 15000;
                int azure_elapsed = 0;
                while (!azureIoT->isConnected() && azure_elapsed < azure_timeout_ms) {
                    azureIoT->process(); // Process MQTT events
                    vTaskDelay(pdMS_TO_TICKS(500));
                    azure_elapsed += 500;
                }
                
                if (azureIoT->isConnected()) {
                    logger->LOGI(TAG, "Azure IoT Hub connected successfully");
                } else {
                    logger->LOGE(TAG, "Azure IoT Hub connection failed or timed out");
                }
            } else {
                if (!lowLevel->get_wifi().isConnected()) {
                    logger->LOGW(TAG, "WiFi not connected, skipping Azure IoT connection");
                }
                if (!azureIoT) {
                    logger->LOGW(TAG, "Azure IoT not initialized, skipping connection");
                }
            }
            
            // Restore logging before entering quiescent mode (which blocks)
            esp_log_level_set("*", ESP_LOG_INFO);
            
            enterQuiescentMode();
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Restore logging after provisioning mode exits (safety measure)
    esp_log_level_set("*", ESP_LOG_INFO);
    
    // Handle unexpected mode transitions after exiting provisioning loop
    if (currentMode != AppMode::PROVISIONING && currentMode != AppMode::QUIESCENT) {
        logger->LOGW(TAG, "Provisioning loop exited with unexpected mode: %d, transitioning to QUIESCENT", static_cast<int>(currentMode));
        currentMode = AppMode::QUIESCENT;
    }
    if (currentMode == AppMode::QUIESCENT) {
        // Logging already restored above, but ensure it's restored before entering quiescent mode
        esp_log_level_set("*", ESP_LOG_INFO);
        enterQuiescentMode();
        return;  // enterQuiescentMode() will call quiescentModeLoop() which blocks
    }
}

bool ILSSLanyardApplication::checkProvisioningComplete() {
    // Check if we have all required provisioning data
    return state->getIsProvisioned() &&
           !state->getWifiSsid().empty() &&
           !state->getSessionId().empty() &&
           !state->getPersona().empty() &&
           !state->getUserFirstName().empty() &&
           !state->getUserLastName().empty();
}

void ILSSLanyardApplication::enterQuiescentMode() {
    logger->LOGI(TAG, "Entering Quiescent Mode");
    currentMode = AppMode::QUIESCENT;
    
    // Initialize timing variables
    uint32_t now = Utils::getCurrentTimestamp();
    lastBeaconScanTime = now;
    lastLocationUpdateTime = now;
    
    // Set beacon scan interval to quiescent mode
    updateBeaconScanInterval(false);
    
    // Start beacon scanning
    if (bluetooth && state->getEnableHoneywellBeaconScanning()) {
        int scanInterval = state->getQuiescentBeaconScanModeScanIntervalMs();
        int scanWindow = scanInterval / 2;
        if (bluetooth->startBeaconScanning(scanInterval, scanWindow)) {
            logger->LOGI(TAG, "Beacon scanning started (interval: %d ms, window: %d ms)", scanInterval, scanWindow);
        } else {
            logger->LOGE(TAG, "Failed to start beacon scanning");
        }
    } else {
        if (!bluetooth) {
            logger->LOGW(TAG, "Bluetooth not initialized, cannot start beacon scanning");
        }
        if (!state->getEnableHoneywellBeaconScanning()) {
            logger->LOGW(TAG, "Honeywell beacon scanning is disabled in state");
        }
    }
    
    // Turn off LED (or show normal status) - send command to LED task
    sendLEDCommand(LEDCommand::STOP_EFFECT);
    
    // Don't send initial location update here - wait for first scan to complete
    // Location updates will be sent after each scan completes with beacon data
    if (!azureIoT || !azureIoT->isConnected()) {
        logger->LOGW(TAG, "Azure IoT not connected, location updates will be queued");
    }
    
    // Enter quiescent loop
    quiescentModeLoop();
}

void ILSSLanyardApplication::quiescentModeLoop() {
    logger->LOGI(TAG, "Quiescent Mode loop started");
    
    // Create scan interval timer
    int scanInterval = state->getQuiescentBeaconScanModeScanIntervalMs();
    if (!scan_interval_timer) {
        esp_timer_create_args_t scan_timer_args = {};
        scan_timer_args.callback = scanIntervalTimerCallback;
        scan_timer_args.arg = this;
        scan_timer_args.name = "ilss_scan_interval";
        scan_timer_args.dispatch_method = ESP_TIMER_TASK;
        esp_timer_create(&scan_timer_args, &scan_interval_timer);
    }
    
    // Start scan interval timer
    esp_timer_start_periodic(scan_interval_timer, scanInterval * 1000);  // Convert ms to microseconds
    
    // Start first scan immediately
    uint32_t now = Utils::getCurrentTimestamp();
    logger->LOGI(TAG, "Starting initial beacon scan");
    performBeaconScan();
    scanInProgress = true;
    scanStartTime = now;
    lastBeaconScanTime = now;
    
    // Reconnect attempt tracking (static to persist across function calls if needed)
    static uint32_t lastWifiReconnectAttempt = 0;
    static uint32_t lastAzureReconnectAttempt = 0;
    const uint32_t WIFI_RECONNECT_INTERVAL_MS = 30000;  // Try WiFi reconnect every 30 seconds
    const uint32_t AZURE_RECONNECT_INTERVAL_MS = 10000; // Try Azure reconnect every 10 seconds
    
    // Simplified loop - just coordinate state, all processing is in dedicated tasks
    while (currentMode == AppMode::QUIESCENT) {
        uint32_t now = Utils::getCurrentTimestamp();
        
        // Try to reconnect WiFi if disconnected and credentials are available
        if (!lowLevel->get_wifi().isConnected() && !state->getWifiSsid().empty()) {
            if (lastWifiReconnectAttempt == 0 || (now - lastWifiReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS)) {
                logger->LOGW(TAG, "WiFi disconnected, attempting reconnect to: %s", state->getWifiSsid().c_str());
                lowLevel->get_wifi().connect(state->getWifiSsid(), state->getWifiPassword());
                lastWifiReconnectAttempt = now;
            }
        } else if (lowLevel->get_wifi().isConnected()) {
            // Reset reconnect attempt timer when connected
            lastWifiReconnectAttempt = 0;
        }
        
        // Process Azure IoT (non-blocking, runs on main task)
        if (azureIoT) {
            azureIoT->process();
            
            // Reconnect Azure IoT if WiFi is connected but Azure IoT is not
            if (lowLevel->get_wifi().isConnected() && !azureIoT->isConnected()) {
                if (lastAzureReconnectAttempt == 0 || (now - lastAzureReconnectAttempt >= AZURE_RECONNECT_INTERVAL_MS)) {
                    logger->LOGW(TAG, "Azure IoT disconnected, attempting reconnect...");
                    azureIoT->connect();
                    lastAzureReconnectAttempt = now;
                }
            } else if (azureIoT->isConnected()) {
                // Reset reconnect attempt timer when connected
                lastAzureReconnectAttempt = 0;
            }
        }
        
        // Check buttons (non-blocking)
        checkButtons();
        checkModeButton();
        
        // Check for mode transitions (e.g., from Azure message handler)
        if (currentMode == AppMode::FIRE_EVENT) {
            logger->LOGI(TAG, "Mode transition detected: QUIESCENT -> FIRE_EVENT");
            break;  // Exit quiescent loop, will enter fire event mode
        }
        if (currentMode == AppMode::PERSONAL_ALERT) {
            logger->LOGI(TAG, "Mode transition detected: QUIESCENT -> PERSONAL_ALERT");
            break;  // Exit quiescent loop, will enter personal alert mode
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Handle mode transitions after exiting quiescent loop
    if (currentMode == AppMode::FIRE_EVENT) {
        enterFireEventMode();
        return;  // enterFireEventMode() will call fireEventModeLoop() which blocks
    }
    if (currentMode == AppMode::PERSONAL_ALERT) {
        enterPersonalAlertMode();
        return;  // enterPersonalAlertMode() will call personalAlertModeLoop() which blocks
    }
    
    // Stop timers when exiting mode (safely)
    if (scan_interval_timer) {
        esp_err_t err = esp_timer_stop(scan_interval_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop scan interval timer: %s", esp_err_to_name(err));
        }
    }
    
    logger->LOGI(TAG, "Quiescent Mode loop exited");
}

void ILSSLanyardApplication::performBeaconScan() {
    if (!bluetooth || !state->getEnableHoneywellBeaconScanning()) {
        if (!bluetooth) {
            logger->LOGW(TAG, "Bluetooth not initialized, cannot perform beacon scan");
        }
        if (!state->getEnableHoneywellBeaconScanning()) {
            logger->LOGD(TAG, "Honeywell beacon scanning is disabled");
        }
        return;
    }
    
    // Stop any existing scan first
    if (bluetooth->isBeaconScanning()) {
        logger->LOGD(TAG, "Stopping existing scan before starting new one");
        bluetooth->stopBeaconScanning();
        vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay to ensure scan is stopped
    }
    
    // Start a new scan with the configured interval and window
    int scanInterval = state->isFastBeaconScanMode() ? 
        state->getFastBeaconScanModeScanIntervalMs() : 
        state->getQuiescentBeaconScanModeScanIntervalMs();
    int scanWindow = SCAN_WINDOW_MS;  // Use fixed 5-second window
    if (bluetooth->startBeaconScanning(scanInterval, scanWindow)) {
        logger->LOGI(TAG, "Started beacon scan (interval: %d ms, window: %d ms)", scanInterval, scanWindow);
    } else {
        logger->LOGE(TAG, "Failed to start beacon scan");
    }
}

void ILSSLanyardApplication::sendLocationUpdate() {
    if (!azureIoT) {
        logger->LOGW(TAG, "Azure IoT not initialized, cannot send location update");
        return;
    }
    
    if (!azureIoT->isConnected()) {
        logger->LOGW(TAG, "Azure IoT not connected, cannot send location update");
        return;
    }
    
    std::string message = buildLocationUpdateMessage();
    if (!message.empty()) {
        bool success = azureIoT->publishTelemetry(message.c_str());
        if (success) {
            logger->LOGI(TAG, "Sent LOCATION_UPDATE: %s", message.c_str());
            
            // In quiescent mode, show a nice water drop effect in blue at low brightness
            if (currentMode == AppMode::QUIESCENT && state->getEnableLedIndications()) {
                logger->LOGD(TAG, "Showing blue water drop effect for location update");
                sendLEDCommand(LEDCommand::SET_EFFECT,
                               static_cast<uint32_t>(LedEffect::WATER_DROP),
                               static_cast<uint32_t>(LedColor::BLUE),
                               static_cast<uint32_t>(Brightness::B20));  // 20% brightness for subtle indication
            }
        } else {
            logger->LOGE(TAG, "Failed to send LOCATION_UPDATE");
        }
    } else {
        logger->LOGW(TAG, "Location update message is empty");
    }
}

std::string ILSSLanyardApplication::buildLocationUpdateMessage() {
    JsonBuilder json = JsonBuilder::object();
    
    json.addNumber("id", generateEventId());
    json.addString("event", "LOCATION_UPDATE");
    json.addString("sessionId", state->getSessionId().c_str());
    json.addString("deviceId", state->getDeviceId().c_str());
    json.addNumber("errorCode", 0);
    
    // Get best beacons
    BLEBeacon* best = state->getBestBeacon();
    if (best) {
        std::stringstream ss;
        ss << best->getIdentifier();
        json.addString("primaryPointId", ss.str().c_str());
    } else {
        json.addNull("primaryPointId");
    }
    
    // Get second best beacon if available
    const auto& beacons = state->getBeacons();
    if (beacons.size() > 1) {
        std::stringstream ss;
        ss << beacons[1].getIdentifier();
        json.addString("secondaryPointId", ss.str().c_str());
    } else {
        json.addNull("secondaryPointId");
    }
    
    // Get third best beacon if available
    if (beacons.size() > 2) {
        std::stringstream ss;
        ss << beacons[2].getIdentifier();
        json.addString("tertiaryPointId", ss.str().c_str());
    } else {
        json.addNull("tertiaryPointId");
    }
    
    json.addString("batteryStatus", getBatteryStatusString().c_str());
    json.addNumber("batteryLevel", state->getBatteryLevel());
    
    return json.toString();
}

void ILSSLanyardApplication::checkButtons() {
    // Button checking is handled via callback set in begin()
    // This method is kept for future use if needed
}

void ILSSLanyardApplication::checkModeButton() {
    // Mode button checking is handled via callback set in begin()
    // This method is kept for future use if needed
}

void ILSSLanyardApplication::enterFireEventMode() {
    logger->LOGI(TAG, "Entering Fire Event Mode");
    currentMode = AppMode::FIRE_EVENT;
    
    // Set fast beacon scan interval
    updateBeaconScanInterval(true);
    state->setFastBeaconScanMode(true);
    
    // Start fast beacon scanning
    if (bluetooth && state->getEnableHoneywellBeaconScanning()) {
        bluetooth->startBeaconScanning();
    }
    
    // Send location update immediately
    sendLocationUpdate();
    
    // Start fire alerts immediately
    updateFireAlerts();
    lastFireAlertTime = Utils::getCurrentTimestamp();
    
    // Reset timing
    lastBeaconScanTime = Utils::getCurrentTimestamp();
    lastLocationUpdateTime = Utils::getCurrentTimestamp();
    
    // Enter fire event loop
    fireEventModeLoop();
}

void ILSSLanyardApplication::fireEventModeLoop() {
    logger->LOGI(TAG, "Fire Event Mode loop started");
    
    // Create fire alert timer (every 15 seconds)
    if (!fire_alert_timer) {
        esp_timer_create_args_t fire_timer_args = {};
        fire_timer_args.callback = fireAlertTimerCallback;
        fire_timer_args.arg = this;
        fire_timer_args.name = "ilss_fire_alert";
        fire_timer_args.dispatch_method = ESP_TIMER_TASK;
        esp_timer_create(&fire_timer_args, &fire_alert_timer);
    }
    esp_timer_start_periodic(fire_alert_timer, 15000 * 1000);  // 15 seconds
    
    // Create fire haptic timer (every 3 seconds)
    if (!fire_haptic_timer) {
        esp_timer_create_args_t fire_haptic_timer_args = {};
        fire_haptic_timer_args.callback = fireHapticTimerCallback;
        fire_haptic_timer_args.arg = this;
        fire_haptic_timer_args.name = "ilss_fire_haptic";
        fire_haptic_timer_args.dispatch_method = ESP_TIMER_TASK;
        esp_timer_create(&fire_haptic_timer_args, &fire_haptic_timer);
    }
    esp_timer_start_periodic(fire_haptic_timer, 3000 * 1000);  // 3 seconds
    
    // Update scan interval timer for fast scanning
    int scanInterval = state->getFastBeaconScanModeScanIntervalMs();
    if (scan_interval_timer) {
        esp_err_t err = esp_timer_stop(scan_interval_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop scan interval timer: %s", esp_err_to_name(err));
        }
        err = esp_timer_start_periodic(scan_interval_timer, scanInterval * 1000);
        if (err != ESP_OK) {
            logger->LOGE(TAG, "Failed to start scan interval timer: %s", esp_err_to_name(err));
        }
    }
    
    // Start first scan immediately
    uint32_t now = Utils::getCurrentTimestamp();
    performBeaconScan();
    scanInProgress = true;
    scanStartTime = now;
    lastBeaconScanTime = now;
    
    // Trigger initial fire alerts (LED)
    updateFireAlerts();
    // Start buzzer once (continuous, will run until stopped)
    startFireAlarmBuzzer();
    lastFireAlertTime = now;
    
    // Simplified loop - just coordinate state
    while (currentMode == AppMode::FIRE_EVENT) {
        // Process Azure IoT (non-blocking)
        if (azureIoT) {
            azureIoT->process();
        }
        
        // Check buttons (non-blocking) - personal alert can still be triggered during fire event
        checkButtons();
        
        // Check for mode transitions (e.g., from Azure message handler)
        if (currentMode == AppMode::QUIESCENT) {
            logger->LOGI(TAG, "Mode transition detected: FIRE_EVENT -> QUIESCENT");
            break;  // Exit fire event loop, will enter quiescent mode
        }
        if (currentMode == AppMode::PERSONAL_ALERT) {
            logger->LOGI(TAG, "Mode transition detected: FIRE_EVENT -> PERSONAL_ALERT");
            break;  // Exit fire event loop, will enter personal alert mode
        }
        
        // Check for scan completion and trigger periodic scans
        if (!scanInProgress && bluetooth && state->getEnableHoneywellBeaconScanning()) {
            uint32_t now = Utils::getCurrentTimestamp();
            int scanInterval = state->getFastBeaconScanModeScanIntervalMs();
            if (now - lastBeaconScanTime >= scanInterval) {
                logger->LOGD(TAG, "Triggering periodic beacon scan in fire event mode");
                performBeaconScan();
                scanInProgress = true;
                scanStartTime = now;
                lastBeaconScanTime = now;
            }
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Handle mode transitions after exiting fire event loop
    if (currentMode == AppMode::QUIESCENT) {
        enterQuiescentMode();
        return;  // enterQuiescentMode() will call quiescentModeLoop() which blocks
    }
    if (currentMode == AppMode::PERSONAL_ALERT) {
        enterPersonalAlertMode();
        return;  // enterPersonalAlertMode() will call personalAlertModeLoop() which blocks
    }
    
    // Stop timers when exiting mode (safely)
    if (fire_alert_timer) {
        esp_err_t err = esp_timer_stop(fire_alert_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop fire alert timer: %s", esp_err_to_name(err));
        }
    }
    if (fire_haptic_timer) {
        esp_err_t err = esp_timer_stop(fire_haptic_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop fire haptic timer: %s", esp_err_to_name(err));
        }
    }
    
    // Stop haptic pattern 118 (continuous pattern needs explicit stop)
    if (lowLevel->get_haptics().isReady()) {
        lowLevel->get_haptics().stop();
    }
    
    logger->LOGI(TAG, "Fire Event Mode loop exited");
}

void ILSSLanyardApplication::updateFireAlerts() {
    logger->LOGD(TAG, "Updating fire alerts (LED, buzzer, haptics)");
    
    // Double flashing red LED (every 2 seconds) - continuous effect
    // The LED task will handle the continuous flashing
    sendLEDCommand(LEDCommand::SET_EFFECT, 
                   static_cast<uint32_t>(LedEffect::DOUBLE_FLASH),
                   static_cast<uint32_t>(LedColor::RED),
                   static_cast<uint32_t>(Brightness::B100));
    
    // Note: Buzzer is started once when entering fire event mode (not restarted here)
    // Note: Haptic pattern 118 is now handled by fire_haptic_timer (every 3 seconds)
}

void ILSSLanyardApplication::startFireAlarmBuzzer() {
    // Code-3 sweep siren - send to buzzer task
    // Code-3 temporal pattern with warbling siren sweep
    // Use cycles=0 for continuous operation (infinite until stopped)
    if (state->getEnableBuzzer()) {
        logger->LOGD(TAG, "Starting buzzer Code-3 sweep (continuous)");
        sendBuzzerCommand(BuzzerCommand::QUEUE_CODE3_SWEEP, 2700, 3500, 0);  // cycles=0 = infinite until stopped
    }
}

void ILSSLanyardApplication::enterPersonalAlertMode() {
    logger->LOGI(TAG, "Entering Personal Alert Mode");
    currentMode = AppMode::PERSONAL_ALERT;
    
    // Set fast beacon scan interval
    updateBeaconScanInterval(true);
    state->setFastBeaconScanMode(true);
    
    // Send personal alert message immediately
    std::string message = buildPersonalAlertMessage();
    if (azureIoT && azureIoT->isConnected()) {
        azureIoT->publishTelemetry(message.c_str());
        logger->LOGI(TAG, "Sent PERSONAL_ALERT: %s", message.c_str());
    }
    
    // Send location update
    sendLocationUpdate();
    
    // Start personal alert indicators immediately
    updatePersonalAlerts();
    lastPersonalAlertTime = Utils::getCurrentTimestamp();
    
    // Reset timing
    lastBeaconScanTime = Utils::getCurrentTimestamp();
    lastLocationUpdateTime = Utils::getCurrentTimestamp();
    
    // Enter personal alert loop
    personalAlertModeLoop();
}

void ILSSLanyardApplication::personalAlertModeLoop() {
    logger->LOGI(TAG, "Personal Alert Mode loop started");
    
    // Create personal alert timer (every 5 seconds)
    if (!personal_alert_timer) {
        esp_timer_create_args_t pa_timer_args = {};
        pa_timer_args.callback = personalAlertTimerCallback;
        pa_timer_args.arg = this;
        pa_timer_args.name = "ilss_personal_alert";
        pa_timer_args.dispatch_method = ESP_TIMER_TASK;
        esp_timer_create(&pa_timer_args, &personal_alert_timer);
    }
    esp_timer_start_periodic(personal_alert_timer, 5000 * 1000);  // 5 seconds
    
    // Create personal alert haptic timer (every 3 seconds)
    if (!personal_haptic_timer) {
        esp_timer_create_args_t pa_haptic_timer_args = {};
        pa_haptic_timer_args.callback = personalHapticTimerCallback;
        pa_haptic_timer_args.arg = this;
        pa_haptic_timer_args.name = "ilss_personal_haptic";
        pa_haptic_timer_args.dispatch_method = ESP_TIMER_TASK;
        esp_timer_create(&pa_haptic_timer_args, &personal_haptic_timer);
    }
    esp_timer_start_periodic(personal_haptic_timer, 3000 * 1000);  // 3 seconds
    
    // Update scan interval timer for fast scanning
    int scanInterval = state->getFastBeaconScanModeScanIntervalMs();
    if (scan_interval_timer) {
        esp_err_t err = esp_timer_stop(scan_interval_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop scan interval timer: %s", esp_err_to_name(err));
        }
        err = esp_timer_start_periodic(scan_interval_timer, scanInterval * 1000);
        if (err != ESP_OK) {
            logger->LOGE(TAG, "Failed to start scan interval timer: %s", esp_err_to_name(err));
        }
    }
    
    // Start first scan immediately
    uint32_t now = Utils::getCurrentTimestamp();
    performBeaconScan();
    scanInProgress = true;
    scanStartTime = now;
    lastBeaconScanTime = now;
    
    // Trigger initial personal alerts (LED)
    updatePersonalAlerts();
    // Start buzzer once (continuous, will run until stopped)
    startPersonalAlertBuzzer();
    lastPersonalAlertTime = now;
    
    // Simplified loop - just coordinate state
    while (currentMode == AppMode::PERSONAL_ALERT) {
        // Process Azure IoT (non-blocking)
        if (azureIoT) {
            azureIoT->process();
        }
        
        // Check for mode transitions (e.g., from Azure message handler)
        if (currentMode == AppMode::QUIESCENT) {
            logger->LOGI(TAG, "Mode transition detected: PERSONAL_ALERT -> QUIESCENT");
            break;  // Exit personal alert loop, will enter quiescent mode
        }
        
        // Check for scan completion and trigger periodic scans
        if (!scanInProgress && bluetooth && state->getEnableHoneywellBeaconScanning()) {
            uint32_t now = Utils::getCurrentTimestamp();
            int scanInterval = state->getFastBeaconScanModeScanIntervalMs();
            if (now - lastBeaconScanTime >= scanInterval) {
                logger->LOGD(TAG, "Triggering periodic beacon scan in personal alert mode");
                performBeaconScan();
                scanInProgress = true;
                scanStartTime = now;
                lastBeaconScanTime = now;
            }
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Stop timers when exiting mode (safely)
    if (personal_alert_timer) {
        esp_err_t err = esp_timer_stop(personal_alert_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop personal alert timer: %s", esp_err_to_name(err));
        }
    }
    if (personal_haptic_timer) {
        esp_err_t err = esp_timer_stop(personal_haptic_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop personal haptic timer: %s", esp_err_to_name(err));
        }
    }
    
    // Stop haptic pattern 118 (continuous pattern needs explicit stop)
    if (lowLevel->get_haptics().isReady()) {
        lowLevel->get_haptics().stop();
    }
    
    logger->LOGI(TAG, "Personal Alert Mode loop exited");
    
    // Handle mode transitions after exiting personal alert loop
    if (currentMode == AppMode::QUIESCENT) {
        enterQuiescentMode();
        return;  // enterQuiescentMode() will call quiescentModeLoop() which blocks
    }
}

void ILSSLanyardApplication::updatePersonalAlerts() {
    // Pulsing purple LED - send to LED task
    sendLEDCommand(LEDCommand::SET_EFFECT,
                   static_cast<uint32_t>(LedEffect::RAPID_PULSE),
                   static_cast<uint32_t>(LedColor::PURPLE),
                   static_cast<uint32_t>(Brightness::B100));
    
    // Note: Buzzer is started once when entering personal alert mode (not restarted here)
    // Note: Haptic pattern 118 is now handled by personal_haptic_timer (every 3 seconds)
}

void ILSSLanyardApplication::startPersonalAlertBuzzer() {
    // Code-3 siren - send to buzzer task
    // Code-3 temporal pattern with smooth up/down siren sweep
    // Use cycles=0 for continuous operation (infinite until stopped)
    if (state->getEnableBuzzer()) {
        logger->LOGD(TAG, "Starting buzzer Code-3 siren (continuous)");
        sendBuzzerCommand(BuzzerCommand::QUEUE_CODE3_SIREN, 2700, 3500, 0);  // cycles=0 = infinite until stopped
    }
}

void ILSSLanyardApplication::enterInactivityAlertMode() {
    logger->LOGI(TAG, "Entering Inactivity Alert Mode (stub)");
    currentMode = AppMode::INACTIVITY_ALERT;
    
    // Stub implementation - will be implemented when accelerometer is available
    // For now, just return to quiescent mode
    enterQuiescentMode();
}

void ILSSLanyardApplication::enterErrorMode() {
    logger->LOGE(TAG, "Entering ERROR Mode - WiFi credentials missing");
    currentMode = AppMode::ERROR;
    
    // Start red LED rapid pulse effect (continuous flashing)
    sendLEDCommand(LEDCommand::SET_EFFECT,
                   static_cast<uint32_t>(LedEffect::FLASH_2S),
                   static_cast<uint32_t>(LedColor::RED),
                   static_cast<uint32_t>(Brightness::B100));
    
    // Play low pitch buzzer beep (600Hz, 200ms) repeatedly
    if (state->getEnableBuzzer()) {
        sendBuzzerCommand(BuzzerCommand::BEEP, 600, 200, 0);  // Low pitch beep
    }
    
    // Enter error mode loop
    errorModeLoop();
}

void ILSSLanyardApplication::errorModeLoop() {
    logger->LOGE(TAG, "Error Mode loop started - Hold LEFT button for 10 seconds to factory reset");
    
    const uint32_t FACTORY_RESET_HOLD_TIME_MS = 10000;  // 10 seconds
    bool leftButtonPressed = false;
    uint32_t leftButtonPressStartTime = 0;
    
    while (currentMode == AppMode::ERROR) {
        uint32_t now = Utils::getCurrentTimestamp();
        
        // Check LEFT button state
        bool leftPressed = sideButtons && sideButtons->isLeftPressed();
        
        if (leftPressed && !leftButtonPressed) {
            // Button just pressed
            leftButtonPressed = true;
            leftButtonPressStartTime = now;
            logger->LOGI(TAG, "LEFT button pressed - hold for 10 seconds to factory reset");
        } else if (!leftPressed && leftButtonPressed) {
            // Button released before 10 seconds
            leftButtonPressed = false;
            leftButtonPressStartTime = 0;
            logger->LOGD(TAG, "LEFT button released before factory reset threshold");
        } else if (leftPressed && leftButtonPressed) {
            // Button still held - check if 10 seconds elapsed
            uint32_t holdDuration = now - leftButtonPressStartTime;
            if (holdDuration >= FACTORY_RESET_HOLD_TIME_MS) {
                logger->LOGE(TAG, "Factory reset triggered by 10-second LEFT button hold");
                performFactoryReset();
                return;  // performFactoryReset() will reboot, so we won't return here
            }
        }
        
        // Continue flashing red LED and beeping
        // LED is handled by LED task, buzzer needs to be retriggered periodically
        static uint32_t lastBeepTime = 0;
        if (state->getEnableBuzzer() && (now - lastBeepTime >= 2000)) {  // Beep every 2 seconds
            sendBuzzerCommand(BuzzerCommand::BEEP, 600, 200, 0);  // Low pitch beep
            lastBeepTime = now;
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void ILSSLanyardApplication::performFactoryReset() {
    logger->LOGE(TAG, "Performing factory reset - erasing NVS and rebooting");
    
    // Erase all NVS data
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        logger->LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
        // Continue anyway to attempt reboot
    } else {
        logger->LOGI(TAG, "NVS erased successfully");
    }
    
    // Small delay before reboot
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Reboot the system
    logger->LOGI(TAG, "Rebooting system...");
    esp_restart();
}

std::string ILSSLanyardApplication::buildPersonalAlertMessage() {
    JsonBuilder json = JsonBuilder::object();
    
    json.addNumber("id", generateEventId());
    json.addString("event", "PERSONAL_ALERT");
    json.addString("sessionId", state->getSessionId().c_str());
    json.addString("deviceId", state->getDeviceId().c_str());
    json.addNumber("errorCode", 0);
    
    // Get best beacons
    BLEBeacon* best = state->getBestBeacon();
    if (best) {
        std::stringstream ss;
        ss << best->getIdentifier();
        json.addString("primaryPointId", ss.str().c_str());
    } else {
        json.addNull("primaryPointId");
    }
    
    const auto& beacons = state->getBeacons();
    if (beacons.size() > 1) {
        std::stringstream ss;
        ss << beacons[1].getIdentifier();
        json.addString("secondaryPointId", ss.str().c_str());
    } else {
        json.addNull("secondaryPointId");
    }
    
    if (beacons.size() > 2) {
        std::stringstream ss;
        ss << beacons[2].getIdentifier();
        json.addString("tertiaryPointId", ss.str().c_str());
    } else {
        json.addNull("tertiaryPointId");
    }
    
    json.addString("batteryStatus", getBatteryStatusString().c_str());
    json.addNumber("batteryLevel", state->getBatteryLevel());
    
    return json.toString();
}

std::string ILSSLanyardApplication::buildInactivityAlertMessage() {
    JsonBuilder json = JsonBuilder::object();
    
    json.addNumber("id", generateEventId());
    json.addString("event", "INACTIVITY_ALERT");
    json.addString("sessionId", state->getSessionId().c_str());
    json.addString("deviceId", state->getDeviceId().c_str());
    json.addNumber("errorCode", 0);
    
    BLEBeacon* best = state->getBestBeacon();
    if (best) {
        std::stringstream ss;
        ss << best->getIdentifier();
        json.addString("primaryPointId", ss.str().c_str());
    } else {
        json.addNull("primaryPointId");
    }
    
    const auto& beacons = state->getBeacons();
    if (beacons.size() > 1) {
        std::stringstream ss;
        ss << beacons[1].getIdentifier();
        json.addString("secondaryPointId", ss.str().c_str());
    } else {
        json.addNull("secondaryPointId");
    }
    
    if (beacons.size() > 2) {
        std::stringstream ss;
        ss << beacons[2].getIdentifier();
        json.addString("tertiaryPointId", ss.str().c_str());
    } else {
        json.addNull("tertiaryPointId");
    }
    
    json.addString("batteryStatus", getBatteryStatusString().c_str());
    json.addNumber("batteryLevel", state->getBatteryLevel());
    
    return json.toString();
}

void ILSSLanyardApplication::setupAzureMessageCallback() {
    if (azureIoT) {
        azureIoT->setMessageCallback([this](const char* topic, const uint8_t* payload, size_t len) {
            handleAzureMessage(topic, payload, len);
        });
    }
}

void ILSSLanyardApplication::handleAzureMessage(const char* topic, const uint8_t* payload, size_t len) {
    if (!payload || len == 0) {
        return;
    }
    
    std::string message((const char*)payload, len);
    logger->LOGI(TAG, "Received Azure message on topic %s: %s", topic, message.c_str());
    
    JsonParser parser(message);
    if (!parser.valid()) {
        logger->LOGW(TAG, "Invalid JSON in Azure message");
        return;
    }
    
    std::string event = parser.getString("event", "");
    
    if (event == "FIRE_ALARM") {
        processFireAlarmMessage(parser);
    } else if (event == "FIRE_ALARM_RESET") {
        processFireAlarmResetMessage(parser);
    } else if (event == "PERSONAL_ALERT_RESET" || event == "PERSONAL_ALERT_CLEAR") {
        processPersonalAlertResetMessage(parser);
    } else if (event == "INACTIVITY_ALERT_RESET" || event == "INACTIVITY_ALERT_CLEAR") {
        processInactivityAlertResetMessage(parser);
    }
}

void ILSSLanyardApplication::processFireAlarmMessage(const JsonParser& parser) {
    logger->LOGI(TAG, "Processing FIRE_ALARM message");
    
    // Only enter fire event mode if not in personal alert mode
    if (currentMode != AppMode::PERSONAL_ALERT) {
        state->setFastBeaconScanMode(true);
        // Set mode flag - the current loop will detect the change and transition
        // Don't call enterFireEventMode() directly as it's blocking and we're in a callback context
        currentMode = AppMode::FIRE_EVENT;
        logger->LOGI(TAG, "Fire alarm mode flag set, will transition on next loop iteration");
    }
}

void ILSSLanyardApplication::processFireAlarmResetMessage(const JsonParser& parser) {
    logger->LOGI(TAG, "Processing FIRE_ALARM_RESET message");
    
    // Stop all fire-related timers IMMEDIATELY to prevent further buzzer triggers
    if (fire_alert_timer) {
        esp_err_t err = esp_timer_stop(fire_alert_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop fire alert timer: %s", esp_err_to_name(err));
        }
    }
    if (fire_haptic_timer) {
        esp_err_t err = esp_timer_stop(fire_haptic_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop fire haptic timer: %s", esp_err_to_name(err));
        }
    }
    
    // Stop buzzer immediately
    sendBuzzerCommand(BuzzerCommand::STOP);
    
    // Stop LED effect
    sendLEDCommand(LEDCommand::STOP_EFFECT);
    
    // Stop haptic pattern 118 (continuous pattern needs explicit stop)
    if (lowLevel->get_haptics().isReady()) {
        lowLevel->get_haptics().stop();
    }
    
    // Return to quiescent mode
    if (currentMode == AppMode::FIRE_EVENT) {
        state->setFastBeaconScanMode(false);
        // Set mode flag - the current loop will detect the change and transition
        // Don't call enterQuiescentMode() directly as it's blocking and we're in a callback context
        currentMode = AppMode::QUIESCENT;
        logger->LOGI(TAG, "Quiescent mode flag set, will transition on next loop iteration");
    }
}

void ILSSLanyardApplication::processPersonalAlertResetMessage(const JsonParser& parser) {
    logger->LOGI(TAG, "Processing PERSONAL_ALERT_RESET message");
    
    // Stop all personal alert timers IMMEDIATELY to prevent further buzzer triggers
    if (personal_alert_timer) {
        esp_err_t err = esp_timer_stop(personal_alert_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop personal alert timer: %s", esp_err_to_name(err));
        }
    }
    if (personal_haptic_timer) {
        esp_err_t err = esp_timer_stop(personal_haptic_timer);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            logger->LOGW(TAG, "Failed to stop personal haptic timer: %s", esp_err_to_name(err));
        }
    }
    
    // Stop buzzer immediately
    sendBuzzerCommand(BuzzerCommand::STOP);
    
    // Stop LED effect
    sendLEDCommand(LEDCommand::STOP_EFFECT);
    
    // Stop haptic pattern 118 (continuous pattern needs explicit stop)
    if (lowLevel->get_haptics().isReady()) {
        lowLevel->get_haptics().stop();
    }
    
    // Return to quiescent mode
    if (currentMode == AppMode::PERSONAL_ALERT) {
        state->setFastBeaconScanMode(false);
        // Set mode flag - the current loop will detect the change and transition
        // Don't call enterQuiescentMode() directly as it's blocking and we're in a callback context
        currentMode = AppMode::QUIESCENT;
        logger->LOGI(TAG, "Quiescent mode flag set, will transition on next loop iteration");
    }
}

void ILSSLanyardApplication::processInactivityAlertResetMessage(const JsonParser& parser) {
    logger->LOGI(TAG, "Processing INACTIVITY_ALERT_RESET message");
    
    // Return to quiescent mode
    if (currentMode == AppMode::INACTIVITY_ALERT) {
        // Set mode flag - the current loop will detect the change and transition
        // Don't call enterQuiescentMode() directly as it's blocking and we're in a callback context
        currentMode = AppMode::QUIESCENT;
        logger->LOGI(TAG, "Quiescent mode flag set, will transition on next loop iteration");
    }
}

std::string ILSSLanyardApplication::getBatteryStatusString() {
    BatteryChargingStatus status = state->getBatteryChargingStatus();
    uint8_t level = state->getBatteryLevel();
    
    if (status == BatteryChargingStatus::CHARGING) {
        return "charging";
    } else if (level < 20) {
        return "low";
    } else if (level > 80) {
        return "normal";
    } else {
        return "normal";
    }
}

int ILSSLanyardApplication::generateEventId() {
    return eventIdCounter++;
}

// ============================================================================
// Dual-Core Task Architecture Implementation
// ============================================================================

void ILSSLanyardApplication::initDualCoreTasks() {
    // Create queues for buzzer and LED commands
    buzzer_queue = xQueueCreate(10, sizeof(BuzzerCommandMsg));
    led_queue = xQueueCreate(10, sizeof(LEDCommandMsg));
    
    if (!buzzer_queue || !led_queue) {
        logger->LOGE(TAG, "Failed to create command queues");
        return;
    }
    
    // Create buzzer task pinned to core 1
    xTaskCreatePinnedToCore(
        buzzerTask,
        "buzzer_task",
        4096,  // Stack size
        this,
        5,     // Priority (higher than default)
        &buzzer_task_handle,
        1      // Core 1
    );
    
    // Create LED task pinned to core 1
    xTaskCreatePinnedToCore(
        ledTask,
        "led_task",
        4096,  // Stack size
        this,
        4,     // Priority (slightly lower than buzzer)
        &led_task_handle,
        1      // Core 1
    );
    
    if (!buzzer_task_handle || !led_task_handle) {
        logger->LOGE(TAG, "Failed to create dual-core tasks");
        return;
    }
    
    logger->LOGI(TAG, "Dual-core tasks initialized (buzzer and LED on core 1)");
}

void ILSSLanyardApplication::deinitDualCoreTasks() {
    // Send stop commands to tasks
    if (buzzer_queue) {
        BuzzerCommandMsg stop_cmd = {BuzzerCommand::STOP, 0, 0, 0};
        xQueueSend(buzzer_queue, &stop_cmd, portMAX_DELAY);
    }
    
    if (led_queue) {
        LEDCommandMsg stop_cmd = {LEDCommand::STOP_EFFECT, 0, 0, 0};
        xQueueSend(led_queue, &stop_cmd, portMAX_DELAY);
    }
    
    // Wait a bit for tasks to process stop commands
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Delete tasks
    if (buzzer_task_handle) {
        vTaskDelete(buzzer_task_handle);
        buzzer_task_handle = nullptr;
    }
    
    if (led_task_handle) {
        vTaskDelete(led_task_handle);
        led_task_handle = nullptr;
    }
    
    // Delete queues
    if (buzzer_queue) {
        vQueueDelete(buzzer_queue);
        buzzer_queue = nullptr;
    }
    
    if (led_queue) {
        vQueueDelete(led_queue);
        led_queue = nullptr;
    }
}

void ILSSLanyardApplication::buzzerTask(void* arg) {
    ILSSLanyardApplication* app = static_cast<ILSSLanyardApplication*>(arg);
    if (!app || !app->buzzer_queue) {
        // Invalid app or queue - task should not have been created
        // But we can't return, so just wait forever
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    // Wait for buzzer to be initialized (it's created in begin() after tasks)
    while (!app->buzzer) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    BuzzerCommandMsg msg;
    
    while (true) {
        // Wait for command from queue (blocking)
        if (xQueueReceive(app->buzzer_queue, &msg, portMAX_DELAY) == pdTRUE) {
            // Check if buzzer is ready before processing
            if (!app->buzzer || !app->buzzer->isReady()) {
                continue;  // Skip this command if buzzer not ready
            }
            switch (msg.cmd) {
                case BuzzerCommand::TICK:
                    app->buzzer->tick(msg.param1);
                    break;
                case BuzzerCommand::BEEP:
                    app->buzzer->beep(msg.param1, msg.param2);
                    break;
                case BuzzerCommand::START_CONTINUOUS:
                    app->buzzer->startContinuous(msg.param1);
                    break;
                case BuzzerCommand::STOP:
                    app->buzzer->requestStop();  // Set flag immediately to interrupt patterns
                    app->buzzer->stop();         // Then stop the hardware
                    break;
                case BuzzerCommand::PLAY_ALTERNATING:
                    app->buzzer->playAlternating(msg.param1, msg.param2, msg.param3);
                    break;
                case BuzzerCommand::PLAY_MEDIUM_SWEEP:
                    app->buzzer->playMediumSweep(msg.param1, msg.param2, msg.param3);
                    break;
                case BuzzerCommand::PLAY_SIREN:
                    app->buzzer->playSiren(msg.param1, msg.param2, msg.param3);
                    break;
                case BuzzerCommand::PLAY_CODE3_TEMPORAL:
                    app->buzzer->playCode3Temporal(msg.param1, msg.param3);
                    break;
                case BuzzerCommand::PLAY_LF_BUZZ:
                    app->buzzer->playLFBuzz(msg.param1, msg.param2, msg.param3);
                    break;
                case BuzzerCommand::QUEUE_SIREN:
                    app->buzzer->queueSiren(msg.param1, msg.param2, msg.param3);
                    // Process immediately
                    app->buzzer->processPendingBuzzer();
                    break;
                case BuzzerCommand::QUEUE_CODE3_TEMPORAL:
                    app->buzzer->queueCode3Temporal(msg.param1, msg.param3);
                    app->buzzer->processPendingBuzzer();
                    break;
                case BuzzerCommand::QUEUE_CODE3_SWEEP:
                    app->buzzer->queueCode3Sweep(msg.param1, msg.param2, msg.param3);
                    app->buzzer->processPendingBuzzer();
                    break;
                case BuzzerCommand::QUEUE_CODE3_SIREN:
                    app->buzzer->queueCode3Siren(msg.param1, msg.param2, msg.param3);
                    app->buzzer->processPendingBuzzer();
                    break;
                case BuzzerCommand::QUEUE_BEEP:
                    app->buzzer->queueBeep(msg.param1, msg.param2);
                    app->buzzer->processPendingBuzzer();
                    break;
                case BuzzerCommand::QUEUE_TICK:
                    app->buzzer->queueTick(msg.param1);
                    app->buzzer->processPendingBuzzer();
                    break;
                case BuzzerCommand::QUEUE_LF_BUZZ:
                    app->buzzer->queueLFBuzz(msg.param1, msg.param2, msg.param3);
                    app->buzzer->processPendingBuzzer();
                    break;
                case BuzzerCommand::QUEUE_MEDIUM_SWEEP:
                    app->buzzer->queueMediumSweep(msg.param1, msg.param2, msg.param3);
                    app->buzzer->processPendingBuzzer();
                    break;
                case BuzzerCommand::QUEUE_ALTERNATING:
                    app->buzzer->queueAlternating(msg.param1, msg.param2, msg.param3);
                    app->buzzer->processPendingBuzzer();
                    break;
            }
        }
    }
}

void ILSSLanyardApplication::ledTask(void* arg) {
    ILSSLanyardApplication* app = static_cast<ILSSLanyardApplication*>(arg);
    if (!app || !app->led_queue) {
        // Invalid app or queue - task should not have been created
        // But we can't return, so just wait forever
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    // Wait for LED to be initialized (it's created in begin() after tasks)
    while (!app->rgbLed) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    LEDCommandMsg msg;
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t process_interval = pdMS_TO_TICKS(20);  // 50Hz for LED updates
    
    while (true) {
        // Check if LED is ready before processing
        if (!app->rgbLed || !app->rgbLed->isReady()) {
            vTaskDelayUntil(&last_wake_time, process_interval);
            continue;
        }
        // Process LED commands from queue (non-blocking)
        while (xQueueReceive(app->led_queue, &msg, 0) == pdTRUE) {
            switch (msg.cmd) {
                case LEDCommand::SET_EFFECT:
                    // param1 = effect, param2 = color, param3 = brightness
                    // For duration, we use a fixed duration based on effect type and color:
                    // - WATER_DROP: 1000ms
                    // - DOUBLE_FLASH + PURPLE: 500ms (for button feedback)
                    // - DOUBLE_FLASH + RED: 0 (infinite, for fire alarm)
                    // - Others: 0 (infinite/until stopped)
                    {
                        LedEffect effect = static_cast<LedEffect>(msg.param1);
                        LedColor color = static_cast<LedColor>(msg.param2);
                        uint32_t duration = 0;
                        if (effect == LedEffect::WATER_DROP) {
                            duration = 1000;  // 1 second for water drop
                        } else if (effect == LedEffect::DOUBLE_FLASH && color == LedColor::PURPLE) {
                            // Only apply 500ms duration for button feedback (purple)
                            // Fire alarm (red) needs infinite duration
                            duration = 500;  // 500ms for button feedback flash
                        }
                        app->rgbLed->queueEffect(effect, 
                                                color,
                                                static_cast<Brightness>(msg.param3),
                                                duration);
                    }
                    break;
                case LEDCommand::STOP_EFFECT:
                    app->rgbLed->stopEffect();
                    break;
                case LEDCommand::PROCESS:
                    // Process is called periodically below
                    break;
            }
        }
        
        // Process LED effects at 50Hz (20ms interval)
        app->rgbLed->process();
        
        // Wait for next cycle
        vTaskDelayUntil(&last_wake_time, process_interval);
    }
}

void ILSSLanyardApplication::sendBuzzerCommand(BuzzerCommand cmd, uint32_t param1, uint32_t param2, uint32_t param3) {
    if (buzzer_queue) {
        BuzzerCommandMsg msg = {cmd, param1, param2, param3};
        BaseType_t result = xQueueSend(buzzer_queue, &msg, 0);  // Non-blocking
        if (result != pdTRUE) {
            logger->LOGW(TAG, "Buzzer command queue full, dropping command");
        }
    }
}

void ILSSLanyardApplication::sendLEDCommand(LEDCommand cmd, uint32_t param1, uint32_t param2, uint32_t param3) {
    if (led_queue) {
        LEDCommandMsg msg = {cmd, param1, param2, param3};
        BaseType_t result = xQueueSend(led_queue, &msg, 0);  // Non-blocking
        if (result != pdTRUE) {
            logger->LOGW(TAG, "LED command queue full, dropping command");
        }
    }
}

void ILSSLanyardApplication::scanIntervalTimerCallback(void* arg) {
    ILSSLanyardApplication* app = static_cast<ILSSLanyardApplication*>(arg);
    if (app && (app->currentMode == AppMode::QUIESCENT || app->currentMode == AppMode::FIRE_EVENT || app->currentMode == AppMode::PERSONAL_ALERT)) {
        if (!app->scanInProgress) {
            uint32_t now = Utils::getCurrentTimestamp();
            app->logger->LOGD(app->TAG, "Scan interval timer fired, starting beacon scan (mode: %d)", app->currentMode);
            app->performBeaconScan();
            app->scanInProgress = true;
            app->scanStartTime = now;
            app->lastBeaconScanTime = now;
        } else {
            app->logger->LOGD(app->TAG, "Scan interval timer fired but scan already in progress");
        }
    }
}

void ILSSLanyardApplication::fireAlertTimerCallback(void* arg) {
    ILSSLanyardApplication* app = static_cast<ILSSLanyardApplication*>(arg);
    if (app && app->currentMode == AppMode::FIRE_EVENT) {
        app->updateFireAlerts();
    }
}

void ILSSLanyardApplication::personalAlertTimerCallback(void* arg) {
    ILSSLanyardApplication* app = static_cast<ILSSLanyardApplication*>(arg);
    if (app && app->currentMode == AppMode::PERSONAL_ALERT) {
        app->updatePersonalAlerts();
    }
}

void ILSSLanyardApplication::fireHapticTimerCallback(void* arg) {
    ILSSLanyardApplication* app = static_cast<ILSSLanyardApplication*>(arg);
    if (app && app->currentMode == AppMode::FIRE_EVENT) {
        // Play haptic pattern 118 every 3 seconds during fire alarm
        // Pattern 118 is "Long buzz for programmatic stopping" - continuous until stopped
        // Stop previous instance before starting new one to prevent overlap
        if (!app->lowLevel->get_haptics().isReady()) {
            app->logger->LOGW(app->TAG, "Haptics not ready (fire alarm)");
            return;
        }
        if (!app->state->getEnableHaptics()) {
            app->logger->LOGD(app->TAG, "Haptics disabled in state (fire alarm)");
            return;
        }
        // Stop any currently playing pattern before starting new one
        app->lowLevel->get_haptics().stop();
        app->logger->LOGI(app->TAG, "Playing haptic pattern 118 (fire alarm)");
        app->lowLevel->get_haptics().play_pattern(118);
    }
}

void ILSSLanyardApplication::personalHapticTimerCallback(void* arg) {
    ILSSLanyardApplication* app = static_cast<ILSSLanyardApplication*>(arg);
    if (app && app->currentMode == AppMode::PERSONAL_ALERT) {
        // Play haptic pattern 118 every 3 seconds during personal alert
        // Pattern 118 is "Long buzz for programmatic stopping" - continuous until stopped
        // Stop previous instance before starting new one to prevent overlap
        if (!app->lowLevel->get_haptics().isReady()) {
            app->logger->LOGW(app->TAG, "Haptics not ready (personal alert)");
            return;
        }
        if (!app->state->getEnableHaptics()) {
            app->logger->LOGD(app->TAG, "Haptics disabled in state (personal alert)");
            return;
        }
        // Stop any currently playing pattern before starting new one
        app->lowLevel->get_haptics().stop();
        app->logger->LOGI(app->TAG, "Playing haptic pattern 118 (personal alert)");
        app->lowLevel->get_haptics().play_pattern(118);
    }
}

void ILSSLanyardApplication::onScanComplete() {
    // Process scan results and send location update
    if (bluetooth) {
        bluetooth->removeStaleBeacons();
        const std::vector<BLEBeacon> &beacons = bluetooth->getBeacons();
        if (!beacons.empty()) {
            logger->LOGI(TAG, "Scan complete: %zu active beacons", beacons.size());
            BLEBeacon* bestBeacon = bluetooth->getBestBeacon();
            if (bestBeacon) {
                logger->LOGI(TAG, "Best beacon: id=%llu rssi:%d",
                             bestBeacon->getIdentifier(), bestBeacon->getRssi());
            }
        } else {
            logger->LOGI(TAG, "Scan complete: no beacons found");
        }
    }
    
    // Send location update with updated beacon data
    sendLocationUpdate();
    scanInProgress = false;
}

void ILSSLanyardApplication::updateBeaconScanInterval(bool fastMode) {
    if (!bluetooth) {
        return;
    }
    
    if (fastMode) {
        int interval = state->getFastBeaconScanModeScanIntervalMs();
        bluetooth->setScanInterval(interval, interval / 2);
    } else {
        int interval = state->getQuiescentBeaconScanModeScanIntervalMs();
        bluetooth->setScanInterval(interval, interval / 2);
    }
}

