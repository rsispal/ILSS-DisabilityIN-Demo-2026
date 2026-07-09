#include "State.h"
#include <algorithm>
#include <cstring>
#include "../layers/ble-beacon/BLEBeacon.h"
#include "../utils/Utils.h"
#include "../lowlevel/nvs/NVSLowLevelDriver.h"
#include <sstream>

// Implement constructor
State::State(Logger *logger) : logger(logger), nvs_driver_(nullptr)
{
    // Don't override the log level - let main.cpp control it
}

void State::setNVSDriver(NVSLowLevelDriver* nvs_driver)
{
    nvs_driver_ = nvs_driver;
}

State::~State()
{
}

bool State::loadDefaultState()
{
    // Try to load from NVS first
    bool nvs_loaded = loadFromNVS();
    
    if (!nvs_loaded) {
        // Load defaults only if NVS load completely failed
        this->setIsProvisioned(false);
        this->setWifiSsid("51MD_Wi-Fi_Guest");
        this->setWifiPassword("51meadowdrive");
        this->setSessionId("");
        this->setPersona("");
        this->setUserFirstName("");
        this->setUserLastName("");
    }
    
    // Always apply Kconfig defaults for preferences that weren't loaded from NVS
    // This ensures Kconfig values are used even if NVS loaded but didn't have these preferences
    #ifdef CONFIG_ILSS_PREFERENCES_PERSONAL_ALERT_BUTTONS_ENABLED
    if (!nvs_loaded || !nvs_loaded_en_pa_btn) {
        this->setEnablePersonalAlertButtons(CONFIG_ILSS_PREFERENCES_PERSONAL_ALERT_BUTTONS_ENABLED);
    }
    #else
    if (!nvs_loaded || !nvs_loaded_en_pa_btn) {
        this->setEnablePersonalAlertButtons(true);
    }
    #endif
    
    #ifdef CONFIG_ILSS_PREFERENCES_HAPTICS_ENABLED
    if (!nvs_loaded || !nvs_loaded_en_haptics) {
        this->setEnableHaptics(CONFIG_ILSS_PREFERENCES_HAPTICS_ENABLED);
    }
    #else
    if (!nvs_loaded || !nvs_loaded_en_haptics) {
        this->setEnableHaptics(true);
    }
    #endif
    
    #ifdef CONFIG_ILSS_PREFERENCES_AUDIBLE_INDICATORS_ENABLED
    if (!nvs_loaded || !nvs_loaded_en_buzzer) {
        this->setEnableBuzzer(CONFIG_ILSS_PREFERENCES_AUDIBLE_INDICATORS_ENABLED);
    }
    #else
    if (!nvs_loaded || !nvs_loaded_en_buzzer) {
        this->setEnableBuzzer(true);
    }
    #endif
    
    #ifdef CONFIG_ILSS_PREFERENCES_LED_INDICATORS_ENABLED
    if (!nvs_loaded || !nvs_loaded_en_led) {
        this->setEnableLedIndications(CONFIG_ILSS_PREFERENCES_LED_INDICATORS_ENABLED);
    }
    #else
    if (!nvs_loaded || !nvs_loaded_en_led) {
        this->setEnableLedIndications(true);
    }
    #endif
    
    #ifdef CONFIG_ILSS_PREFERENCES_INACTIVITY_ALERT_ENABLED
    if (!nvs_loaded || !nvs_loaded_en_inact) {
        this->setEnableInactivityAlerts(CONFIG_ILSS_PREFERENCES_INACTIVITY_ALERT_ENABLED);
    }
    #else
    if (!nvs_loaded || !nvs_loaded_en_inact) {
        this->setEnableInactivityAlerts(false);
    }
    #endif
    
    #ifdef CONFIG_ILSS_PREFERENCES_NFC_SESSION_SHARING_ENABLED
    if (!nvs_loaded || !nvs_loaded_en_nfc) {
        this->setEnableNfcSessionSharing(CONFIG_ILSS_PREFERENCES_NFC_SESSION_SHARING_ENABLED);
    }
    #else
    if (!nvs_loaded || !nvs_loaded_en_nfc) {
        this->setEnableNfcSessionSharing(true);
    }
    #endif
    
    #ifdef CONFIG_ILSS_PREFERENCES_HONEYWELL_BEACON_SCANNING_ENABLED
    if (!nvs_loaded || !nvs_loaded_en_hw_beacon) {
        this->setEnableHoneywellBeaconScanning(CONFIG_ILSS_PREFERENCES_HONEYWELL_BEACON_SCANNING_ENABLED);
    }
    #else
    if (!nvs_loaded || !nvs_loaded_en_hw_beacon) {
        this->setEnableHoneywellBeaconScanning(true);
    }
    #endif
    
    #ifdef CONFIG_ILSS_PREFERENCES_QUIESCENT_BEACON_SCAN_MODE_SCAN_INTERVAL_MS
    if (!nvs_loaded || !nvs_loaded_q_scan_int) {
        this->setQuiescentBeaconScanModeScanIntervalMs(CONFIG_ILSS_PREFERENCES_QUIESCENT_BEACON_SCAN_MODE_SCAN_INTERVAL_MS);
    }
    #else
    if (!nvs_loaded || !nvs_loaded_q_scan_int) {
        this->setQuiescentBeaconScanModeScanIntervalMs(60000);
    }
    #endif
    
    #ifdef CONFIG_ILSS_PREFERENCES_FAST_BEACON_SCAN_MODE_SCAN_INTERVAL_MS
    if (!nvs_loaded || !nvs_loaded_f_scan_int) {
        this->setFastBeaconScanModeScanIntervalMs(CONFIG_ILSS_PREFERENCES_FAST_BEACON_SCAN_MODE_SCAN_INTERVAL_MS);
    }
    #else
    if (!nvs_loaded || !nvs_loaded_f_scan_int) {
        this->setFastBeaconScanModeScanIntervalMs(20000);
    }
    #endif
    
    #ifdef CONFIG_ILSS_PREFERENCES_PERSONAL_ALERT_BUTTON_TRIGGER_DELAY_MS
    if (!nvs_loaded || !nvs_loaded_pa_btn_dly) {
        this->setPersonalAlertButtonTriggerDelayMs(CONFIG_ILSS_PREFERENCES_PERSONAL_ALERT_BUTTON_TRIGGER_DELAY_MS);
    }
    #else
    if (!nvs_loaded || !nvs_loaded_pa_btn_dly) {
        this->setPersonalAlertButtonTriggerDelayMs(5000);
    }
    #endif

    // TODO: general preferences
    
    return true;
}

