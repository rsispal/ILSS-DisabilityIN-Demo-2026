#include "WiFiLowLevelDriver.h"
#include "../../utils/Logger.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <cstring>
#include <algorithm>
#include <set>

static const char* TAG = "WiFiLowLevelDriver";

WiFiLowLevelDriver::WiFiLowLevelDriver(Logger* logger) 
    : logger_(logger), status_(ConnectionStatus::Disconnected), initialized_(false) {
    ip_address_ = esp_ip4_addr_t{0};
    scan_results_.clear();
}

WiFiLowLevelDriver::~WiFiLowLevelDriver() {
    if (initialized_) {
        disconnect();
    }
}

bool WiFiLowLevelDriver::begin() {
    if (initialized_) {
        logger_->LOGW(TAG, "WiFi already initialized");
        return true;
    }

    esp_err_t err;

    err = esp_netif_init();
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "Netif init failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        logger_->LOGE(TAG, "Event loop create failed: %s", esp_err_to_name(err));
        return false;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &WiFiLowLevelDriver::eventHandler,
                                              this,
                                              NULL);
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "WiFi event handler register failed: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              &WiFiLowLevelDriver::eventHandler,
                                              this,
                                              NULL);
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "IP event handler register failed: %s", esp_err_to_name(err));
        return false;
    }

    // Note: WIFI_EVENT_SCAN_DONE is already covered by WIFI_EVENT with ESP_EVENT_ANY_ID above

    // Set WiFi mode to STA (station)
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "WiFi set mode failed: %s", esp_err_to_name(err));
        return false;
    }

    // Start WiFi - THIS IS CRITICAL!
    err = esp_wifi_start();
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(err));
        return false;
    }

    initialized_ = true;
    logger_->LOGI(TAG, "WiFi initialized and started");
    // Don't call getWiFiHostname() here - it might not be ready yet and could cause issues
    return true;
}

std::vector<WiFiNetwork> WiFiLowLevelDriver::scan() {
    if (!initialized_) {
        logger_->LOGE(TAG, "WiFi not initialized. Call begin() first.");
        return {};
    }

    // Check if already connected - scan is blocked if connected
    if (isConnected()) {
        logger_->LOGE(TAG, "Cannot scan while connected to WiFi");
        return {};
    }

    scan_results_.clear();
    logger_->LOGI(TAG, "Starting WiFi scan...");

    wifi_scan_config_t scan_config = {};
    scan_config.ssid = nullptr;
    scan_config.bssid = nullptr;
    scan_config.channel = 0;
    scan_config.show_hidden = false;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.scan_time.active.min = 100;
    scan_config.scan_time.active.max = 300;

    // Use blocking scan (true) to ensure all channels are scanned before returning
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "WiFi scan start failed: %s", esp_err_to_name(err));
        return {};
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        logger_->LOGI(TAG, "No networks found");
        return {};
    }

    wifi_ap_record_t* ap_records = new wifi_ap_record_t[ap_count];
    err = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(err));
        delete[] ap_records;
        return {};
    }

    // Deduplicate by MAC address, keeping strongest signal
    std::set<std::string> seen_macs;
    for (uint16_t i = 0; i < ap_count; i++) {
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                 ap_records[i].bssid[0], ap_records[i].bssid[1], ap_records[i].bssid[2],
                 ap_records[i].bssid[3], ap_records[i].bssid[4], ap_records[i].bssid[5]);
        std::string mac = mac_str;

        if (seen_macs.find(mac) == seen_macs.end()) {
            seen_macs.insert(mac);
            WiFiNetwork network;
            network.ssid = std::string((char*)ap_records[i].ssid);
            network.rssi = ap_records[i].rssi;
            network.channel = ap_records[i].primary;
            
            // Convert security type
            if (ap_records[i].authmode == WIFI_AUTH_OPEN) {
                network.security = 0;
            } else if (ap_records[i].authmode == WIFI_AUTH_WEP) {
                network.security = 2;
            } else if (ap_records[i].authmode == WIFI_AUTH_WPA2_PSK || ap_records[i].authmode == WIFI_AUTH_WPA_PSK) {
                network.security = 1;
            } else if (ap_records[i].authmode == WIFI_AUTH_WPA3_PSK) {
                network.security = 3;
            } else {
                network.security = 1; // Default to WPA/WPA2
            }

            // Convert frequency band
            if (ap_records[i].primary >= 1 && ap_records[i].primary <= 14) {
                network.frequency_band = 1; // 2.4GHz
            } else if (ap_records[i].primary >= 36) {
                network.frequency_band = 2; // 5GHz
            } else {
                network.frequency_band = 0; // Unknown
            }

            memcpy(network.bssid, ap_records[i].bssid, 6);
            scan_results_.push_back(network);
        } else {
            // Update if better RSSI
            auto it = std::find_if(scan_results_.begin(), scan_results_.end(),
                [&mac](const WiFiNetwork& n) {
                    char n_mac[18];
                    snprintf(n_mac, sizeof(n_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                             n.bssid[0], n.bssid[1], n.bssid[2], n.bssid[3], n.bssid[4], n.bssid[5]);
                    return std::string(n_mac) == mac;
                });
            if (it != scan_results_.end() && ap_records[i].rssi > it->rssi) {
                it->rssi = ap_records[i].rssi;
                it->channel = ap_records[i].primary;
            }
        }
    }

    delete[] ap_records;

    // Sort by RSSI (strongest first)
    std::sort(scan_results_.begin(), scan_results_.end(),
        [](const WiFiNetwork& a, const WiFiNetwork& b) {
            return a.rssi > b.rssi;
        });

    logger_->LOGI(TAG, "Scan completed. Found %zu networks", scan_results_.size());
    return scan_results_;
}

