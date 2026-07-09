#pragma once

#include <string>
#include <vector>
#include "esp_err.h"
#include "esp_netif_types.h"  // For esp_ip4_addr_t
#include "esp_wifi_types.h"   // For wifi_ap_record_t

// Forward declaration
class Logger;

// WiFi network information structure
struct WiFiNetwork {
    std::string ssid;
    int rssi;
    int security;  // 0=Open, 1=WPA/WPA2, 2=WEP, 3=WPA3
    int frequency_band;  // 0=Unknown, 1=2.4GHz, 2=5GHz
    int channel;
    uint8_t bssid[6];  // MAC address of the access point
};

/**
 * WiFiLowLevelDriver - Low-level WiFi driver
 * 
 * Handles WiFi initialization, connection, and status management.
 * Designed for dependency injection into higher-level feature classes.
 */
class WiFiLowLevelDriver {
    const char* TAG = "WiFiLowLevelDriver";

public:
    enum class ConnectionStatus {
        Disconnected,
        Connecting,
        Connected,
        Failed
    };

    WiFiLowLevelDriver(Logger* logger);
    ~WiFiLowLevelDriver();

    // Initialize WiFi subsystem (call once at startup)
    bool begin();

    // Scan for available WiFi networks
    std::vector<WiFiNetwork> scan();

    // Connect to WiFi network
    bool connect(const std::string& ssid, const std::string& password, int band = 0);

    // Disconnect from WiFi
    bool disconnect();

    // Get current connection status
    ConnectionStatus getStatus() const { return status_; }

    // Get assigned IP address as string (valid only when connected)
    std::string getIPAddress() const;

    // Get assigned IP address struct (valid only when connected)
    esp_ip4_addr_t getIpAddress() const { return ip_address_; }

    // Check if currently connected
    bool isConnected() const { return status_ == ConnectionStatus::Connected; }
    
    // Check if WiFi driver is ready (initialized)
    bool isReady() const { return initialized_; }

    // Wait for connection (blocking, with timeout)
    bool waitForConnection(int timeout_ms = 30000);

    // Get current SSID
    std::string getCurrentSSID() const { return current_ssid_; }

    // Get WiFi driver version
    std::string getVersion() const;

    // Get WiFi hostname
    std::string getWiFiHostname() const;

    // HTTP request methods
    std::string httpGet(const std::string& url);
    std::string httpPost(const std::string& url, const std::string& body);
    std::string httpPut(const std::string& url, const std::string& body);

private:
    Logger* logger_;
    ConnectionStatus status_;
    esp_ip4_addr_t ip_address_;
    std::string current_ssid_;
    bool initialized_;
    std::vector<WiFiNetwork> scan_results_;

    // Static event handler - dispatches to instance via user_data
    static void eventHandler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data);

    // Instance method to handle events
    void handleEvent(esp_event_base_t event_base, int32_t event_id, void* event_data);

    // Internal scan event handler
    void handleScanResult(wifi_ap_record_t* ap_record);
};