bool State::loadFromNVS()
{
    if (!nvs_driver_ || !nvs_driver_->isReady()) {
        logger->LOGD(TAG, "NVS driver not available, using defaults");
        return false;
    }

    logger->LOGI(TAG, "loadFromNVS: Loading state from NVS");
    nvs_handle_t nvs_handle = nvs_driver_->openNamespace("ilss_state", true);  // Read-only
    if (nvs_handle == 0) {
        logger->LOGD(TAG, "NVS namespace 'ilss_state' not found or unavailable, using defaults");
        return false;
    }

    bool loaded_any = false;
    // Reset tracking flags
    nvs_loaded_en_pa_btn = false;
    nvs_loaded_en_haptics = false;
    nvs_loaded_en_buzzer = false;
    nvs_loaded_en_led = false;
    nvs_loaded_en_inact = false;
    nvs_loaded_en_nfc = false;
    nvs_loaded_en_hw_beacon = false;
    nvs_loaded_q_scan_int = false;
    nvs_loaded_f_scan_int = false;
    nvs_loaded_pa_btn_dly = false;

    // Load string values
    std::string str_val;
    if (nvs_driver_->getString(nvs_handle, "wifi_ssid", str_val)) {
        wifiSsid = str_val;
        loaded_any = true;
    }
    if (nvs_driver_->getString(nvs_handle, "wifi_pass", str_val)) {
        wifiPassword = str_val;
        loaded_any = true;
    }
    if (nvs_driver_->getString(nvs_handle, "session_id", str_val)) {
        sessionId = str_val;
        loaded_any = true;
    }
    if (nvs_driver_->getString(nvs_handle, "persona", str_val)) {
        persona = str_val;
        loaded_any = true;
    }
    if (nvs_driver_->getString(nvs_handle, "user_fname", str_val)) {
        userFirstName = str_val;
        loaded_any = true;
    }
    if (nvs_driver_->getString(nvs_handle, "user_lname", str_val)) {
        userLastName = str_val;
        loaded_any = true;
    }

    // Load uint8_t values
    uint8_t u8_val;
    if (nvs_driver_->getUInt8(nvs_handle, "wifi_band", u8_val)) {
        wifiBand = u8_val;
        loaded_any = true;
    }
    if (nvs_driver_->getUInt8(nvs_handle, "battery_lvl", u8_val)) {
        batteryLevel = u8_val;
        loaded_any = true;
    }
    if (nvs_driver_->getUInt8(nvs_handle, "battery_chg", u8_val)) {
        batteryChargingStatus = static_cast<BatteryChargingStatus>(u8_val);
        loaded_any = true;
    }

    // Load bool values
    bool bool_val;
    if (nvs_driver_->getBool(nvs_handle, "is_prov", bool_val)) {
        isProvisioned = bool_val;
        loaded_any = true;
        if (logger != nullptr) {
            logger->LOGD(TAG, "loadFromNVS: Loaded isProvisioned = %s", isProvisioned ? "true" : "false");
        }
    } else {
        if (logger != nullptr) {
            logger->LOGD(TAG, "loadFromNVS: is_prov not found in NVS");
        }
    }
    if (nvs_driver_->getBool(nvs_handle, "fast_scan", bool_val)) {
        isInFastBeaconScanMode = bool_val;
        loaded_any = true;
    }
    if (nvs_driver_->getBool(nvs_handle, "en_pa_btn", bool_val)) {
        enablePersonalAlertButtons = bool_val;
        loaded_any = true;
        nvs_loaded_en_pa_btn = true;
    }
    if (nvs_driver_->getBool(nvs_handle, "en_haptics", bool_val)) {
        enableHaptics = bool_val;
        loaded_any = true;
        nvs_loaded_en_haptics = true;
    }
    if (nvs_driver_->getBool(nvs_handle, "en_buzzer", bool_val)) {
        enableBuzzer = bool_val;
        loaded_any = true;
        nvs_loaded_en_buzzer = true;
    }
    if (nvs_driver_->getBool(nvs_handle, "en_led", bool_val)) {
        enableLedIndications = bool_val;
        loaded_any = true;
        nvs_loaded_en_led = true;
    }
    if (nvs_driver_->getBool(nvs_handle, "en_inact", bool_val)) {
        enableInactivityAlerts = bool_val;
        loaded_any = true;
        nvs_loaded_en_inact = true;
    }
    if (nvs_driver_->getBool(nvs_handle, "en_nfc", bool_val)) {
        enableNfcSessionSharing = bool_val;
        loaded_any = true;
        nvs_loaded_en_nfc = true;
    }
    if (nvs_driver_->getBool(nvs_handle, "en_hw_beacon", bool_val)) {
        enableHoneywellBeaconScanning = bool_val;
        loaded_any = true;
        nvs_loaded_en_hw_beacon = true;
    }
    if (nvs_driver_->getBool(nvs_handle, "fire_mode", bool_val)) {
        isInFireEventMode = bool_val;
        loaded_any = true;
    }
    if (nvs_driver_->getBool(nvs_handle, "pa_mode", bool_val)) {
        isInPersonalAlertMode = bool_val;
        loaded_any = true;
    }

    // Load int values
    int32_t i32_val;
    if (nvs_driver_->getInt32(nvs_handle, "q_scan_int", i32_val)) {
        quiescentBeaconScanModeScanIntervalMs = i32_val;
        loaded_any = true;
        nvs_loaded_q_scan_int = true;
    }
    if (nvs_driver_->getInt32(nvs_handle, "f_scan_int", i32_val)) {
        fastBeaconScanModeScanIntervalMs = i32_val;
        loaded_any = true;
        nvs_loaded_f_scan_int = true;
    }
    if (nvs_driver_->getInt32(nvs_handle, "pa_btn_dly", i32_val)) {
        personalAlertButtonTriggerDelayMs = i32_val;
        loaded_any = true;
        nvs_loaded_pa_btn_dly = true;
    }

    nvs_driver_->closeNamespace(nvs_handle);

    if (logger != nullptr && loaded_any) {
        logger->LOGI(TAG, "Loaded state from NVS");
    }

    return loaded_any;
}

