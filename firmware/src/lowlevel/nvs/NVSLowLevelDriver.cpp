#include "NVSLowLevelDriver.h"
#include "esp_log.h"

NVSLowLevelDriver::NVSLowLevelDriver(Logger* logger)
    : logger_(logger), initialized_(false)
{
}

NVSLowLevelDriver::~NVSLowLevelDriver()
{
}

bool NVSLowLevelDriver::begin()
{
    if (initialized_) {
        logger_->LOGW(TAG, "NVS already initialized");
        return true;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        logger_->LOGW(TAG, "NVS partition needs erasing, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return false;
    }

    initialized_ = true;
    logger_->LOGI(TAG, "NVS initialized successfully");
    return true;
}

bool NVSLowLevelDriver::isReady() const
{
    return initialized_;
}

nvs_handle_t NVSLowLevelDriver::openNamespace(const char* namespace_name, bool read_only)
{
    if (!initialized_) {
        logger_->LOGE(TAG, "NVS not initialized, cannot open namespace '%s'", namespace_name);
        return 0;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(namespace_name, read_only ? NVS_READONLY : NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            logger_->LOGD(TAG, "Namespace '%s' not found", namespace_name);
        } else {
            logger_->LOGE(TAG, "Failed to open namespace '%s': %s", namespace_name, esp_err_to_name(err));
        }
        return 0;
    }

    return handle;
}

void NVSLowLevelDriver::closeNamespace(nvs_handle_t handle)
{
    if (handle != 0) {
        nvs_close(handle);
    }
}

bool NVSLowLevelDriver::commit(nvs_handle_t handle)
{
    if (handle == 0) {
        logger_->LOGE(TAG, "Invalid handle for commit");
        return false;
    }

    esp_err_t err = nvs_commit(handle);
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

bool NVSLowLevelDriver::getString(nvs_handle_t handle, const char* key, std::string& value)
{
    if (handle == 0) {
        return false;
    }

    size_t required_size = 0;
    esp_err_t err = nvs_get_str(handle, key, nullptr, &required_size);
    if (err != ESP_OK || required_size == 0) {
        return false;
    }

    char* buf = new char[required_size];
    err = nvs_get_str(handle, key, buf, &required_size);
    if (err == ESP_OK) {
        value = std::string(buf);
        delete[] buf;
        return true;
    }

    delete[] buf;
    return false;
}

bool NVSLowLevelDriver::setString(nvs_handle_t handle, const char* key, const std::string& value)
{
    if (handle == 0) {
        return false;
    }

    esp_err_t err = nvs_set_str(handle, key, value.c_str());
    if (err != ESP_OK) {
        logger_->LOGW(TAG, "Failed to set string '%s': %s", key, esp_err_to_name(err));
        return false;
    }

    return true;
}

bool NVSLowLevelDriver::getInt32(nvs_handle_t handle, const char* key, int32_t& value)
{
    if (handle == 0) {
        return false;
    }

    esp_err_t err = nvs_get_i32(handle, key, &value);
    return (err == ESP_OK);
}

bool NVSLowLevelDriver::setInt32(nvs_handle_t handle, const char* key, int32_t value)
{
    if (handle == 0) {
        return false;
    }

    esp_err_t err = nvs_set_i32(handle, key, value);
    if (err != ESP_OK) {
        logger_->LOGW(TAG, "Failed to set int32 '%s': %s", key, esp_err_to_name(err));
        return false;
    }

    return true;
}

bool NVSLowLevelDriver::getUInt8(nvs_handle_t handle, const char* key, uint8_t& value)
{
    if (handle == 0) {
        return false;
    }

    esp_err_t err = nvs_get_u8(handle, key, &value);
    return (err == ESP_OK);
}

bool NVSLowLevelDriver::setUInt8(nvs_handle_t handle, const char* key, uint8_t value)
{
    if (handle == 0) {
        return false;
    }

    esp_err_t err = nvs_set_u8(handle, key, value);
    if (err != ESP_OK) {
        logger_->LOGW(TAG, "Failed to set uint8 '%s': %s", key, esp_err_to_name(err));
        return false;
    }

    return true;
}

bool NVSLowLevelDriver::getBool(nvs_handle_t handle, const char* key, bool& value)
{
    if (handle == 0) {
        return false;
    }

    uint8_t u8_val;
    if (getUInt8(handle, key, u8_val)) {
        value = (u8_val != 0);
        return true;
    }

    return false;
}

bool NVSLowLevelDriver::setBool(nvs_handle_t handle, const char* key, bool value)
{
    return setUInt8(handle, key, value ? 1 : 0);
}

bool NVSLowLevelDriver::eraseKey(nvs_handle_t handle, const char* key)
{
    if (handle == 0) {
        return false;
    }

    esp_err_t err = nvs_erase_key(handle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        logger_->LOGW(TAG, "Failed to erase key '%s': %s", key, esp_err_to_name(err));
        return false;
    }

    return true;
}
