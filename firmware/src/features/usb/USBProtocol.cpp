#include "USBProtocol.h"
#include "../../utils/Logger.h"
#include "../../utils/JSON.h"  // Includes both JsonParser and JsonBuilder
#include "../../lowlevel/LowLevel.h"
#include "../../lowlevel/wifi/WiFiLowLevelDriver.h"  // Includes WiFiNetwork struct
#include "../../state/State.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <vector>

USBProtocol::USBProtocol(Logger* logger, LowLevel* lowLevel, State* state)
    : logger_(logger), lowLevel_(lowLevel), state_(state) {
}

USBProtocol::~USBProtocol() {
}

std::string USBProtocol::processCommand(const std::string& json_command) {
    USBCommandVersion1 command;
    std::string data;

    if (!parseJsonCommand(json_command, command, data)) {
        logger_->LOGE(TAG, "Failed to parse JSON command");
        return createResponse(USBCommandVersion1::GET_INFO, false, "", "Invalid command format");
    }

    logger_->LOGD(TAG, "Command parsed - Command: %u", static_cast<uint32_t>(command));

    cJSON* response_data = nullptr;
    bool success = true;

    switch (command) {
        case USBCommandVersion1::GET_INFO:
            response_data = handleGetInfo();
            break;

        case USBCommandVersion1::RESET_DEVICE:
            response_data = handleResetDevice();
            break;

        case USBCommandVersion1::SCAN_WIFI_NETWORKS:
            // Check WiFi readiness and connection status before scanning
            if (!lowLevel_->get_wifi().isReady()) {
                return buildResponse(command, false, nullptr, "WiFi not initialized");
            }
            if (lowLevel_->get_wifi().isConnected()) {
                std::string current_ssid = lowLevel_->get_wifi().getCurrentSSID();
                return buildResponse(command, false, nullptr, 
                                   ("Already connected to " + current_ssid + ", scan aborted").c_str());
            }
            response_data = handleScanWifiNetworks();
            if (!response_data) {
                return buildResponse(command, false, nullptr, "WiFi scan failed");
            }
            break;

        case USBCommandVersion1::CONNECT_WIFI:
            response_data = handleConnectWifi(data);
            if (!response_data) {
                success = false;
            }
            break;

        case USBCommandVersion1::GET_WIFI_STATUS:
            response_data = handleGetWifiStatus();
            break;

        case USBCommandVersion1::DISCONNECT_WIFI:
            response_data = handleDisconnectWifi();
            if (!response_data) {
                success = false;
            }
            break;

        case USBCommandVersion1::SET_SESSION_ID:
            response_data = handleSetSessionId(data);
            break;

        case USBCommandVersion1::SET_USER_FIRST_NAME:
            response_data = handleSetUserFirstName(data);
            break;

        case USBCommandVersion1::SET_USER_LAST_NAME:
            response_data = handleSetUserLastName(data);
            break;

        case USBCommandVersion1::SET_USER_PERSONA:
            response_data = handleSetUserPersona(data);
            break;

        case USBCommandVersion1::GET_BATTERY_STATUS:
            response_data = handleGetBatteryStatus();
            break;

        case USBCommandVersion1::ENABLE_PERSONAL_ALERT_BUTTONS:
        case USBCommandVersion1::DISABLE_PERSONAL_ALERT_BUTTONS:
            response_data = handleEnableFeature("personal_alert_buttons",
                                                command == USBCommandVersion1::ENABLE_PERSONAL_ALERT_BUTTONS);
            break;

        case USBCommandVersion1::ENABLE_HAPTICS:
        case USBCommandVersion1::DISABLE_HAPTICS:
            response_data = handleEnableFeature("haptics",
                                                command == USBCommandVersion1::ENABLE_HAPTICS);
            break;

        case USBCommandVersion1::ENABLE_BUZZER:
        case USBCommandVersion1::DISABLE_BUZZER:
            response_data = handleEnableFeature("buzzer",
                                                command == USBCommandVersion1::ENABLE_BUZZER);
            break;

        case USBCommandVersion1::ENABLE_LED_INDICATIONS:
        case USBCommandVersion1::DISABLE_LED_INDICATIONS:
            response_data = handleEnableFeature("led_indications",
                                                command == USBCommandVersion1::ENABLE_LED_INDICATIONS);
            break;

        case USBCommandVersion1::ENABLE_INACTIVITY_ALERTS:
        case USBCommandVersion1::DISABLE_INACTIVITY_ALERTS:
            response_data = handleEnableFeature("inactivity_alerts",
                                                command == USBCommandVersion1::ENABLE_INACTIVITY_ALERTS);
            break;

        case USBCommandVersion1::ENABLE_NFC_SESSION_SHARING:
        case USBCommandVersion1::DISABLE_NFC_SESSION_SHARING:
            response_data = handleEnableFeature("nfc_session_sharing",
                                                command == USBCommandVersion1::ENABLE_NFC_SESSION_SHARING);
            break;

        case USBCommandVersion1::SET_TIME:
            response_data = handleSetTime(data);
            break;

        case USBCommandVersion1::SET_QUIESCENT_BEACON_SCAN_INTERVAL:
            response_data = handleSetQuiescentBeaconScanInterval(data);
            break;

        case USBCommandVersion1::SET_FAST_BEACON_SCAN_INTERVAL:
            response_data = handleSetFastBeaconScanInterval(data);
            break;

        case USBCommandVersion1::SET_PERSONAL_ALERT_BUTTON_DELAY:
            response_data = handleSetPersonalAlertButtonDelay(data);
            break;

        default:
            logger_->LOGE(TAG, "Unknown command: %u", static_cast<uint32_t>(command));
            success = false;
            break;
    }

    // Note: buildResponse() takes ownership of response_data via addItem()
    // so we must NOT delete it here - it's freed when the JsonBuilder destructor runs
    return buildResponse(command, success, response_data, 
                        success ? "" : "Unknown command");
}

