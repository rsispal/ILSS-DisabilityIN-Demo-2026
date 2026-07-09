#include "USBCLI.h"
#include "USBProtocol.h"
#include "../../utils/Logger.h"
#include "../../lowlevel/LowLevel.h"
#include "../../lowlevel/usb/UsbLowLevelDriver.h"
#include "../../lowlevel/wifi/WiFiLowLevelDriver.h"
#include "../../state/State.h"
#include "../../utils/JSON.h"
#include "cJSON.h"
#include "../../features/buzzer/Buzzer.h"
#include "../../features/rgb-led/RGBLED.h"
#include "../../features/side-buttons/SideButtons.h"
#include "../../application/Hardware.h"
#include "../../lowlevel/haptics/DRV2605Driver.h"
#include "../../lowlevel/buzzer/BuzzerLowLevelDriver.h"
#include "../../lowlevel/nvs/NVSLowLevelDriver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <string>
#include <algorithm>

USBCLI::USBCLI(Logger* logger, LowLevel* lowLevel, State* state)
    : logger_(logger), lowLevel_(lowLevel), state_(state) {
}

USBCLI::~USBCLI() {
}

void USBCLI::begin() {
    logger_->LOGI(TAG, "USB CLI initialized");
}

void USBCLI::processCommands() {
    if (!lowLevel_ || !lowLevel_->get_usb().isReady()) {
        return;
    }

    std::string line = readline();
    if (!line.empty()) {
        logger_->LOGD(TAG, "Received CLI input: %s", line.c_str());
        // Process CLI commands here if needed
    }
}

void USBCLI::processJSONCommands() {
    if (!lowLevel_ || !lowLevel_->get_usb().isReady()) {
        return;
    }
    
    // Try to read a JSON command (non-blocking, no echo for clean protocol)
    std::string line = readline(1024, false); // Max 1024 bytes, no echo
    if (line.empty()) {
        return;
    }
    
    // Try to parse as JSON (basic validation)
    JsonParser parser(line);
    if (!parser.valid()) {
        // Not valid JSON, ignore
        return;
    }
    
    // Check for cmd field
    int cmd_id = parser.getInt("cmd", -1);
    if (cmd_id < 0) {
        // No cmd field, ignore
        return;
    }
    
    logger_->LOGD(TAG, "Processing JSON command %d", cmd_id);
    
    // For long-running commands, send an immediate ACK before processing
    // This lets the host know the command was received and is being processed
    // Commands: 3=SCAN_WIFI_NETWORKS, 4=CONNECT_WIFI
    bool isLongRunningCommand = (cmd_id == 3 || cmd_id == 4);
    if (isLongRunningCommand) {
        // Send ACK immediately
        char ackBuffer[64];
        snprintf(ackBuffer, sizeof(ackBuffer), "{\"cmd\":%d,\"ack\":true}\r\n", cmd_id);
        writeChunked(ackBuffer);
        
        logger_->LOGD(TAG, "Sent ACK for long-running command %d", cmd_id);
    }
    
    // Delegate to USBProtocol for all command processing
    USBProtocol protocol(logger_, lowLevel_, state_);
    std::string response = protocol.processCommand(line);
    
    logger_->LOGD(TAG, "Command %d response length: %zu bytes", cmd_id, response.length());
    
    // Send response with chunking for large responses
    std::string fullResponse = response + "\r\n";
    writeChunked(fullResponse.c_str());
    
    logger_->LOGD(TAG, "Command %d response sent", cmd_id);
    
    // Check if provisioning is complete after successful command
    if (!state_->getIsProvisioned() &&
        !state_->getSessionId().empty() && 
        !state_->getUserFirstName().empty() && 
        !state_->getUserLastName().empty() &&
        !state_->getPersona().empty() &&
        !state_->getWifiSsid().empty() &&
        !state_->getWifiPassword().empty()) {
            state_->setIsProvisioned(true);
            // Explicitly save to NVS since setIsProvisioned doesn't auto-save
            state_->saveToNVS();
            logger_->LOGI(TAG, "Device marked as provisioned (all required fields set) and saved to NVS");
    }
}

bool USBCLI::waitForAnyKey(int timeout_ms) {
    if (!lowLevel_ || !lowLevel_->get_usb().isReady()) {
        // USB not ready, skip CLI mode
        return false;
    }
    
    writeln("\r\nPress any key within the next 5 seconds to enter configuration mode...");
    int elapsed = 0;
    const int step_ms = 50;
    uint8_t ch;
    while (elapsed < timeout_ms) {
        int r = readByte(&ch, step_ms);
        if (r > 0) {
            writeln("\r\nConfiguration mode triggered.");
            return true;
        }
        elapsed += step_ms;
    }
    writeln("No key pressed; continuing normal boot.");
    return false;
}

void USBCLI::runConfigurationMode() {
    while (true) {
    writeln("\r\n=== ILSS Configuration Mode ===");
        writeln("c = Configure data parameters");
        writeln("t = Test mode");
        writeln("f = Factory mode (device ID/keys)");
        writeln("s = Show current status");
        writeln("reset = Factory reset (erase NVS & reboot)");
        writeln("e = Exit and continue with application");
        writeln("\r\nEnter choice: ");
        
        std::string choice = readline();
        
        if (choice == "c" || choice == "C") {
            runConfigureDataParameters();
        } else if (choice == "t" || choice == "T") {
            runTestMode();
        } else if (choice == "f" || choice == "F") {
            runFactoryMode();
        } else if (choice == "s" || choice == "S") {
            showStatus();
        } else if (choice == "reset" || choice == "RESET" || choice == "factory" || choice == "FACTORY") {
            runFactoryReset();
        } else if (choice == "e" || choice == "E") {
            writeln("Exiting configuration mode...");
            return;
        } else {
            writeln("Invalid choice.");
        }
    }
}

void USBCLI::runConfigureDataParameters() {
    writeln("\r\n=== Configure Data Parameters ===");
    
    writeln("Session ID: ");
    std::string session_id = readline();
    state_->setSessionId(session_id);

    writeln("Persona: ACCESSIBLE_USER,HOUSEKEEPER,LONE_WORKER,SECURITY,EMPLOYEE,ELDERLY,FIRE_WARDEN");
    std::string persona = readline();
    state_->setPersona(persona);

    writeln("WiFi SSID: ");
    std::string wifi_ssid = readline();
    state_->setWifiSsid(wifi_ssid);
    
    writeln("WiFi Password: ");
    std::string wifi_pass = readline();
    state_->setWifiPassword(wifi_pass);

    writeln("First Name: ");
    std::string first_name = readline();
    state_->setUserFirstName(first_name);

    writeln("Last Name: ");
    std::string last_name = readline();
    state_->setUserLastName(last_name);
    
    // Boolean preferences
    writeln("\r\n--- Feature Preferences (y/n) ---");
    
    writeln("Enable Personal Alert Buttons (y/n) [default: y]: ");
    std::string en_pa_btn = readline();
    if (en_pa_btn.empty() || en_pa_btn == "y" || en_pa_btn == "Y" || en_pa_btn == "yes" || en_pa_btn == "YES") {
        state_->setEnablePersonalAlertButtons(true);
    } else {
        state_->setEnablePersonalAlertButtons(false);
    }
    
    writeln("Enable Haptics (y/n) [default: y]: ");
    std::string en_haptics = readline();
    if (en_haptics.empty() || en_haptics == "y" || en_haptics == "Y" || en_haptics == "yes" || en_haptics == "YES") {
        state_->setEnableHaptics(true);
    } else {
        state_->setEnableHaptics(false);
    }
    
    writeln("Enable Buzzer (y/n) [default: y]: ");
    std::string en_buzzer = readline();
    if (en_buzzer.empty() || en_buzzer == "y" || en_buzzer == "Y" || en_buzzer == "yes" || en_buzzer == "YES") {
        state_->setEnableBuzzer(true);
    } else {
        state_->setEnableBuzzer(false);
    }
    
    writeln("Enable LED Indications (y/n) [default: y]: ");
    std::string en_led = readline();
    if (en_led.empty() || en_led == "y" || en_led == "Y" || en_led == "yes" || en_led == "YES") {
        state_->setEnableLedIndications(true);
    } else {
        state_->setEnableLedIndications(false);
    }
    
    writeln("Enable Inactivity Alerts (y/n) [default: n]: ");
    std::string en_inact = readline();
    if (en_inact == "y" || en_inact == "Y" || en_inact == "yes" || en_inact == "YES") {
        state_->setEnableInactivityAlerts(true);
    } else {
        state_->setEnableInactivityAlerts(false);
    }
    
    writeln("Enable NFC Session Sharing (y/n) [default: y]: ");
    std::string en_nfc = readline();
    if (en_nfc.empty() || en_nfc == "y" || en_nfc == "Y" || en_nfc == "yes" || en_nfc == "YES") {
        state_->setEnableNfcSessionSharing(true);
    } else {
        state_->setEnableNfcSessionSharing(false);
    }
    
    writeln("Enable Honeywell Beacon Scanning (y/n) [default: y]: ");
    std::string en_hw_beacon = readline();
    if (en_hw_beacon.empty() || en_hw_beacon == "y" || en_hw_beacon == "Y" || en_hw_beacon == "yes" || en_hw_beacon == "YES") {
        state_->setEnableHoneywellBeaconScanning(true);
    } else {
        state_->setEnableHoneywellBeaconScanning(false);
    }
    
    // Set provisioned state if we have all required values
    if (!session_id.empty() && !persona.empty() && 
        !wifi_ssid.empty() && !wifi_pass.empty() &&
        !first_name.empty() && !last_name.empty()) {
        state_->setIsProvisioned(true);
        logger_->LOGI(TAG, "Device marked as provisioned");
    } else {
        logger_->LOGW(TAG, "Missing required values, device not marked as provisioned");
    }
    
    writeln("\r\n--- Received values ---");
    logger_->LOGI(TAG, "Session ID: %s", session_id.c_str());
    logger_->LOGI(TAG, "Persona: %s", persona.c_str());
    logger_->LOGI(TAG, "WiFi SSID: %s", wifi_ssid.c_str());
    logger_->LOGI(TAG, "WiFi Pass: ****");
    logger_->LOGI(TAG, "First Name: %s", first_name.c_str());
    logger_->LOGI(TAG, "Last Name: %s", last_name.c_str());
    logger_->LOGI(TAG, "Enable Personal Alert Buttons: %s", state_->getEnablePersonalAlertButtons() ? "true" : "false");
    logger_->LOGI(TAG, "Enable Haptics: %s", state_->getEnableHaptics() ? "true" : "false");
    logger_->LOGI(TAG, "Enable Buzzer: %s", state_->getEnableBuzzer() ? "true" : "false");
    logger_->LOGI(TAG, "Enable LED Indications: %s", state_->getEnableLedIndications() ? "true" : "false");
    logger_->LOGI(TAG, "Enable Inactivity Alerts: %s", state_->getEnableInactivityAlerts() ? "true" : "false");
    logger_->LOGI(TAG, "Enable NFC Session Sharing: %s", state_->getEnableNfcSessionSharing() ? "true" : "false");
    logger_->LOGI(TAG, "Enable Honeywell Beacon Scanning: %s", state_->getEnableHoneywellBeaconScanning() ? "true" : "false");
    logger_->LOGI(TAG, "Provisioned: %s", state_->getIsProvisioned() ? "true" : "false");
    writeln("--- End ---");
}

