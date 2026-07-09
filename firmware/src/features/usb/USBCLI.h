#pragma once

#include <string>

// Forward declarations
class Logger;
class LowLevel;
class State;

/**
 * USBCLI - Simple USB CLI interface
 * 
 * Provides a simple command-line interface over USB for device configuration.
 * Implements the interactive CLI pattern from main.cpp.
 */
class USBCLI {
    const char* TAG = "USBCLI";

public:
    USBCLI(Logger* logger, LowLevel* lowLevel, State* state);
    ~USBCLI();

    // Initialize and start CLI
    void begin();

    // Process CLI commands (call in main loop)
    void processCommands();
    
    // Process JSON protocol commands (for Provisioning Mode)
    void processJSONCommands();

    // Wait for any key press (returns true if key pressed within timeout)
    bool waitForAnyKey(int timeout_ms = 5000);

    // Run interactive configuration mode
    void runConfigurationMode();

private:
    Logger* logger_;
    LowLevel* lowLevel_;
    State* state_;

    // USB I/O helpers
    void write(const char* s);
    void writeln(const char* s);
    void writeChunked(const char* s);  // Write large data in chunks with proper buffering
    int readByte(uint8_t* out, int timeout_ms);
    std::string readline(int maxlen = 512, bool echo = true);
    
    // Configuration mode sub-menus
    void runConfigureDataParameters();
    void runTestMode();
    void runBuzzerTestMenu();
    void runFactoryMode();
    void showStatus();
    
    // Test mode routines
    void testLEDCycle();       // Cycle through RGB colors
    void runLEDPatternTest();  // LED pattern test (choose effect & color)
    void testHapticDemo();     // Demo haptic patterns
    void testPersonalAlert();  // Personal alert sim (all indicators)
    void testFireAlarm();      // Fire alarm sim (all indicators)
    void runSideButtonTest();  // Side button test routine
    void runNVSTest();         // NVS storage test routine
    void runWiFiTestMenu();    // WiFi test sub-menu
    void testErrorMode();      // Error mode test
    void runUSBProtocolTest(); // USB provisioning protocol test
    void runFactoryReset();    // Factory reset (erase NVS and reboot)
    
    // Buzzer pattern tests
    void runAllBuzzerPatterns();
    void testBuzzerPattern(const char* name, int pattern_type);
    void testSpeechPlayback();  // Experimental speech through piezo
    
    // WiFi test routines
    void testWiFiScan();       // Scan WiFi networks
    void testWiFiConnect();    // Connect to WiFi network
    void testWiFiStatus();     // Get WiFi connection status
    void testWiFiDisconnect(); // Disconnect from WiFi
    void testWiFiHttpGet();    // Test HTTP GET request to httpbin
    
    // Helper to wait for 'e' key press to stop test
    bool waitForExitKey();
};