bool State::saveToNVS()
{
    if (!nvs_driver_ || !nvs_driver_->isReady()) {
        logger->LOGW(TAG, "NVS driver not available, cannot save state");
        return false;
    }

    logger->LOGI(TAG, "saveToNVS: Saving state to NVS");
    nvs_handle_t nvs_handle = nvs_driver_->openNamespace("ilss_state", false);  // Read-write
    if (nvs_handle == 0) {
        logger->LOGE(TAG, "Failed to open NVS namespace 'ilss_state'");
        return false;
    }

    bool saved_any = false;

    // Save string values
    if (nvs_driver_->setString(nvs_handle, "wifi_ssid", wifiSsid)) {
        saved_any = true;
    }
    if (nvs_driver_->setString(nvs_handle, "wifi_pass", wifiPassword)) {
        saved_any = true;
    }
    if (nvs_driver_->setString(nvs_handle, "session_id", sessionId)) {
        saved_any = true;
    }
    if (nvs_driver_->setString(nvs_handle, "persona", persona)) {
        saved_any = true;
    }
    if (nvs_driver_->setString(nvs_handle, "user_fname", userFirstName)) {
        saved_any = true;
    }
    if (nvs_driver_->setString(nvs_handle, "user_lname", userLastName)) {
        saved_any = true;
    }

    // Save uint8_t values
    if (nvs_driver_->setUInt8(nvs_handle, "wifi_band", wifiBand)) {
        saved_any = true;
    }
    if (nvs_driver_->setUInt8(nvs_handle, "battery_lvl", batteryLevel)) {
        saved_any = true;
    }
    if (nvs_driver_->setUInt8(nvs_handle, "battery_chg", static_cast<uint8_t>(batteryChargingStatus))) {
        saved_any = true;
    }

    // Save bool values
    if (nvs_driver_->setBool(nvs_handle, "is_prov", isProvisioned)) {
        saved_any = true;
    }
    if (nvs_driver_->setBool(nvs_handle, "fast_scan", isInFastBeaconScanMode)) {
        saved_any = true;
    }
    if (nvs_driver_->setBool(nvs_handle, "en_pa_btn", enablePersonalAlertButtons)) {
        saved_any = true;
    }
    if (nvs_driver_->setBool(nvs_handle, "en_haptics", enableHaptics)) {
        saved_any = true;
    }
    if (nvs_driver_->setBool(nvs_handle, "en_buzzer", enableBuzzer)) {
        saved_any = true;
    }
    if (nvs_driver_->setBool(nvs_handle, "en_led", enableLedIndications)) {
        saved_any = true;
    }
    if (nvs_driver_->setBool(nvs_handle, "en_inact", enableInactivityAlerts)) {
        saved_any = true;
    }
    if (nvs_driver_->setBool(nvs_handle, "en_nfc", enableNfcSessionSharing)) {
        saved_any = true;
    }
    if (nvs_driver_->setBool(nvs_handle, "en_hw_beacon", enableHoneywellBeaconScanning)) {
        saved_any = true;
    }
    if (nvs_driver_->setBool(nvs_handle, "fire_mode", isInFireEventMode)) {
        saved_any = true;
    }
    if (nvs_driver_->setBool(nvs_handle, "pa_mode", isInPersonalAlertMode)) {
        saved_any = true;
    }

    // Save int values
    if (nvs_driver_->setInt32(nvs_handle, "q_scan_int", quiescentBeaconScanModeScanIntervalMs)) {
        saved_any = true;
    }
    if (nvs_driver_->setInt32(nvs_handle, "f_scan_int", fastBeaconScanModeScanIntervalMs)) {
        saved_any = true;
    }
    if (nvs_driver_->setInt32(nvs_handle, "pa_btn_dly", personalAlertButtonTriggerDelayMs)) {
        saved_any = true;
    }

    // Commit changes
    if (!nvs_driver_->commit(nvs_handle)) {
        nvs_driver_->closeNamespace(nvs_handle);
        return false;
    }

    nvs_driver_->closeNamespace(nvs_handle);

    if (logger != nullptr && saved_any) {
        logger->LOGI(TAG, "Saved state to NVS");
    }

    return saved_any;
}