void USBCLI::runFactoryMode() {
    writeln("\r\n=== Factory Mode ===");
    writeln("1 = Generate Device ID (UUIDv4)");
    writeln("2 = Generate ATECC608A device keys");
    writeln("3 = Show SHA256 thumbprints");
    writeln("4 = Full factory provisioning");
    writeln("e = Exit");
    writeln("\r\nChoice: ");
    
    std::string choice = readline();
    
    if (choice == "1") {
        writeln("\r\n[NOT IMPLEMENTED] Generate Device ID");
        writeln("This feature requires hardware random number generator.");
        writeln("Would generate UUIDv4 and store in secure element.");
        writeln("\r\nCurrent Device ID: ");
        writeln(state_->getDeviceId().c_str());
    } else if (choice == "2") {
        writeln("\r\n[NOT IMPLEMENTED] Generate ATECC608A Keys");
        writeln("This feature requires ATECC608A secure element.");
        writeln("Would generate:");
        writeln("  - Device private key (stored in secure element)");
        writeln("  - Device public key (for Azure DPS)");
        writeln("  - Signing key for authentication");
    } else if (choice == "3") {
        writeln("\r\n[NOT IMPLEMENTED] SHA256 Thumbprints");
        writeln("This feature requires generated keys.");
        writeln("Would display:");
        writeln("  - Primary thumbprint (SHA256 of public key)");
        writeln("  - Secondary thumbprint (SHA256 of secondary key)");
        writeln("\r\nExample format:");
        writeln("  Primary:   AB:CD:EF:12:34:56:78:90:...");
        writeln("  Secondary: 12:34:56:78:90:AB:CD:EF:...");
    } else if (choice == "4") {
        writeln("\r\n[NOT IMPLEMENTED] Full Factory Provisioning");
        writeln("This feature requires ATECC608A secure element.");
        writeln("Would perform:");
        writeln("  1. Generate UUIDv4 device ID");
        writeln("  2. Initialize ATECC608A secure element");
        writeln("  3. Generate device key pair");
        writeln("  4. Generate secondary key pair");
        writeln("  5. Display SHA256 thumbprints for Azure DPS enrollment");
        writeln("  6. Store device ID in NVS");
    } else if (choice == "e" || choice == "E") {
        return;
    } else {
        writeln("Invalid choice.");
    }
}

void USBCLI::showStatus() {
    writeln("\r\n=== Device Status ===");
    
    char buf[128];
    
    // Device info
    writeln("\r\n--- Device Info ---");
    snprintf(buf, sizeof(buf), "Device ID: %s", state_->getDeviceId().c_str());
    writeln(buf);
    snprintf(buf, sizeof(buf), "Provisioned: %s", state_->getIsProvisioned() ? "YES" : "NO");
    writeln(buf);
    
    // User info
    writeln("\r\n--- User Info ---");
    snprintf(buf, sizeof(buf), "Session ID: %s", state_->getSessionId().empty() ? "(not set)" : state_->getSessionId().c_str());
    writeln(buf);
    snprintf(buf, sizeof(buf), "First Name: %s", state_->getUserFirstName().empty() ? "(not set)" : state_->getUserFirstName().c_str());
    writeln(buf);
    snprintf(buf, sizeof(buf), "Last Name: %s", state_->getUserLastName().empty() ? "(not set)" : state_->getUserLastName().c_str());
    writeln(buf);
    snprintf(buf, sizeof(buf), "Persona: %s", state_->getPersona().empty() ? "(not set)" : state_->getPersona().c_str());
    writeln(buf);
    
    // WiFi info
    writeln("\r\n--- WiFi ---");
    snprintf(buf, sizeof(buf), "SSID: %s", state_->getWifiSsid().empty() ? "(not set)" : state_->getWifiSsid().c_str());
    writeln(buf);
    snprintf(buf, sizeof(buf), "Password: %s", state_->getWifiPassword().empty() ? "(not set)" : "****");
    writeln(buf);
    
    // Check what's missing for provisioning
    writeln("\r\n--- Provisioning Checklist ---");
    bool session_ok = !state_->getSessionId().empty();
    bool fname_ok = !state_->getUserFirstName().empty();
    bool lname_ok = !state_->getUserLastName().empty();
    bool persona_ok = !state_->getPersona().empty();
    bool ssid_ok = !state_->getWifiSsid().empty();
    bool pass_ok = !state_->getWifiPassword().empty();
    
    snprintf(buf, sizeof(buf), "[%s] Session ID", session_ok ? "X" : " ");
    writeln(buf);
    snprintf(buf, sizeof(buf), "[%s] First Name", fname_ok ? "X" : " ");
    writeln(buf);
    snprintf(buf, sizeof(buf), "[%s] Last Name", lname_ok ? "X" : " ");
    writeln(buf);
    snprintf(buf, sizeof(buf), "[%s] Persona", persona_ok ? "X" : " ");
    writeln(buf);
    snprintf(buf, sizeof(buf), "[%s] WiFi SSID", ssid_ok ? "X" : " ");
    writeln(buf);
    snprintf(buf, sizeof(buf), "[%s] WiFi Password", pass_ok ? "X" : " ");
    writeln(buf);
    
    if (session_ok && fname_ok && lname_ok && persona_ok && ssid_ok && pass_ok) {
        writeln("\r\nAll required fields set - device can be provisioned.");
    } else {
        writeln("\r\nMissing required fields - device cannot be provisioned.");
    }
    
    // Feature flags
    writeln("\r\n--- Feature Flags ---");
    snprintf(buf, sizeof(buf), "Personal Alert Buttons: %s", state_->getEnablePersonalAlertButtons() ? "ON" : "OFF");
    writeln(buf);
    snprintf(buf, sizeof(buf), "Haptics: %s", state_->getEnableHaptics() ? "ON" : "OFF");
    writeln(buf);
    snprintf(buf, sizeof(buf), "Buzzer: %s", state_->getEnableBuzzer() ? "ON" : "OFF");
    writeln(buf);
    snprintf(buf, sizeof(buf), "LED Indications: %s", state_->getEnableLedIndications() ? "ON" : "OFF");
    writeln(buf);
    snprintf(buf, sizeof(buf), "Beacon Scanning: %s", state_->getEnableHoneywellBeaconScanning() ? "ON" : "OFF");
    writeln(buf);
}

void USBCLI::write(const char* s) {
    if (!s || !lowLevel_ || !lowLevel_->get_usb().isReady()) return;
    lowLevel_->get_usb().write(std::string(s));
}

void USBCLI::writeln(const char* s) {
    if (!s || !lowLevel_ || !lowLevel_->get_usb().isReady()) return;
    lowLevel_->get_usb().writeLine(std::string(s));
}

