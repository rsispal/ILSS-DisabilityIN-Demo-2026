#include "JSON.h"
#include "cJSON.h"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "JsonParser";

// ============================================================================
// JsonParser Implementation
// ============================================================================

JsonParser::JsonParser(const std::string& json_str) : root_(cJSON_Parse(json_str.c_str())) {
    if (!root_) {
        const char* err = cJSON_GetErrorPtr();
        if (err) {
            ESP_LOGE(TAG, "JSON parse error: %s", err);
        }
    }
}

JsonParser::~JsonParser() {
    if (root_) {
        cJSON_Delete(root_);
    }
}

cJSON* JsonParser::get(const char* key) const {
    return root_ ? cJSON_GetObjectItem(root_, key) : nullptr;
}

std::string JsonParser::getString(const char* key, const char* default_value) const {
    cJSON* item = get(key);
    return (item && cJSON_IsString(item)) ? cJSON_GetStringValue(item) : default_value;
}

int JsonParser::getInt(const char* key, int default_value) const {
    cJSON* item = get(key);
    return (item && cJSON_IsNumber(item)) ? item->valueint : default_value;
}

bool JsonParser::getBool(const char* key, bool default_value) const {
    cJSON* item = get(key);
    return (item && cJSON_IsBool(item)) ? cJSON_IsTrue(item) : default_value;
}

std::string JsonParser::getStringPath(const char* path, const char* default_value) const {
    cJSON* item = getNested(path);
    return (item && cJSON_IsString(item)) ? cJSON_GetStringValue(item) : default_value;
}

int JsonParser::getIntPath(const char* path, int default_value) const {
    cJSON* item = getNested(path);
    return (item && cJSON_IsNumber(item)) ? item->valueint : default_value;
}

cJSON* JsonParser::getNested(const char* path) const {
    if (!root_) return nullptr;
    cJSON* current = root_;

    const char* start = path;
    while (*start) {
        const char* dot = strchr(start, '.');
        size_t len = dot ? (dot - start) : strlen(start);

        char key[64];
        if (len >= sizeof(key)) return nullptr;
        strncpy(key, start, len);
        key[len] = '\0';

        current = cJSON_GetObjectItem(current, key);
        if (!current) return nullptr;

        if (!dot) break;
        start = dot + 1;
    }
    return current;
}

// ============================================================================
// JsonBuilder Implementation
// ============================================================================

JsonBuilder JsonBuilder::object() {
    JsonBuilder builder;
    builder.root_ = cJSON_CreateObject();
    builder.owns_root_ = true;
    return builder;
}

JsonBuilder JsonBuilder::array() {
    JsonBuilder builder;
    builder.root_ = cJSON_CreateArray();
    builder.owns_root_ = true;
    return builder;
}

JsonBuilder::JsonBuilder(cJSON* json) : root_(json), owns_root_(true) {
}

JsonBuilder::JsonBuilder(JsonBuilder&& other) noexcept 
    : root_(other.root_), owns_root_(other.owns_root_) {
    other.root_ = nullptr;
    other.owns_root_ = false;
}

JsonBuilder& JsonBuilder::operator=(JsonBuilder&& other) noexcept {
    if (this != &other) {
        if (owns_root_ && root_) {
            cJSON_Delete(root_);
        }
        root_ = other.root_;
        owns_root_ = other.owns_root_;
        other.root_ = nullptr;
        other.owns_root_ = false;
    }
    return *this;
}

JsonBuilder::~JsonBuilder() {
    if (owns_root_ && root_) {
        cJSON_Delete(root_);
    }
}

cJSON* JsonBuilder::release() {
    cJSON* result = root_;
    root_ = nullptr;
    owns_root_ = false;
    return result;
}

std::string JsonBuilder::toString() const {
    if (!root_) return "{}";
    
    char* json_string = cJSON_PrintUnformatted(root_);
    if (!json_string) {
        return "{}";
    }
    
    std::string result(json_string);
    free(json_string);
    return result;
}