std::string USBProtocol::createResponse(USBCommandVersion1 command, bool success, 
                                       const std::string& data, const std::string& error) {
    // Legacy method - convert string data to cJSON and use buildResponse
    cJSON* data_obj = nullptr;
    
    if (success && !data.empty()) {
        // Try to parse as JSON first
        cJSON* parsed = cJSON_Parse(data.c_str());
        if (parsed) {
            data_obj = parsed;
        } else {
            // If not valid JSON, treat as string
            data_obj = cJSON_CreateString(data.c_str());
        }
    }
    
    // Note: buildResponse() takes ownership of data_obj via addItem()
    // so we must NOT delete it here - it's freed when the JsonBuilder destructor runs
    return buildResponse(command, success, data_obj, error);
}

std::string USBProtocol::buildResponse(USBCommandVersion1 command, bool success, 
                                       cJSON* data_obj, const std::string& error_msg) {
    JsonBuilder response = JsonBuilder::object();
    if (!response.valid()) {
        return "{\"cmd\":" + std::to_string(static_cast<int>(command)) + ",\"success\":false,\"error_message\":\"Internal error\"}";
    }

    // Add command ID
    response.addNumber("cmd", static_cast<int>(command));
    
    // Add success status
    response.addBool("success", success);

    // Add data or error message
    if (success) {
        if (data_obj) {
            response.addItem("data", data_obj);
        }
    } else {
        if (!error_msg.empty()) {
            response.addString("error_message", error_msg.c_str());
        }
    }

    return response.toString();
}

std::string USBProtocol::cjsonToString(cJSON* json) {
    if (!json) return "{}";
    JsonBuilder builder(json);
    return builder.toString();
}

bool USBProtocol::parseJsonCommand(const std::string& json_str, USBCommandVersion1& command, std::string& data) {
    JsonParser json(json_str);
    if (!json.valid()) {
        return false;
    }

    int cmd_int = json.getInt("cmd", -1);
    if (cmd_int < 0 || cmd_int > 255) {
        return false;
    }

    command = static_cast<USBCommandVersion1>(cmd_int);
    
    // Extract data field - can be object, array, or string
    cJSON* data_item = json.get("data");
    if (data_item) {
        char* data_str = cJSON_PrintUnformatted(data_item);
        if (data_str) {
            data = std::string(data_str);
            free(data_str);
        } else {
            data = "";
        }
    } else {
        data = "";
    }
    
    return true;
}

cJSON* USBProtocol::handleGetInfo() {
    JsonBuilder data = JsonBuilder::object();
    data.addString("hw", "ESP32-S3")
        .addString("sw", "1.0.0")
        .addString("model", "ILSS-Lanyard")
        .addNumber("serial", 0)
        .addNumber("manu", 0);
    
    return data.release();
}

cJSON* USBProtocol::handleResetDevice() {
    logger_->LOGI(TAG, "Device reset requested");
    state_->loadDefaultState();
    return cJSON_CreateTrue();
}

