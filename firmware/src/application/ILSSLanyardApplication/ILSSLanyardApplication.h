#pragma once

#include "../../state/State.h"
#include "../../utils/Logger.h"
#include "../../lowlevel/LowLevel.h"
#include "../../features/azure-iot/AzureIoT.h"
#include "../../features/bluetooth/Bluetooth.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/side-buttons/SideButtons.h"
#include "../../features/buzzer/Buzzer.h"
#include "../../features/usb/USBCLI.h"
#include "../../utils/JSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include <string>

// Buzzer command types for task queue
enum class BuzzerCommand {
    TICK,
    BEEP,
    START_CONTINUOUS,
    STOP,
    PLAY_ALTERNATING,
    PLAY_MEDIUM_SWEEP,
    PLAY_SIREN,
    PLAY_CODE3_TEMPORAL,
    PLAY_LF_BUZZ,
    QUEUE_SIREN,
    QUEUE_CODE3_TEMPORAL,
    QUEUE_CODE3_SWEEP,
    QUEUE_CODE3_SIREN,
    QUEUE_BEEP,
    QUEUE_TICK,
    QUEUE_LF_BUZZ,
    QUEUE_MEDIUM_SWEEP,
    QUEUE_ALTERNATING
};

// Buzzer command message structure
struct BuzzerCommandMsg {
    BuzzerCommand cmd;
    uint32_t param1;  // freq, low_freq, etc.
    uint32_t param2;  // duration_ms, high_freq, etc.
    uint32_t param3;  // cycles
};

// LED command types for task queue
enum class LEDCommand {
    SET_EFFECT,
    STOP_EFFECT,
    PROCESS  // Process current effect
};

// LED command message structure
struct LEDCommandMsg {
    LEDCommand cmd;
    uint32_t param1;  // effect, color, etc.
    uint32_t param2;  // brightness, duration, etc.
    uint32_t param3;  // duration
};

/**
 * ILSSLanyardApplication - Main application for ILSS Lanyard Device Version 1
 * 
 * Implements the complete lanyard functionality including:
 * - Bootup sequence with LED patterns
 * - Quiescent Mode (beacon scanning, location updates)
 * - Provisioning Mode (USB-based configuration)
 * - Fire Event Mode (alerts and fast scanning)
 * - Personal Alert Mode (button-triggered emergency)
 * - Inactivity Alert (stub for future implementation)
 */
class ILSSLanyardApplication {
    const char* TAG = "ILSSLanyard";

public:
    ILSSLanyardApplication(Logger* logger, LowLevel* lowLevel, State* state);
    ~ILSSLanyardApplication();
    
    void begin();

private:
    State* state;
    Logger* logger;
    LowLevel* lowLevel;
    
    // Feature instances
    AzureIoT* azureIoT;
    Bluetooth* bluetooth;
    RGBLED* rgbLed;
    SideButtons* sideButtons;
    Buzzer* buzzer;
    
    // Application state
    enum class AppMode {
        BOOTUP,
        PROVISIONING,
        QUIESCENT,
        FIRE_EVENT,
        PERSONAL_ALERT,
        INACTIVITY_ALERT,
        ERROR
    };
    AppMode currentMode;
    
    // Timing and counters
    uint32_t lastBeaconScanTime;
    uint32_t lastLocationUpdateTime;
    uint32_t lastFireAlertTime;
    uint32_t lastPersonalAlertTime;
    uint32_t eventIdCounter;
    
    // Beacon scan state management
    bool scanInProgress;
    uint32_t scanStartTime;
    static constexpr uint32_t SCAN_WINDOW_MS = 5000;  // 5 seconds maximum scan window
    
    // Dual-core task architecture
    TaskHandle_t buzzer_task_handle;  // Buzzer task pinned to core 1
    TaskHandle_t led_task_handle;     // LED task pinned to core 1
    QueueHandle_t buzzer_queue;       // Queue for buzzer commands
    QueueHandle_t led_queue;          // Queue for LED commands
    
    // Timers for periodic tasks (simple one-shot timers only)
    esp_timer_handle_t scan_interval_timer;  // For triggering scans at intervals
    esp_timer_handle_t fire_alert_timer;     // For fire alert updates (15s)
    esp_timer_handle_t personal_alert_timer; // For personal alert updates (5s)
    esp_timer_handle_t fire_haptic_timer;    // For haptic pattern 118 in fire mode (3s)
    esp_timer_handle_t personal_haptic_timer; // For haptic pattern 118 in personal alert mode (3s)
    
    // Bootup sequence
    void bootupSequence();
    void showBootupLEDPattern();
    
    // Mode handlers
    void enterProvisioningMode();
    void enterQuiescentMode();
    void enterFireEventMode();
    void enterPersonalAlertMode();
    void enterInactivityAlertMode();
    void enterErrorMode();
    
    // Quiescent Mode operations
    void quiescentModeLoop();
    void performBeaconScan();
    void sendLocationUpdate();
    void checkButtons();
    void checkModeButton();
    
    // Error Mode operations
    void errorModeLoop();
    void performFactoryReset();
    
    // Provisioning Mode operations
    void provisioningModeLoop();
    bool checkProvisioningComplete();
    
    // Fire Event Mode operations
    void fireEventModeLoop();
    void updateFireAlerts();
    void startFireAlarmBuzzer();
    
    // Personal Alert Mode operations
    void personalAlertModeLoop();
    void updatePersonalAlerts();
    void startPersonalAlertBuzzer();
    
    // Message handling
    void setupAzureMessageCallback();
    void handleAzureMessage(const char* topic, const uint8_t* payload, size_t len);
    void processFireAlarmMessage(const JsonParser& parser);
    void processFireAlarmResetMessage(const JsonParser& parser);
    void processPersonalAlertResetMessage(const JsonParser& parser);
    void processInactivityAlertResetMessage(const JsonParser& parser);
    
    // JSON message builders
    std::string buildLocationUpdateMessage();
    std::string buildPersonalAlertMessage();
    std::string buildInactivityAlertMessage();
    
    // Helper methods
    std::string getBatteryStatusString();
    int generateEventId();
    void updateBeaconScanInterval(bool fastMode);
    
    // Dual-core task methods
    void initDualCoreTasks();
    void deinitDualCoreTasks();
    static void buzzerTask(void* arg);
    static void ledTask(void* arg);
    
    // Command queue helpers
    void sendBuzzerCommand(BuzzerCommand cmd, uint32_t param1 = 0, uint32_t param2 = 0, uint32_t param3 = 0);
    void sendLEDCommand(LEDCommand cmd, uint32_t param1 = 0, uint32_t param2 = 0, uint32_t param3 = 0);
    
    // Timer callbacks (static for C compatibility - only post to queues or set flags)
    static void scanIntervalTimerCallback(void* arg);
    static void fireAlertTimerCallback(void* arg);
    static void personalAlertTimerCallback(void* arg);
    static void fireHapticTimerCallback(void* arg);
    static void personalHapticTimerCallback(void* arg);
    
    // Scan completion callback (from Bluetooth)
    void onScanComplete();
};

