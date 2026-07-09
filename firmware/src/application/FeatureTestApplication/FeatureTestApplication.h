#pragma once

#include "../../state/State.h"
#include "../../utils/Logger.h"
#include "../../lowlevel/LowLevel.h"
#include "../../utils/JSON.h"
#include "../../lowlevel/network/NetworkClient.h"
#include "../../lowlevel/wifi/WiFiLowLevelDriver.h"
#include "../../features/bluetooth/Bluetooth.h"
#include "../../features/buzzer/Buzzer.h"
#include "../../features/side-buttons/SideButtons.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/azure-iot/AzureIoT.h"
#include "../../layers/ble-beacon/BLEBeacon.h"

/**
 * FeatureTestApplication - Application for testing hardware features
 */
class FeatureTestApplication {
    const char* TAG = "FeatureTest";

public:
    FeatureTestApplication(Logger* logger, LowLevel* lowLevel, State* state);
    ~FeatureTestApplication();
    
    void begin();

private:
    Logger* logger;
    LowLevel* lowLevel;
    State* state;
    Bluetooth* bluetooth;

    // Test routines
    void testJson();
    void testWiFi();
    void testNetworkRequests();
    void testBluetooth();
    void testBuzzer();
    void testHaptics();
    void testSideButtons();
    void testSideButtonsBareMetal();
    void testRGBLED();
    void testAzureIoT();
    
    // HTTP helpers
    static void print_http_response(const char *label, const NetworkClient::HttpResponse &resp);
};
