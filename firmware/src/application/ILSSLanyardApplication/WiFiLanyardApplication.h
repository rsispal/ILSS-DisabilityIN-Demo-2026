#pragma once

#include <map>
#include <string>
#include "../../state/State.h"
#include "../../utils/Logger.h"
#include "../../lowlevel/LowLevel.h"
#include "../../utils/JSON.h"

/**
 * WiFiLanyardApplication - Main application for WiFi-enabled lanyard
 * 
 * Demonstrates dependency injection pattern with LowLevel drivers.
 */
class WiFiLanyardApplication {
    const char* TAG = "WiFiLanyard";

public:
    WiFiLanyardApplication(Logger* logger, LowLevel* lowLevel, State* state);
    ~WiFiLanyardApplication();
    void begin();

private:
    State* state = nullptr;
    Logger* logger = nullptr;
    LowLevel* lowLevel = nullptr;
};