bool WiFiLowLevelDriver::connect(const std::string& ssid, const std::string& password, int band) {
    if (!initialized_) {
        logger_->LOGE(TAG, "WiFi not initialized. Call begin() first.");
        return false;
    }

    status_ = ConnectionStatus::Connecting;

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; // Default to WPA2_PSK

    // Set band preference if specified
    if (band == 2) {
        wifi_config.sta.pmf_cfg.capable = true;
        wifi_config.sta.pmf_cfg.required = false;
    }

    // WiFi mode is already set and started in begin(), so just set config and connect
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "WiFi set config failed: %s", esp_err_to_name(err));
        status_ = ConnectionStatus::Failed;
        return false;
    }

    // Connect to the configured network
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(err));
        status_ = ConnectionStatus::Failed;
        return false;
    }

    current_ssid_ = ssid;
    logger_->LOGI(TAG, "WiFi connection initiated to: %s (band: %d)", ssid.c_str(), band);
    return true;
}

bool WiFiLowLevelDriver::disconnect() {
    if (!initialized_) {
        return false;
    }

    esp_err_t err = esp_wifi_disconnect();
    if (err == ESP_OK) {
        status_ = ConnectionStatus::Disconnected;
        ip_address_ = esp_ip4_addr_t{0};
        current_ssid_.clear();
        logger_->LOGI(TAG, "WiFi disconnected");
        return true;
    }
    return false;
}

bool WiFiLowLevelDriver::waitForConnection(int timeout_ms) {
    int elapsed = 0;
    const int step_ms = 100;

    while (elapsed < timeout_ms) {
        if (status_ == ConnectionStatus::Connected) {
            return true;
        }
        if (status_ == ConnectionStatus::Failed) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(step_ms));
        elapsed += step_ms;
    }

    return false;
}

void WiFiLowLevelDriver::eventHandler(void* arg, esp_event_base_t event_base,
                       int32_t event_id, void* event_data) {
    WiFiLowLevelDriver* self = static_cast<WiFiLowLevelDriver*>(arg);
    if (!self) {
        ESP_LOGE("WiFiLowLevelDriver", "WiFi event handler: user_data is null");
        return;
    }
    self->handleEvent(event_base, event_id, event_data);
}

void WiFiLowLevelDriver::handleEvent(esp_event_base_t event_base, int32_t event_id, void* event_data) {
    // Keep event handler minimal and non-blocking to avoid deadlocks
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            // Use ESP_LOG directly instead of logger to avoid potential mutex issues
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            // esp_wifi_connect() is now explicitly called by connect() method
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
            status_ = ConnectionStatus::Disconnected;
            current_ssid_.clear();
            ip_address_ = esp_ip4_addr_t{0};
        } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
            // IP_EVENT_STA_GOT_IP will follow
        } else if (event_id == WIFI_EVENT_SCAN_DONE) {
            ESP_LOGI(TAG, "WIFI_EVENT_SCAN_DONE");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);
        ip_address_ = event->ip_info.ip;
        status_ = ConnectionStatus::Connected;
        ESP_LOGI(TAG, "WiFi connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

std::string WiFiLowLevelDriver::getIPAddress() const {
    if (!isConnected()) {
        return "";
    }
    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_address_));
    return std::string(ip_str);
}

