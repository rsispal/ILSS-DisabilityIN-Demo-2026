#pragma once

#include <vector>
#include <cstdint>
#include "../constants/Constants.h"
#include "../utils/Logger.h"

// Forward declaration
class BLEBeacon;

// Forward declaration
class NVSLowLevelDriver;

class State
{
    const char *TAG = "State";

public:
    State(Logger *logger);
    ~State();

    void setLogLevel(LogLevel logLevel);
    LogLevel getLogLevel() const;

    /**
     * Set NVS driver (must be called before loadFromNVS/saveToNVS)
     * @param nvs_driver Pointer to NVSLowLevelDriver instance
     */
    void setNVSDriver(NVSLowLevelDriver* nvs_driver);

    bool loadDefaultState();
    bool loadFromNVS();
    bool saveToNVS();

    bool getIsProvisioned() const;
    std::string getDeviceId() const;
    std::string getWifiSsid() const;
    std::string getWifiPassword() const;
    uint8_t getWiFiBand() const;
    std::string getSessionId() const;
    std::string getPersona() const;
    std::string getUserFirstName() const;
    std::string getUserLastName() const;
    bool getEnablePersonalAlertButtons() const;
    bool getEnableHaptics() const;
    bool getEnableBuzzer() const;
    bool getEnableLedIndications() const;
    bool getEnableInactivityAlerts() const;
    bool getEnableNfcSessionSharing() const;
    bool getEnableHoneywellBeaconScanning() const;
    int getQuiescentBeaconScanModeScanIntervalMs() const;
    int getFastBeaconScanModeScanIntervalMs() const;
    int getPersonalAlertButtonTriggerDelayMs() const;


    bool setIsProvisioned(bool isProvisioned);
    bool setWifiSsid(const std::string &wifiSsid);
    bool setWifiPassword(const std::string &wifiPassword);
    bool setWiFiBand(uint8_t wifiBand);
    bool setSessionId(const std::string &sessionId);
    bool setPersona(const std::string &persona);
    bool setUserFirstName(const std::string &userFirstName);
    bool setUserLastName(const std::string &userLastName);
    bool setEnablePersonalAlertButtons(bool enablePersonalAlertButtons);
    bool setEnableHaptics(bool enableHaptics);
    bool setEnableBuzzer(bool enableBuzzer);
    bool setEnableLedIndications(bool enableLedIndications);
    bool setEnableInactivityAlerts(bool enableInactivityAlerts);
    bool setEnableNfcSessionSharing(bool enableNfcSessionSharing);
    bool setEnableHoneywellBeaconScanning(bool enableHoneywellBeaconScanning);
    bool setQuiescentBeaconScanModeScanIntervalMs(int quiescentBeaconScanModeScanIntervalMs);
    bool setFastBeaconScanModeScanIntervalMs(int fastBeaconScanModeScanIntervalMs);
    bool setPersonalAlertButtonTriggerDelayMs(int personalAlertButtonTriggerDelayMs);

    uint8_t getBatteryLevel() const;
    bool setBatteryLevel(uint8_t batteryLevel);
    BatteryChargingStatus getBatteryChargingStatus() const;
    bool setBatteryChargingStatus(BatteryChargingStatus batteryChargingStatus);

    // Beacon management methods
    void addOrUpdateBeacon(const BLEBeacon &beacon);
    void removeStaleBeacons();
    void clearBeacons();
    const std::vector<BLEBeacon> &getBeacons() const;
    BLEBeacon *getBestBeacon();

    // Fast beacon scan mode
    bool isFastBeaconScanMode() const;
    void setFastBeaconScanMode(bool enabled);
    int getCurrentBeaconScanWaitTimeSeconds() const;

    // Public access for dependency injection
    Logger *logger = nullptr;

private:
    LogLevel logLevel = LogLevel::NONE;

    // Provisioning state
    bool isProvisioned = false;
    bool isInFastBeaconScanMode = false;
    std::string wifiSsid;
    std::string wifiPassword;
    uint8_t wifiBand = 0;
    std::string sessionId;
    std::string persona;
    std::string userFirstName;
    std::string userLastName;
    const std::string deviceId = "019a74e0-d22b-72ca-8d4a-96314801ac6f"; // Read only value for the device ID matching the device's certificate (TODO: ensure this is set alongside the certificate info)
    // const std::string deviceId = "019a74e0-d22b-72ca-8d4a-11111111aa1b"; // Read only value for the device ID matching the device's certificate (TODO: ensure this is set alongside the certificate info)
    
    // Feature flags
    bool enablePersonalAlertButtons = true;
    bool enableHaptics = true;
    bool enableBuzzer = true;
    bool enableLedIndications = true;
    bool enableInactivityAlerts = false;
    bool enableNfcSessionSharing = true;
    bool enableHoneywellBeaconScanning = true;

    int quiescentBeaconScanModeScanIntervalMs = 30000;
    int fastBeaconScanModeScanIntervalMs = 15000;
    int personalAlertButtonTriggerDelayMs = 5000;

    // System State
    uint8_t batteryLevel = 0;
    BatteryChargingStatus batteryChargingStatus = BatteryChargingStatus::NOT_CHARGING;

    // System state - incoming fire alarm event
    bool isInFireEventMode = false;
    // System state - personal alert
    bool isInPersonalAlertMode = false;

    // Beacon scan results - maximum of 5 beacons
    std::vector<BLEBeacon> beacons;
    static constexpr size_t MAX_BEACONS = 5;
    static constexpr uint32_t BEACON_STALE_MS = 30000; // 30 seconds
    
    // NVS driver (set via setNVSDriver)
    NVSLowLevelDriver* nvs_driver_ = nullptr;

    // Track which preferences were loaded from NVS (to know when to apply Kconfig defaults)
    bool nvs_loaded_en_pa_btn = false;
    bool nvs_loaded_en_haptics = false;
    bool nvs_loaded_en_buzzer = false;
    bool nvs_loaded_en_led = false;
    bool nvs_loaded_en_inact = false;
    bool nvs_loaded_en_nfc = false;
    bool nvs_loaded_en_hw_beacon = false;
    bool nvs_loaded_q_scan_int = false;
    bool nvs_loaded_f_scan_int = false;
    bool nvs_loaded_pa_btn_dly = false;
};