void State::setLogLevel(LogLevel logLevel)
{
    this->logLevel = logLevel;
    if (this->logger != nullptr)
    {
        this->logger->setLogLevel(logLevel);
    }
}

LogLevel State::getLogLevel() const
{
    return this->logLevel;
}

bool State::getIsProvisioned() const
{
    // First check the explicit provisioned flag
    if (!this->isProvisioned) {
        if (logger != nullptr) {
            logger->LOGD(TAG, "getIsProvisioned: isProvisioned flag is false");
        }
        return false;
    }
    
    // Then verify we have all required fields (wifiBand is optional, defaults to 0 for auto)
    if (this->wifiSsid.empty() || this->wifiPassword.empty() || 
        this->sessionId.empty() || this->persona.empty() || 
        this->userFirstName.empty() || this->userLastName.empty())
    {
        if (logger != nullptr) {
            logger->LOGD(TAG, "getIsProvisioned: flag is true but missing required fields - ssid:%s pass:%d session:%d persona:%d fname:%d lname:%d",
                wifiSsid.empty() ? "empty" : "set",
                wifiPassword.empty() ? 0 : 1,
                sessionId.empty() ? 0 : 1,
                persona.empty() ? 0 : 1,
                userFirstName.empty() ? 0 : 1,
                userLastName.empty() ? 0 : 1);
        }
        return false;
    }
    
    if (logger != nullptr) {
        logger->LOGD(TAG, "getIsProvisioned: returning true (flag=true, all fields present)");
    }
    return true;
}

