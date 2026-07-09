#include "AzureIoT.h"
#include "certs/azure_ca_cert.h"
#include "certs/azure_device_cert.h"
#include "certs/azure_device_key.h"
#include <cstring>
#include <cstdio>

// Azure IoT Hub connection details from Kconfig
#define AZURE_IOT_HUB_HOSTNAME CONFIG_ILSS_AZURE_IOT_HUB_HOSTNAME
#define AZURE_IOT_DEVICE_ID CONFIG_ILSS_AZURE_IOT_DEVICE_ID
#define AZURE_IOT_SERVER_PORT CONFIG_ILSS_AZURE_IOT_SERVER_PORT

AzureIoT::AzureIoT(State* state)
    : state(state), m_initialized(false),
      mqtt_client(nullptr),
      connectionStatus(AzureConnectionStatus::DISCONNECTED),
      messageCallback(nullptr),
      deviceId(AZURE_IOT_DEVICE_ID),
      hostname(AZURE_IOT_HUB_HOSTNAME),
      server_port(AZURE_IOT_SERVER_PORT)
{
    logger.setLogLevel(static_cast<LogLevel>(CONFIG_ILSS_AZURE_IOT_LOG_LEVEL));
}

AzureIoT::~AzureIoT()
{
    disconnect();
    if (mqtt_client) {
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = nullptr;
    }
}

bool AzureIoT::begin()
{
    if (m_initialized) {
        logger.LOGW(TAG, "Azure IoT already initialized");
        return true;
    }
    
    logger.LOGI(TAG, "Initializing Azure IoT Hub client...");
    
    // Setup MQTT topics
    setup_topics();
    
    m_initialized = true;
    logger.LOGI(TAG, "Azure IoT Hub client initialized");
    logger.LOGI(TAG, "Device ID: %s", deviceId.c_str());
    logger.LOGI(TAG, "Hostname: %s", hostname.c_str());
    logger.LOGI(TAG, "Port: %d", server_port);
    return true;
}

bool AzureIoT::isReady() const
{
    return m_initialized && mqtt_client != nullptr;
}

void AzureIoT::setup_topics()
{
    // Azure IoT Hub topic format:
    // Telemetry: devices/{device_id}/messages/events/
    // C2D: devices/{device_id}/messages/devicebound/#
    // Twin Response: $iothub/twin/res/#
    // Twin Patch: $iothub/twin/PATCH/properties/desired/#
    
    char buf[128];
    
    snprintf(buf, sizeof(buf), "devices/%s/messages/events/", deviceId.c_str());
    telemetry_topic = buf;
    
    snprintf(buf, sizeof(buf), "devices/%s/messages/devicebound/#", deviceId.c_str());
    c2d_topic = buf;
    
    twin_res_topic = "$iothub/twin/res/#";
    twin_patch_topic = "$iothub/twin/PATCH/properties/desired/#";
    
    logger.LOGD(TAG, "Telemetry topic: %s", telemetry_topic.c_str());
    logger.LOGD(TAG, "C2D topic: %s", c2d_topic.c_str());
}

bool AzureIoT::setup_mqtt_config(esp_mqtt_client_config_t& mqtt_cfg)
{
    // Build MQTT URI: mqtts://hostname:port
    static char uri[256];
    snprintf(uri, sizeof(uri), "mqtts://%s:%d", hostname.c_str(), server_port);
    mqtt_cfg.broker.address.uri = uri;
    
    // Azure IoT Hub requires MQTT 3.1.1
    mqtt_cfg.session.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
    
    // Azure IoT Hub requires clean session
    mqtt_cfg.session.keepalive = CONFIG_ILSS_AZURE_IOT_KEEPALIVE;
    mqtt_cfg.session.disable_clean_session = false;
    
    // Setup client ID (Azure IoT Hub uses device ID)
    mqtt_cfg.credentials.client_id = deviceId.c_str();
    logger.LOGD(TAG, "MQTT Client ID: %s", deviceId.c_str());
    
    // Setup username (Azure IoT Hub format)
    static char username_buf[256];
    snprintf(username_buf, sizeof(username_buf),
             "%s/%s/?api-version=2021-04-12",
             hostname.c_str(), deviceId.c_str());
    mqtt_cfg.credentials.username = username_buf;
    logger.LOGD(TAG, "MQTT Username: %s", username_buf);
    
#if CONFIG_ILSS_AZURE_IOT_USE_X509
    // X.509 certificate authentication
    mqtt_cfg.credentials.authentication.certificate = AZURE_DEVICE_CERT;
    mqtt_cfg.credentials.authentication.certificate_len = strlen(AZURE_DEVICE_CERT) + 1;
    mqtt_cfg.credentials.authentication.key = AZURE_DEVICE_KEY;
    mqtt_cfg.credentials.authentication.key_len = strlen(AZURE_DEVICE_KEY) + 1;
    mqtt_cfg.credentials.authentication.password = nullptr;  // No password for X.509
    logger.LOGD(TAG, "Using X.509 certificate authentication");
#else
    // SAS token authentication
    const char* sas_token = CONFIG_ILSS_AZURE_IOT_SAS_TOKEN;
    if (strlen(sas_token) == 0) {
        logger.LOGE(TAG, "SAS token is empty! Please set CONFIG_ILSS_AZURE_IOT_SAS_TOKEN");
        return false;
    }
    mqtt_cfg.credentials.authentication.password = sas_token;
    mqtt_cfg.credentials.authentication.certificate = nullptr;
    mqtt_cfg.credentials.authentication.key = nullptr;
    logger.LOGD(TAG, "Using SAS token authentication");
#endif
    
    // CA certificate for server verification
    mqtt_cfg.broker.verification.certificate = AZURE_CA_CERT;
    mqtt_cfg.broker.verification.certificate_len = strlen(AZURE_CA_CERT) + 1;
    mqtt_cfg.broker.verification.skip_cert_common_name_check = false;
    
    return true;
}

