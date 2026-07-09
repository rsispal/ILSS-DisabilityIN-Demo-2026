#include "FeatureTestApplication.h"
#include "../Hardware.h"
#include "../../lowlevel/wifi/WiFiLowLevelDriver.h"
#include "../../lowlevel/haptics/DRV2605Driver.h"
#include "../../lowlevel/buzzer/BuzzerLowLevelDriver.h"
#include "../../features/buzzer/Buzzer.h"
#include "../../utils/Utils.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

FeatureTestApplication::FeatureTestApplication(Logger *logger, LowLevel *lowLevel, State *state)
    : logger(logger), lowLevel(lowLevel), state(state), bluetooth(nullptr)
{
}

FeatureTestApplication::~FeatureTestApplication()
{
    if (bluetooth) {
        delete bluetooth;
        bluetooth = nullptr;
    }
}

void FeatureTestApplication::testJson()
{
    logger->LOGI(TAG, "Testing JSON...");
    JsonParser parser("{\"name\":\"John\", \"age\":30, \"city\":\"New York\"}");
    if (parser.valid())
    {
        logger->LOGI(TAG, "Name: %s", parser.getString("name").c_str());
        logger->LOGI(TAG, "Age: %d", parser.getInt("age"));
        logger->LOGI(TAG, "City: %s", parser.getString("city").c_str());
    }
    else
    {
        logger->LOGI(TAG, "JSON: Invalid");
    }
}

void FeatureTestApplication::print_http_response(const char *label, const NetworkClient::HttpResponse &resp)
{
    constexpr size_t chunk_size = 200;
    ESP_LOGI("FeatureTest", "\n=== %s Response ===", label);
    ESP_LOGI("FeatureTest", "Success: %s", resp.success ? "yes" : "no");
    ESP_LOGI("FeatureTest", "Status code: %d", resp.status_code);
    ESP_LOGI("FeatureTest", "Headers count: %zu", resp.headers.size());
    ESP_LOGI("FeatureTest", "Body size: %zu bytes", resp.body.length());

    if (!resp.body.empty())
    {
        ESP_LOGI("FeatureTest", "Body content:");
        const char *body_ptr = resp.body.c_str();
        size_t body_len = resp.body.length();

        for (size_t i = 0; i < body_len; i += chunk_size)
        {
            size_t remaining = body_len - i;
            size_t copy_len = remaining > chunk_size ? chunk_size : remaining;
            char chunk[chunk_size + 1];
            memcpy(chunk, body_ptr + i, copy_len);
            chunk[copy_len] = '\0';
            ESP_LOGI("FeatureTest", "%s", chunk);
        }
    }
    else
    {
        ESP_LOGI("FeatureTest", "Body: (empty)");
    }
    ESP_LOGI("FeatureTest", "===================");
}

void FeatureTestApplication::testWiFi()
{
    logger->LOGI(TAG, "Testing WiFi scan...");

    // Load default state
    state->loadDefaultState();

    // Start WiFi first (required before scanning)
    logger->LOGI(TAG, "Starting WiFi...");
    if (!lowLevel->get_wifi().begin()) {
        logger->LOGE(TAG, "Failed to start WiFi");
        return;
    }

    logger->LOGI(TAG, "Starting WiFi scan...");
    std::vector<WiFiNetwork> networks = lowLevel->get_wifi().scan();

    logger->LOGI(TAG, "WiFi scan completed. Found %zu unique networks:", networks.size());
    for (const auto &network : networks)
    {
        const char *security_str = "UNKNOWN";
        switch (network.security)
        {
        case 0:
            security_str = "OPEN";
            break;
        case 1:
            security_str = "WPA/WPA2";
            break;
        case 2:
            security_str = "WEP";
            break;
        case 3:
            security_str = "WPA3";
            break;
        default:
            security_str = "UNKNOWN";
            break;
        }

        logger->LOGI(TAG, "  %s - RSSI: %d dBm, Security: %s, Channel: %d",
                     network.ssid.c_str(),
                     network.rssi,
                     security_str,
                     network.channel);
    }

    // Try to connect to configured network
    if (!state->getWifiSsid().empty())
    {
        logger->LOGI(TAG, "Attempting to connect to '%s'...", state->getWifiSsid().c_str());
        if (lowLevel->get_wifi().connect(state->getWifiSsid(), state->getWifiPassword()))
        {
            if (lowLevel->get_wifi().waitForConnection(30000))
            {
                logger->LOGI(TAG, "Connected successfully! IP: %s", lowLevel->get_wifi().getIPAddress().c_str());
            }
            else
            {
                logger->LOGE(TAG, "Connection timeout");
            }
        }
        else
        {
            logger->LOGE(TAG, "Failed to initiate connection");
        }
    }
    else
    {
        logger->LOGW(TAG, "No WiFi credentials configured");
    }
}

