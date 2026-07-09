#pragma once

#include <string>
#include "../constants/Constants.h"

// Forward declarations
class Logger;
class LowLevel;
class State;
struct cJSON;

/**
 * USBProtocol - USB command protocol handler
 * 
 * Handles structured JSON-based USB commands following the USBCommandVersion1 protocol.
 * Processes commands and returns JSON responses.
 */
class USBProtocol {
    const char* TAG = "USBProtocol";

public:
    USBProtocol(Logger* logger, LowLevel* lowLevel, State* state);
    ~USBProtocol();

    // Process a JSON command string and return JSON response
    std::string processCommand(const std::string& json_command);

    // Create a JSON response
    std::string createResponse(USBCommandVersion1 command, bool success, 
                               const std::string& data = "", const std::string& error = "");

private:
    Logger* logger_;
    LowLevel* lowLevel_;
    State* state_;

    // Command handlers - return cJSON objects for data
    cJSON* handleGetInfo();
    cJSON* handleResetDevice();
    cJSON* handleScanWifiNetworks();
    cJSON* handleConnectWifi(const std::string& data);
    cJSON* handleGetWifiStatus();
    cJSON* handleDisconnectWifi();
    cJSON* handleSetSessionId(const std::string& data);
    cJSON* handleSetUserFirstName(const std::string& data);
    cJSON* handleSetUserLastName(const std::string& data);
    cJSON* handleSetUserPersona(const std::string& data);
    cJSON* handleGetBatteryStatus();
    cJSON* handleEnableFeature(const std::string& feature_name, bool enable);
    cJSON* handleSetTime(const std::string& data);
    cJSON* handleSetQuiescentBeaconScanInterval(const std::string& data);
    cJSON* handleSetFastBeaconScanInterval(const std::string& data);
    cJSON* handleSetPersonalAlertButtonDelay(const std::string& data);

    // Helper methods
    bool parseJsonCommand(const std::string& json_str, USBCommandVersion1& command, std::string& data);
    
    // JSON response building helpers
    std::string buildResponse(USBCommandVersion1 command, bool success, cJSON* data_obj = nullptr, const std::string& error_msg = "");
    std::string cjsonToString(cJSON* json);
};

