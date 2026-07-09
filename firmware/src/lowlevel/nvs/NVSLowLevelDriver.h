#pragma once

#include <string>
#include <cstdint>
#include "../../utils/Logger.h"
#include "nvs.h"
#include "nvs_flash.h"

/**
 * NVSLowLevelDriver - Low-level driver for Non-Volatile Storage (NVS)
 * 
 * Provides a clean interface for reading/writing persistent configuration
 * and state data. Wraps ESP-IDF NVS API with application-specific methods.
 */
class NVSLowLevelDriver {
    const char* TAG = "NVSLowLevelDriver";

public:
    NVSLowLevelDriver(Logger* logger);
    ~NVSLowLevelDriver();

    /**
     * Initialize NVS flash storage
     * @return true on success, false on failure
     */
    bool begin();

    /**
     * Check if NVS is ready for use
     * @return true if initialized and ready
     */
    bool isReady() const;

    /**
     * Open a namespace for read/write operations
     * @param namespace_name Name of the namespace to open
     * @param read_only If true, open in read-only mode
     * @return Handle to the namespace, or nullptr on failure
     */
    nvs_handle_t openNamespace(const char* namespace_name, bool read_only = false);

    /**
     * Close a namespace handle
     * @param handle Handle returned by openNamespace
     */
    void closeNamespace(nvs_handle_t handle);

    /**
     * Commit changes to NVS (must be called after writes)
     * @param handle Namespace handle
     * @return true on success, false on failure
     */
    bool commit(nvs_handle_t handle);

    // String operations
    bool getString(nvs_handle_t handle, const char* key, std::string& value);
    bool setString(nvs_handle_t handle, const char* key, const std::string& value);

    // Integer operations
    bool getInt32(nvs_handle_t handle, const char* key, int32_t& value);
    bool setInt32(nvs_handle_t handle, const char* key, int32_t value);

    // Unsigned 8-bit integer operations (used for bools and small values)
    bool getUInt8(nvs_handle_t handle, const char* key, uint8_t& value);
    bool setUInt8(nvs_handle_t handle, const char* key, uint8_t value);

    // Boolean operations (stored as uint8_t)
    bool getBool(nvs_handle_t handle, const char* key, bool& value);
    bool setBool(nvs_handle_t handle, const char* key, bool value);

    // Erase operations
    bool eraseKey(nvs_handle_t handle, const char* key);

private:
    Logger* logger_;
    bool initialized_;
};