bool AzureIoT::connect()
{
    if (connectionStatus == AzureConnectionStatus::CONNECTED) {
        logger.LOGW(TAG, "Already connected");
        return true;
    }
    
    if (mqtt_client != nullptr) {
        logger.LOGW(TAG, "MQTT client already exists, destroying...");
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = nullptr;
    }
    
    logger.LOGI(TAG, "Connecting to Azure IoT Hub...");
    connectionStatus = AzureConnectionStatus::CONNECTING;
    
    // Setup MQTT configuration
    esp_mqtt_client_config_t mqtt_cfg = {};
    if (!setup_mqtt_config(mqtt_cfg)) {
        logger.LOGE(TAG, "Failed to setup MQTT configuration");
        connectionStatus = AzureConnectionStatus::DISCONNECTED;
        return false;
    }
    
    // Initialize MQTT client
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == nullptr) {
        logger.LOGE(TAG, "Failed to initialize MQTT client");
        connectionStatus = AzureConnectionStatus::DISCONNECTED;
        return false;
    }
    
    // Register event handler
    esp_mqtt_client_register_event(mqtt_client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqtt_event_handler, this);
    
    // Start MQTT client (this will trigger connection)
    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        logger.LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        connectionStatus = AzureConnectionStatus::DISCONNECTED;
        return false;
    }
    
    // Wait for connection to establish (up to 15 seconds)
    int timeout_ms = 15000;
    int elapsed = 0;
    while (connectionStatus != AzureConnectionStatus::CONNECTED && elapsed < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(500));
        elapsed += 500;
    }
    
    // Check if connection was successful
    if (connectionStatus != AzureConnectionStatus::CONNECTED) {
        logger.LOGE(TAG, "Connection failed or timed out after %d ms", elapsed);
        return false;
    }
    
    logger.LOGI(TAG, "Connected to Azure IoT Hub (subscription will happen in CONNECTED event)");
    
    // Note: Subscription now happens in MQTT_EVENT_CONNECTED handler
    // This ensures we subscribe after connection is fully established
    
    return true;
}

void AzureIoT::disconnect()
{
    if (connectionStatus != AzureConnectionStatus::CONNECTED && 
        connectionStatus != AzureConnectionStatus::CONNECTING) {
        return;
    }
    
    logger.LOGI(TAG, "Disconnecting from Azure IoT Hub...");
    connectionStatus = AzureConnectionStatus::DISCONNECTING;
    
    if (mqtt_client) {
        esp_mqtt_client_stop(mqtt_client);
    }
    
    connectionStatus = AzureConnectionStatus::DISCONNECTED;
    logger.LOGI(TAG, "Disconnected");
}

bool AzureIoT::publishTelemetry(const char* payload)
{
    if (connectionStatus != AzureConnectionStatus::CONNECTED) {
        logger.LOGW(TAG, "Not connected");
        return false;
    }
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, telemetry_topic.c_str(), payload, 
                                         strlen(payload), 1, 0);
    if (msg_id < 0) {
        logger.LOGE(TAG, "Failed to publish telemetry (msg_id: %d)", msg_id);
        return false;
    }
    
    logger.LOGD(TAG, "Telemetry published (msg_id: %d)", msg_id);
    return true;
}

bool AzureIoT::publishProperty(const char* payload)
{
    if (connectionStatus != AzureConnectionStatus::CONNECTED) {
        logger.LOGW(TAG, "Not connected");
        return false;
    }
    
    // Twin update topic: $iothub/twin/PATCH/properties/reported/?$rid={request_id}
    static char topic_buf[128];
    snprintf(topic_buf, sizeof(topic_buf),
             "$iothub/twin/PATCH/properties/reported/?$rid=1");
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic_buf, payload, 
                                         strlen(payload), 1, 0);
    if (msg_id < 0) {
        logger.LOGE(TAG, "Failed to publish property (msg_id: %d)", msg_id);
        return false;
    }
    
    logger.LOGD(TAG, "Property published (msg_id: %d)", msg_id);
    return true;
}

