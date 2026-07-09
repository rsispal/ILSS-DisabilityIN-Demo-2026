#include "BluetoothLowLevelDriver.h"
#include "../../utils/Logger.h"
#include "../../layers/ble-beacon/BLEBeacon.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "esp_timer.h"
#include <algorithm>

// Static instance pointer for callbacks
BluetoothLowLevelDriver* BluetoothLowLevelDriver::s_instance = nullptr;

BluetoothLowLevelDriver::BluetoothLowLevelDriver(Logger* logger)
    : logger_(logger), initialized_(false), hci_initialized_(false), is_scanning_(false), 
      sync_sem_(nullptr), scan_window_timer_(nullptr), current_scan_interval_ms_(0)
{
    sync_sem_ = xSemaphoreCreateBinary();
    s_instance = this;
}

BluetoothLowLevelDriver::~BluetoothLowLevelDriver()
{
    stopScanning();
    
    // Delete scan window timer if it exists
    if (scan_window_timer_) {
        esp_timer_delete(scan_window_timer_);
        scan_window_timer_ = nullptr;
    }
    
    if (sync_sem_) {
        vSemaphoreDelete(sync_sem_);
    }
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

bool BluetoothLowLevelDriver::initController()
{
    // For ESP32-S3 with NimBLE, the controller is initialized automatically
    // when BT is enabled in sdkconfig. We don't need to initialize it manually.
    // The coexistence is handled automatically by ESP-IDF when both WiFi and BT
    // are enabled. This function is a placeholder for future use if needed.
    logger_->LOGD(TAG, "BT controller initialization handled by ESP-IDF (coexistence enabled)");
    return true;
}

bool BluetoothLowLevelDriver::begin()
{
    if (initialized_) {
        logger_->LOGW(TAG, "Already initialized");
        return true;
    }

    logger_->LOGI(TAG, "Initializing NimBLE stack...");

    // Initialize NimBLE HCI (this initializes the transport layer)
    // The BT controller is already initialized by ESP-IDF when BT is enabled
    if (!hci_initialized_) {
        esp_err_t ret = esp_nimble_hci_init();
        if (ret == ESP_OK) {
            hci_initialized_ = true;
            logger_->LOGD(TAG, "HCI initialized successfully");
        } else if (ret == ESP_ERR_INVALID_STATE) {
            // Already initialized - that's OK
            hci_initialized_ = true;
            logger_->LOGD(TAG, "HCI already initialized");
        } else {
            // ESP_FAIL or other errors might indicate HCI is already initialized
            // but in a different state. Since the host stack will work anyway,
            // we'll treat this as a warning and continue.
            logger_->LOGW(TAG, "esp_nimble_hci_init returned %s (0x%x) - HCI may already be initialized, continuing...", 
                         esp_err_to_name(ret), ret);
            hci_initialized_ = true;  // Mark as initialized to avoid retry loop
        }
    } else {
        logger_->LOGD(TAG, "HCI already initialized, skipping");
    }

    // Initialize NimBLE host
    nimble_port_init();

    // Set sync callback
    ble_hs_cfg.sync_cb = onHostSync;

    // Start NimBLE host task
    nimble_port_freertos_init(hostTask);

    // Wait for sync
    if (xSemaphoreTake(sync_sem_, pdMS_TO_TICKS(5000)) != pdTRUE) {
        logger_->LOGE(TAG, "Timeout waiting for BLE host sync");
        return false;
    }

    initialized_ = true;
    logger_->LOGI(TAG, "NimBLE stack initialized");
    return true;
}

void BluetoothLowLevelDriver::stop()
{
    if (is_scanning_) {
        stopScanning();
    }
    logger_->LOGD(TAG, "Stopping Bluetooth driver");
}

bool BluetoothLowLevelDriver::startScanning(uint16_t scanInterval, uint16_t scanWindow)
{
    if (!initialized_) {
        if (!begin()) {
            logger_->LOGE(TAG, "Failed to initialize before scanning");
            return false;
        }
    }

    if (is_scanning_) {
        logger_->LOGW(TAG, "Scanning already in progress");
        return false;
    }

    // Convert milliseconds to 0.625ms units (NimBLE scan parameter unit)
    // Use default scan parameters if not specified (100ms interval, 50ms window)
    uint16_t interval_ms = scanInterval ? scanInterval : 100;  // 100ms default
    uint16_t window_ms = scanWindow ? scanWindow : 50;          // 50ms default
    
    // BLE spec limits: interval and window must be between 2.5ms (0x0004) and 10.24s (0x4000)
    // Additionally, window must be <= interval
    // Application limit: maximum scan window is 5 seconds to limit power consumption
    const uint16_t BLE_MAX_INTERVAL_MS = 10240;  // 10.24 seconds in ms (BLE spec max)
    const uint16_t BLE_MIN_INTERVAL_MS = 3;      // 2.5ms, rounded up
    const uint16_t APP_MAX_WINDOW_MS = 5000;      // 5 seconds (application limit)
    
    // Scale down if interval exceeds BLE maximum (maintains ratio)
    if (interval_ms > BLE_MAX_INTERVAL_MS) {
        float scale = (float)BLE_MAX_INTERVAL_MS / (float)interval_ms;
        interval_ms = BLE_MAX_INTERVAL_MS;
        window_ms = (uint16_t)(window_ms * scale);
        logger_->LOGW(TAG, "Scan interval %u ms exceeds BLE max (%u ms), scaling to %u ms (window: %u ms)", 
                      scanInterval, BLE_MAX_INTERVAL_MS, interval_ms, window_ms);
    }
    
    // Ensure minimum values
    if (interval_ms < BLE_MIN_INTERVAL_MS) interval_ms = BLE_MIN_INTERVAL_MS;
    if (window_ms < BLE_MIN_INTERVAL_MS) window_ms = BLE_MIN_INTERVAL_MS;
    
    // Ensure window <= interval (BLE requirement)
    if (window_ms > interval_ms) {
        window_ms = interval_ms;
        logger_->LOGW(TAG, "Scan window exceeds interval, clamping to interval value");
    }
    
    // Apply application limit: maximum scan window is 5 seconds
    if (window_ms > APP_MAX_WINDOW_MS) {
        window_ms = APP_MAX_WINDOW_MS;
        logger_->LOGD(TAG, "Scan window clamped to application maximum of %u ms (5 seconds)", APP_MAX_WINDOW_MS);
    }
    
    // Ensure window is at least 10% smaller than interval to avoid continuous scanning
    // This prevents the device from scanning continuously when window ≈ interval
    uint16_t max_window = (interval_ms * 9) / 10;  // 90% of interval
    if (window_ms > max_window) {
        window_ms = max_window;
        logger_->LOGD(TAG, "Scan window clamped to 90%% of interval to prevent continuous scanning");
    }
    
    // Convert to 0.625ms units (BLE spec: scan interval/window are in 0.625ms units)
    uint16_t interval = (interval_ms * 1000) / 625;
    uint16_t window = (window_ms * 1000) / 625;
    
    // Final validation (shouldn't be needed after above checks, but just in case)
    if (interval < 0x0004) interval = 0x0004;
    if (interval > 0x4000) interval = 0x4000;
    if (window < 0x0004) window = 0x0004;
    if (window > interval) window = interval;
    
    struct ble_gap_disc_params scan_params = {};
    scan_params.itvl = interval;
    scan_params.window = window;
    scan_params.passive = 1;  // Passive scanning (no scan requests)
    scan_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
    scan_params.limited = 0;

    logger_->LOGD(TAG, "Starting BLE scan (interval=%u ms -> %u units, window=%u ms -> %u units)", 
                  interval_ms, interval, window_ms, window);

    // Retry logic for EBUSY errors (can occur with WiFi/Bluetooth coexistence)
    int rc = 0;
    int retry_count = 0;
    const int max_retries = 3;
    const int retry_delay_ms = 100;
    
    while (retry_count < max_retries) {
        rc = ble_gap_disc(0, BLE_HS_FOREVER, &scan_params, onGapEvent, this);
        if (rc == 0) {
            break;  // Success
        }
        
        // BLE_HS_EBUSY (3) can occur with WiFi/Bluetooth coexistence - retry
        if (rc == 3 && retry_count < max_retries - 1) {
            retry_count++;
            logger_->LOGW(TAG, "ble_gap_disc returned EBUSY (WiFi/BT coexistence), retrying (%d/%d)...", 
                          retry_count, max_retries);
            vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
            continue;
        }
        
        // Other errors or max retries reached
        logger_->LOGE(TAG, "ble_gap_disc failed: %d (after %d retries)", rc, retry_count);
        if (onScanError_) {
            onScanError_(rc);
        }
        return false;
    }

    is_scanning_ = true;
    current_scan_interval_ms_ = interval_ms;  // Store interval for periodic restarts
    logger_->LOGD(TAG, "Scan started (interval=0x%04X, window=0x%04X)", interval, window);

    if (onScanStarted_) {
        onScanStarted_();
    }

    // Create and start timer to explicitly stop scan after 5 seconds (APP_MAX_WINDOW_MS)
    // This ensures the scan window is strictly limited to 5 seconds
    if (scan_window_timer_ == nullptr) {
        esp_timer_create_args_t timer_args = {};
        timer_args.callback = [](void* arg) {
            BluetoothLowLevelDriver* driver = static_cast<BluetoothLowLevelDriver*>(arg);
            if (driver && driver->is_scanning_) {
                driver->logger_->LOGD(driver->TAG, "Scan window timer expired, stopping scan after 5 seconds");
                driver->stopScanning();
            }
        };
        timer_args.arg = this;
        timer_args.name = "ble_scan_window";
        timer_args.dispatch_method = ESP_TIMER_TASK;
        
        esp_err_t err = esp_timer_create(&timer_args, &scan_window_timer_);
        if (err != ESP_OK) {
            logger_->LOGE(TAG, "Failed to create scan window timer: %s", esp_err_to_name(err));
        }
    }
    
    // Start the timer to stop scan after 5 seconds
    if (scan_window_timer_) {
        esp_err_t err = esp_timer_start_once(scan_window_timer_, APP_MAX_WINDOW_MS * 1000);  // Convert ms to microseconds
        if (err != ESP_OK) {
            logger_->LOGE(TAG, "Failed to start scan window timer: %s", esp_err_to_name(err));
        } else {
            logger_->LOGD(TAG, "Scan window timer started: will stop scan after %u ms", APP_MAX_WINDOW_MS);
        }
    }

    return true;
}

bool BluetoothLowLevelDriver::stopScanning()
{
    if (!is_scanning_) {
        return true;
    }

    logger_->LOGD(TAG, "Stopping BLE scan");

    // Stop the scan window timer if it's running
    if (scan_window_timer_) {
        esp_timer_stop(scan_window_timer_);
    }

    int rc = ble_gap_disc_cancel();
    if (rc != 0) {
        logger_->LOGE(TAG, "ble_gap_disc_cancel failed: %d", rc);
        if (onScanError_) {
            onScanError_(rc);
        }
        // Force the scanning state to false even if stop fails
        is_scanning_ = false;
        return false;
    }

    is_scanning_ = false;
    logger_->LOGD(TAG, "Scan stopped");

    if (onScanStopped_) {
        onScanStopped_();
    }

    return true;
}

void BluetoothLowLevelDriver::setBeaconDiscoveredCallback(BeaconDiscoveredCallback callback)
{
    onBeaconDiscovered_ = callback;
}

void BluetoothLowLevelDriver::setScanStartedCallback(ScanStartedCallback callback)
{
    onScanStarted_ = callback;
}

void BluetoothLowLevelDriver::setScanStoppedCallback(ScanStoppedCallback callback)
{
    onScanStopped_ = callback;
}

void BluetoothLowLevelDriver::setScanErrorCallback(ScanErrorCallback callback)
{
    onScanError_ = callback;
}

void BluetoothLowLevelDriver::addManufacturerFilter(uint16_t manufacturerId)
{
    for (auto id : manufacturerFilters_) {
        if (id == manufacturerId) {
            return;  // Already in filter list
        }
    }
    manufacturerFilters_.push_back(manufacturerId);
    logger_->LOGD(TAG, "Added filter: 0x%04X", manufacturerId);
}

void BluetoothLowLevelDriver::clearManufacturerFilters()
{
    manufacturerFilters_.clear();
    logger_->LOGD(TAG, "Cleared filters");
}

void BluetoothLowLevelDriver::onHostSync()
{
    ESP_LOGI("BLE", "BLE host synced");
    
    // Infer own address (pass address type pointer, not nullptr)
    uint8_t addr_type;
    int rc = ble_hs_id_infer_auto(0, &addr_type);
    if (rc != 0) {
        ESP_LOGE("BLE", "ble_hs_id_infer_auto failed: %d", rc);
    } else {
        ESP_LOGD("BLE", "BLE address type inferred: %d", addr_type);
    }

    // Signal that we're ready
    if (s_instance && s_instance->sync_sem_) {
        xSemaphoreGive(s_instance->sync_sem_);
    }
}

int BluetoothLowLevelDriver::onGapEvent(struct ble_gap_event *event, void *arg)
{
    BluetoothLowLevelDriver* driver = static_cast<BluetoothLowLevelDriver*>(arg);

    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            const ble_gap_disc_desc *d = &event->disc;

            // Parse manufacturer data from advertisement
            uint16_t manufacturer_id = 0;
            const uint8_t* manufacturer_data = nullptr;
            size_t manufacturer_data_len = 0;

            // NimBLE stores ad data in fields, not a flat array
            struct ble_hs_adv_fields fields;
            int rc = ble_hs_adv_parse_fields(&fields, d->data, d->length_data);
            
            if (rc == 0 && fields.mfg_data != nullptr && fields.mfg_data_len >= 2) {
                // Manufacturer ID is first 2 bytes (little endian)
                manufacturer_id = fields.mfg_data[0] | (fields.mfg_data[1] << 8);
                manufacturer_data = fields.mfg_data + 2;
                manufacturer_data_len = fields.mfg_data_len - 2;

                // Process the advertisement data
                if (driver) {
                    driver->processAdvertisementData(
                        d->addr.val,
                        d->rssi,
                        manufacturer_id,
                        manufacturer_data,
                        manufacturer_data_len
                    );
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

void BluetoothLowLevelDriver::hostTask(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void BluetoothLowLevelDriver::processAdvertisementData(const uint8_t* address, int8_t rssi,
                                                       uint16_t manufacturer_id,
                                                       const uint8_t* manufacturer_data,
                                                       size_t manufacturer_data_len)
{
    if (!manufacturer_data || manufacturer_data_len == 0) {
        return;
    }

    // Check if we should process this manufacturer ID
    if (!shouldProcessManufacturerData(manufacturer_id)) {
        return;
    }

    // Create beacon object and parse
                BLEBeacon beacon;
    beacon.parse(manufacturer_data, manufacturer_data_len, manufacturer_id, rssi);

    if (beacon.getType() != SupportedBeaconType::UNKNOWN) {
        // Beacon discovery logging is handled at the Bluetooth feature level
        if (onBeaconDiscovered_) {
            onBeaconDiscovered_(beacon);
        }
    }
}

bool BluetoothLowLevelDriver::shouldProcessManufacturerData(uint16_t manufacturerId) const
{
    if (manufacturerFilters_.empty()) {
        return true;  // No filters = accept all
    }

    for (auto id : manufacturerFilters_) {
        if (id == manufacturerId) {
            return true;
        }
    }

    return false;
}

bool BluetoothLowLevelDriver::parseManufacturerData(
    const uint8_t* adv_data, size_t adv_len,
    uint16_t* manufacturer_id,
    const uint8_t** manufacturer_data,
    size_t* manufacturer_data_len)
{
    size_t i = 0;
    while (i < adv_len) {
        uint8_t len = adv_data[i];
        if (len == 0 || (i + 1 + len) > adv_len) {
            break;  // Invalid or end of data
        }

        uint8_t type = adv_data[i + 1];

        // 0xFF is manufacturer specific data
        if (type == 0xFF && len >= 3) {
            // Manufacturer ID is 2 bytes (little endian)
            *manufacturer_id = adv_data[i + 2] | (adv_data[i + 3] << 8);
            *manufacturer_data = &adv_data[i + 4];
            *manufacturer_data_len = len - 3;  // len includes type byte
            return true;
        }

        i += 1 + len;  // Move to next AD structure
    }

    return false;
}
