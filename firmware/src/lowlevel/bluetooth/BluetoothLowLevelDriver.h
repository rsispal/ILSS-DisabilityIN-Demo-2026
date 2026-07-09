#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

class Logger;

/**
 * BluetoothLowLevelDriver - NimBLE peripheral for digital twin GATT.
 */
class BluetoothLowLevelDriver {
    const char* TAG = "BluetoothLowLevel";

public:
    using WriteCallback = std::function<void(uint16_t attr_handle, const uint8_t* data, size_t len)>;
    using ConnCallback = std::function<void(bool connected, uint16_t conn_handle)>;

    BluetoothLowLevelDriver(Logger* logger);
    ~BluetoothLowLevelDriver();

    bool begin();
    bool initController();
    void stop();

    bool startAdvertising(const char* name);
    bool stopAdvertising();
    bool isConnected() const { return connected_; }
    uint16_t connHandle() const { return conn_handle_; }

    void setWriteCallback(WriteCallback cb) { on_write_ = std::move(cb); }
    void setConnectionCallback(ConnCallback cb) { on_conn_ = std::move(cb); }

    /** Notify on a characteristic value handle. */
    bool notify(uint16_t attr_handle, const uint8_t* data, size_t len);

    /** Register GATT services (called once after begin). */
    bool registerGattServices();

    // Exposed handles for BleTwin feature
    uint16_t handle_cmd_ = 0;
    uint16_t handle_event_ = 0;
    uint16_t handle_status_ = 0;
    uint16_t handle_pairing_ = 0;
    uint16_t handle_log_ = 0;
    uint16_t handle_serial_ = 0;
    uint16_t handle_model_ = 0;
    uint16_t handle_swver_ = 0;
    uint16_t handle_brand_ = 0;
    uint16_t handle_batt_ = 0;

    bool log_notify_enabled_ = false;
    bool event_notify_enabled_ = false;
    bool status_notify_enabled_ = false;
    bool pairing_notify_enabled_ = false;

    // Metadata buffers (set by BleTwin before advertising)
    char serial_[32] = "ILSS-LY-0000";
    char model_[32] = "ILSS-Lanyard-Breakout";
    char sw_version_[32] = "0.1.0";
    uint8_t brand_ = 1;
    uint8_t battery_ = 100;
    uint8_t status_bytes_[6]{};

    static BluetoothLowLevelDriver* instance() { return s_instance; }

private:
    Logger* logger_;
    bool initialized_ = false;
    bool controller_ready_ = false;
    bool connected_ = false;
    uint16_t conn_handle_ = 0xFFFF;
    WriteCallback on_write_;
    ConnCallback on_conn_;

    static BluetoothLowLevelDriver* s_instance;
    static void hostTask(void* param);
    static void onHostSync();
    static void onHostReset(int reason);
    static int onGapEvent(struct ble_gap_event* event, void* arg);
    static int gattAccess(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt* ctxt, void* arg);
};