void FeatureTestApplication::testNetworkRequests()
{
    logger->LOGI(TAG, "Testing NetworkClient...");

    // Set WiFi driver for NetworkClient
    NetworkClient::setWiFiDriver(&lowLevel->get_wifi());

    // // Test HTTP GET
    // logger->LOGI(TAG, "Making HTTP GET request...");
    // NetworkClient::HttpResponse httpResp = NetworkClient::get("http://httpbin.org/get");
    // print_http_response("HTTP GET", httpResp);

    // vTaskDelay(pdMS_TO_TICKS(2000));

    // Test HTTPS GET
    logger->LOGI(TAG, "Making HTTPS GET request...");
    NetworkClient::HttpResponse httpsResp = NetworkClient::get("https://httpbin.org/get");
    print_http_response("HTTPS GET", httpsResp);

    vTaskDelay(pdMS_TO_TICKS(2000));

    // Test POST with JSON body
    logger->LOGI(TAG, "Making HTTP POST request with JSON body...");
    std::string postBody = R"({"name": "ILSS Lanyard", "status": "testing", "temperature": 25.5})";
    std::vector<std::string> postHeaders = {
        "Content-Type: application/json"
    };
    NetworkClient::HttpResponse postResp = NetworkClient::post("http://httpbin.org/post", postBody, postHeaders);
    print_http_response("HTTP POST", postResp);

    vTaskDelay(pdMS_TO_TICKS(2000));

    // // Test PUT with JSON body
    // logger->LOGI(TAG, "Making HTTP PUT request with JSON body...");
    // std::string putBody = R"({"device_id": "esp32s3-001", "firmware_version": "1.0.0", "uptime": 3600})";
    // std::vector<std::string> putHeaders = {
    //     "Content-Type: application/json"
    // };
    // NetworkClient::HttpResponse putResp = NetworkClient::put("http://httpbin.org/put", putBody, putHeaders);
    // print_http_response("HTTP PUT", putResp);

    // vTaskDelay(pdMS_TO_TICKS(2000));

    // // Test DELETE
    // logger->LOGI(TAG, "Making HTTP DELETE request...");
    // NetworkClient::HttpResponse deleteResp = NetworkClient::del("http://httpbin.org/delete");
    // print_http_response("HTTP DELETE", deleteResp);
}

