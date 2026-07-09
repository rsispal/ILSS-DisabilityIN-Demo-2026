#pragma once

#include <string>

// Forward declaration
struct cJSON;

/**
 * JsonParser - Utility class for JSON parsing
 * 
 * Lightweight RAII wrapper for cJSON library.
 * Simple utility class for parsing JSON strings.
 */
class JsonParser {
public:
    // Parse JSON from string
    explicit JsonParser(const std::string& json_str);
    
    // Destructor - automatically cleans up cJSON object
    ~JsonParser();

    // Check if JSON was parsed successfully
    bool valid() const { return root_ != nullptr; }

    // Get raw cJSON root object (for advanced usage)
    cJSON* get() const { return root_; }

    // Get cJSON object by key
    cJSON* get(const char* key) const;

    // Extract string value by key
    std::string getString(const char* key, const char* default_value = "") const;

    // Extract integer value by key
    int getInt(const char* key, int default_value = 0) const;

    // Extract boolean value by key
    bool getBool(const char* key, bool default_value = false) const;

    // Extract nested string value by path (e.g., "data.device_id")
    std::string getStringPath(const char* path, const char* default_value = "") const;

    // Extract nested integer value by path (e.g., "data.timestamp")
    int getIntPath(const char* path, int default_value = 0) const;

private:
    cJSON* root_;

    // Get nested object by path (e.g., "data.device_id")
    cJSON* getNested(const char* path) const;
};

/**
 * JsonBuilder - Utility class for building JSON objects and arrays
 * 
 * RAII wrapper for cJSON that provides a clean API for creating JSON structures.
 * Automatically handles memory management.
 */
class JsonBuilder {
public:
    // Create a new JSON object
    static JsonBuilder object();
    
    // Create a new JSON array
    static JsonBuilder array();
    
    // Wrap an existing cJSON object (takes ownership)
    explicit JsonBuilder(cJSON* json);
    
    // Destructor - automatically cleans up cJSON object
    ~JsonBuilder();
    
    // Move constructor
    JsonBuilder(JsonBuilder&& other) noexcept;
    
    // Move assignment
    JsonBuilder& operator=(JsonBuilder&& other) noexcept;
    
    // Delete copy constructor/assignment (prevent accidental copying)
    JsonBuilder(const JsonBuilder&) = delete;
    JsonBuilder& operator=(const JsonBuilder&) = delete;

    // Check if JSON object is valid
    bool valid() const { return root_ != nullptr; }

    // Get raw cJSON object (for advanced usage or passing to other functions)
    cJSON* get() const { return root_; }
    
    // Release ownership of cJSON object (caller must delete)
    cJSON* release();

    // Convert to JSON string
    std::string toString() const;

    // Object methods
    JsonBuilder& addString(const char* key, const char* value);
    JsonBuilder& addNumber(const char* key, int value);
    JsonBuilder& addNumber(const char* key, double value);
    JsonBuilder& addBool(const char* key, bool value);
    JsonBuilder& addNull(const char* key);
    JsonBuilder& addObject(const char* key, JsonBuilder&& obj);
    JsonBuilder& addArray(const char* key, JsonBuilder&& arr);
    JsonBuilder& addItem(const char* key, cJSON* item); // Takes ownership of item

    // Array methods
    JsonBuilder& addString(const char* value);
    JsonBuilder& addNumber(int value);
    JsonBuilder& addNumber(double value);
    JsonBuilder& addBool(bool value);
    JsonBuilder& addNull();
    JsonBuilder& addObject(JsonBuilder&& obj);
    JsonBuilder& addArray(JsonBuilder&& arr);
    JsonBuilder& addItem(cJSON* item); // Takes ownership of item

private:
    cJSON* root_;
    bool owns_root_; // Track if we own the root object

    JsonBuilder() : root_(nullptr), owns_root_(false) {}
};