// Object methods
JsonBuilder& JsonBuilder::addString(const char* key, const char* value) {
    if (root_ && cJSON_IsObject(root_)) {
        cJSON_AddStringToObject(root_, key, value);
    }
    return *this;
}

JsonBuilder& JsonBuilder::addNumber(const char* key, int value) {
    if (root_ && cJSON_IsObject(root_)) {
        cJSON_AddNumberToObject(root_, key, value);
    }
    return *this;
}

JsonBuilder& JsonBuilder::addNumber(const char* key, double value) {
    if (root_ && cJSON_IsObject(root_)) {
        cJSON_AddNumberToObject(root_, key, value);
    }
    return *this;
}

JsonBuilder& JsonBuilder::addBool(const char* key, bool value) {
    if (root_ && cJSON_IsObject(root_)) {
        cJSON_AddBoolToObject(root_, key, value ? cJSON_True : cJSON_False);
    }
    return *this;
}

JsonBuilder& JsonBuilder::addNull(const char* key) {
    if (root_ && cJSON_IsObject(root_)) {
        cJSON_AddNullToObject(root_, key);
    }
    return *this;
}

JsonBuilder& JsonBuilder::addObject(const char* key, JsonBuilder&& obj) {
    if (root_ && cJSON_IsObject(root_)) {
        cJSON* obj_json = obj.release();
        cJSON_AddItemToObject(root_, key, obj_json);
    }
    return *this;
}

JsonBuilder& JsonBuilder::addArray(const char* key, JsonBuilder&& arr) {
    if (root_ && cJSON_IsObject(root_)) {
        cJSON* arr_json = arr.release();
        cJSON_AddItemToObject(root_, key, arr_json);
    }
    return *this;
}

JsonBuilder& JsonBuilder::addItem(const char* key, cJSON* item) {
    if (root_ && cJSON_IsObject(root_) && item) {
        cJSON_AddItemToObject(root_, key, item);
    }
    return *this;
}

// Array methods
JsonBuilder& JsonBuilder::addString(const char* value) {
    if (root_ && cJSON_IsArray(root_)) {
        cJSON_AddItemToArray(root_, cJSON_CreateString(value));
    }
    return *this;
}

JsonBuilder& JsonBuilder::addNumber(int value) {
    if (root_ && cJSON_IsArray(root_)) {
        cJSON_AddItemToArray(root_, cJSON_CreateNumber(value));
    }
    return *this;
}

JsonBuilder& JsonBuilder::addNumber(double value) {
    if (root_ && cJSON_IsArray(root_)) {
        cJSON_AddItemToArray(root_, cJSON_CreateNumber(value));
    }
    return *this;
}

JsonBuilder& JsonBuilder::addBool(bool value) {
    if (root_ && cJSON_IsArray(root_)) {
        cJSON_AddItemToArray(root_, cJSON_CreateBool(value ? cJSON_True : cJSON_False));
    }
    return *this;
}

JsonBuilder& JsonBuilder::addNull() {
    if (root_ && cJSON_IsArray(root_)) {
        cJSON_AddItemToArray(root_, cJSON_CreateNull());
    }
    return *this;
}

JsonBuilder& JsonBuilder::addObject(JsonBuilder&& obj) {
    if (root_ && cJSON_IsArray(root_)) {
        cJSON* obj_json = obj.release();
        cJSON_AddItemToArray(root_, obj_json);
    }
    return *this;
}

JsonBuilder& JsonBuilder::addArray(JsonBuilder&& arr) {
    if (root_ && cJSON_IsArray(root_)) {
        cJSON* arr_json = arr.release();
        cJSON_AddItemToArray(root_, arr_json);
    }
    return *this;
}

JsonBuilder& JsonBuilder::addItem(cJSON* item) {
    if (root_ && cJSON_IsArray(root_) && item) {
        cJSON_AddItemToArray(root_, item);
    }
    return *this;
}

