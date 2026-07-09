#include "NetworkClient.h"
#include "../wifi/WiFiLowLevelDriver.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include <sstream>
#include <algorithm>

static const char* TAG = "NetworkClient";

// Static member initialization
WiFiLowLevelDriver* NetworkClient::wifi_driver_ = nullptr;

// Event handler to capture HTTP response data
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    NetworkClient::HttpResponse* response = static_cast<NetworkClient::HttpResponse*>(evt->user_data);
    
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            if (response && evt->header_key && evt->header_value) {
                std::string header_line = std::string(evt->header_key) + ": " + std::string(evt->header_value);
                response->headers.push_back(header_line);
            }
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Not chunked - append data to response body
                if (response && evt->data && evt->data_len > 0) {
                    response->body.append((char*)evt->data, evt->data_len);
                    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA: %d bytes", evt->data_len);
                }
            } else {
                // Chunked response - still append
                if (response && evt->data && evt->data_len > 0) {
                    response->body.append((char*)evt->data, evt->data_len);
                    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA (chunked): %d bytes", evt->data_len);
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

void NetworkClient::setWiFiDriver(WiFiLowLevelDriver* wifi)
{
    wifi_driver_ = wifi;
}

NetworkClient::HttpResponse NetworkClient::get(const std::string &url, const std::vector<std::string> &headers)
{
    return makeRequest("GET", url, headers, "");
}

NetworkClient::HttpResponse NetworkClient::post(const std::string &url, const std::string &body, const std::vector<std::string> &headers)
{
    return makeRequest("POST", url, headers, body);
}

NetworkClient::HttpResponse NetworkClient::put(const std::string &url, const std::string &body, const std::vector<std::string> &headers)
{
    return makeRequest("PUT", url, headers, body);
}

NetworkClient::HttpResponse NetworkClient::patch(const std::string &url, const std::string &body, const std::vector<std::string> &headers)
{
    return makeRequest("PATCH", url, headers, body);
}

NetworkClient::HttpResponse NetworkClient::del(const std::string &url, const std::vector<std::string> &headers)
{
    return makeRequest("DELETE", url, headers, "");
}

NetworkClient::HttpResponse NetworkClient::makeRequest(const std::string &method, 
                                                       const std::string &url, 
                                                       const std::vector<std::string> &headers, 
                                                       const std::string &body)
{
    HttpResponse response;
    response.success = false;
    response.status_code = 0;

    if (!wifi_driver_)
    {
        ESP_LOGE(TAG, "WiFi driver not set. Call setWiFiDriver() first.");
        return response;
    }

    if (!wifi_driver_->isConnected())
    {
        ESP_LOGE(TAG, "WiFi not connected");
        return response;
    }

    // Determine HTTP method
    esp_http_client_method_t http_method = HTTP_METHOD_GET;
    if (method == "GET") {
        http_method = HTTP_METHOD_GET;
    } else if (method == "POST") {
        http_method = HTTP_METHOD_POST;
    } else if (method == "PUT") {
        http_method = HTTP_METHOD_PUT;
    } else if (method == "PATCH") {
        http_method = HTTP_METHOD_PATCH;
    } else if (method == "DELETE") {
        http_method = HTTP_METHOD_DELETE;
    } else {
        ESP_LOGE(TAG, "Unsupported HTTP method: %s", method.c_str());
        return response;
    }

    // Configure HTTP client with event handler to capture response body
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 30000;
    config.method = http_method;
    config.skip_cert_common_name_check = false;
    config.event_handler = http_event_handler;
    config.user_data = &response;  // Pass response struct to event handler

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "HTTP client init failed");
        return response;
    }

    // Set custom headers
    for (const auto& header : headers) {
        size_t colon_pos = header.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = header.substr(0, colon_pos);
            std::string value = header.substr(colon_pos + 1);
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            esp_http_client_set_header(client, key.c_str(), value.c_str());
        }
    }

    // Set request body for POST/PUT/PATCH
    if ((method == "POST" || method == "PUT" || method == "PATCH") && !body.empty()) {
        esp_http_client_set_post_field(client, body.c_str(), body.length());
    }

    // Perform request - event handler will capture response body automatically
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        response.status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        ESP_LOGI(TAG, "HTTP %s status: %d, content_length: %d, body_size: %zu", 
                 method.c_str(), response.status_code, content_length, response.body.size());
        
        // Response body and headers are already captured by event handler
        if (response.status_code >= 200 && response.status_code < 300) {
            response.success = true;
        } else {
            ESP_LOGW(TAG, "HTTP %s returned status code: %d", method.c_str(), response.status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP %s perform failed: %s (0x%x)", method.c_str(), esp_err_to_name(err), err);
    }

    esp_http_client_cleanup(client);
    return response;
}
