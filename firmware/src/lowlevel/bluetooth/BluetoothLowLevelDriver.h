#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

class Logger;

/**
 * BluetoothLowLevelDriver - NimBLE peripheral for digital twin GATT.
 * Public surface is accessors only; GATT/metadata buffers stay private.
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
    /** Terminate the active GATT connection (if any). */
    bool disconnect();
    bool isConnected() const { return connected_; }
    uint16_t connHandle() const { return conn_handle_; }

    void setWriteCallback(WriteCallback cb) { on_write_ = std::move(cb); }
    void setConnectionCallback(ConnCallback cb) { on_conn_ = std::move(cb); }

    /** Notify on a characteristic value handle. */
    bool notify(uint16_t attr_handle, const uint8_t* data, size_t len);

    /** Register GATT services (called once after begin). */
    bool registerGattServices();

    /** Copy NimBLE-assigned GATT handles into members (safe after host sync). */
    void refreshGattHandles();

    // --- GATT value handles (read-only after sync) ---
    uint16_t handleCmd() const { return handle_cmd_; }
    uint16_t handleEvent() const { return handle_event_; }
    uint16_t handleStatus() const { return handle_status_; }
    uint16_t handlePairing() const { return handle_pairing_; }
    uint16_t handleLog() const { return handle_log_; }

    // --- CCCD / notify subscription state ---
    bool logNotifyEnabled() const { return log_notify_enabled_; }
    bool eventNotifyEnabled() const { return event_notify_enabled_; }
    bool statusNotifyEnabled() const { return status_notify_enabled_; }
    bool pairingNotifyEnabled() const { return pairing_notify_enabled_; }

    // --- Device metadata (set before advertising) ---
    void setSerial(const char* s) {
        if (!s) return;
        strncpy(serial_, s, sizeof(serial_) - 1);
        serial_[sizeof(serial_) - 1] = '\0';
    }
    const char* serial() const { return serial_; }

    void setModel(const char* s) {
        if (!s) return;
        strncpy(model_, s, sizeof(model_) - 1);
        model_[sizeof(model_) - 1] = '\0';
    }
    void setSwVersion(const char* s) {
        if (!s) return;
        strncpy(sw_version_, s, sizeof(sw_version_) - 1);
        sw_version_[sizeof(sw_version_) - 1] = '\0';
    }
    void setBrand(uint8_t b) { brand_ = b; }
    void setBattery(uint8_t b) { battery_ = b; }

    void setStatusBytes(const uint8_t* bytes, size_t len) {
        if (!bytes || len > sizeof(status_bytes_)) return;
        std::memcpy(status_bytes_, bytes, len);
    }
    uint8_t* statusBytes() { return status_bytes_; }
    size_t statusBytesSize() const { return sizeof(status_bytes_); }

    static BluetoothLowLevelDriver* instance() { return s_instance; }

private:
    Logger* logger_;
    bool initialized_ = false;
    bool controller_ready_ = false;
    bool connected_ = false;
    uint16_t conn_handle_ = 0xFFFF;
    WriteCallback on_write_;
    ConnCallback on_conn_;

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

    char serial_[32] = "ILSS-LY-0000";
    char model_[32] = "ILSS-Lanyard-Breakout";
    char sw_version_[32] = "0.1.0";
    uint8_t brand_ = 1;
    uint8_t battery_ = 100;
    uint8_t status_bytes_[7]{};

    static BluetoothLowLevelDriver* s_instance;
    static void hostTask(void* param);
    static void onHostSync();
    static void onHostReset(int reason);
    static int onGapEvent(struct ble_gap_event* event, void* arg);
    static int gattAccess(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt* ctxt, void* arg);
};