std::string State::getDeviceId() const
{
    return this->deviceId;
}

std::string State::getWifiSsid() const
{
    return this->wifiSsid;
}

std::string State::getWifiPassword() const
{
    return this->wifiPassword;
}

uint8_t State::getWiFiBand() const
{
    return this->wifiBand;
}

std::string State::getSessionId() const
{
    return this->sessionId;
}

std::string State::getUserFirstName() const
{
    return this->userFirstName;
}

std::string State::getUserLastName() const
{
    return this->userLastName;
}

bool State::getEnablePersonalAlertButtons() const
{
    return this->enablePersonalAlertButtons;
}

bool State::getEnableHaptics() const
{
    return this->enableHaptics;
}

bool State::getEnableBuzzer() const
{
    return this->enableBuzzer;
}

bool State::getEnableLedIndications() const
{
    return this->enableLedIndications;
}

bool State::getEnableInactivityAlerts() const
{
    return this->enableInactivityAlerts;
}

bool State::getEnableNfcSessionSharing() const
{
    return this->enableNfcSessionSharing;
}

bool State::getEnableHoneywellBeaconScanning() const
{
    return this->enableHoneywellBeaconScanning;
}

std::string State::getPersona() const
{
    return this->persona;
}

int State::getQuiescentBeaconScanModeScanIntervalMs() const
{
    return this->quiescentBeaconScanModeScanIntervalMs;
}

int State::getFastBeaconScanModeScanIntervalMs() const
{
    return this->fastBeaconScanModeScanIntervalMs;
}

int State::getPersonalAlertButtonTriggerDelayMs() const
{
    return this->personalAlertButtonTriggerDelayMs;
}

bool State::setIsProvisioned(bool isProvisioned)
{
    this->isProvisioned = isProvisioned;
    return true;
}

bool State::setWifiSsid(const std::string &wifiSsid)
{
    this->wifiSsid = wifiSsid;
    return true;
}

bool State::setWifiPassword(const std::string &wifiPassword)
{
    this->wifiPassword = wifiPassword;
    return true;
}