void AzureIoT::process()
{
    // MQTT client handles events asynchronously via event handler
    // This method can be used for periodic tasks if needed
    if (connectionStatus == AzureConnectionStatus::CONNECTED) {
        // Keep-alive is handled automatically by the MQTT client
    }
}

void AzureIoT::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    AzureIoT *instance = static_cast<AzureIoT *>(handler_args);
    if (instance) {
        instance->handle_mqtt_event(base, event_id, event_data);
    }
}

void AzureIoT::handle_mqtt_event(esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    
    esp_mqtt_event_id_t mqtt_event_id = static_cast<esp_mqtt_event_id_t>(event_id);
    
    switch (mqtt_event_id) {
    case MQTT_EVENT_ERROR:
        logger.LOGE(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle) {
            logger.LOGE(TAG, "Error type: %d, error code: 0x%x", 
                        event->error_handle->error_type,
                        event->error_handle->esp_tls_last_esp_err);
            if (event->error_handle->esp_tls_stack_err) {
                logger.LOGE(TAG, "TLS stack error: 0x%x", event->error_handle->esp_tls_stack_err);
            }
            if (event->error_handle->esp_transport_sock_errno) {
                logger.LOGE(TAG, "Transport socket error: %d", event->error_handle->esp_transport_sock_errno);
            }
        }
        connectionStatus = AzureConnectionStatus::DISCONNECTED;
        break;
        
    case MQTT_EVENT_CONNECTED: {
        logger.LOGI(TAG, "MQTT_EVENT_CONNECTED (session_present: %d)", event->session_present);
        connectionStatus = AzureConnectionStatus::CONNECTED;
        
        // Subscribe to C2D and Twin topics after connection
        int msg_id = esp_mqtt_client_subscribe(client, c2d_topic.c_str(), 1);
        if (msg_id < 0) {
            logger.LOGE(TAG, "Failed to subscribe to C2D topic");
        } else {
            logger.LOGI(TAG, "Subscribed to C2D topic: %s (msg_id: %d)", c2d_topic.c_str(), msg_id);
        }
        
        msg_id = esp_mqtt_client_subscribe(client, twin_res_topic.c_str(), 1);
        if (msg_id < 0) {
            logger.LOGE(TAG, "Failed to subscribe to Twin response topic");
        } else {
            logger.LOGI(TAG, "Subscribed to Twin response topic: %s (msg_id: %d)", twin_res_topic.c_str(), msg_id);
        }
        break;
    }
        
    case MQTT_EVENT_DISCONNECTED:
        logger.LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
        connectionStatus = AzureConnectionStatus::DISCONNECTED;
        break;
        
    case MQTT_EVENT_SUBSCRIBED:
        logger.LOGD(TAG, "MQTT_EVENT_SUBSCRIBED (msg_id: %d)", event->msg_id);
        break;
        
    case MQTT_EVENT_UNSUBSCRIBED:
        logger.LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED (msg_id: %d)", event->msg_id);
        break;
        
    case MQTT_EVENT_PUBLISHED:
        logger.LOGD(TAG, "MQTT_EVENT_PUBLISHED (msg_id: %d)", event->msg_id);
        break;
        
    case MQTT_EVENT_DATA: {
        logger.LOGD(TAG, "MQTT_EVENT_DATA");
        
        // Extract topic (only available on first chunk)
        static char topic[128];
        static uint8_t* payload_buffer = nullptr;
        static size_t payload_received = 0;
        
        if (event->current_data_offset == 0) {
            // First chunk - extract topic and allocate buffer
            if (event->topic_len > 0 && event->topic_len < sizeof(topic)) {
                memcpy(topic, event->topic, event->topic_len);
                topic[event->topic_len] = '\0';
            } else {
                topic[0] = '\0';
            }
            
            // Allocate buffer for complete payload
            if (payload_buffer != nullptr) {
                delete[] payload_buffer;
            }
            payload_buffer = new uint8_t[event->total_data_len + 1];
            if (payload_buffer == nullptr) {
                logger.LOGE(TAG, "Failed to allocate payload buffer");
                break;
            }
            payload_received = 0;
        }
        
        // Copy data chunk to buffer
        if (payload_buffer != nullptr && event->data_len > 0) {
            size_t offset = event->current_data_offset;
            if (offset + event->data_len <= event->total_data_len) {
                memcpy(payload_buffer + offset, event->data, event->data_len);
                payload_received += event->data_len;
            }
        }
        
        // If this is the last chunk, process the message
        if (payload_received >= event->total_data_len && payload_buffer != nullptr) {
            payload_buffer[event->total_data_len] = '\0';
            logger.LOGD(TAG, "Message received: topic='%s', len=%zu", topic, event->total_data_len);
            
            // Call user callback
            if (messageCallback) {
                messageCallback(topic, payload_buffer, event->total_data_len);
            }
            
            delete[] payload_buffer;
            payload_buffer = nullptr;
            payload_received = 0;
        }
        break;
    }
        
    case MQTT_EVENT_BEFORE_CONNECT:
        logger.LOGD(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
        
    default:
        logger.LOGD(TAG, "MQTT event: %ld", event_id);
        break;
    }
}

