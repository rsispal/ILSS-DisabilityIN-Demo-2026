#include "BluetoothLowLevelDriver.h"
#include "../../utils/Logger.h"

#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <cstring>
#include <vector>

BluetoothLowLevelDriver* BluetoothLowLevelDriver::s_instance = nullptr;

// ILSS UUID helper: xxxxxxxx-494c-5353-4c59-0000000000yy
static ble_uuid128_t make_uuid(uint8_t svc_byte) {
    ble_uuid128_t u;
    u.u.type = BLE_UUID_TYPE_128;
    // LSB-first 128-bit UUID
    const uint8_t base[16] = {
        svc_byte, 0x00, 0x00, 0x00, 0x00, 0x00, 0x59, 0x4c,
        0x53, 0x53, 0x4c, 0x49, 0x00, 0x00, 0x00, 0x00
    };
    std::memcpy(u.value, base, 16);
    return u;
}

static const ble_uuid128_t UUID_META_SVC = make_uuid(0x01);
static const ble_uuid128_t UUID_SERIAL = make_uuid(0x02);
static const ble_uuid128_t UUID_MODEL = make_uuid(0x03);
static const ble_uuid128_t UUID_SWVER = make_uuid(0x04);
static const ble_uuid128_t UUID_BRAND = make_uuid(0x05);
static const ble_uuid128_t UUID_BATT = make_uuid(0x06);

static const ble_uuid128_t UUID_TWIN_SVC = make_uuid(0x10);
static const ble_uuid128_t UUID_CMD = make_uuid(0x11);
static const ble_uuid128_t UUID_EVENT = make_uuid(0x12);
static const ble_uuid128_t UUID_STATUS = make_uuid(0x13);
static const ble_uuid128_t UUID_PAIRING = make_uuid(0x14);
static const ble_uuid128_t UUID_LOG = make_uuid(0x15);

BluetoothLowLevelDriver::BluetoothLowLevelDriver(Logger* logger) : logger_(logger) {
    s_instance = this;
}

BluetoothLowLevelDriver::~BluetoothLowLevelDriver() {
    stop();
    if (s_instance == this) s_instance = nullptr;
}

bool BluetoothLowLevelDriver::initController() {
    controller_ready_ = true;
    return true;
}

void BluetoothLowLevelDriver::hostTask(void* /*param*/) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void BluetoothLowLevelDriver::onHostReset(int reason) {
    if (s_instance && s_instance->logger_) {
        s_instance->logger_->LOGW(s_instance->TAG, "NimBLE reset reason=%d", reason);
    }
}

void BluetoothLowLevelDriver::onHostSync() {
    if (!s_instance) return;
    s_instance->logger_->LOGI(s_instance->TAG, "NimBLE host synced");
}

int BluetoothLowLevelDriver::gattAccess(uint16_t conn_handle, uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt* ctxt, void* /*arg*/) {
    auto* self = s_instance;
    if (!self) return BLE_ATT_ERR_UNLIKELY;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        const char* str = nullptr;
        const uint8_t* bytes = nullptr;
        size_t len = 0;
        uint8_t one = 0;

        if (attr_handle == self->handle_serial_) {
            str = self->serial_;
        } else if (attr_handle == self->handle_model_) {
            str = self->model_;
        } else if (attr_handle == self->handle_swver_) {
            str = self->sw_version_;
        } else if (attr_handle == self->handle_brand_) {
            one = self->brand_;
            bytes = &one;
            len = 1;
        } else if (attr_handle == self->handle_batt_) {
            one = self->battery_;
            bytes = &one;
            len = 1;
        } else if (attr_handle == self->handle_status_) {
            bytes = self->status_bytes_;
            len = sizeof(self->status_bytes_);
        }

        if (str) {
            int rc = os_mbuf_append(ctxt->om, str, strlen(str));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (bytes) {
            int rc = os_mbuf_append(ctxt->om, bytes, len);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        std::vector<uint8_t> buf(om_len);
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf.data(), om_len, nullptr);
        if (rc != 0) return BLE_ATT_ERR_UNLIKELY;
        if (self->on_write_) {
            self->on_write_(attr_handle, buf.data(), buf.size());
        }
        return 0;
    }

    return 0;
}

bool BluetoothLowLevelDriver::registerGattServices() {
    // Persistent handle storage (must outlive GATTS registration)
    static uint16_t h_serial, h_model, h_swver, h_brand, h_batt;
    static uint16_t h_cmd, h_event, h_status, h_pairing, h_log;

    // Field order must match ble_gatt_chr_def: uuid, access_cb, arg, descriptors, flags, min_key_size, val_handle
    static struct ble_gatt_chr_def meta_chars[] = {
        { &UUID_SERIAL.u, gattAccess, nullptr, nullptr, BLE_GATT_CHR_F_READ, 0, &h_serial },
        { &UUID_MODEL.u, gattAccess, nullptr, nullptr, BLE_GATT_CHR_F_READ, 0, &h_model },
        { &UUID_SWVER.u, gattAccess, nullptr, nullptr, BLE_GATT_CHR_F_READ, 0, &h_swver },
        { &UUID_BRAND.u, gattAccess, nullptr, nullptr, BLE_GATT_CHR_F_READ, 0, &h_brand },
        { &UUID_BATT.u, gattAccess, nullptr, nullptr, BLE_GATT_CHR_F_READ, 0, &h_batt },
        {0},
    };

    static struct ble_gatt_chr_def twin_chars[] = {
        { &UUID_CMD.u, gattAccess, nullptr, nullptr,
          static_cast<ble_gatt_chr_flags>(BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP), 0, &h_cmd },
        { &UUID_EVENT.u, gattAccess, nullptr, nullptr, BLE_GATT_CHR_F_NOTIFY, 0, &h_event },
        { &UUID_STATUS.u, gattAccess, nullptr, nullptr,
          static_cast<ble_gatt_chr_flags>(BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY), 0, &h_status },
        { &UUID_PAIRING.u, gattAccess, nullptr, nullptr,
          static_cast<ble_gatt_chr_flags>(BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY), 0, &h_pairing },
        { &UUID_LOG.u, gattAccess, nullptr, nullptr, BLE_GATT_CHR_F_NOTIFY, 0, &h_log },
        {0},
    };

    static const struct ble_gatt_svc_def svcs[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = &UUID_META_SVC.u,
            .includes = nullptr,
            .characteristics = meta_chars,
        },
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = &UUID_TWIN_SVC.u,
            .includes = nullptr,
            .characteristics = twin_chars,
        },
        {0},
    };

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(svcs);
    if (rc != 0) {
        logger_->LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return false;
    }
    rc = ble_gatts_add_svcs(svcs);
    if (rc != 0) {
        logger_->LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return false;
    }

    handle_serial_ = h_serial;
    handle_model_ = h_model;
    handle_swver_ = h_swver;
    handle_brand_ = h_brand;
    handle_batt_ = h_batt;
    handle_cmd_ = h_cmd;
    handle_event_ = h_event;
    handle_status_ = h_status;
    handle_pairing_ = h_pairing;
    handle_log_ = h_log;
    return true;
}