std::string WiFiLowLevelDriver::getVersion() const {
    return std::string("ESP32-S3 WiFi Driver v1.0");
}

std::string WiFiLowLevelDriver::getWiFiHostname() const {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        const char* hostname = nullptr;
        esp_err_t err = esp_netif_get_hostname(netif, &hostname);
        if (err == ESP_OK && hostname) {
            return std::string(hostname);
        }
    }
    return std::string("ILSS-LANYARD-unknown");
}

// Event handler to capture HTTP response body (works for all status codes and HTTP methods)
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    std::string* response_body = static_cast<std::string*>(evt->user_data);
    
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // Capture response body data for both chunked and non-chunked responses
            if (response_body && evt->data && evt->data_len > 0) {
                response_body->append((char*)evt->data, evt->data_len);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

std::string WiFiLowLevelDriver::httpGet(const std::string& url) {
    if (!isConnected()) {
        logger_->LOGE(TAG, "Cannot perform HTTP request: not connected");
        return "";
    }

    std::string response_body;

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 30000;
    config.skip_cert_common_name_check = false;
    config.event_handler = http_event_handler;
    config.user_data = &response_body;  // Pass response_body to event handler

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        logger_->LOGE(TAG, "HTTP client init failed");
        return "";
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        logger_->LOGI(TAG, "HTTP GET status: %d, content_length: %d, body_size: %zu", 
                     status_code, content_length, response_body.size());
        
        if (status_code < 200 || status_code >= 300) {
            logger_->LOGW(TAG, "HTTP GET returned non-success status code: %d", status_code);
        }
    } else {
        logger_->LOGE(TAG, "HTTP GET perform failed: %s (0x%x)", esp_err_to_name(err), err);
    }

    esp_http_client_cleanup(client);
    return response_body;
}

std::string WiFiLowLevelDriver::httpPost(const std::string& url, const std::string& body) {
    if (!isConnected()) {
        logger_->LOGE(TAG, "Cannot perform HTTP request: not connected");
        return "";
    }

    std::string response_body;

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 30000;
    config.method = HTTP_METHOD_POST;
    config.skip_cert_common_name_check = false;
    config.event_handler = http_event_handler;
    config.user_data = &response_body;  // Pass response_body to event handler

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        logger_->LOGE(TAG, "HTTP client init failed");
        return "";
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body.c_str(), body.length());

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        logger_->LOGI(TAG, "HTTP POST status: %d, content_length: %d, body_size: %zu", 
                     status_code, content_length, response_body.size());
        
        if (status_code < 200 || status_code >= 300) {
            logger_->LOGW(TAG, "HTTP POST returned non-success status code: %d", status_code);
        }
    } else {
        logger_->LOGE(TAG, "HTTP POST perform failed: %s (0x%x)", esp_err_to_name(err), err);
    }

    esp_http_client_cleanup(client);
    return response_body;
}

std::string WiFiLowLevelDriver::httpPut(const std::string& url, const std::string& body) {
    if (!isConnected()) {
        logger_->LOGE(TAG, "Cannot perform HTTP request: not connected");
        return "";
    }

    std::string response_body;

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 30000;
    config.method = HTTP_METHOD_PUT;
    config.skip_cert_common_name_check = false;
    config.event_handler = http_event_handler;
    config.user_data = &response_body;  // Pass response_body to event handler

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        logger_->LOGE(TAG, "HTTP client init failed");
        return "";
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body.c_str(), body.length());

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        logger_->LOGI(TAG, "HTTP PUT status: %d, content_length: %d, body_size: %zu", 
                     status_code, content_length, response_body.size());
        
        if (status_code < 200 || status_code >= 300) {
            logger_->LOGW(TAG, "HTTP PUT returned non-success status code: %d", status_code);
        }
    } else {
        logger_->LOGE(TAG, "HTTP PUT perform failed: %s (0x%x)", esp_err_to_name(err), err);
    }

    esp_http_client_cleanup(client);
    return response_body;
}