void USBCLI::writeChunked(const char* s) {
    if (!s || !lowLevel_ || !lowLevel_->get_usb().isReady()) return;
    
    const size_t len = strlen(s);
    const size_t CHUNK_SIZE = 256; // USB buffer is 512, use smaller chunks for safety
    size_t offset = 0;
    
    while (offset < len) {
        size_t remaining = len - offset;
        size_t chunk_len = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        
        // Write chunk with extended timeout (500ms per chunk)
        bool success = lowLevel_->get_usb().writeBytes(
            reinterpret_cast<const uint8_t*>(s + offset), 
            chunk_len, 
            500
        );
        
        if (!success) {
            logger_->LOGE(TAG, "writeChunked failed at offset %zu", offset);
            break;
        }
        
        offset += chunk_len;
        
        // Small delay between chunks to allow USB buffer to drain
        if (offset < len) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    
    // Final delay to ensure all data is flushed
    vTaskDelay(pdMS_TO_TICKS(10));
}

int USBCLI::readByte(uint8_t* out, int timeout_ms) {
    if (!out || !lowLevel_ || !lowLevel_->get_usb().isReady()) return 0;
    return lowLevel_->get_usb().read(out, timeout_ms);
}

std::string USBCLI::readline(int maxlen, bool echo) {
    if (!lowLevel_ || !lowLevel_->get_usb().isReady()) return "";
    
    std::string result;
    uint8_t ch;
    
    // Keep reading until we get a newline character
    while ((int)result.length() < maxlen) {
        int r = readByte(&ch, 1000); // 1 second timeout per character
        if (r > 0) {
            // Handle backspace/delete for editing (only if echo enabled)
            if (echo && (ch == '\b' || ch == 0x7F)) { // Backspace or DEL
                if (!result.empty()) {
                    result.pop_back();
                    // Send backspace sequence: \b \b to erase character
                    write("\b \b");
                }
                continue;
            }
            
            // Echo the character (except newline) - only if echo enabled
            if (echo && ch != '\r' && ch != '\n') {
                char echo_char[2] = {(char)ch, 0};
                write(echo_char);
            }
            
            // Check for newline - this is when we return
            if (ch == '\r' || ch == '\n') {
                if (ch == '\r') {
                    // Check for \n after \r (CRLF)
                    uint8_t next;
                    readByte(&next, 10); // Short timeout to check for LF
                    if (next != '\n') {
                        // No LF, but we still got CR, so we're done
                    }
                }
                // Echo newline for proper terminal display (only if echo enabled)
                if (echo) {
                writeln("");
                }
                break; // Got newline, return the line
            }
            
            // Add character to result
            result += (char)ch;
        }
        // If timeout, continue waiting (don't break) - we want to wait for newline
    }
    
    return result;
}

void USBCLI::runTestMode() {
    while (true) {
        writeln("\r\n=== Test Mode ===");
        writeln("b   = Buzzer sub-menu");
        writeln("led = LED color cycle");
        writeln("ledp = LED pattern test (choose effect & color)");
        writeln("hap = Haptic patterns demo");
        writeln("pa  = Personal Alert sim");
        writeln("fa  = Fire Alarm sim");
        writeln("sb  = Side Button test");
        writeln("nvs = NVS storage test");
        writeln("wifi = WiFi test sub-menu");
        writeln("err = Error mode test");
        writeln("usb = USB provisioning protocol test");
        writeln("e   = Exit");
        writeln("\r\nChoice: ");
        
        std::string choice = readline();
        
        if (choice == "e" || choice == "E") {
            writeln("Returning to main menu...");
            return;
        } else if (choice == "b" || choice == "B") {
            runBuzzerTestMenu();
        } else if (choice == "led" || choice == "LED") {
            testLEDCycle();
        } else if (choice == "ledp" || choice == "LEDP") {
            runLEDPatternTest();
        } else if (choice == "hap" || choice == "HAP") {
            testHapticDemo();
        } else if (choice == "pa" || choice == "PA") {
            testPersonalAlert();
        } else if (choice == "fa" || choice == "FA") {
            testFireAlarm();
        } else if (choice == "sb" || choice == "SB") {
            runSideButtonTest();
        } else if (choice == "nvs" || choice == "NVS") {
            runNVSTest();
        } else if (choice == "wifi" || choice == "WIFI" || choice == "w" || choice == "W") {
            runWiFiTestMenu();
        } else if (choice == "err" || choice == "ERR" || choice == "error" || choice == "ERROR") {
            testErrorMode();
        } else if (choice == "usb" || choice == "USB") {
            runUSBProtocolTest();
        } else {
            writeln("Invalid choice.");
        }
    }
}

bool USBCLI::waitForExitKey() {
    uint8_t ch;
    int r = readByte(&ch, 50);  // Non-blocking check
    if (r > 0) {
        // Check for 'e' or 'E', but ignore newline/carriage return from previous input
        if (ch == 'e' || ch == 'E') {
            return true;
        }
        // Ignore other characters (newline, carriage return, etc.)
    }
    return false;
}

void USBCLI::runBuzzerTestMenu() {
    while (true) {
        writeln("\r\n=== Buzzer Test Sub-Menu ===");
        writeln("0 = Run ALL patterns");
        writeln("1 = Tick (short beep)");
        writeln("2 = Beep (1 second tone)");
        writeln("3 = Code-3 Temporal (constant tone)");
        writeln("4 = Code-3 Sweep (3x ramp UP)");
        writeln("5 = Code-3 Siren (3x smooth siren)");
        writeln("6 = Siren (continuous up/down)");
        writeln("7 = Medium Sweep");
        writeln("8 = Alternating (two-tone)");
        writeln("9 = Fire Horn Buzz");
        writeln("s = Speech Test (experimental)");
        writeln("e = Exit");
        writeln("\r\nChoice: ");
        
        std::string choice = readline();
        
        if (choice == "e" || choice == "E") {
            writeln("Returning to test menu...");
            return;
        } else if (choice == "0") {
            runAllBuzzerPatterns();
        } else if (choice == "1") {
            testBuzzerPattern("Tick", 1);
        } else if (choice == "2") {
            testBuzzerPattern("Beep", 2);
        } else if (choice == "3") {
            testBuzzerPattern("Code-3 Temporal", 3);
        } else if (choice == "4") {
            testBuzzerPattern("Code-3 Sweep", 4);
        } else if (choice == "5") {
            testBuzzerPattern("Code-3 Siren", 5);
        } else if (choice == "6") {
            testBuzzerPattern("Siren", 6);
        } else if (choice == "7") {
            testBuzzerPattern("Medium Sweep", 7);
        } else if (choice == "8") {
            testBuzzerPattern("Alternating", 8);
        } else if (choice == "9") {
            testBuzzerPattern("LF Buzz", 9);
        } else if (choice == "s" || choice == "S") {
            testSpeechPlayback();
        } else {
            writeln("Invalid choice.");
        }
    }
}

void USBCLI::testSpeechPlayback() {
    writeln("\r\n=== Speech Playback Test ===");
    writeln("Playing: [Sweep] -> [Fire Alarm] -> [Sweep] -> [Occupant Alert]");
    writeln("Press any key when done...");
    
    // Initialize buzzer
    Buzzer buzzer(state_, lowLevel_);
    if (!buzzer.begin()) {
        writeln("ERROR: Failed to initialize buzzer");
        return;
    }
    
    // Play pattern: [sweep][audio][sweep][audio]
    writeln("\r\n1. Code-3 Sweep...");
    buzzer.playCode3Sweep(2700, 3500, 1);
    // Wait for pattern to complete: 3 pulses (500ms each) + 2 gaps (500ms) + final gap (1500ms) = ~4000ms
    vTaskDelay(pdMS_TO_TICKS(4500));
    
    writeln("2. Fire Alarm speech...");
    buzzer.playSpeech(SpeechType::FIRE_ALARM);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    writeln("3. Code-3 Sweep...");
    buzzer.playCode3Sweep(2700, 3500, 1);
    // Wait for pattern to complete: 3 pulses (500ms each) + 2 gaps (500ms) + final gap (1500ms) = ~4000ms
    vTaskDelay(pdMS_TO_TICKS(4500));
    
    writeln("4. Occupant Alert speech...");
    buzzer.playSpeech(SpeechType::OCCUPANT_ALERT);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    buzzer.stop();
    writeln("\r\nSpeech test complete.");
    
    // Wait for key press to return
    uint8_t dummy;
    while (readByte(&dummy, 10) <= 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void USBCLI::runAllBuzzerPatterns() {
    writeln("\r\n=== Running ALL Buzzer Patterns ===");
    
    // Initialize buzzer
    Buzzer buzzer(state_, lowLevel_);
    if (!buzzer.begin()) {
        writeln("ERROR: Failed to initialize buzzer");
        return;
    }
    
    // Pattern list with names and brief descriptions
    struct PatternInfo {
        const char* name;
        int pattern_type;
    };
    
    PatternInfo patterns[] = {
        {"1. Tick", 1},
        {"2. Beep", 2},
        {"3. Code-3 Temporal", 3},
        {"4. Code-3 Sweep", 4},
        {"5. Code-3 Siren", 5},
        {"6. Siren", 6},
        {"7. Medium Sweep", 7},
        {"8. Alternating", 8},
        {"9. LF Buzz", 9},
    };
    
    const int num_patterns = sizeof(patterns) / sizeof(patterns[0]);
    
    for (int i = 0; i < num_patterns; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "\r\nPlaying: %s", patterns[i].name);
        writeln(msg);
        
        switch (patterns[i].pattern_type) {
            case 1:  // Tick
                buzzer.tick(3000);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            case 2:  // Beep
                buzzer.beep(2000, 1000);
                vTaskDelay(pdMS_TO_TICKS(1500));
                break;
            case 3:  // Code-3 Temporal
                buzzer.queueCode3Temporal(3000, 1);
                buzzer.processPendingBuzzer();
                // Wait for pattern to complete: 3 pulses (500ms each) + 2 gaps (500ms) + final gap (1500ms) = ~4000ms
                vTaskDelay(pdMS_TO_TICKS(4500));
                break;
            case 4:  // Code-3 Sweep
                buzzer.queueCode3Sweep(2700, 3500, 1);
                buzzer.processPendingBuzzer();
                // Wait for pattern to complete: 3 pulses (500ms each) + 2 gaps (500ms) + final gap (1500ms) = ~4000ms
                vTaskDelay(pdMS_TO_TICKS(4500));
                break;
            case 5:  // Code-3 Siren
                buzzer.queueCode3Siren(2700, 3500, 1);
                buzzer.processPendingBuzzer();
                // Wait for pattern to complete: 3 pulses (500ms each) + 2 gaps (500ms) + final gap (1500ms) = ~4000ms
                vTaskDelay(pdMS_TO_TICKS(4500));
                break;
            case 6:  // Siren
                buzzer.queueSiren(2700, 3500, 5);  // 5 cycles for demo
                buzzer.processPendingBuzzer();
                break;
            case 7:  // Medium Sweep
                buzzer.queueMediumSweep(800, 970, 2);  // 2 cycles for demo
                buzzer.processPendingBuzzer();
                break;
            case 8:  // Alternating
                buzzer.queueAlternating(800, 970, 4);  // 4 cycles for demo
                buzzer.processPendingBuzzer();
                break;
            case 9:  // LF Buzz (fire horn)
                buzzer.queueLFBuzz(600, 650, 2);  // 2 cycles for demo
                buzzer.processPendingBuzzer();
                break;
        }
        
        buzzer.stop();
        vTaskDelay(pdMS_TO_TICKS(300));  // Brief pause between patterns
    }
    
    buzzer.stop();
    writeln("\r\nAll patterns complete.");
}

// Structure to pass data to buzzer task
struct BuzzerTaskParams {
    Buzzer* buzzer;
    int pattern_type;
    volatile bool* running;
    volatile bool* taskDone;  // Signal when task has exited
};

// Buzzer task function - runs on second core for seamless continuous patterns
static void buzzerTaskFunc(void* params) {
    BuzzerTaskParams* p = static_cast<BuzzerTaskParams*>(params);
    
    // Run the pattern once with cycles=0 (infinite until shouldStop is set)
    switch (p->pattern_type) {
        case 6:  // Continuous Siren
            p->buzzer->playSiren(2700, 3500, 0);
            break;
        case 7:  // Medium Sweep
            p->buzzer->playMediumSweep(800, 970, 0);
            break;
        case 8:  // Alternating
            p->buzzer->playAlternating(800, 970, 0);
            break;
        case 9:  // Low Frequency Buzz
            p->buzzer->playLFBuzz(600, 650, 0);
            break;
        default:
            break;
    }
    
    // Signal that task is done before deleting
    *(p->taskDone) = true;
    vTaskDelete(NULL);
}

void USBCLI::testBuzzerPattern(const char* name, int pattern_type) {
    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n=== Testing: %s ===", name);
    writeln(msg);
    writeln("Press 'e' to stop");
    
    // Clear any pending input
    uint8_t dummy;
    while (readByte(&dummy, 10) > 0) {}
    
    // Initialize buzzer
    Buzzer buzzer(state_, lowLevel_);
    if (!buzzer.begin()) {
        writeln("ERROR: Failed to initialize buzzer");
        return;
    }
    
    // For continuous patterns (6-9), use a background task on core 0
    if (pattern_type >= 6 && pattern_type <= 9) {
        volatile bool taskRunning = true;
        volatile bool taskDone = false;
        BuzzerTaskParams taskParams = { &buzzer, pattern_type, &taskRunning, &taskDone };
        TaskHandle_t buzzerTask = NULL;
        
        // Create task on core 0 (main app typically runs on core 1)
        xTaskCreatePinnedToCore(
            buzzerTaskFunc,
            "buzzer_test",
            4096,
            &taskParams,
            5,
            &buzzerTask,
            0  // Core 0
        );
        
        // Wait for exit key in main thread
        while (!waitForExitKey()) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        // Stop the pattern
        buzzer.requestStop();
        
        // Wait for task to actually finish (with timeout)
        int timeout = 50;  // 500ms max wait
        while (!taskDone && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout--;
        }
        
    } else {
        // For discrete patterns (1-5), use blocking approach
        bool exitRequested = false;
        do {
            switch (pattern_type) {
                case 1:  // Tick
                    buzzer.tick(3000);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    break;
                case 2:  // Beep
                    buzzer.beep(2000, 1000);
                    vTaskDelay(pdMS_TO_TICKS(1500));
                    break;
                case 3:  // Code-3 Temporal
                    buzzer.queueCode3Temporal(3000, 0);  // cycles=0 for continuous until stopped
                    buzzer.processPendingBuzzer();
                    // Pattern runs continuously, check for exit key frequently
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
                case 4:  // Code-3 Sweep
                    buzzer.queueCode3Sweep(2700, 3500, 0);  // cycles=0 for continuous until stopped
                    buzzer.processPendingBuzzer();
                    // Pattern runs continuously, check for exit key frequently
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
                case 5:  // Code-3 Siren
                    buzzer.queueCode3Siren(2700, 3500, 0);  // cycles=0 for continuous until stopped
                    buzzer.processPendingBuzzer();
                    // Pattern runs continuously, check for exit key frequently
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
                default:
                    vTaskDelay(pdMS_TO_TICKS(100));
                    break;
            }
            exitRequested = waitForExitKey();
        } while (!exitRequested);
    }
    
    buzzer.stop();
    writeln("\r\nTest stopped.");
}

// Alert simulator buzzer task - runs Code-3 Siren continuously
static void personalAlertBuzzerTask(void* params) {
    BuzzerTaskParams* p = static_cast<BuzzerTaskParams*>(params);
    p->buzzer->playCode3Siren(2700, 3500, 0);  // cycles=0 = infinite until stopped
    *(p->taskDone) = true;
    vTaskDelete(NULL);
}

// Alert simulator buzzer task - runs Code-3 Sweep continuously
static void fireAlarmBuzzerTask(void* params) {
    BuzzerTaskParams* p = static_cast<BuzzerTaskParams*>(params);
    p->buzzer->playCode3Sweep(2700, 3500, 0);  // cycles=0 = infinite until stopped
    *(p->taskDone) = true;
    vTaskDelete(NULL);
}

void USBCLI::testPersonalAlert() {
    writeln("\r\n=== Personal Alert Simulator ===");
    writeln("Simulating personal alert (LED, buzzer, haptics)...");
    writeln("Press 'e' to stop");
    
    // Clear any pending input
    uint8_t dummy;
    while (readByte(&dummy, 10) > 0) {}
    
    // Initialize all features
    Buzzer buzzer(state_, lowLevel_);
    RGBLED rgbLed(state_, lowLevel_);
    
    buzzer.begin();
    rgbLed.begin();
    // Haptics should already be initialized in LowLevel::begin()
    
    if (!buzzer.isReady()) writeln("WARNING: Buzzer not ready");
    if (!rgbLed.isReady()) writeln("WARNING: LED not ready");
    if (!lowLevel_->get_haptics().isReady()) writeln("WARNING: Haptics not ready");
    
    // Start LED effect (rapid pulse purple for personal alert)
    if (rgbLed.isReady()) {
        rgbLed.queueEffect(LedEffect::RAPID_PULSE, LedColor::PURPLE, Brightness::B100, 0);
        rgbLed.process();
    }
    
    // Start buzzer in background task on Core 0
    volatile bool taskDone = false;
    BuzzerTaskParams taskParams = { &buzzer, 5, nullptr, &taskDone };  // pattern_type 5 = Code-3 Siren
    TaskHandle_t buzzerTask = NULL;
    
    if (buzzer.isReady()) {
        xTaskCreatePinnedToCore(
            personalAlertBuzzerTask,
            "pa_buzzer",
            4096,
            &taskParams,
            5,
            &buzzerTask,
            0  // Core 0
        );
    }
    
    // Start haptic immediately
    uint32_t lastHapticTime = 0;
    const uint32_t hapticInterval = 3000;  // 3 seconds
    
    if (lowLevel_->get_haptics().isReady()) {
        lowLevel_->get_haptics().play_pattern(118);
        lastHapticTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    
    // Main loop: LED updates and haptic timing on main thread
    while (!waitForExitKey()) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Process LED (non-blocking)
        if (rgbLed.isReady()) {
            rgbLed.process();
        }
        
        // Update haptic every 3 seconds
        if (lowLevel_->get_haptics().isReady() && 
            (now - lastHapticTime >= hapticInterval)) {
            lowLevel_->get_haptics().play_pattern(118);
            lastHapticTime = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
    // Stop buzzer and wait for task to finish
    if (buzzer.isReady()) {
        buzzer.requestStop();
        int timeout = 50;
        while (!taskDone && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout--;
        }
        buzzer.stop();
    }
    
    if (rgbLed.isReady()) rgbLed.stopEffect();
    
    writeln("\r\nTest stopped.");
}

void USBCLI::testLEDCycle() {
    writeln("\r\n=== LED Color Cycle ===");
    writeln("Cycling through colors...");
    
    // Initialize LED
    RGBLED rgbLed(state_, lowLevel_);
    if (!rgbLed.begin()) {
        writeln("ERROR: Failed to initialize LED");
        return;
    }
    
    // Define colors to cycle through
    struct ColorTest {
        LedColor color;
        const char* name;
    };
    
    ColorTest colors[] = {
        {LedColor::RED, "Red"},
        {LedColor::GREEN, "Green"},
        {LedColor::BLUE, "Blue"},
        {LedColor::PURPLE, "Purple"},
        {LedColor::YELLOW, "Yellow"},
        {LedColor::CYAN, "Cyan"},
        {LedColor::WHITE, "White"},
    };
    
    const int num_colors = sizeof(colors) / sizeof(colors[0]);
    const uint32_t display_time_ms = 1500;  // Show each color for 1.5 seconds
    
    for (int i = 0; i < num_colors; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "  %s", colors[i].name);
        writeln(msg);
        
        // Show solid color
        rgbLed.queueEffect(LedEffect::CONTINUOUS, colors[i].color, Brightness::B100, display_time_ms);
        
        // Process for display time
        uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
        while ((xTaskGetTickCount() * portTICK_PERIOD_MS) - start < display_time_ms) {
            rgbLed.process();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    
    rgbLed.stopEffect();
    writeln("Done.");
}

void USBCLI::testHapticDemo() {
    writeln("\r\n=== Haptic Patterns Demo ===");
    writeln("Playing sequence of haptic effects...");
    
    // Haptics should already be initialized in LowLevel::begin()
    // Just check if ready instead of calling begin() again
    if (!lowLevel_->get_haptics().isReady()) {
        writeln("ERROR: Haptics not ready (may need device restart)");
        return;
    }
    
    // Define haptic patterns to demonstrate
    // DRV2605 patterns: 
    //   1=Strong Click, 4=Sharp Click, 48=?, 52=Buzz, 58=Ramp Down, 63=Ramp Up
    //   118=Long Buzz for programmatic stopping (continuous until stopped)
    struct HapticDemo {
        uint8_t pattern;
        const char* name;
        uint32_t delay_after_ms;
        bool needs_stop;  // True if pattern runs continuously and needs to be stopped
    };
    
    HapticDemo demos[] = {
        {52, "Buzz", 800, false},
        {48, "Pattern 48", 1000, false},  // Testing pattern 48 as alternative to 118
        {1, "Strong Click", 800, false},
        {4, "Sharp Click", 800, false},
        {118, "Long Buzz (118)", 2000, true},  // Pattern 118 is continuous - will be stopped after 2 seconds
        {63, "Ramp Up", 1000, false},
        {58, "Ramp Down", 1000, false},
    };
    
    const int num_demos = sizeof(demos) / sizeof(demos[0]);
    
    for (int i = 0; i < num_demos; i++) {
        char msg[48];
        snprintf(msg, sizeof(msg), "  %s (pattern %d)", demos[i].name, demos[i].pattern);
        writeln(msg);
        
        int ret = lowLevel_->get_haptics().play_pattern(demos[i].pattern);
        if (ret < 0) {
            writeln("    (failed)");
            continue;
        }
        
        if (demos[i].needs_stop) {
            // Pattern 118 is "Long buzz for programmatic stopping" - runs continuously
            // Wait for desired duration, then stop it programmatically
            vTaskDelay(pdMS_TO_TICKS(demos[i].delay_after_ms));
            lowLevel_->get_haptics().stop();
            writeln("    (stopped programmatically)");
            vTaskDelay(pdMS_TO_TICKS(200));  // Brief spacing after stop
        } else {
            // Wait for pattern to complete naturally
            vTaskDelay(pdMS_TO_TICKS(demos[i].delay_after_ms));
        }
    }
    
    writeln("Done.");
}

void USBCLI::testFireAlarm() {
    writeln("\r\n=== Fire Alarm Simulator ===");
    writeln("Simulating fire alarm (LED, buzzer, haptics)...");
    writeln("Press 'e' to stop");
    
    // Clear any pending input
    uint8_t dummy;
    while (readByte(&dummy, 10) > 0) {}
    
    // Initialize all features
    Buzzer buzzer(state_, lowLevel_);
    RGBLED rgbLed(state_, lowLevel_);
    
    buzzer.begin();
    rgbLed.begin();
    // Haptics should already be initialized in LowLevel::begin()
    
    if (!buzzer.isReady()) writeln("WARNING: Buzzer not ready");
    if (!rgbLed.isReady()) writeln("WARNING: LED not ready");
    if (!lowLevel_->get_haptics().isReady()) writeln("WARNING: Haptics not ready");
    
    // Start LED effect (double flash red for fire alarm)
    if (rgbLed.isReady()) {
        rgbLed.queueEffect(LedEffect::DOUBLE_FLASH, LedColor::RED, Brightness::B100, 0);
        rgbLed.process();
    }
    
    // Start buzzer in background task on Core 0
    volatile bool taskDone = false;
    BuzzerTaskParams taskParams = { &buzzer, 4, nullptr, &taskDone };  // pattern_type 4 = Code-3 Sweep
    TaskHandle_t buzzerTask = NULL;
    
    if (buzzer.isReady()) {
        xTaskCreatePinnedToCore(
            fireAlarmBuzzerTask,
            "fa_buzzer",
            4096,
            &taskParams,
            5,
            &buzzerTask,
            0  // Core 0
        );
    }
    
    // Start haptic immediately
    uint32_t lastHapticTime = 0;
    const uint32_t hapticInterval = 3000;  // 3 seconds
    
    if (lowLevel_->get_haptics().isReady()) {
        lowLevel_->get_haptics().play_pattern(118);
        lastHapticTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    
    // Main loop: LED updates and haptic timing on main thread
    while (!waitForExitKey()) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Process LED (non-blocking)
        if (rgbLed.isReady()) {
            rgbLed.process();
        }
        
        // Update haptic every 3 seconds
        if (lowLevel_->get_haptics().isReady() && 
            (now - lastHapticTime >= hapticInterval)) {
            lowLevel_->get_haptics().play_pattern(118);
            lastHapticTime = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
    // Stop buzzer and wait for task to finish
    if (buzzer.isReady()) {
        buzzer.requestStop();
        int timeout = 50;
        while (!taskDone && timeout > 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            timeout--;
        }
        buzzer.stop();
    }
    
    if (rgbLed.isReady()) rgbLed.stopEffect();
    
    writeln("\r\nTest stopped.");
}

// Helper function to play a beep asynchronously (runs in a task)
static void playBeepTask(void* params) {
    struct BeepParams {
        BuzzerLowLevelDriver* buzzer;
        uint32_t frequency;
        uint32_t duration_ms;
    };
    BeepParams* p = static_cast<BeepParams*>(params);
    
    if (p->buzzer && p->buzzer->isReady()) {
        p->buzzer->setPWM(p->frequency, 50);  // 50% duty cycle
        vTaskDelay(pdMS_TO_TICKS(p->duration_ms));
        p->buzzer->stop();
    }
    
    delete p;
    vTaskDelete(NULL);
}

void USBCLI::runSideButtonTest() {
    writeln("\r\n=== Side Button Test ===");
    writeln("Testing side button GPIO configuration and events...");
    writeln("Press 'e' to exit");
    
    // Clear any pending input
    uint8_t dummy;
    while (readByte(&dummy, 10) > 0) {}
    
    // Initialize buzzer low-level driver for tactile feedback
    bool buzzer_initialized = false;
    if (lowLevel_ && lowLevel_->get_buzzer().begin()) {
        if (lowLevel_->get_buzzer().isReady()) {
            buzzer_initialized = true;
            writeln("Buzzer initialized for tactile feedback");
        }
    }
    if (!buzzer_initialized) {
        writeln("WARNING: Buzzer initialization failed - tactile feedback disabled");
    }
    
    // Create and initialize SideButtons instance
    SideButtons buttons(state_, lowLevel_, HARDWARE_BUTTON_LEFT_PIN, HARDWARE_BUTTON_RIGHT_PIN);
    
    if (!buttons.begin()) {
        writeln("ERROR: Failed to initialize side buttons");
        writeln("Check GPIO configuration and hardware connections.");
        return;
    }
    
    if (!buttons.isReady()) {
        writeln("ERROR: Side buttons not ready");
        return;
    }
    
    writeln("\r\nSide buttons initialized successfully");
    char config_msg[128];
    snprintf(config_msg, sizeof(config_msg), "Left GPIO: %d, Right GPIO: %d", 
             HARDWARE_BUTTON_LEFT_PIN, HARDWARE_BUTTON_RIGHT_PIN);
    writeln(config_msg);
    snprintf(config_msg, sizeof(config_msg), "Active High: %d, Pullup: %d", 
             HARDWARE_BUTTON_ACTIVE_HIGH, HARDWARE_BUTTON_PULLUP);
    writeln(config_msg);
    writeln("\r\nMonitoring button events...");
    writeln("(Press buttons to see events, 'e' to exit)");
    
    // Track last displayed state to avoid spam
    bool last_left = false;
    bool last_right = false;
    uint32_t last_event_time = 0;
    
    // Set up event callback to log button events and provide tactile feedback
    buttons.setEventCallback([this, buzzer_initialized](ButtonEvent event) {
        const char* event_name = "";
        uint32_t beep_freq = 0;
        uint32_t beep_duration = 0;
        
        switch (event) {
            case ButtonEvent::LEFT_PRESS_DOWN: 
                event_name = "LEFT_PRESS_DOWN"; 
                beep_freq = 1000;  // Higher pitch in left range (down)
                beep_duration = 50;  // Short beep
                break;
            case ButtonEvent::LEFT_PRESS_UP: 
                event_name = "LEFT_PRESS_UP"; 
                beep_freq = 900;  // Lower pitch in left range (up)
                beep_duration = 30;  // Very short beep
                break;
            case ButtonEvent::RIGHT_PRESS_DOWN: 
                event_name = "RIGHT_PRESS_DOWN"; 
                beep_freq = 2700;  // Higher pitch in right range (down)
                beep_duration = 50;  // Short beep
                break;
            case ButtonEvent::RIGHT_PRESS_UP: 
                event_name = "RIGHT_PRESS_UP"; 
                beep_freq = 2400;  // Lower pitch in right range (up)
                beep_duration = 30;  // Very short beep
                break;
            case ButtonEvent::LEFT_PRESS: 
                event_name = "LEFT_PRESS"; 
                break;
            case ButtonEvent::RIGHT_PRESS: 
                event_name = "RIGHT_PRESS"; 
                break;
            case ButtonEvent::LEFT_HOLD: 
                event_name = "LEFT_HOLD"; 
                beep_freq = 1100;  // Distinct tone in left range for hold
                beep_duration = 150;  // Longer beep for hold
                break;
            case ButtonEvent::RIGHT_HOLD: 
                event_name = "RIGHT_HOLD"; 
                beep_freq = 2800;  // Distinct tone in right range for hold
                beep_duration = 150;  // Longer beep for hold
                break;
            case ButtonEvent::BOTH_HOLD_DOWN: 
                event_name = "BOTH_HOLD_DOWN"; 
                beep_freq = 1800;  // Medium tone for both buttons (between left and right ranges)
                beep_duration = 80;
                break;
            case ButtonEvent::BOTH_HOLD: 
                event_name = "BOTH_HOLD"; 
                beep_freq = 2000;  // Distinct tone for both hold (between left and right ranges)
                beep_duration = 200;  // Longer beep for hold threshold
                break;
            case ButtonEvent::BOTH_HOLD_UP: 
                event_name = "BOTH_HOLD_UP"; 
                break;
        }
        char msg[64];
        snprintf(msg, sizeof(msg), "  Event: %s", event_name);
        writeln(msg);
        
        // Play tactile feedback beep if buzzer is initialized
        if (buzzer_initialized && beep_freq > 0 && lowLevel_ && lowLevel_->get_buzzer().isReady()) {
            // Create task to play beep asynchronously (non-blocking)
            struct BeepParams {
                BuzzerLowLevelDriver* buzzer;
                uint32_t frequency;
                uint32_t duration_ms;
            };
            BeepParams* beepParams = new BeepParams();
            beepParams->buzzer = &lowLevel_->get_buzzer();
            beepParams->frequency = beep_freq;
            beepParams->duration_ms = beep_duration;
            
            xTaskCreate(
                playBeepTask,
                "beep_task",
                2048,
                beepParams,
                1,  // Low priority
                nullptr  // Don't need handle
            );
        }
    });
    
    // Main monitoring loop
    while (!waitForExitKey()) {
        bool left_pressed = buttons.isLeftPressed();
        bool right_pressed = buttons.isRightPressed();
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Display state changes (throttle to avoid spam)
        if ((left_pressed != last_left || right_pressed != last_right) && 
            (now - last_event_time > 100)) {
            char state_msg[64];
            snprintf(state_msg, sizeof(state_msg), "  State: Left=%s, Right=%s", 
                     left_pressed ? "PRESSED" : "released",
                     right_pressed ? "PRESSED" : "released");
            writeln(state_msg);
            last_left = left_pressed;
            last_right = right_pressed;
            last_event_time = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Stop buzzer before exiting
    if (buzzer_initialized && lowLevel_ && lowLevel_->get_buzzer().isReady()) {
        lowLevel_->get_buzzer().stop();
    }
    
    writeln("\r\nTest stopped.");
}

void USBCLI::runNVSTest() {
    writeln("\r\n=== NVS Storage Test ===");
    writeln("Testing NVS read/write/erase operations...");
    
    if (!lowLevel_ || !lowLevel_->get_nvs().isReady()) {
        writeln("ERROR: NVS driver not available");
        writeln("Press 'e' to exit");
        while (!waitForExitKey()) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        return;
    }
    
    auto& nvs = lowLevel_->get_nvs();
    const char* namespace_name = "ilss_state";
    const char* test_string_key = "test_string_key";
    const char* test_int_key = "test_int32_key";
    
    // Open namespace for read-write operations
    nvs_handle_t handle = nvs.openNamespace(namespace_name, false);
    if (handle == 0) {
        writeln("ERROR: Failed to open NVS namespace");
        writeln("Press 'e' to exit");
        while (!waitForExitKey()) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        return;
    }
    
    writeln("\r\n--- Step 1: Reading all existing state ---");
    
    // List of all known keys from State
    struct KeyInfo {
        const char* key;
        const char* type;
        const char* description;
    };
    
    KeyInfo known_keys[] = {
        // Strings
        {"wifi_ssid", "string", "WiFi SSID"},
        {"wifi_pass", "string", "WiFi Password"},
        {"session_id", "string", "Session ID"},
        {"persona", "string", "Persona"},
        {"user_fname", "string", "User First Name"},
        {"user_lname", "string", "User Last Name"},
        // uint8_t
        {"wifi_band", "uint8", "WiFi Band"},
        {"battery_lvl", "uint8", "Battery Level"},
        {"battery_chg", "uint8", "Battery Charging Status"},
        // bool (stored as uint8)
        {"is_prov", "bool", "Is Provisioned"},
        {"fast_scan", "bool", "Fast Scan Mode"},
        {"en_pa_btn", "bool", "Enable Personal Alert Buttons"},
        {"en_haptics", "bool", "Enable Haptics"},
        {"en_buzzer", "bool", "Enable Buzzer"},
        {"en_led", "bool", "Enable LED"},
        {"en_inact", "bool", "Enable Inactivity Alerts"},
        {"en_nfc", "bool", "Enable NFC"},
        {"en_hw_beacon", "bool", "Enable Honeywell Beacon"},
        {"fire_mode", "bool", "Fire Event Mode"},
        {"pa_mode", "bool", "Personal Alert Mode"},
        // int32
        {"q_scan_int", "int32", "Quiescent Scan Interval"},
        {"f_scan_int", "int32", "Fast Scan Interval"},
        {"pa_btn_dly", "int32", "Personal Alert Button Delay"},
    };
    
    int found_count = 0;
    for (size_t i = 0; i < sizeof(known_keys) / sizeof(known_keys[0]); i++) {
        char msg[128];
        if (strcmp(known_keys[i].type, "string") == 0) {
            std::string str_val;
            if (nvs.getString(handle, known_keys[i].key, str_val)) {
                // Mask sensitive values for security
                const char* display_val = str_val.c_str();
                if (strcmp(known_keys[i].key, "wifi_pass") == 0) {
                    display_val = "***";
                }
                snprintf(msg, sizeof(msg), "  [%s] %s = \"%s\"", known_keys[i].type, known_keys[i].key, display_val);
                writeln(msg);
                found_count++;
            }
        } else if (strcmp(known_keys[i].type, "bool") == 0) {
            bool bool_val;
            if (nvs.getBool(handle, known_keys[i].key, bool_val)) {
                snprintf(msg, sizeof(msg), "  [%s] %s = %s", known_keys[i].type, known_keys[i].key, bool_val ? "true" : "false");
                writeln(msg);
                found_count++;
            }
        } else if (strcmp(known_keys[i].type, "uint8") == 0) {
            uint8_t u8_val;
            if (nvs.getUInt8(handle, known_keys[i].key, u8_val)) {
                snprintf(msg, sizeof(msg), "  [%s] %s = %u", known_keys[i].type, known_keys[i].key, u8_val);
                writeln(msg);
                found_count++;
            }
        } else if (strcmp(known_keys[i].type, "int32") == 0) {
            int32_t i32_val;
            if (nvs.getInt32(handle, known_keys[i].key, i32_val)) {
                snprintf(msg, sizeof(msg), "  [%s] %s = %ld", known_keys[i].type, known_keys[i].key, i32_val);
                writeln(msg);
                found_count++;
            }
        }
    }
    
    char summary[64];
    snprintf(summary, sizeof(summary), "\r\nFound %d existing keys in namespace '%s'", found_count, namespace_name);
    writeln(summary);
    
    writeln("\r\n--- Step 2: Writing test values ---");
    
    // Write test string
    std::string test_string_value = "NVS_TEST_VALUE_12345";
    if (nvs.setString(handle, test_string_key, test_string_value)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "  Written: %s = \"%s\"", test_string_key, test_string_value.c_str());
        writeln(msg);
    } else {
        writeln("  ERROR: Failed to write test string");
    }
    
    // Write test int32
    int32_t test_int_value = 99999;
    if (nvs.setInt32(handle, test_int_key, test_int_value)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "  Written: %s = %ld", test_int_key, test_int_value);
        writeln(msg);
    } else {
        writeln("  ERROR: Failed to write test int32");
    }
    
    // Commit changes
    if (nvs.commit(handle)) {
        writeln("  Committed changes to NVS");
    } else {
        writeln("  ERROR: Failed to commit changes");
    }
    
    writeln("\r\n--- Step 3: Reading test values back ---");
    
    // Read test string back
    std::string read_string;
    if (nvs.getString(handle, test_string_key, read_string)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "  Read: %s = \"%s\"", test_string_key, read_string.c_str());
        writeln(msg);
        if (read_string == test_string_value) {
            writeln("  ✓ String value matches!");
        } else {
            writeln("  ✗ String value mismatch!");
        }
    } else {
        writeln("  ERROR: Failed to read test string");
    }
    
    // Read test int32 back
    int32_t read_int;
    if (nvs.getInt32(handle, test_int_key, read_int)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "  Read: %s = %ld", test_int_key, read_int);
        writeln(msg);
        if (read_int == test_int_value) {
            writeln("  ✓ Int32 value matches!");
        } else {
            writeln("  ✗ Int32 value mismatch!");
        }
    } else {
        writeln("  ERROR: Failed to read test int32");
    }
    
    writeln("\r\n--- Step 4: Deleting test values ---");
    
    // Delete test string
    if (nvs.eraseKey(handle, test_string_key)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "  Deleted: %s", test_string_key);
        writeln(msg);
    } else {
        writeln("  ERROR: Failed to delete test string");
    }
    
    // Delete test int32
    if (nvs.eraseKey(handle, test_int_key)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "  Deleted: %s", test_int_key);
        writeln(msg);
    } else {
        writeln("  ERROR: Failed to delete test int32");
    }
    
    // Commit deletions
    if (nvs.commit(handle)) {
        writeln("  Committed deletions to NVS");
    } else {
        writeln("  ERROR: Failed to commit deletions");
    }
    
    writeln("\r\n--- Step 5: Verifying deletion ---");
    
    // Verify test string is deleted
    std::string verify_string;
    if (nvs.getString(handle, test_string_key, verify_string)) {
        writeln("  ✗ ERROR: Test string still exists after deletion!");
    } else {
        writeln("  ✓ Test string successfully deleted");
    }
    
    // Verify test int32 is deleted
    int32_t verify_int;
    if (nvs.getInt32(handle, test_int_key, verify_int)) {
        writeln("  ✗ ERROR: Test int32 still exists after deletion!");
    } else {
        writeln("  ✓ Test int32 successfully deleted");
    }
    
    // Close namespace
    nvs.closeNamespace(handle);
    
    writeln("\r\n--- Test Complete ---");
    writeln("All NVS operations completed successfully.");
    writeln("Press 'e' to exit");
    
    // Wait for exit key (test must complete before allowing exit)
    while (!waitForExitKey()) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    writeln("\r\nReturning to test menu...");
}

void USBCLI::runFactoryReset() {
    writeln("\r\n=== Factory Reset ===");
    writeln("WARNING: This will erase ALL NVS data and reboot the device!");
    writeln("This action cannot be undone.");
    writeln("\r\nAre you sure you want to continue? (yes/no): ");
    
    std::string confirmation = readline();
    if (confirmation != "yes" && confirmation != "YES" && confirmation != "y" && confirmation != "Y") {
        writeln("Factory reset cancelled.");
        return;
    }
    
    writeln("\r\nPerforming factory reset...");
    writeln("Erasing NVS...");
    
    // Erase all NVS data
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        char msg[128];
        snprintf(msg, sizeof(msg), "ERROR: Failed to erase NVS: %s", esp_err_to_name(err));
        writeln(msg);
        return;
    }
    
    writeln("NVS erased successfully.");
    writeln("Playing reset sequence...");
    
    // Initialize haptics for ramp up effect
    bool haptics_ok = false;
    if (lowLevel_ && lowLevel_->get_haptics().begin()) {
        if (lowLevel_->get_haptics().isReady()) {
            haptics_ok = true;
        }
    }
    
    // Initialize buzzer
    bool buzzer_ok = false;
    if (lowLevel_ && lowLevel_->get_buzzer().begin()) {
        if (lowLevel_->get_buzzer().isReady()) {
            buzzer_ok = true;
        }
    }
    
    // Initialize LED
    RGBLED rgbLed(state_, lowLevel_);
    rgbLed.begin();
    
    // Play ramp up haptic (0 to 127 over ~500ms)
    if (haptics_ok) {
        for (uint8_t i = 0; i <= 127; i += 8) {
            lowLevel_->get_haptics().set_realtime_value(i);
            vTaskDelay(pdMS_TO_TICKS(20)); // ~20ms per step = ~500ms total
        }
        // Hold at max for a moment
        vTaskDelay(pdMS_TO_TICKS(100));
        lowLevel_->get_haptics().stop();
        
        // Play haptic pattern 92
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay before pattern
        lowLevel_->get_haptics().play_pattern(92);
        vTaskDelay(pdMS_TO_TICKS(500)); // Let pattern complete
    }
    
    // Play buzzer beep (2000Hz, 200ms)
    if (buzzer_ok) {
        lowLevel_->get_buzzer().setPWM(2000, 50); // 2000Hz, 50% duty
        vTaskDelay(pdMS_TO_TICKS(200));
        lowLevel_->get_buzzer().stop();
    }
    
    // Flash green LED (double flash)
    if (rgbLed.isReady()) {
        rgbLed.queueEffect(LedEffect::DOUBLE_FLASH, LedColor::GREEN, Brightness::B100, 1000);
        rgbLed.process();
        vTaskDelay(pdMS_TO_TICKS(500)); // Let the flash complete
    }
    
    writeln("\r\nRebooting...");
    vTaskDelay(pdMS_TO_TICKS(500)); // Small delay before reboot
    
    // Reboot the system
    esp_restart();
}

void USBCLI::runWiFiTestMenu() {
    while (true) {
        writeln("\r\n=== WiFi Test Sub-Menu ===");
        writeln("1 = Scan networks");
        writeln("2 = Connect to SSID");
        writeln("3 = Get WiFi connection status");
        writeln("4 = Disconnect");
        writeln("5 = Test GET request to httpbin");
        writeln("e = Exit");
        writeln("\r\nChoice: ");
        
        std::string choice = readline();
        
        if (choice == "e" || choice == "E") {
            writeln("Returning to test menu...");
            return;
        } else if (choice == "1") {
            testWiFiScan();
        } else if (choice == "2") {
            testWiFiConnect();
        } else if (choice == "3") {
            testWiFiStatus();
        } else if (choice == "4") {
            testWiFiDisconnect();
        } else if (choice == "5") {
            testWiFiHttpGet();
        } else {
            writeln("Invalid choice.");
        }
    }
}

void USBCLI::testWiFiScan() {
    writeln("\r\n=== WiFi Network Scan ===");
    
    // Initialize WiFi if not already initialized
    if (!lowLevel_ || !lowLevel_->get_wifi().isReady()) {
        writeln("Initializing WiFi...");
        if (!lowLevel_->get_wifi().begin()) {
            writeln("ERROR: Failed to initialize WiFi");
            // Show orange LED for error
            RGBLED rgbLed(state_, lowLevel_);
            if (rgbLed.begin() && rgbLed.isReady()) {
                rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::ORANGE, Brightness::B100, 2000);
                rgbLed.processWait(2000);
                rgbLed.stopEffect();
            }
            return;
        }
    }
    
    // Check if already connected
    if (lowLevel_->get_wifi().isConnected()) {
        writeln("WARNING: Already connected to WiFi. Disconnecting first...");
        lowLevel_->get_wifi().disconnect();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Initialize LED for scanning feedback
    RGBLED rgbLed(state_, lowLevel_);
    bool led_ready = false;
    if (rgbLed.begin()) {
        led_ready = rgbLed.isReady();
    }
    
    writeln("Scanning for WiFi networks...");
    writeln("(This may take 10-15 seconds)");
    
    // Start chasing blue LEDs
    if (led_ready) {
        rgbLed.queueEffect(LedEffect::CHASE_FADE, LedColor::BLUE, Brightness::B100, 0);
    }
    
    // Perform scan (blocking, but we'll process LED in parallel)
    // Since scan() is blocking, we need to run it in a task
    std::vector<WiFiNetwork> networks;
    volatile bool scan_complete = false;
    
    struct ScanParams {
        WiFiLowLevelDriver* wifi;
        std::vector<WiFiNetwork>* result;
        volatile bool* complete;
    } scanParams = { &lowLevel_->get_wifi(), &networks, &scan_complete };
    
    TaskHandle_t scanTaskHandle = NULL;
    xTaskCreate(
        [](void* param) {
            ScanParams* p = static_cast<ScanParams*>(param);
            *(p->result) = p->wifi->scan();
            *(p->complete) = true;
            vTaskDelete(NULL);
        },
        "wifi_scan",
        8192,
        &scanParams,
        5,
        &scanTaskHandle
    );
    
    // Process LED while scanning
    while (!scan_complete) {
        if (led_ready) {
            rgbLed.process();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Stop LED effect
    if (led_ready) {
        rgbLed.stopEffect();
    }
    
    // Display results
    writeln("\r\n--- Scan Results ---");
    if (networks.empty()) {
        writeln("No networks found.");
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "Found %zu networks:\r\n", networks.size());
        writeln(msg);
        writeln("MAC Address          SSID                          Channel");
        writeln("------------------------------------------------------------");
        
        for (const auto& network : networks) {
            char mac_str[18];
            snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                     network.bssid[0], network.bssid[1], network.bssid[2],
                     network.bssid[3], network.bssid[4], network.bssid[5]);
            
            snprintf(msg, sizeof(msg), "%-18s %-30s %d",
                     mac_str, network.ssid.c_str(), network.channel);
            writeln(msg);
        }
    }
    
    writeln("\r\nPress any key to continue...");
    uint8_t dummy;
    while (readByte(&dummy, 10) <= 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void USBCLI::testWiFiConnect() {
    writeln("\r\n=== Connect to WiFi Network ===");
    
    // Initialize WiFi if not already initialized
    if (!lowLevel_ || !lowLevel_->get_wifi().isReady()) {
        writeln("Initializing WiFi...");
        if (!lowLevel_->get_wifi().begin()) {
            writeln("ERROR: Failed to initialize WiFi");
            // Show orange LED for error
            RGBLED rgbLed(state_, lowLevel_);
            if (rgbLed.begin() && rgbLed.isReady()) {
                rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::ORANGE, Brightness::B100, 2000);
                rgbLed.processWait(2000);
                rgbLed.stopEffect();
            }
            return;
        }
    }
    
    // Disconnect if already connected
    if (lowLevel_->get_wifi().isConnected()) {
        writeln("Already connected. Disconnecting first...");
        lowLevel_->get_wifi().disconnect();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // Get SSID
    writeln("Enter SSID: ");
    std::string ssid = readline();
    if (ssid.empty()) {
        writeln("ERROR: SSID cannot be empty");
        return;
    }
    
    // Get password
    writeln("Enter password (leave empty for open networks): ");
    std::string password = readline();
    
    // Get channel (optional, default to 0 for auto)
    // Channel is an unsigned 8-bit integer (0-255)
    writeln("Enter channel (0 for auto, 1-255 for specific channel): ");
    std::string channel_str = readline();
    uint8_t channel = 0;
    if (!channel_str.empty()) {
        char* endptr = nullptr;
        unsigned long channel_ulong = std::strtoul(channel_str.c_str(), &endptr, 10);
        if (endptr != nullptr && *endptr == '\0' && channel_ulong <= 255) {
            channel = static_cast<uint8_t>(channel_ulong);
        } else {
            writeln("WARNING: Invalid channel input (must be 0-255), using auto (0)");
            channel = 0;
        }
    }
    
    // Determine band from channel (1-14 = 2.4GHz, 36+ = 5GHz)
    int band = 0; // Auto
    if (channel >= 1 && channel <= 14) {
        band = 1; // 2.4GHz
    } else if (channel >= 36) {
        band = 2; // 5GHz
    }
    
    // Initialize LED
    RGBLED rgbLed(state_, lowLevel_);
    bool led_ready = false;
    if (rgbLed.begin()) {
        led_ready = rgbLed.isReady();
    }
    
    writeln("\r\nConnecting to WiFi...");
    char msg[128];
    snprintf(msg, sizeof(msg), "SSID: %s, Channel: %d, Band: %s",
             ssid.c_str(), channel, (band == 1 ? "2.4GHz" : (band == 2 ? "5GHz" : "Auto")));
    writeln(msg);
    
    // Attempt connection
    bool connect_success = lowLevel_->get_wifi().connect(ssid, password, band);
    
    if (!connect_success) {
        writeln("ERROR: Failed to initiate connection");
        if (led_ready) {
            rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::ORANGE, Brightness::B100, 2000);
            rgbLed.processWait(2000);
            rgbLed.stopEffect();
        }
        return;
    }
    
    // Wait for connection with LED feedback (connecting = blue pulse)
    if (led_ready) {
        rgbLed.queueEffect(LedEffect::PULSE, LedColor::BLUE, Brightness::B100, 0);
    }
    
    bool connected = lowLevel_->get_wifi().waitForConnection(30000); // 30 second timeout
    
    if (connected) {
        writeln("SUCCESS: Connected to WiFi!");
        std::string ip = lowLevel_->get_wifi().getIPAddress();
        snprintf(msg, sizeof(msg), "IP Address: %s", ip.c_str());
        writeln(msg);
        
        // Show green LED for success
        if (led_ready) {
            rgbLed.stopEffect();
            rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::GREEN, Brightness::B100, 2000);
            rgbLed.processWait(2000);
            rgbLed.stopEffect();
        }
    } else {
        writeln("ERROR: Connection failed or timed out");
        // Show orange LED for error
        if (led_ready) {
            rgbLed.stopEffect();
            rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::ORANGE, Brightness::B100, 2000);
            rgbLed.processWait(2000);
            rgbLed.stopEffect();
        }
    }
    
    writeln("\r\nPress any key to continue...");
    uint8_t dummy;
    while (readByte(&dummy, 10) <= 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void USBCLI::testWiFiStatus() {
    writeln("\r\n=== WiFi Connection Status ===");
    
    if (!lowLevel_ || !lowLevel_->get_wifi().isReady()) {
        writeln("WiFi not initialized");
        return;
    }
    
    auto status = lowLevel_->get_wifi().getStatus();
    bool connected = lowLevel_->get_wifi().isConnected();
    
    char msg[128];
    
    writeln("--- Status ---");
    switch (status) {
        case WiFiLowLevelDriver::ConnectionStatus::Disconnected:
            writeln("Status: Disconnected");
            break;
        case WiFiLowLevelDriver::ConnectionStatus::Connecting:
            writeln("Status: Connecting");
            break;
        case WiFiLowLevelDriver::ConnectionStatus::Connected:
            writeln("Status: Connected");
            break;
        case WiFiLowLevelDriver::ConnectionStatus::Failed:
            writeln("Status: Failed");
            break;
    }
    
    if (connected) {
        std::string ssid = lowLevel_->get_wifi().getCurrentSSID();
        std::string ip = lowLevel_->get_wifi().getIPAddress();
        snprintf(msg, sizeof(msg), "SSID: %s", ssid.c_str());
        writeln(msg);
        snprintf(msg, sizeof(msg), "IP Address: %s", ip.c_str());
        writeln(msg);
        
        // Show green LED briefly
        RGBLED rgbLed(state_, lowLevel_);
        if (rgbLed.begin() && rgbLed.isReady()) {
            rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::GREEN, Brightness::B100, 1000);
            rgbLed.processWait(1000);
            rgbLed.stopEffect();
        }
    } else {
        // Show red LED briefly
        RGBLED rgbLed(state_, lowLevel_);
        if (rgbLed.begin() && rgbLed.isReady()) {
            rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::RED, Brightness::B100, 1000);
            rgbLed.processWait(1000);
            rgbLed.stopEffect();
        }
    }
    
    writeln("\r\nPress any key to continue...");
    uint8_t dummy;
    while (readByte(&dummy, 10) <= 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void USBCLI::testWiFiDisconnect() {
    writeln("\r\n=== Disconnect from WiFi ===");
    
    if (!lowLevel_ || !lowLevel_->get_wifi().isReady()) {
        writeln("WiFi not initialized");
        return;
    }
    
    if (!lowLevel_->get_wifi().isConnected()) {
        writeln("Not currently connected to WiFi");
        return;
    }
    
    std::string ssid = lowLevel_->get_wifi().getCurrentSSID();
    char msg[128];
    snprintf(msg, sizeof(msg), "Disconnecting from: %s", ssid.c_str());
    writeln(msg);
    
    bool success = lowLevel_->get_wifi().disconnect();
    
    if (success) {
        writeln("SUCCESS: Disconnected from WiFi");
        
        // Show red LED for disconnected
        RGBLED rgbLed(state_, lowLevel_);
        if (rgbLed.begin() && rgbLed.isReady()) {
            rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::RED, Brightness::B100, 2000);
            rgbLed.processWait(2000);
            rgbLed.stopEffect();
        }
    } else {
        writeln("ERROR: Failed to disconnect");
        
        // Show orange LED for error
        RGBLED rgbLed(state_, lowLevel_);
        if (rgbLed.begin() && rgbLed.isReady()) {
            rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::ORANGE, Brightness::B100, 2000);
            rgbLed.processWait(2000);
            rgbLed.stopEffect();
        }
    }
    
    writeln("\r\nPress any key to continue...");
    uint8_t dummy;
    while (readByte(&dummy, 10) <= 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void USBCLI::testWiFiHttpGet() {
    writeln("\r\n=== Test GET Request to httpbin ===");
    
    if (!lowLevel_ || !lowLevel_->get_wifi().isReady()) {
        writeln("WiFi not initialized");
        return;
    }
    
    if (!lowLevel_->get_wifi().isConnected()) {
        writeln("ERROR: Not connected to WiFi. Please connect first.");
        // Show orange LED for error
        RGBLED rgbLed(state_, lowLevel_);
        if (rgbLed.begin() && rgbLed.isReady()) {
            rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::ORANGE, Brightness::B100, 2000);
            rgbLed.processWait(2000);
            rgbLed.stopEffect();
        }
        return;
    }
    
    writeln("Sending GET request to https://httpbin.org/get...");
    writeln("(Note: Certificate bundle is loaded - TLS verification enabled)");
    
    // Perform HTTP GET request
    std::string response = lowLevel_->get_wifi().httpGet("https://httpbin.org/get");
    
    // Initialize LED
    RGBLED rgbLed(state_, lowLevel_);
    bool led_ready = false;
    if (rgbLed.begin()) {
        led_ready = rgbLed.isReady();
    }
    
    // Check if request completed (response may be empty or contain error message)
    if (response.empty()) {
        writeln("WARNING: HTTP GET request returned empty response");
        writeln("(This may indicate a connection error or server issue)");
        // Show orange LED for error
        if (led_ready) {
            rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::ORANGE, Brightness::B100, 2000);
            rgbLed.processWait(2000);
            rgbLed.stopEffect();
        }
    } else {
        writeln("HTTP GET request completed");
        char msg[128];
        snprintf(msg, sizeof(msg), "Response length: %zu bytes", response.size());
        writeln(msg);
        
        // Check if response indicates an error (common HTTP error patterns)
        bool is_error = false;
        std::string response_lower = response;
        std::transform(response_lower.begin(), response_lower.end(), response_lower.begin(), ::tolower);
        
        if (response_lower.find("<title>") != std::string::npos) {
            // HTML error page detected
            if (response_lower.find("503") != std::string::npos || 
                response_lower.find("500") != std::string::npos ||
                response_lower.find("404") != std::string::npos ||
                response_lower.find("403") != std::string::npos ||
                response_lower.find("401") != std::string::npos ||
                response_lower.find("400") != std::string::npos ||
                response_lower.find("service unavailable") != std::string::npos ||
                response_lower.find("not found") != std::string::npos ||
                response_lower.find("forbidden") != std::string::npos ||
                response_lower.find("unauthorized") != std::string::npos ||
                response_lower.find("bad request") != std::string::npos) {
                is_error = true;
            }
        }
        
        writeln("\r\n--- Response (first 500 chars) ---");
        
        // Display first 500 characters of response
        size_t display_len = response.size() > 500 ? 500 : response.size();
        std::string display = response.substr(0, display_len);
        writeln(display.c_str());
        if (response.size() > 500) {
            writeln("... (truncated)");
        }
        
        // Show appropriate LED based on response
        if (led_ready) {
            if (is_error) {
                writeln("\r\nNOTE: Server returned an error response (e.g., 503 Service Unavailable)");
                writeln("This indicates httpbin.org is temporarily unavailable, not a code issue.");
                // Show orange LED for server error
                rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::ORANGE, Brightness::B100, 2000);
                rgbLed.processWait(2000);
                rgbLed.stopEffect();
            } else {
                // Show green LED for success
                rgbLed.queueEffect(LedEffect::CONTINUOUS, LedColor::GREEN, Brightness::B100, 2000);
                rgbLed.processWait(2000);
                rgbLed.stopEffect();
            }
        }
    }
    
    writeln("\r\nPress any key to continue...");
    uint8_t dummy;
    while (readByte(&dummy, 10) <= 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void USBCLI::runLEDPatternTest() {
    writeln("\r\n=== LED Pattern Test ===");
    writeln("Choose an LED effect:");
    writeln("0  = OFF");
    writeln("1  = PULSE (smooth sine wave, 2s cycle)");
    writeln("2  = RAPID_PULSE (rapid sine wave, 500ms cycle)");
    writeln("3  = BLINK_ALTERNATE (on/off toggle, 1s cycle)");
    writeln("4  = FLASH_1S (quick flash every 1s)");
    writeln("5  = FLASH_2S (quick flash every 2s)");
    writeln("6  = CHASE_FADE (chasing LED with fade trail)");
    writeln("7  = CONTINUOUS (solid color)");
    writeln("8  = DOUBLE_FLASH (double flash pattern)");
    writeln("9  = WATER_DROP (water drop effect)");
    writeln("\r\nEffect choice (0-9): ");
    
    std::string effect_str = readline();
    int effect_num = -1;
    if (!effect_str.empty()) {
        char* endptr = nullptr;
        long effect_long = std::strtol(effect_str.c_str(), &endptr, 10);
        if (endptr != nullptr && *endptr == '\0' && effect_long >= 0 && effect_long <= 9) {
            effect_num = static_cast<int>(effect_long);
        }
    }
    
    if (effect_num < 0 || effect_num > 9) {
        writeln("Invalid effect choice.");
        return;
    }
    
    writeln("\r\nChoose a color:");
    writeln("0 = RED");
    writeln("1 = GREEN");
    writeln("2 = BLUE");
    writeln("3 = PURPLE");
    writeln("4 = YELLOW");
    writeln("5 = CYAN");
    writeln("6 = WHITE");
    writeln("7 = ORANGE");
    writeln("\r\nColor choice (0-7): ");
    
    std::string color_str = readline();
    int color_num = -1;
    if (!color_str.empty()) {
        char* endptr = nullptr;
        long color_long = std::strtol(color_str.c_str(), &endptr, 10);
        if (endptr != nullptr && *endptr == '\0' && color_long >= 0 && color_long <= 7) {
            color_num = static_cast<int>(color_long);
        }
    }
    
    if (color_num < 0 || color_num > 7) {
        writeln("Invalid color choice.");
        return;
    }
    
    // Map effect number to LedEffect enum
    LedEffect effect = LedEffect::OFF;
    const char* effect_name = "OFF";
    switch (effect_num) {
        case 0: effect = LedEffect::OFF; effect_name = "OFF"; break;
        case 1: effect = LedEffect::PULSE; effect_name = "PULSE"; break;
        case 2: effect = LedEffect::RAPID_PULSE; effect_name = "RAPID_PULSE"; break;
        case 3: effect = LedEffect::BLINK_ALTERNATE; effect_name = "BLINK_ALTERNATE"; break;
        case 4: effect = LedEffect::FLASH_1S; effect_name = "FLASH_1S"; break;
        case 5: effect = LedEffect::FLASH_2S; effect_name = "FLASH_2S"; break;
        case 6: effect = LedEffect::CHASE_FADE; effect_name = "CHASE_FADE"; break;
        case 7: effect = LedEffect::CONTINUOUS; effect_name = "CONTINUOUS"; break;
        case 8: effect = LedEffect::DOUBLE_FLASH; effect_name = "DOUBLE_FLASH"; break;
        case 9: effect = LedEffect::WATER_DROP; effect_name = "WATER_DROP"; break;
    }
    
    // Map color number to LedColor enum
    LedColor color = LedColor::RED;
    const char* color_name = "RED";
    switch (color_num) {
        case 0: color = LedColor::RED; color_name = "RED"; break;
        case 1: color = LedColor::GREEN; color_name = "GREEN"; break;
        case 2: color = LedColor::BLUE; color_name = "BLUE"; break;
        case 3: color = LedColor::PURPLE; color_name = "PURPLE"; break;
        case 4: color = LedColor::YELLOW; color_name = "YELLOW"; break;
        case 5: color = LedColor::CYAN; color_name = "CYAN"; break;
        case 6: color = LedColor::WHITE; color_name = "WHITE"; break;
        case 7: color = LedColor::ORANGE; color_name = "ORANGE"; break;
    }
    
    // Initialize LED
    RGBLED rgbLed(state_, lowLevel_);
    if (!rgbLed.begin()) {
        writeln("ERROR: Failed to initialize LED");
        return;
    }
    
    if (!rgbLed.isReady()) {
        writeln("ERROR: LED not ready");
        return;
    }
    
    char msg[128];
    snprintf(msg, sizeof(msg), "\r\nPlaying: %s effect with %s color", effect_name, color_name);
    writeln(msg);
    writeln("Press 'e' to stop");
    
    // Clear any pending input
    uint8_t dummy;
    while (readByte(&dummy, 10) > 0) {}
    
    // Queue the effect (0 duration = infinite until stopped)
    rgbLed.queueEffect(effect, color, Brightness::B100, 0);
    
    // Process LED until exit key is pressed
    while (!waitForExitKey()) {
        rgbLed.process();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
    rgbLed.stopEffect();
    writeln("\r\nTest stopped.");
}

void USBCLI::testErrorMode() {
    writeln("\r\n=== Error Mode Test ===");
    writeln("Simulating Error Mode behavior:");
    writeln("- Red LED: FLASH_2S (continuous flashing every 2s)");
    writeln("- Buzzer: Low-pitch beep (600Hz, 200ms) every 2 seconds");
    writeln("Press 'e' to stop");
    
    // Clear any pending input
    uint8_t dummy;
    while (readByte(&dummy, 10) > 0) {}
    
    // Initialize features
    RGBLED rgbLed(state_, lowLevel_);
    Buzzer buzzer(state_, lowLevel_);
    
    rgbLed.begin();
    buzzer.begin();
    
    if (!rgbLed.isReady()) writeln("WARNING: LED not ready");
    if (!buzzer.isReady()) writeln("WARNING: Buzzer not ready");
    
    // Start red LED flash effect (FLASH_2S)
    if (rgbLed.isReady()) {
        rgbLed.queueEffect(LedEffect::FLASH_2S, LedColor::RED, Brightness::B100, 0);
        rgbLed.process();
    }
    
    // Start buzzer beep pattern (low pitch, 600Hz, 200ms every 2 seconds)
    uint32_t lastBeepTime = 0;
    const uint32_t beepInterval = 2000;  // 2 seconds
    
    if (buzzer.isReady()) {
        buzzer.beep(600, 200);  // Initial beep
        lastBeepTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    
    // Main loop: LED updates and buzzer timing
    while (!waitForExitKey()) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Process LED (non-blocking)
        if (rgbLed.isReady()) {
            rgbLed.process();
        }
        
        // Update buzzer every 2 seconds
        if (buzzer.isReady() && (now - lastBeepTime >= beepInterval)) {
            buzzer.beep(600, 200);  // Low pitch beep
            lastBeepTime = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Stop buzzer
    if (buzzer.isReady()) {
        buzzer.stop();
    }
    
    // Stop LED
    if (rgbLed.isReady()) {
        rgbLed.stopEffect();
    }
    
    writeln("\r\nTest stopped.");
}

void USBCLI::runUSBProtocolTest() {
    while (true) {
        writeln("\r\n=== USB Provisioning Protocol Test ===");
        writeln("Quick commands:");
        writeln("1  = Get device info (cmd 1)");
        writeln("3  = Scan WiFi networks (cmd 3)");
        writeln("4  = Connect to WiFi (cmd 4)");
        writeln("5  = Get WiFi status (cmd 5)");
        writeln("6  = Disconnect WiFi (cmd 6)");
        writeln("7  = Set session ID (cmd 7)");
        writeln("11 = Get battery status (cmd 11)");
        writeln("c  = Send custom JSON command");
        writeln("e  = Exit");
        writeln("\r\nChoice: ");
        
        std::string choice = readline();
        
        if (choice == "e" || choice == "E") {
            writeln("Returning to test menu...");
            return;
        }
        
        std::string json_command;
        std::string command_name;
        
        // Build JSON command based on choice
        if (choice == "1") {
            json_command = "{\"cmd\":1}";
            command_name = "GET_INFO";
        } else if (choice == "3") {
            json_command = "{\"cmd\":3}";
            command_name = "SCAN_WIFI_NETWORKS";
        } else if (choice == "4") {
            writeln("\r\nEnter SSID: ");
            std::string ssid = readline();
            writeln("Enter password (leave empty for open networks): ");
            std::string password = readline();
            writeln("Enter band (0=auto, 1=2.4GHz, 2=5GHz) [default: 0]: ");
            std::string band_str = readline();
            int band = 0;
            if (!band_str.empty()) {
                char* endptr = nullptr;
                long band_long = std::strtol(band_str.c_str(), &endptr, 10);
                if (endptr != nullptr && *endptr == '\0' && band_long >= 0 && band_long <= 2) {
                    band = static_cast<int>(band_long);
                }
            }
            
            char cmd_buf[256];
            snprintf(cmd_buf, sizeof(cmd_buf), "{\"cmd\":4,\"data\":{\"ssid\":\"%s\",\"password\":\"%s\",\"band\":%d}}",
                     ssid.c_str(), password.c_str(), band);
            json_command = cmd_buf;
            command_name = "CONNECT_WIFI";
        } else if (choice == "5") {
            json_command = "{\"cmd\":5}";
            command_name = "GET_WIFI_STATUS";
        } else if (choice == "6") {
            json_command = "{\"cmd\":6}";
            command_name = "DISCONNECT_WIFI";
        } else if (choice == "7") {
            writeln("\r\nEnter session ID: ");
            std::string session_id = readline();
            char cmd_buf[256];
            snprintf(cmd_buf, sizeof(cmd_buf), "{\"cmd\":7,\"data\":\"%s\"}", session_id.c_str());
            json_command = cmd_buf;
            command_name = "SET_SESSION_ID";
        } else if (choice == "11") {
            json_command = "{\"cmd\":11}";
            command_name = "GET_BATTERY_STATUS";
        } else if (choice == "c" || choice == "C") {
            writeln("\r\nEnter custom JSON command: ");
            json_command = readline();
            command_name = "CUSTOM";
        } else {
            writeln("Invalid choice.");
            continue;
        }
        
        // Send command and get response
        writeln("\r\n--- Sending Command ---");
        writeln(json_command.c_str());
        
        USBProtocol protocol(logger_, lowLevel_, state_);
        std::string response = protocol.processCommand(json_command);
        
        writeln("\r\n--- Response ---");
        writeln(response.c_str());
        
        // Try to parse and display response nicely
        JsonParser parser(response);
        if (parser.valid()) {
            bool success = parser.getBool("success", false);
            int cmd_id = parser.getInt("cmd", -1);
            
            char msg[128];
            snprintf(msg, sizeof(msg), "\r\nCommand: %d, Success: %s", cmd_id, success ? "true" : "false");
            writeln(msg);
            
            if (!success) {
                std::string error = parser.getString("error_message", "");
                if (!error.empty()) {
                    char err_msg[256];
                    snprintf(err_msg, sizeof(err_msg), "Error: %s", error.c_str());
                    writeln(err_msg);
                }
            }
            
            // Check if data field exists
            cJSON* data_item = parser.get("data");
            if (data_item) {
                char* data_str = cJSON_PrintUnformatted(data_item);
                if (data_str) {
                    char data_msg[512];
                    snprintf(data_msg, sizeof(data_msg), "Data: %s", data_str);
                    writeln(data_msg);
                    free(data_str);
                }
            } else {
                writeln("Data: (none)");
            }
        } else {
            writeln("(Response is not valid JSON)");
        }
        
        writeln("\r\nPress any key to continue...");
        uint8_t dummy;
        while (readByte(&dummy, 10) <= 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}
