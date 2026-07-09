#pragma once

#include <string>
#include <functional>
#include "../../utils/Logger.h"
#include "../../state/State.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum class AzureConnectionStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
};

typedef std::function<void(const char* topic, const uint8_t* payload, size_t len)> AzureMessageCallback;

class AzureIoT {
    const char* TAG = "AzureIoT";
public:
    AzureIoT(State* state);
    ~AzureIoT();

    bool begin();
    void process(); // Call regularly in main loop
    bool connect();
    void disconnect();
    
    bool publishTelemetry(const char* payload);
    bool publishProperty(const char* payload);
    
    void setMessageCallback(AzureMessageCallback cb) { messageCallback = cb; }
    
    AzureConnectionStatus getConnectionStatus() const { return connectionStatus; }
    bool isConnected() const { return connectionStatus == AzureConnectionStatus::CONNECTED; }
    bool isReady() const;

private:
    Logger logger;
    State* state;
    bool m_initialized;

    esp_mqtt_client_handle_t mqtt_client;
    AzureConnectionStatus connectionStatus;
    AzureMessageCallback messageCallback;

    // MQTT event handler
    static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    void handle_mqtt_event(esp_event_base_t base, int32_t event_id, void *event_data);

    // Connection management
    bool setup_mqtt_config(esp_mqtt_client_config_t& mqtt_cfg);
    
    // MQTT topics
    std::string telemetry_topic;
    std::string c2d_topic;
    std::string twin_res_topic;
    std::string twin_patch_topic;
    
    void setup_topics();
    
    // Helper methods
    std::string deviceId;
    std::string hostname;
    int server_port;
};