void FeatureTestApplication::testBluetooth()
{
    logger->LOGI(TAG, "Testing Bluetooth beacon scanning...");

    // Initialize Bluetooth feature  
    bluetooth = new Bluetooth(state, lowLevel);
    bluetooth->begin();

    // Scan window duration (5 seconds default, matching Zephyr CONFIG_ILSS_PREFERENCES_BEACON_SCAN_WINDOW_MS)
    const uint32_t SCAN_WINDOW_MS = 5000;
    
    bool scanInProgress = false;
    uint32_t nextScanTime = 0;
    uint32_t scanEndTime = 0;
    
    logger->LOGI(TAG, "Starting BLE beacon scan test (scan window: %dms)", SCAN_WINDOW_MS);
    logger->LOGI(TAG, "Scanning for Honeywell beacons (Company ID: 0x0526)");
    
    // Run scan cycle for 30 seconds total
    uint32_t testStartTime = Utils::getCurrentTimestamp();
    const uint32_t TEST_DURATION_MS = 30000;

    while (true)
    {
        uint32_t currentTime = Utils::getCurrentTimestamp();

        if (!scanInProgress && currentTime >= nextScanTime)
        {
            // Time to start a new scan
            logger->LOGI(TAG, "Starting BLE scan");
            bool scanStarted = bluetooth->startBeaconScanning();

            if (!scanStarted)
            {
                logger->LOGE(TAG, "Failed to start BLE scanning, retrying in 10s");
                nextScanTime = currentTime + 10000; // 10 seconds in milliseconds
                vTaskDelay(pdMS_TO_TICKS(100)); // Small sleep to prevent tight loop
                continue;
            }

            logger->LOGI(TAG, "BLE scan started, waiting %dms", SCAN_WINDOW_MS);
            logger->LOGI(TAG, "Scanning for Honeywell beacons (Company ID: 0x0526)");
            logger->LOGD(TAG, "Logging: new beacons + updates with RSSI changes");

            scanInProgress = true;
            scanEndTime = currentTime + SCAN_WINDOW_MS;
        }
        else if (scanInProgress)
        {
            // Check if scan should be stopped
            if (currentTime >= scanEndTime || !bluetooth->isBeaconScanning())
            {
                if (!bluetooth->isBeaconScanning())
                {
                    logger->LOGW(TAG, "BLE scanning stopped unexpectedly");
                }
                else
                {
                    logger->LOGI(TAG, "%dms elapsed, stopping scan", SCAN_WINDOW_MS);
                }

                // Stop scanning
                logger->LOGI(TAG, "Stopping BLE scan");
                bool scanStopped = bluetooth->stopBeaconScanning();

                if (!scanStopped)
                {
                    logger->LOGW(TAG, "Failed to stop BLE scanning cleanly");
                }

                logger->LOGI(TAG, "BLE scan stopped, removing stale beacons");

                // Remove stale beacons
                bluetooth->removeStaleBeacons();
                logger->LOGI(TAG, "Stale beacons removed");

                // Log scan summary
                const std::vector<BLEBeacon> &beacons = bluetooth->getBeacons();

                if (!beacons.empty())
                {
                    logger->LOGI(TAG, "Scan complete: %zu active beacons", beacons.size());
                }

                // Print current beacons in state (condensed)
                if (beacons.empty())
                {
                    logger->LOGI(TAG, "No beacons found");
                }
                else
                {
                    logger->LOGI(TAG, "%zu beacons:", beacons.size());
                    for (size_t i = 0; i < beacons.size(); i++)
                    {
                        const BLEBeacon &beacon = beacons[i];
                        logger->LOGI(TAG, "  [%zu]: id=%llu rssi:%d age:%lu ms",
                                   i,
                                     beacon.getIdentifier(),
                                     beacon.getRssi(),
                                     beacon.getTimeSinceLastSeen());
                    }

                    // Show best beacon
                    BLEBeacon *bestBeacon = bluetooth->getBestBeacon();
                    if (bestBeacon)
                    {
                        logger->LOGI(TAG, "Best: id=%llu rssi:%d",
                                     bestBeacon->getIdentifier(),
                                     bestBeacon->getRssi());
                    }
                }


                logger->LOGI(TAG, "Making HTTPS GET request...");
                NetworkClient::HttpResponse httpsResp = NetworkClient::get("https://httpbin.org/get");
                print_http_response("HTTPS GET", httpsResp);


      

                // Calculate next scan time
                int waitTimeSeconds = state->getCurrentBeaconScanWaitTimeSeconds();
                logger->LOGI(TAG, "Waiting %ds before next scan (%s mode)",
                           waitTimeSeconds, state->isFastBeaconScanMode() ? "fast" : "normal");

                nextScanTime = currentTime + (waitTimeSeconds * 1000); // Convert seconds to milliseconds
                scanInProgress = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to prevent tight loop
    }
    
    // Final cleanup
    if (bluetooth->isBeaconScanning())
    {
        bluetooth->stopBeaconScanning();
    }
    
    logger->LOGI(TAG, "Bluetooth beacon scan test completed");
}

void FeatureTestApplication::testBuzzer()
{
    logger->LOGI(TAG, "Testing Buzzer...");
    
    Buzzer buzzer(state, lowLevel);
    if (!buzzer.begin())
    {
        logger->LOGE(TAG, "Failed to initialize buzzer");
        return;
    }
    
    logger->LOGI(TAG, "Running buzzer test sequence...");
    buzzer.test();
    
    logger->LOGI(TAG, "Buzzer test completed");
}

void FeatureTestApplication::testHaptics()
{
    logger->LOGI(TAG, "Testing Haptics (DRV2605)...");
    
    // I2C should already be initialized in LowLevel::begin()
    if (!lowLevel->get_i2c().is_ready())
    {
        logger->LOGE(TAG, "I2C driver not ready. Ensure LowLevel::begin() was called.");
        return;
    }
    
    DRV2605Driver* haptics = &lowLevel->get_haptics();
    if (!haptics->begin())
    {
        logger->LOGE(TAG, "Failed to initialize DRV2605");
        return;
    }
    
    logger->LOGI(TAG, "Playing haptic patterns...");
    
    // Test various haptic patterns (waveform IDs from DRV2605 library)
    // Pattern 1: Strong Click
    logger->LOGI(TAG, "Pattern 1: Strong Click");
    haptics->play_pattern(1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Pattern 2: Double Click
    logger->LOGI(TAG, "Pattern 2: Double Click");
    haptics->play_pattern(2);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Pattern 3: Triple Click
    logger->LOGI(TAG, "Pattern 3: Triple Click");
    haptics->play_pattern(3);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Pattern 4: Soft Fuzz
    logger->LOGI(TAG, "Pattern 4: Soft Fuzz");
    haptics->play_pattern(4);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Pattern 5: Strong Buzz
    logger->LOGI(TAG, "Pattern 5: Strong Buzz");
    haptics->play_pattern(5);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    logger->LOGI(TAG, "Haptics test completed");
}

void FeatureTestApplication::testSideButtonsBareMetal()
{
    logger->LOGI(TAG, "Testing Side Buttons (Bare Metal GPIO)...");
    
    // Right button is now correctly configured as GPIO8 (found via GPIO scanner)
    // Diagnostic test removed - GPIO8 confirmed working
    
    // SCAN MULTIPLE GPIOs to find where the right button is actually connected
    // Only scan a few safe GPIOs to avoid conflicts
    logger->LOGI(TAG, "=== SCANNING GPIOs to find right button ===");
    logger->LOGI(TAG, "Press the RIGHT button now! Scanning for 5 seconds...");
    
    // GPIO scanner - right button found on GPIO8, so this is now just for verification
    // Test only a few safe GPIO pins (excluding pins already in use)
    // Skip: GPIO2 (buzzer), GPIO5/6 (I2C), GPIO8 (right button - FOUND!), GPIO9 (left button)
    gpio_num_t test_pins[] = {
        GPIO_NUM_1, GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_7,
        GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_12, GPIO_NUM_13
    };
    const int num_pins = sizeof(test_pins) / sizeof(test_pins[0]);
    
    logger->LOGI(TAG, "Testing %d GPIO pins: 1,3,4,7,10,11,12,13 (GPIO8 already confirmed as right button)", num_pins);
    
    // Configure all test pins as inputs with pullup (with error handling)
    int configured_pins = 0;
    for (int i = 0; i < num_pins; i++) {
        esp_err_t ret = gpio_reset_pin(test_pins[i]);
        if (ret == ESP_OK) {
            ret = gpio_set_direction(test_pins[i], GPIO_MODE_INPUT);
            if (ret == ESP_OK) {
                ret = gpio_set_pull_mode(test_pins[i], GPIO_PULLUP_ONLY);
                if (ret == ESP_OK) {
                    configured_pins++;
                } else {
                    logger->LOGW(TAG, "Failed to set pull mode for GPIO%d: %s", test_pins[i], esp_err_to_name(ret));
                }
            } else {
                logger->LOGW(TAG, "Failed to set direction for GPIO%d: %s", test_pins[i], esp_err_to_name(ret));
            }
        } else {
            logger->LOGW(TAG, "Failed to reset GPIO%d: %s", test_pins[i], esp_err_to_name(ret));
        }
    }
    logger->LOGI(TAG, "Successfully configured %d/%d GPIO pins", configured_pins, num_pins);
    
    if (configured_pins == 0) {
        logger->LOGE(TAG, "Failed to configure any GPIO pins for scanning! Skipping scan.");
        // Don't return - continue with the rest of the test
    } else {
        // Read initial states
        int initial_states[9] = {0}; // Max 9 pins
        for (int i = 0; i < num_pins; i++) {
            initial_states[i] = gpio_get_level(test_pins[i]);
        }
        
        logger->LOGI(TAG, "Initial states read. Starting 5-second scan...");
        
        // Monitor for 5 seconds, checking for changes
        int64_t scan_start = esp_timer_get_time() / 1000;
        int64_t scan_duration = 5000; // 5 seconds
        
        while ((esp_timer_get_time() / 1000 - scan_start) < scan_duration) {
            for (int i = 0; i < num_pins; i++) {
                int current_state = gpio_get_level(test_pins[i]);
                if (current_state != initial_states[i]) {
                    logger->LOGI(TAG, "*** GPIO%d CHANGED from %d to %d (RIGHT BUTTON FOUND?) ***", 
                                test_pins[i], initial_states[i], current_state);
                    initial_states[i] = current_state; // Update to avoid repeated logs
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50ms
        }
        
        logger->LOGI(TAG, "=== GPIO scan complete ===");
    }
    
    // Configure GPIO pins as inputs with pull-up (bare metal, no interrupts)
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE; // No interrupts for bare metal test
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << HARDWARE_BUTTON_LEFT_PIN) | (1ULL << HARDWARE_BUTTON_RIGHT_PIN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        logger->LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return;
    }
    
    logger->LOGI(TAG, "Bare metal GPIO test: Left=GPIO%d, Right=GPIO%d", HARDWARE_BUTTON_LEFT_PIN, HARDWARE_BUTTON_RIGHT_PIN);
    logger->LOGI(TAG, "Reading buttons every 100ms for 30 seconds...");
    logger->LOGI(TAG, "Note: 0 = pressed (pulled low), 1 = released (pulled high)");
    
    int64_t start_time = esp_timer_get_time() / 1000; // milliseconds
    int64_t test_duration_ms = 30000; // 30 seconds
    int last_left_state = -1;
    int last_right_state = -1;
    int64_t last_change_time = 0;
    int64_t last_log_time = 0;
    
    // Read initial state multiple times to check for consistency
    int right_reads[5];
    for (int i = 0; i < 5; i++) {
        right_reads[i] = gpio_get_level(HARDWARE_BUTTON_RIGHT_PIN);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    logger->LOGI(TAG, "Initial state - Left: %d, Right: %d (5 reads: %d,%d,%d,%d,%d)", 
                gpio_get_level(HARDWARE_BUTTON_LEFT_PIN),
                gpio_get_level(HARDWARE_BUTTON_RIGHT_PIN),
                right_reads[0], right_reads[1], right_reads[2], right_reads[3], right_reads[4]);
    
    while ((esp_timer_get_time() / 1000 - start_time) < test_duration_ms)
    {
        int left_state = gpio_get_level(HARDWARE_BUTTON_LEFT_PIN);
        int right_state = gpio_get_level(HARDWARE_BUTTON_RIGHT_PIN);
        int64_t now = esp_timer_get_time() / 1000;
        
        // Log state changes immediately
        if (left_state != last_left_state || right_state != last_right_state) {
            int64_t time_since_last_change = (last_change_time > 0) ? (now - last_change_time) : 0;
            
            logger->LOGI(TAG, "[%lld ms] *** CHANGE *** Left: %d (%s), Right: %d (%s) [change after %lld ms]",
                        now - start_time,
                        left_state, left_state == 0 ? "PRESSED" : "RELEASED",
                        right_state, right_state == 0 ? "PRESSED" : "RELEASED",
                        time_since_last_change);
            
            last_left_state = left_state;
            last_right_state = right_state;
            last_change_time = now;
        }
        
        // Also log every 2 seconds to show current state
        if (now - last_log_time >= 2000) {
            logger->LOGI(TAG, "[%lld ms] Current state - Left: %d (%s), Right: %d (%s)",
                        now - start_time,
                        left_state, left_state == 0 ? "PRESSED" : "RELEASED",
                        right_state, right_state == 0 ? "PRESSED" : "RELEASED");
            last_log_time = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    logger->LOGI(TAG, "Bare metal GPIO test completed");
}

void FeatureTestApplication::testSideButtons()
{
    logger->LOGI(TAG, "Testing Side Buttons (with SideButtons class and tactile feedback)...");
    
    // Initialize buzzer and haptics for tactile feedback
    Buzzer buzzer(state, lowLevel);
    if (!buzzer.begin()) {
        logger->LOGE(TAG, "Failed to initialize buzzer for button test");
    }
    
    // Check if haptics is available
    bool haptics_available = false;
    if (lowLevel) {
        auto& haptics_ref = lowLevel->get_haptics();
        if (haptics_ref.begin()) {
            haptics_available = true;
            logger->LOGI(TAG, "Haptics initialized for button test");
        } else {
            logger->LOGW(TAG, "Haptics initialization failed");
        }
    }
    
    SideButtons buttons(state, lowLevel, HARDWARE_BUTTON_LEFT_PIN, HARDWARE_BUTTON_RIGHT_PIN);
    
    // Set up event callback with tactile feedback
    buttons.setEventCallback([&buzzer, haptics_available, this](ButtonEvent event) {
        const char* event_name = "";
        switch (event) {
            case ButtonEvent::LEFT_PRESS_DOWN: 
                event_name = "LEFT_PRESS_DOWN";
                // Short beep on press (left button: slightly lower pitch)
                buzzer.beep(2000, 50);  // 2000Hz, 50ms - left button pitch
                if (haptics_available && lowLevel) {
                    lowLevel->get_haptics().play_pattern(1);  // Light click
                }
                break;
            case ButtonEvent::LEFT_PRESS_UP: 
                event_name = "LEFT_PRESS_UP";
                // Lower pitch beep on release (left button)
                buzzer.beep(1500, 50);  // 1500Hz, 50ms - lower pitch for release
                break;
            case ButtonEvent::RIGHT_PRESS_DOWN: 
                event_name = "RIGHT_PRESS_DOWN";
                // Short beep on press (right button: slightly higher pitch to distinguish)
                buzzer.beep(2200, 50);  // 2200Hz, 50ms - right button pitch (higher than left)
                if (haptics_available && lowLevel) {
                    lowLevel->get_haptics().play_pattern(1);  // Light click
                }
                break;
            case ButtonEvent::RIGHT_PRESS_UP: 
                event_name = "RIGHT_PRESS_UP";
                // Lower pitch beep on release (right button)
                buzzer.beep(1700, 50);  // 1700Hz, 50ms - lower pitch for release (still higher than left)
                break;
            case ButtonEvent::LEFT_PRESS: 
                event_name = "LEFT_PRESS";
                // Complete press event (already handled by DOWN/UP, but keep for compatibility)
                if (haptics_available && lowLevel) {
                    lowLevel->get_haptics().play_pattern(1);  // Light click
                }
                break;
            case ButtonEvent::RIGHT_PRESS: 
                event_name = "RIGHT_PRESS";
                // Complete press event (already handled by DOWN/UP, but keep for compatibility)
                if (haptics_available && lowLevel) {
                    lowLevel->get_haptics().play_pattern(1);  // Light click
                }
                break;
            case ButtonEvent::LEFT_HOLD: 
                event_name = "LEFT_HOLD";
                // Alternating or medium sweep for long press (left button)
                buzzer.playMediumSweep(800, 970, 4);  // Medium sweep pattern
                if (haptics_available && lowLevel) {
                    lowLevel->get_haptics().play_pattern(12);  // Medium click
                }
                break;
            case ButtonEvent::RIGHT_HOLD: 
                event_name = "RIGHT_HOLD";
                // Alternating or medium sweep for long press (right button)
                buzzer.playAlternating(800, 970, 4);  // Alternating pattern (different from left)
                if (haptics_available && lowLevel) {
                    lowLevel->get_haptics().play_pattern(12);  // Medium click
                }
                break;
            case ButtonEvent::BOTH_HOLD_DOWN: 
                event_name = "BOTH_HOLD_DOWN";
                // Quick feedback when both buttons pressed
                buzzer.tick(1500);  // Short tick
                if (haptics_available && lowLevel) {
                    lowLevel->get_haptics().play_pattern(2);  // Double click
                }
                break;
            case ButtonEvent::BOTH_HOLD: 
                event_name = "BOTH_HOLD";
                // Siren pattern for both buttons long hold
                buzzer.playSiren(2700, 3500, 4);  // Siren pattern
                if (haptics_available && lowLevel) {
                    lowLevel->get_haptics().play_pattern(47);  // Strong click
                }
                break;
            case ButtonEvent::BOTH_HOLD_UP: 
                event_name = "BOTH_HOLD_UP";
                // Short beep on release after both hold
                buzzer.beep(1500, 100);  // Lower pitch beep
                if (haptics_available && lowLevel) {
                    lowLevel->get_haptics().play_pattern(1);  // Light click
                }
                break;
        }
        ESP_LOGI("FeatureTest", "Button event: %s", event_name);
    });
    
    if (!buttons.begin())
    {
        logger->LOGE(TAG, "Failed to initialize side buttons");
        return;
    }
    
    logger->LOGI(TAG, "Side buttons initialized with tactile feedback.");
    logger->LOGI(TAG, "Press buttons to test. Test will run for 60 seconds.");
    logger->LOGI(TAG, "Feedback: Buzzer + Haptics for each event type");
    
    // First, do a quick GPIO hardware test to verify buttons are physically working
    logger->LOGI(TAG, "Performing quick GPIO hardware test...");
    gpio_set_direction(HARDWARE_BUTTON_LEFT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(HARDWARE_BUTTON_LEFT_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction(HARDWARE_BUTTON_RIGHT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(HARDWARE_BUTTON_RIGHT_PIN, GPIO_PULLUP_ONLY);
    
    int left_state = gpio_get_level(HARDWARE_BUTTON_LEFT_PIN);
    int right_state = gpio_get_level(HARDWARE_BUTTON_RIGHT_PIN);
    logger->LOGI(TAG, "Initial GPIO states - Left: %d (%s), Right: %d (%s)",
                left_state, left_state == 0 ? "PRESSED" : "RELEASED",
                right_state, right_state == 0 ? "PRESSED" : "RELEASED");
    
    // Monitor buttons for 60 seconds
    int64_t start_time = esp_timer_get_time() / 1000; // milliseconds
    int64_t test_duration_ms = 60000; // 60 seconds
    int64_t last_gpio_check = start_time;
    
    while ((esp_timer_get_time() / 1000 - start_time) < test_duration_ms)
    {
        // Process pending buzzer patterns
        buzzer.processPendingBuzzer();
        
        // Periodically check GPIO state to verify hardware (every 2 seconds)
        int64_t now = esp_timer_get_time() / 1000;
        if (now - last_gpio_check >= 2000) {
            int current_left = gpio_get_level(HARDWARE_BUTTON_LEFT_PIN);
            int current_right = gpio_get_level(HARDWARE_BUTTON_RIGHT_PIN);
            if (current_left != left_state || current_right != right_state) {
                logger->LOGI(TAG, "[%lld ms] GPIO state changed - Left: %d->%d, Right: %d->%d",
                            now - start_time,
                            left_state, current_left,
                            right_state, current_right);
                left_state = current_left;
                right_state = current_right;
            }
            last_gpio_check = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    logger->LOGI(TAG, "Side buttons test completed");
}

void FeatureTestApplication::testRGBLED()
{
    logger->LOGI(TAG, "Testing RGB LED (WS2813)...");
    
    RGBLED rgbLed(state, lowLevel);
    
    if (!rgbLed.begin())
    {
        logger->LOGE(TAG, "Failed to initialize RGB LED");
        return;
    }
    
    logger->LOGI(TAG, "RGB LED initialized. Running test sequence...");
    
    // Run the built-in test sequence
    rgbLed.test();
    
    logger->LOGI(TAG, "RGB LED test completed");
}

void FeatureTestApplication::testAzureIoT()
{
    logger->LOGI(TAG, "Testing Azure IoT Hub connectivity...");
    
    // Ensure WiFi is connected first
    if (!lowLevel->get_wifi().isConnected()) {
        logger->LOGI(TAG, "WiFi not connected. Starting WiFi...");
        lowLevel->get_wifi().begin();
        state->loadDefaultState();
        
        // Wait for connection
        int retries = 30;
        while (!lowLevel->get_wifi().isConnected() && retries > 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            retries--;
        }
        
        if (!lowLevel->get_wifi().isConnected()) {
            logger->LOGE(TAG, "WiFi connection failed. Cannot test Azure IoT.");
            return;
        }
    }
    
    logger->LOGI(TAG, "WiFi connected. Initializing Azure IoT client...");
    
    AzureIoT azureIoT(state);
    
    // Set up message callback
    azureIoT.setMessageCallback([](const char* topic, const uint8_t* payload, size_t len) {
        ESP_LOGI("AzureIoT", "Received message on topic: %s", topic);
        ESP_LOGI("AzureIoT", "Payload (%zu bytes): %.*s", len, (int)len, payload);
    });
    
    if (!azureIoT.begin()) {
        logger->LOGE(TAG, "Failed to initialize Azure IoT client");
        return;
    }
    
    logger->LOGI(TAG, "Connecting to Azure IoT Hub...");
    if (!azureIoT.connect()) {
        logger->LOGE(TAG, "Failed to connect to Azure IoT Hub");
        return;
    }
    
    logger->LOGI(TAG, "Connected! Publishing test telemetry...");
    
    // Publish test telemetry
    const char* test_telemetry = "{\"temperature\":25.5,\"humidity\":60.0,\"test\":true}";
    if (azureIoT.publishTelemetry(test_telemetry)) {
        logger->LOGI(TAG, "Telemetry published successfully");
    } else {
        logger->LOGE(TAG, "Failed to publish telemetry");
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Publish test property
    const char* test_property = "{\"firmwareVersion\":\"1.0.0\",\"status\":\"online\"}";
    if (azureIoT.publishProperty(test_property)) {
        logger->LOGI(TAG, "Property published successfully");
    } else {
        logger->LOGE(TAG, "Failed to publish property");
    }
    
    logger->LOGI(TAG, "Azure IoT test completed. Keeping connection alive for 30 seconds...");
    
    // Keep connection alive and process messages
    for (int i = 0; i < 30; i++) {
        azureIoT.process();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    logger->LOGI(TAG, "Disconnecting from Azure IoT Hub...");
    azureIoT.disconnect();
    
    logger->LOGI(TAG, "Azure IoT test completed");
}

void FeatureTestApplication::begin()
{
    logger->LOGI(TAG, "Feature Test Application starting...");

    // Run test routines
    testJson();
    
    logger->LOGI(TAG, "All tests will run sequentially...");
    
    testWiFi();
    
    // vTaskDelay(pdMS_TO_TICKS(2000));
    
    // testNetworkRequests();
    
    // vTaskDelay(pdMS_TO_TICKS(2000));
    
    // // testBluetooth();
    
    // // vTaskDelay(pdMS_TO_TICKS(2000));
    
    // testBuzzer();
    
    // vTaskDelay(pdMS_TO_TICKS(2000));
    
    // testHaptics();
    
    // vTaskDelay(pdMS_TO_TICKS(2000));
    
    // // Test SideButtons class with corrected GPIO pins
    testSideButtons();
    
    // vTaskDelay(pdMS_TO_TICKS(2000));
    
    // // Test RGB LED
    // testRGBLED();
    
    // vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Test Azure IoT Hub
    testAzureIoT();

    logger->LOGI(TAG, "All tests completed! Entering idle loop...");

    // Idle loop
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