cJSON* USBProtocol::handleScanWifiNetworks() {
    logger_->LOGI(TAG, "WiFi scan requested");

    // Note: WiFi readiness and connection checks are done in processCommand() before calling this

    // Run WiFi scan in a separate task to avoid blocking the USB task
    // This prevents watchdog timeouts and allows USB buffer to be serviced
    std::vector<WiFiNetwork> all_networks;
    volatile bool scan_complete = false;
    
    struct ScanParams {
        WiFiLowLevelDriver* wifi;
        std::vector<WiFiNetwork>* result;
        volatile bool* complete;
    } scanParams = { &lowLevel_->get_wifi(), &all_networks, &scan_complete };
    
    TaskHandle_t scanTaskHandle = NULL;
    BaseType_t taskCreated = xTaskCreate(
        [](void* param) {
            ScanParams* p = static_cast<ScanParams*>(param);
            *(p->result) = p->wifi->scan();
            *(p->complete) = true;
            vTaskDelete(NULL);
        },
        "wifi_scan_task",
        8192,  // Stack size
        &scanParams,
        5,     // Priority
        &scanTaskHandle
    );
    
    if (taskCreated != pdPASS) {
        logger_->LOGE(TAG, "Failed to create WiFi scan task");
        return nullptr;
    }
    
    // Wait for scan to complete, yielding periodically to prevent watchdog
    const int SCAN_TIMEOUT_MS = 20000;  // 20 second timeout
    int elapsed = 0;
    while (!scan_complete && elapsed < SCAN_TIMEOUT_MS) {
        vTaskDelay(pdMS_TO_TICKS(100));  // Yield for 100ms
        elapsed += 100;
    }
    
    if (!scan_complete) {
        logger_->LOGE(TAG, "WiFi scan timeout after %d ms", elapsed);
        // Task may still be running - it will delete itself when done
        return nullptr;
    }
    
    logger_->LOGI(TAG, "WiFi scan completed. Found %zu total networks", all_networks.size());

    // Build JSON array directly using cJSON (avoid JsonBuilder memory issues)
    cJSON* networks_array = cJSON_CreateArray();
    if (!networks_array) {
        logger_->LOGE(TAG, "Failed to create networks array");
        return nullptr;
    }

    int network_count = 0;
    const int MAX_NETWORKS = 20;  // Limit to prevent oversized responses

    for (const auto &network : all_networks) {
        // Skip networks with empty SSIDs
        if (network.ssid.empty()) {
            continue;
        }

        // Limit number of networks
        if (network_count >= MAX_NETWORKS) {
            logger_->LOGW(TAG, "Network limit reached, stopping at %d networks", network_count);
            break;
        }

        // Create network object directly with cJSON
        cJSON* network_obj = cJSON_CreateObject();
        if (!network_obj) {
            continue;
        }

        // Add fields
        cJSON_AddStringToObject(network_obj, "s", network.ssid.c_str());
        cJSON_AddNumberToObject(network_obj, "r", network.rssi);
        cJSON_AddNumberToObject(network_obj, "sec", network.security);
        cJSON_AddNumberToObject(network_obj, "b", network.frequency_band);
        cJSON_AddNumberToObject(network_obj, "c", network.channel);

        // Format MAC address
        char bssid_str[18];
        snprintf(bssid_str, sizeof(bssid_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 network.bssid[0], network.bssid[1], network.bssid[2],
                 network.bssid[3], network.bssid[4], network.bssid[5]);
        cJSON_AddStringToObject(network_obj, "mac", bssid_str);

        // Add to array (cJSON_AddItemToArray takes ownership)
        cJSON_AddItemToArray(networks_array, network_obj);
        network_count++;
    }

    logger_->LOGI(TAG, "Returning %d visible networks", network_count);
    return networks_array;
}

cJSON* USBProtocol::handleConnectWifi(const std::string& data) {
    logger_->LOGI(TAG, "WiFi connection requested: %s", data.c_str());
    
    JsonParser json(data);
    if (!json.valid()) {
        logger_->LOGE(TAG, "Invalid JSON data for WiFi connection");
        return nullptr;
    }

    std::string ssid = json.getString("ssid");
    std::string password = json.getString("password");
    int band = json.getInt("band", 2); // Default to 2.4GHz

    logger_->LOGD(TAG, "Parsed WiFi parameters - SSID: '%s', Password: '****', Band: %d",
                  ssid.c_str(), band);

    if (ssid.empty()) {
        logger_->LOGE(TAG, "No SSID provided for WiFi connection");
        return nullptr;
    }

    logger_->LOGI(TAG, "Attempting to connect to WiFi: SSID='%s', Band=%d", ssid.c_str(), band);

    // Attempt to connect using the WiFi low level driver with band parameter
    bool success = lowLevel_->get_wifi().connect(ssid, password, band);

    if (success) {
        // Wait for connection to complete
        if (lowLevel_->get_wifi().waitForConnection(30000)) {
            logger_->LOGI(TAG, "WiFi connection successful");
            state_->setWifiSsid(ssid);
            state_->setWifiPassword(password);
            state_->setWiFiBand(band);
            
            JsonBuilder response = JsonBuilder::object();
            response.addBool("success", true)
                    .addString("message", ("Connected to " + ssid).c_str());
            return response.release();
        } else {
            logger_->LOGE(TAG, "WiFi connection timeout");
            state_->setWifiSsid("");
            state_->setWifiPassword("");
            state_->setWiFiBand(0);
            return nullptr;
        }
    } else {
        logger_->LOGE(TAG, "WiFi connection failed");
        state_->setWifiSsid("");
        state_->setWifiPassword("");
        state_->setWiFiBand(0);
        return nullptr;
    }
}

cJSON* USBProtocol::handleGetWifiStatus() {
    logger_->LOGI(TAG, "WiFi status requested");

    if (!lowLevel_->get_wifi().isReady()) {
        logger_->LOGE(TAG, "WiFi driver not available");
        return nullptr;
    }

    bool is_connected = lowLevel_->get_wifi().isConnected();
    std::string current_ssid = lowLevel_->get_wifi().getCurrentSSID();

    // Create status response
    JsonBuilder status_json = JsonBuilder::object();
    status_json.addBool("connected", is_connected);
    
    if (is_connected && !current_ssid.empty()) {
        status_json.addString("ssid", current_ssid.c_str())
                   .addString("mode", "STA");
    }

    logger_->LOGI(TAG, "WiFi status: connected=%s, ssid=%s",
                  is_connected ? "true" : "false", current_ssid.c_str());

    // TEMPORARY: Test network connectivity if connected to verify WiFi is working
    if (is_connected && !current_ssid.empty()) {
        logger_->LOGI(TAG, "WiFi connected - testing network connectivity...");
        logger_->LOGI(TAG, "Network interface status: UP, testing basic connectivity...");

        // Try a simple HTTP GET to test basic network stack
        std::string response = lowLevel_->get_wifi().httpGet("http://httpbin.org/get");

        if (!response.empty()) {
            logger_->LOGI(TAG, "Network test SUCCESS! Received %zu bytes from httpbin.org", response.length());

            // Log first line of response to show it's working
            size_t newline_pos = response.find('\n');
            if (newline_pos != std::string::npos) {
                std::string first_line = response.substr(0, newline_pos);
                logger_->LOGI(TAG, "HTTP response first line: %s", first_line.c_str());
            }
        } else {
            logger_->LOGI(TAG, "Network test: Basic connectivity failed - checking network configuration");
            logger_->LOGI(TAG, "This suggests a network routing or gateway configuration issue");
        }
    }

    return status_json.release();
}

cJSON* USBProtocol::handleDisconnectWifi() {
    logger_->LOGI(TAG, "WiFi disconnect requested");

    if (!lowLevel_->get_wifi().isReady()) {
        logger_->LOGE(TAG, "WiFi driver not available");
        return nullptr;
    }

    bool success = lowLevel_->get_wifi().disconnect();

    if (success) {
        logger_->LOGI(TAG, "WiFi disconnect successful");
        JsonBuilder response = JsonBuilder::object();
        response.addBool("success", true)
                .addString("message", "WiFi disconnected");
        return response.release();
    } else {
        logger_->LOGE(TAG, "WiFi disconnect failed");
        return nullptr;
    }
}

cJSON* USBProtocol::handleSetSessionId(const std::string& data) {
    JsonParser json(data);
    if (!json.valid()) {
        // Try as direct string value
        if (!data.empty()) {
            state_->setSessionId(data);
            return cJSON_CreateTrue();
        }
        return cJSON_CreateFalse();
    }
    
    std::string session_id = json.getString("session_id");
    if (session_id.empty()) {
        return cJSON_CreateFalse();
    }
    
    state_->setSessionId(session_id);
    return cJSON_CreateTrue();
}

cJSON* USBProtocol::handleSetUserFirstName(const std::string& data) {
    JsonParser json(data);
    if (!json.valid()) {
        // Try as direct string value
        if (!data.empty()) {
            state_->setUserFirstName(data);
            return cJSON_CreateTrue();
        }
        return cJSON_CreateFalse();
    }
    
    std::string first_name = json.getString("user_first_name");
    if (first_name.empty()) {
        return cJSON_CreateFalse();
    }
    
    state_->setUserFirstName(first_name);
    return cJSON_CreateTrue();
}

cJSON* USBProtocol::handleSetUserLastName(const std::string& data) {
    JsonParser json(data);
    if (!json.valid()) {
        // Try as direct string value
        if (!data.empty()) {
            state_->setUserLastName(data);
            return cJSON_CreateTrue();
        }
        return cJSON_CreateFalse();
    }
    
    std::string last_name = json.getString("user_last_name");
    if (last_name.empty()) {
        return cJSON_CreateFalse();
    }
    
    state_->setUserLastName(last_name);
    return cJSON_CreateTrue();
}

cJSON* USBProtocol::handleSetUserPersona(const std::string& data) {
    JsonParser json(data);
    if (!json.valid()) {
        // Try as direct string value
        if (!data.empty()) {
            state_->setPersona(data);
            return cJSON_CreateTrue();
        }
        return cJSON_CreateFalse();
    }
    
    std::string persona = json.getString("user_persona");
    if (persona.empty()) {
        return cJSON_CreateFalse();
    }
    
    state_->setPersona(persona);
    return cJSON_CreateTrue();
}

cJSON* USBProtocol::handleGetBatteryStatus() {
    JsonBuilder data = JsonBuilder::object();
    data.addNumber("charge_level", state_->getBatteryLevel());
    
    bool is_charging = (state_->getBatteryChargingStatus() == BatteryChargingStatus::CHARGING);
    data.addBool("is_charging", is_charging);
    // voltage_mv not available yet, would need hardware driver
    // data.addNumber("voltage_mv", voltage_mv);
    
    return data.release();
}

cJSON* USBProtocol::handleEnableFeature(const std::string& feature_name, bool enable) {
    bool success = false;
    
    if (feature_name == "personal_alert_buttons") {
        state_->setEnablePersonalAlertButtons(enable);
        success = true;
    } else if (feature_name == "haptics") {
        state_->setEnableHaptics(enable);
        success = true;
    } else if (feature_name == "buzzer") {
        state_->setEnableBuzzer(enable);
        success = true;
    } else if (feature_name == "led_indications") {
        state_->setEnableLedIndications(enable);
        success = true;
    } else if (feature_name == "inactivity_alerts") {
        state_->setEnableInactivityAlerts(enable);
        success = true;
    } else if (feature_name == "nfc_session_sharing") {
        state_->setEnableNfcSessionSharing(enable);
        success = true;
    }
    
    return success ? cJSON_CreateTrue() : cJSON_CreateFalse();
}

cJSON* USBProtocol::handleSetTime(const std::string& data) {
    logger_->LOGI(TAG, "Set time requested - not implemented yet");
    return cJSON_CreateFalse();
}

cJSON* USBProtocol::handleSetQuiescentBeaconScanInterval(const std::string& data) {
    JsonParser json(data);
    if (!json.valid()) {
        return cJSON_CreateFalse();
    }
    
    int interval = json.getInt("quiescent_scan_interval_ms", 60000);
    if (interval < 1000 || interval > 300000) {
        logger_->LOGE(TAG, "Invalid quiescent scan interval: %d (must be 1000-300000ms)", interval);
        return cJSON_CreateFalse();
    }
    
    state_->setQuiescentBeaconScanModeScanIntervalMs(interval);
    return cJSON_CreateTrue();
}

cJSON* USBProtocol::handleSetFastBeaconScanInterval(const std::string& data) {
    JsonParser json(data);
    if (!json.valid()) {
        return cJSON_CreateFalse();
    }
    
    int interval = json.getInt("fast_scan_interval_ms", 20000);
    if (interval < 1000 || interval > 60000) {
        logger_->LOGE(TAG, "Invalid fast scan interval: %d (must be 1000-60000ms)", interval);
        return cJSON_CreateFalse();
    }
    
    state_->setFastBeaconScanModeScanIntervalMs(interval);
    return cJSON_CreateTrue();
}

cJSON* USBProtocol::handleSetPersonalAlertButtonDelay(const std::string& data) {
    JsonParser json(data);
    if (!json.valid()) {
        return cJSON_CreateFalse();
    }
    
    int delay = json.getInt("personal_alert_delay_ms", 5000);
    if (delay < 1000 || delay > 10000) {
        logger_->LOGE(TAG, "Invalid personal alert delay: %d (must be 1000-10000ms)", delay);
        return cJSON_CreateFalse();
    }
    
    state_->setPersonalAlertButtonTriggerDelayMs(delay);
    return cJSON_CreateTrue();
}


