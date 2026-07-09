#pragma once

#include <string>
#include <vector>
#include "../../utils/Logger.h"

// Forward declaration
class WiFiLowLevelDriver;

/**
 * NetworkClient - Wrapper for HTTP/HTTPS requests
 * 
 * Provides a Zephyr-compatible interface while using ESP-IDF's WiFiLowLevelDriver
 * HTTP methods under the hood.
 */
class NetworkClient
{
public:
    static constexpr const char *TAG = "NetworkClient";

    struct HttpResponse
    {
        int status_code;
        std::vector<std::string> headers;
        std::string body;
        bool success;
    };

    // Set the WiFi driver instance (must be called before making requests)
    static void setWiFiDriver(WiFiLowLevelDriver* wifi);

    // Generic request method
    static HttpResponse makeRequest(const std::string &method, 
                                    const std::string &url, 
                                    const std::vector<std::string> &headers = {}, 
                                    const std::string &body = "");

    // Convenience methods
    static HttpResponse get(const std::string &url, const std::vector<std::string> &headers = {});
    static HttpResponse post(const std::string &url, const std::string &body, const std::vector<std::string> &headers = {});
    static HttpResponse put(const std::string &url, const std::string &body, const std::vector<std::string> &headers = {});
    static HttpResponse patch(const std::string &url, const std::string &body, const std::vector<std::string> &headers = {});
    static HttpResponse del(const std::string &url, const std::vector<std::string> &headers = {});

private:
    static WiFiLowLevelDriver* wifi_driver_;
};