bool State::setWiFiBand(uint8_t wifiBand)
{
    this->wifiBand = wifiBand;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setSessionId(const std::string &sessionId)
{
    this->sessionId = sessionId;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setPersona(const std::string &persona)
{
    this->persona = persona;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setUserFirstName(const std::string &userFirstName)
{
    this->userFirstName = userFirstName;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setUserLastName(const std::string &userLastName)
{
    this->userLastName = userLastName;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setEnablePersonalAlertButtons(bool enablePersonalAlertButtons)
{
    this->enablePersonalAlertButtons = enablePersonalAlertButtons;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setEnableHaptics(bool enableHaptics)
{
    this->enableHaptics = enableHaptics;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setEnableBuzzer(bool enableBuzzer)
{
    this->enableBuzzer = enableBuzzer;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setEnableLedIndications(bool enableLedIndications)
{
    this->enableLedIndications = enableLedIndications;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setEnableInactivityAlerts(bool enableInactivityAlerts)
{
    this->enableInactivityAlerts = enableInactivityAlerts;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setEnableNfcSessionSharing(bool enableNfcSessionSharing)
{
    this->enableNfcSessionSharing = enableNfcSessionSharing;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setEnableHoneywellBeaconScanning(bool enableHoneywellBeaconScanning)
{
    this->enableHoneywellBeaconScanning = enableHoneywellBeaconScanning;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setQuiescentBeaconScanModeScanIntervalMs(int quiescentBeaconScanModeScanIntervalMs)
{
    this->quiescentBeaconScanModeScanIntervalMs = quiescentBeaconScanModeScanIntervalMs;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setFastBeaconScanModeScanIntervalMs(int fastBeaconScanModeScanIntervalMs)
{
    this->fastBeaconScanModeScanIntervalMs = fastBeaconScanModeScanIntervalMs;
    saveToNVS(); // Auto-save to NVS
    return true;
}

bool State::setPersonalAlertButtonTriggerDelayMs(int personalAlertButtonTriggerDelayMs)
{
    this->personalAlertButtonTriggerDelayMs = personalAlertButtonTriggerDelayMs;
    saveToNVS(); // Auto-save to NVS
    return true;
}

uint8_t State::getBatteryLevel() const
{
    return this->batteryLevel;
}

bool State::setBatteryLevel(uint8_t batteryLevel)
{
    this->batteryLevel = batteryLevel;
    return true;
}

BatteryChargingStatus State::getBatteryChargingStatus() const
{
    return this->batteryChargingStatus;
}

bool State::setBatteryChargingStatus(BatteryChargingStatus batteryChargingStatus)
{
    this->batteryChargingStatus = batteryChargingStatus;
    return true;
}

// Beacon management methods
void State::addOrUpdateBeacon(const BLEBeacon &beacon)
{
    // Try to find existing beacon with same identifier (serial number)
    for (BLEBeacon &existingBeacon : beacons)
    {
        if (existingBeacon.getIdentifier() == beacon.getIdentifier())
        {
            // Update existing beacon with new RSSI and timestamp
            existingBeacon.updateRssi(beacon.getRssi());
            // RSSI updates are now logged at the Bluetooth feature level for significant changes only
            return;
        }
    }

    // If not found and we have space, add new beacon
    if (beacons.size() < MAX_BEACONS)
    {
        beacons.push_back(beacon);
        // New beacon logging is now handled at the Bluetooth feature level
    }
    else
    {
        // Remove oldest beacon and add new one
        BLEBeacon removedBeacon = beacons.front();
        beacons.erase(beacons.begin());
        beacons.push_back(beacon);
        if (logger != nullptr)
        {
            logger->LOGD(TAG, "Replaced beacon: %llu -> %llu (rssi:%d)",
                         removedBeacon.getIdentifier(), beacon.getIdentifier(), beacon.getRssi());
        }
    }
}

void State::removeStaleBeacons()
{
    uint32_t currentTime = Utils::getCurrentTimestamp();
    beacons.erase(
        std::remove_if(beacons.begin(), beacons.end(),
                       [currentTime](const BLEBeacon &beacon)
                       {
                           return (currentTime - beacon.getLastSeenTimestamp()) > BEACON_STALE_MS;
                       }),
        beacons.end());
}

void State::clearBeacons()
{
    beacons.clear();
}

const std::vector<BLEBeacon> &State::getBeacons() const
{
    return beacons;
}

BLEBeacon *State::getBestBeacon()
{
    if (beacons.empty())
    {
        return nullptr;
    }

    BLEBeacon *bestBeacon = &beacons[0];
    for (size_t i = 1; i < beacons.size(); ++i)
    {
        BLEBeacon *currentBeacon = &beacons[i];

        uint32_t ts1 = currentBeacon->getLastSeenTimestamp();
        uint32_t ts2 = bestBeacon->getLastSeenTimestamp();
        int8_t rssi1 = currentBeacon->getRssi();
        int8_t rssi2 = bestBeacon->getRssi();

        // Higher timestamp is better, if equal then higher RSSI is better
        if (ts1 > ts2 || (ts1 == ts2 && rssi1 > rssi2))
        {
            bestBeacon = currentBeacon;
        }
    }

    return bestBeacon;
}

bool State::isFastBeaconScanMode() const
{
    return isInFastBeaconScanMode;
}

void State::setFastBeaconScanMode(bool enabled)
{
    isInFastBeaconScanMode = enabled;
}

int State::getCurrentBeaconScanWaitTimeSeconds() const
{
    // Return wait time based on scan mode using actual intervals from state
    return isInFastBeaconScanMode ? (fastBeaconScanModeScanIntervalMs / 1000) : (quiescentBeaconScanModeScanIntervalMs / 1000);
}