int BluetoothLowLevelDriver::onGapEvent(struct ble_gap_event* event, void* /*arg*/) {
    auto* self = s_instance;
    if (!self) return 0;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                self->connected_ = true;
                self->conn_handle_ = event->connect.conn_handle;
                self->logger_->LOGI(self->TAG, "Connected handle=%u", self->conn_handle_);
                if (self->on_conn_) self->on_conn_(true, self->conn_handle_);
            } else {
                self->logger_->LOGW(self->TAG, "Connect failed status=%d", event->connect.status);
                self->startAdvertising(self->serial_);
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            self->logger_->LOGI(self->TAG, "Disconnected reason=%d", event->disconnect.reason);
            self->connected_ = false;
            self->conn_handle_ = 0xFFFF;
            self->log_notify_enabled_ = false;
            self->event_notify_enabled_ = false;
            self->status_notify_enabled_ = false;
            self->pairing_notify_enabled_ = false;
            if (self->on_conn_) self->on_conn_(false, 0xFFFF);
            self->startAdvertising(self->serial_);
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == self->handle_log_) {
                self->log_notify_enabled_ = event->subscribe.cur_notify;
            } else if (event->subscribe.attr_handle == self->handle_event_) {
                self->event_notify_enabled_ = event->subscribe.cur_notify;
            } else if (event->subscribe.attr_handle == self->handle_status_) {
                self->status_notify_enabled_ = event->subscribe.cur_notify;
            } else if (event->subscribe.attr_handle == self->handle_pairing_) {
                self->pairing_notify_enabled_ = event->subscribe.cur_notify;
            }
            self->logger_->LOGI(self->TAG, "Subscribe handle=%u notify=%d",
                                event->subscribe.attr_handle, event->subscribe.cur_notify);
            break;
        case BLE_GAP_EVENT_MTU:
            self->logger_->LOGI(self->TAG, "MTU update %u", event->mtu.value);
            break;
        default:
            break;
    }
    return 0;
}

bool BluetoothLowLevelDriver::begin() {
    if (initialized_) return true;
    logger_->LOGI(TAG, "Initializing NimBLE peripheral...");

    esp_err_t ret = esp_nimble_hci_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        logger_->LOGW(TAG, "esp_nimble_hci_init: %s", esp_err_to_name(ret));
    }

    nimble_port_init();
    ble_hs_cfg.sync_cb = onHostSync;
    ble_hs_cfg.reset_cb = onHostReset;

    if (!registerGattServices()) return false;

    nimble_port_freertos_init(hostTask);
    initialized_ = true;
    logger_->LOGI(TAG, "NimBLE peripheral ready");
    return true;
}

bool BluetoothLowLevelDriver::startAdvertising(const char* name) {
    if (name && name[0]) {
        strncpy(serial_, name, sizeof(serial_) - 1);
    }
    ble_svc_gap_device_name_set(serial_);

    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = reinterpret_cast<uint8_t*>(serial_);
    fields.name_len = strlen(serial_);
    fields.name_is_complete = 1;
    fields.uuids128 = const_cast<ble_uuid128_t*>(&UUID_TWIN_SVC);
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        logger_->LOGE(TAG, "ble_gap_adv_set_fields: %d", rc);
        return false;
    }

    struct ble_gap_adv_params adv = {};
    adv.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER, &adv, onGapEvent, this);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        // Fall back to random address if public unavailable
        rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, nullptr, BLE_HS_FOREVER, &adv, onGapEvent, this);
    }
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        logger_->LOGE(TAG, "ble_gap_adv_start: %d", rc);
        return false;
    }
    logger_->LOGI(TAG, "Advertising as %s", serial_);
    return true;
}

bool BluetoothLowLevelDriver::stopAdvertising() {
    ble_gap_adv_stop();
    return true;
}

void BluetoothLowLevelDriver::stop() {
    stopAdvertising();
}

bool BluetoothLowLevelDriver::notify(uint16_t attr_handle, const uint8_t* data, size_t len) {
    if (!connected_ || conn_handle_ == 0xFFFF || !data || len == 0) return false;
    struct os_mbuf* om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return false;
    int rc = ble_gatts_notify_custom(conn_handle_, attr_handle, om);
    return rc == 0;
}
