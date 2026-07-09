#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include <cstring>
#include "lowlevel/LowLevel.h"
#include "lowlevel/usb/UsbLowLevelDriver.h"
#include "state/State.h"
#include "utils/Logger.h"
#include "application/ILSSLanyardApplication/ILSSLanyardApplication.h"
#include "application/FeatureTestApplication/FeatureTestApplication.h"
#include "features/usb/USBCLI.h"
#include "features/rgb-led/RGBLED.h"
#include "features/azure-iot/AzureIoT.h"


Logger logger;

// Helper function to attempt WiFi connection
bool connectToWiFi(LowLevel& low_level, State& state, RGBLED* led) {
    if (state.getWifiSsid().empty() || state.getWifiPassword().empty()) {
        logger.LOGW("main", "WiFi credentials not set");
        return false;
    }
    
    logger.LOGI("main", "Connecting to WiFi: %s", state.getWifiSsid().c_str());
    low_level.get_usb().writeLine("Connecting to WiFi...");
    
    // Attempt connection
    if (!low_level.get_wifi().connect(state.getWifiSsid(), state.getWifiPassword())) {
        logger.LOGE("main", "WiFi connection initiation failed");
        low_level.get_usb().writeLine("WiFi connection failed!");
        return false;
    }
    
    // Wait for connection with LED feedback
    const int wifi_timeout_ms = 15000;
    bool connected = false;
    int elapsed = 0;
    const int step_ms = 100;
    
    while (elapsed < wifi_timeout_ms) {
        if (led) led->process();
        
        if (low_level.get_wifi().isConnected()) {
            connected = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        elapsed += step_ms;
    }
    
    if (connected) {
        logger.LOGI("main", "WiFi connected successfully");
        low_level.get_usb().writeLine("WiFi connected!");
        return true;
    } else {
        logger.LOGE("main", "WiFi connection timeout");
        low_level.get_usb().writeLine("WiFi connection timeout!");
        return false;
    }
}

// Helper function to attempt Azure IoT connection
bool connectToAzure(AzureIoT& azure, LowLevel& low_level, RGBLED* led) {
    logger.LOGI("main", "Connecting to Azure IoT Hub...");
    low_level.get_usb().writeLine("Connecting to Azure IoT Hub...");
    
    // Process LED during connection attempt
    if (led) led->process();
    
    if (!azure.connect()) {
        logger.LOGE("main", "Azure IoT Hub connection failed");
        low_level.get_usb().writeLine("Azure IoT Hub connection failed!");
        return false;
    }
    
    logger.LOGI("main", "Azure IoT Hub connected successfully");
    low_level.get_usb().writeLine("Azure IoT Hub connected!");
    return true;
}

extern "C" void app_main(void)
{
    // If CONFIG_ILSS_LOG_LEVEL is a numeric Kconfig value (like 0, 1, 2, 3) that maps to LogLevel enum, this is correct:
    logger.setLogLevel(static_cast<LogLevel>(CONFIG_ILSS_LOG_LEVEL));

    State state(&logger);
    LowLevel low_level(&logger);
    
    // Initialize LowLevel (which initializes NVS first)
    if (!low_level.begin()) {
        logger.LOGE("main", "LowLevel initialization failed");
        return;
    }
    
    // Set NVS driver on State so it can load/save from NVS
    state.setNVSDriver(&low_level.get_nvs());
    
    // Load default state (including Kconfig preferences) before initializing any features
    // NVS is now initialized and State has access to it
    state.loadDefaultState();

    // Initialize LED and start pulsing white during boot/config prompt
    RGBLED bootLed(&state, &low_level);
    bool led_ok = bootLed.begin();
    if (led_ok) {
        bootLed.queueEffect(LedEffect::PULSE, LedColor::WHITE, Brightness::B100, 0);
        bootLed.process();
    }

    // Initialize USB CLI and check for configuration mode
    USBCLI usb_cli(&logger, &low_level, &state);
    usb_cli.begin();
    
    // Wait for any key press to enter configuration mode (5 second timeout)
    // Continue processing LED effect during wait
    bool config_mode = false;
    int elapsed = 0;
    const int timeout_ms = 5000;
    const int step_ms = 50;
    
    low_level.get_usb().writeLine("\r\nPress any key within the next 5 seconds to enter configuration mode...");
    
    while (elapsed < timeout_ms) {
        // Process LED effect
        if (led_ok) {
            bootLed.process();
        }
        
        // Check for key press
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
        low_level.get_usb().writeLine("No key pressed; continuing normal boot.");
    }
    
    if (config_mode) {
        logger.LOGI("main", "Entering configuration mode");
        usb_cli.runConfigurationMode();
        logger.LOGI("main", "Configuration mode exited");
    }

    // Check provisioning status
    logger.LOGI("main", "Checking provisioning status...");
    bool is_provisioned = state.getIsProvisioned();
    
    if (!is_provisioned) {
        // Check if all required fields are set
        bool has_session = !state.getSessionId().empty();
        bool has_fname = !state.getUserFirstName().empty();
        bool has_lname = !state.getUserLastName().empty();
        bool has_persona = !state.getPersona().empty();
        bool has_ssid = !state.getWifiSsid().empty();
        bool has_pass = !state.getWifiPassword().empty();
        
        if (has_session && has_fname && has_lname && has_persona && has_ssid && has_pass) {
            state.setIsProvisioned(true);
            is_provisioned = true;
            logger.LOGI("main", "All required fields set - device marked as provisioned");
        } else {
            logger.LOGW("main", "Device not provisioned. Missing: %s%s%s%s%s%s",
                       !has_session ? "SessionID " : "",
                       !has_fname ? "FirstName " : "",
                       !has_lname ? "LastName " : "",
                       !has_persona ? "Persona " : "",
                       !has_ssid ? "WiFi_SSID " : "",
                       !has_pass ? "WiFi_Pass " : "");
        }
    }
    
    logger.LOGI("main", "Provisioned: %s", is_provisioned ? "YES" : "NO");

    // If provisioned, attempt WiFi and Azure connections before entering main loop
    bool wifi_connected = false;
    bool azure_connected = false;
    
    if (is_provisioned) {
        // Change LED to indicate connection phase
        if (led_ok) {
            bootLed.queueEffect(LedEffect::PULSE, LedColor::BLUE, Brightness::B100, 0);
            bootLed.process();
        }
        
        // Connect to WiFi
        wifi_connected = connectToWiFi(low_level, state, led_ok ? &bootLed : nullptr);
        
        if (wifi_connected) {
            // Change LED to indicate Azure connection
            if (led_ok) {
                bootLed.queueEffect(LedEffect::PULSE, LedColor::CYAN, Brightness::B100, 0);
                bootLed.process();
            }
            
            // Initialize and connect to Azure
            AzureIoT azure(&state);
            azure.begin();
            azure_connected = connectToAzure(azure, low_level, led_ok ? &bootLed : nullptr);
            
            if (azure_connected) {
                // Successfully connected - disconnect Azure here, will reconnect in app
                azure.disconnect();
            }
        }
        
        // Show connection status
        if (wifi_connected && azure_connected) {
            logger.LOGI("main", "Pre-flight checks passed: WiFi and Azure connectivity verified");
            low_level.get_usb().writeLine("Pre-flight OK: WiFi + Azure verified");
            if (led_ok) {
                bootLed.queueEffect(LedEffect::PULSE, LedColor::GREEN, Brightness::B100, 0);
                bootLed.process();
                vTaskDelay(pdMS_TO_TICKS(1000));  // Show green briefly
            }
        } else {
            logger.LOGW("main", "Pre-flight checks failed: WiFi=%s, Azure=%s",
                       wifi_connected ? "OK" : "FAIL",
                       azure_connected ? "OK" : "FAIL");
            low_level.get_usb().writeLine("Pre-flight FAILED - check WiFi/Azure config");
            if (led_ok) {
                bootLed.queueEffect(LedEffect::RAPID_PULSE, LedColor::RED, Brightness::B100, 0);
                bootLed.process();
                vTaskDelay(pdMS_TO_TICKS(2000));  // Show red warning
            }
        }
    } else {
        logger.LOGI("main", "Skipping connectivity checks - device not provisioned");
        low_level.get_usb().writeLine("Device not provisioned - entering provisioning mode");
    }

    // Stop the boot LED before starting the application
    if (led_ok) {
        bootLed.stopEffect();
    }

    if (strcmp(CONFIG_ILSS_DEBUG_BOOT_INTO_FEATURE_DEVELOPMENT_MODE, "y") == 0) {
        logger.LOGI("main", "Booting into feature development mode");

        FeatureTestApplication feature_test_application(&logger, &low_level, &state);
        feature_test_application.begin();
    } else {
        logger.LOGI("main", "Booting into ILSS Lanyard application");
        ILSSLanyardApplication ilss_lanyard_application(&logger, &low_level, &state);
        ilss_lanyard_application.begin();
    }
}