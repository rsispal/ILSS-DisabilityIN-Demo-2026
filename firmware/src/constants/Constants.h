#pragma once

#include <cstdint>

enum class LogLevel : uint8_t {
    NONE = 0,
    ERROR = 1,
    WARNING = 2,
    INFO = 3,
    DEBUG = 4
};

enum class BatteryChargingStatus : uint8_t {
    NOT_CHARGING = 0,
    CHARGING = 1,
    FULLY_CHARGED = 2,
    ERROR_REPLACE_BATTERY = 9
};

// USB Command Version 1 - Command enumeration
enum class USBCommandVersion1 : uint8_t {
    // Device Information
    GET_INFO = 1,

    // Device Control
    RESET_DEVICE = 2,

    // WiFi Operations
    SCAN_WIFI_NETWORKS = 3,
    CONNECT_WIFI = 4,
    GET_WIFI_STATUS = 5,
    DISCONNECT_WIFI = 6,

    // User Configuration
    SET_SESSION_ID = 7,
    SET_USER_FIRST_NAME = 8,
    SET_USER_LAST_NAME = 9,
    SET_USER_PERSONA = 10,

    // Device Status
    GET_BATTERY_STATUS = 11,

    // Feature Toggles
    ENABLE_PERSONAL_ALERT_BUTTONS = 12,
    DISABLE_PERSONAL_ALERT_BUTTONS = 13,
    ENABLE_HAPTICS = 14,
    DISABLE_HAPTICS = 15,
    ENABLE_BUZZER = 16,
    DISABLE_BUZZER = 17,
    ENABLE_LED_INDICATIONS = 18,
    DISABLE_LED_INDICATIONS = 19,
    ENABLE_INACTIVITY_ALERTS = 20,
    DISABLE_INACTIVITY_ALERTS = 21,
    ENABLE_NFC_SESSION_SHARING = 22,
    DISABLE_NFC_SESSION_SHARING = 23,

    // System Configuration
    SET_TIME = 24,
    SET_QUIESCENT_BEACON_SCAN_INTERVAL = 25,
    SET_FAST_BEACON_SCAN_INTERVAL = 26,
    SET_PERSONAL_ALERT_BUTTON_DELAY = 27
};

// User persona enumeration
enum class UserPersona : uint8_t {
    UNKNOWN = 0,
};

enum class StorageDataKey : uint16_t {
    IS_PROVISIONED = 0x00,
    WIFI_SSID = 0x101,
    WIFI_PASSWORD = 0x102,
    WIFI_BAND = 0x103,
    SESSION_ID = 0x201,
    PERSONA = 0x202,
    USER_FIRST_NAME = 0x203,
    USER_LAST_NAME = 0x204,
    ENABLE_PERSONAL_ALERT_BUTTONS = 0x301,
    ENABLE_HAPTICS = 0x302,
    ENABLE_BUZZER = 0x303,
    ENABLE_LED_INDICATIONS = 0x304,
    ENABLE_INACTIVITY_ALERTS = 0x305,
    ENABLE_NFC_SESSION_SHARING = 0x306,
    QUIESCENT_BEACON_SCAN_MODE_SCAN_INTERVAL_MS = 0x401,
    FAST_BEACON_SCAN_MODE_SCAN_INTERVAL_MS = 0x402,
    PERSONAL_ALERT_BUTTON_TRIGGER_DELAY_MS = 0x403
};

