# ILSS Lanyard Firmware

> [!IMPORTANT]
> **Canonical Documentation**: The live version of this document is maintained in the documentation repository at:
> `documentation/5-phase-3/5-1-lanyard/5-3-firmware/5-3-1-protocols-and-specs/Specification.md`
> Please ensure any updates are reflected in both locations to prevent divergence.

> DRAFT

> Targets hardware revision 0.0.2 - Nordic NRF5340

The ILSS Lanyard has the following features:

2 side buttons (lever microswitch) used for Personal Alert feature
1 Mode button (push button) for future lanyard modes
10x WS2812B RGB LED's
1x DRV2605L haptic motor/driver pair (i2c)
Wi-Fi (ESP32-S3)
Battery management (seeed studio xiao esp32-s3)
Bluetooth LE (ESP32-S3)

Currently the hardware prototype is built upon the Nordic "Thingy53" development hardware and nRF7002EB wi-fi coprocessor module. Once the functionality is settled and firmware developed as a baseline, a formal purpose-built PCB will be engineered.

This codebase is the firmware for the ILSS Lanyard and below is a full breakdown of its required features/behaviour.

## Codebase structure

The primary firmware is retained within the `src/` directory, which has the following structure:

```

src/
main.cpp
applications/
- FeatureDevelopmentApplication/
    - FeatureDevelopmentApplication.cpp
    - FeatureDevelopmentApplication.h
- WiFiLanyardApplication/
    - WiFiLanyardApplication.cpp
    - WiFiLanyardApplication.h
constants/
- Constants.h
- Constants.cpp
state/
- State.cpp
- State.h
layers/
- ble-beacon/
    - BLEBeacon.cpp
    - BLEBeacon.h
- pub-sub
    - PubSub.cpp
    - PubSub.h
features/
- side-buttons/
    - SideButtons.cpp
    - SideButtons.h
- mode-button/
    - ModeButton.cpp
    - ModeButton.h
- rgb-leds/
    - RGBLED.cpp
    - RGBLED.h
- buzzer/
    - Buzzer.cpp
    - Buzzer.h
- haptics/
    - Haptics.cpp
    - Haptics.h
- bluetooth/
    - Bluetooth.cpp
    - Bluetooth.h
- power-management/
    - PowerManagement.cpp
    - PowerManagement.h
- storage/
    - Storage.cpp
    - Storage.h
- usb/
    - USB.cpp
    - USB.h
lowlevel/
- i2c/
    - I2CLowLevelDriver.cpp
    - I2CLowLevelDriver.h
- drv2605l/
    - DRV2605LLowLevelDriver.cpp
    - DRV2605LLowLevelDriver.h
- ws2812b/
    - WS2812BLowLevelDriver.cpp
    - WS2812BLowLevelDriver.h
- buzzer/
    - BuzzerLowLevelDriver.cpp
    - BuzzerLowLevelDriver.h
- power-management/
    - PowerManagementLowLevelDriver.cpp
    - PowerManagementLowLevelDriver.h
- bluetooth/
    - BluetoothLowLevelDriver.cpp
    - BluetoothLowLevelDriver.h
- wifi/
    - WiFiLowLevelDriver.cpp
    - WiFiLowLevelDriver.h
- storage/
    - StorageLowLevelDriver.cpp
    - StorageLowLevelDriver.h
- usb/
    - USBLowLevelDriver.cpp
    - USBLowLevelDriver.h

```

This structure is used for dependency injection and abstraction from the hardware platform itself via the low level driver interface implementing the platform-specific functionality, which gets passed into features (delivering atomic functionality specific to the lanyard's requirements), which get passed into layers which provide more soft/generic functionality on top of the hardware features.

The codebase is primarily class/instance based. Each class will be non-static, but also have a singleton instance reserved for later (depending on the feature, as some zephyr API's require this for interrupts) and have:

- A constructor which takes in ApplicationState
- A const char \* TAG (used for distinguishing logging)
- A mandatry begin() method for initialisation and be called after the class is constructed

## Bootup

When the lanyard boots, we will show a chasing white LED pattern for 3 seconds and end it after lowlevel and all features have been initialised. The user will be offered to enter CLI mode for 5 seconds by pressing any key

Next, if ILSS_DEBUG_BOOT_INTO_FEATURE_DEVELOPMENT_MODE is true we need to run the FeatureDevelopmentApplication begin() method, otherwise run the main WiFiLanyardApplication begin() method (essentially the main customer application).

We won't do anything special in FeatureDevelopmentApplication yet as that is a scratchpad for development use. 


The WiFiLanyardApplication begin() method will perform the following steps:

- LowLevel will offer the prompt for the user to press a keyboard key on the USB CDC terminal to enter "CLI mode" to access certain programming options. Pressing "e" will exit that mode and continue with the following routine
-  State will load any pre-exisitng state from Non Volatile Storage (checks NVS for previously stored session/wifi/preference data and loads it into the State class, or loads reasonable defaults if nothing present)
- If the state isProvisioned property = true we know we have a previous session in state and can resume that
- Initialise the lowlevel class and features
- - Connect to Azure IoT 
- - Enter Quiescent Mode and begin the loop of scanning bluetooth beacons, publishing the LOCATION_UPDATE payloads, checking for incoming messages and also monitoring the mode buttons for the presses

- If the state isProvisioned property = false we do not have a previous session available so should show a pulsing blue LED to denote we're not paired. 


- If we have a long press of both side buttons that would trigger Personal Alert (see below)
- If we have a long press of ONLY the left button that would trigger Provisioning Mode


## USB CLI Mode

The USB CLI provides an interactive configuration and testing interface accessible via USB serial connection. The CLI is available during bootup (5-second window) or can be accessed through the main configuration menu.

### Main Configuration Menu

When entering USB CLI mode, the following options are available:

- **c** = Configure data parameters (see Configure Data Parameters below)
- **t** = Test mode (see Test Mode sub-menu below)
- **f** = Factory mode (device ID/keys management)
- **s** = Show current status
- **reset** = Factory reset (erase NVS & reboot)
- **e** = Exit and continue with application

### Configure Data Parameters

When selecting "Configure data parameters" from the main menu, the user can set the following values:

- **Session ID**: string (UUID of the session)
- **Persona**: string (one of: ACCESSIBLE_USER, HOUSEKEEPER, LONE_WORKER, SECURITY, EMPLOYEE, ELDERLY)
- **WiFi SSID**: string
- **WiFi Password**: string (displayed as masked "***" in NVS test for security)
- **First Name**: string
- **Last Name**: string
- **Feature Preferences**: Enable/disable haptics, buzzer, LED indications, inactivity alerts, NFC session sharing, Honeywell beacon scanning
- **Scan Intervals**: Quiescent and fast scan intervals
- **Personal Alert Button Delay**: Delay in milliseconds before personal alert triggers

### Test Mode Sub-menu

Within test mode, the following diagnostic and testing options are available:

- **b** = Buzzer sub-menu (see Buzzer Test Menu below)
- **led** = LED colour cycle (cycle through RGB colours)
- **ledp** = LED pattern test (choose specific effect and color combination)
- **hap** = Haptic patterns demo (demonstrate haptic feedback patterns)
- **pa** = Personal Alert simulation (simulate personal alert with all indicators)
- **fa** = Fire Alarm simulation (simulate fire alarm with all indicators)
- **sb** = Side Button test (test side button functionality with tactile feedback)
- **nvs** = NVS storage test (read, write, delete, and verify NVS operations)
- **wifi** = WiFi test sub-menu (see WiFi Test Menu below)
- **err** = Error mode test (simulate error mode behavior)
- **e** = Exit (return to main menu)

### Buzzer Test Menu

The buzzer test menu provides options to test various buzzer patterns and speech playback:

- **0** = Run all buzzer patterns (sequential demonstration of all patterns)
- **1** = Tick (short beep)
- **2** = Beep (1 second tone)
- **3** = Code-3 Temporal (constant tone)
- **4** = Code-3 Sweep (3x ramp UP)
- **5** = Code-3 Siren (3x smooth siren)
- **6** = Siren (continuous up/down)
- **7** = Medium Sweep
- **8** = Alternating (two-tone)
- **9** = Fire Horn Buzz (LF Buzz)
- **s** = Speech Test (experimental - fire alarm and occupant alert speech)
- **e** = Exit (return to test mode menu)

### LED Pattern Test

The LED pattern test allows users to select any combination of LED effect and color:

**Effect Options:**
- **0** = OFF
- **1** = PULSE (smooth sine wave, 2s cycle)
- **2** = RAPID_PULSE (rapid sine wave, 500ms cycle)
- **3** = BLINK_ALTERNATE (on/off toggle, 1s cycle)
- **4** = FLASH_1S (quick flash every 1s)
- **5** = FLASH_2S (quick flash every 2s)
- **6** = CHASE_FADE (chasing LED with fade trail)
- **7** = CONTINUOUS (solid color)
- **8** = DOUBLE_FLASH (double flash pattern)
- **9** = WATER_DROP (water drop effect)

**Color Options:**
- **0** = RED
- **1** = GREEN
- **2** = BLUE
- **3** = PURPLE
- **4** = YELLOW
- **5** = CYAN
- **6** = WHITE
- **7** = ORANGE

The selected effect runs continuously until the user presses 'e' to stop.

### WiFi Test Menu

The WiFi test menu provides options to test WiFi functionality:

- **1** = Scan networks (displays list of WiFi networks with MAC address, SSID, and Channel)
- **2** = Connect to SSID (step-by-step prompts for SSID, password, and channel)
- **3** = Get WiFi connection status (shows current connection status, SSID, and IP address)
- **4** = Disconnect (disconnects from current WiFi network)
- **5** = Test GET request to httpbin (performs HTTP GET request to https://httpbin.org/get)
- **e** = Exit (return to test mode menu)

**Note:** WiFi credentials entered during testing are not stored in State. The connection is temporary and for testing purposes only.

**LED Feedback:**
- Chasing blue LEDs during WiFi scanning
- Green LED when connected successfully
- Red LED when disconnected
- Orange LED on errors

### Error Mode Test

The error mode test simulates the Error Mode behavior that occurs when the device is provisioned but WiFi credentials are missing:

- **Red LED**: FLASH_2S effect (continuous flashing every 2 seconds)
- **Buzzer**: Low-pitch beep (600Hz, 200ms) every 2 seconds

This test allows users to preview the error mode indications before encountering the actual error condition. Press 'e' to stop the test.

### Factory Reset

The factory reset option erases all NVS data and reboots the device. Before execution, the user must confirm by typing "yes". The reset sequence includes:

- Ramp-up haptic feedback (0 to 127 intensity over ~500ms)
- Haptic pattern 92
- Buzzer beep (2000Hz, 200ms)
- Green LED double flash
- System reboot

**Warning:** Factory reset is irreversible and will clear all stored configuration data.

## Error Mode

If the device is provisioned but WiFi credentials (SSID or password) are missing, the device enters Error Mode. This prevents the device from operating without network connectivity.

### Error Mode Indications

- **Red LED**: Continuous rapid pulse pattern
- **Buzzer**: Low-pitch beep (600Hz, 200ms) every 2 seconds

### Recovery from Error Mode

To recover from Error Mode, the user must perform a factory reset:

1. Hold the **LEFT button** (personal alert button) for 10 seconds
2. The device will erase all NVS data and reboot
3. After reboot, the device will enter Provisioning Mode for reconfiguration

The factory reset clears all stored data, allowing the device to be provisioned again with valid WiFi credentials.

## Provisioning Mode

The provisioning process is extremely simple, and is just a matter of a provisioning application (i.e. a React frontend with WebUSB) to pass the necessary values to the lanyard via the respective USB commands (detailed in features/Usb/README.md)

The provisioning application pass:

- Wifi credentials (ssid, password, band)
- Session ID
- User first name
- User last name
- Persona ID
- Preferences (enable/disable features like haptics, buzzer, leds, fast/quiescent beacon scanning intervals, personal alert button delay interval)

These values will be saved and the wifi will be connected

After provisioning, the USB connection protocol should be stopped until the device is reset via the mode button or the session is remotely terminated via the PubSub

We have a USBProtocol.cpp which can process certain JSON payloads. Once we have the session ID, successfully connected wifi credentials, user persona and first/last name stored we can exit Provisioning Mode and enter Quiescent Mode

During provisioning mode we should show a chasing blue LED pattern

## Quiescent Mode 

The lanyard quiescent operation mode will:

- Every CONFIG_ILSS_PREFERENCES_QUIESCENT_BEACON_SCAN_MODE_SCAN_INTERVAL_MS the applicaiton will run the feature/Bluetooth beacon scanning method and update a list of nearest beacons seen and thus stored in State in a vector. The period interval is controlled via State class

- Send a status update to PubSub every CONFIG_ILSS_PREFERENCES_QUIESCENT_BEACON_SCAN_MODE_SCAN_INTERVAL_MS seconds with the latest beacon data (if changed), lanyard battery/charge status. We transmit a LOCATION_UPDATE json message with the following structure:

  |      Field       |     Type      | Description                                                                      |
  | :--------------: | :-----------: | :------------------------------------------------------------------------------- |
  |        id        |      int      | unique identifier for the event                                                  |
  |      event       |    string     | "PERSONAL_ALERT", "INACTIVITY_ALERT", "LOCATION_UPDATE", "ERROR", "END_SESSION"  |
  |    sessionId     |    string     | UUID of the session (from state)                                                 |
  |     deviceId     |    string     | UUID of the ILSS device (serial number uuid, from state)                         |
  |    errorCode     |      int      | error code for the error event                                                   |
  |  primaryPointId  | string / null | identifier of the primary point (best beacon ID from state)                      |
  | secondaryPointId | string / null | identifier of the secondary point (second best beacon, if available, from state) |
  | tertiaryPointId  | string / null | identifier of the tertiary point (third best beacon, if available, from state)   |
  |  batteryStatus   |    string     | charging, low, normal, error (from state)                                        |
  |   batteryLevel   |      int      | 0-100 (from state)                                                               |

- [UNUSED ON ESP32, but have a function stubbed for this] Checks if the NFC tag is being interacted with, and if so, share the Session ID with that scanning device (so the app can access this lanyard's online session data). This is known as NFC session sharing.

- Checks the status of the personal alert side buttons, and initiates a haptic/LED/buzzer if either is pressed to give a preemptive warning if they're held in. If both are held in for the delay period in State, the lanyard should enter the Personal Alert Activation method/state. If either is released the timer should be reset

- Check the mode button (left side button) is long pressed (ideally for 10 seconds) to reset the lanyard to default state and re-enter Provisioning Mode (clearing all stored NVS data, disconnecting wifi and loading default state). Note: This functionality is available via USB CLI factory reset command; button-based factory reset is reserved for Error Mode recovery.

- If there is a Personal Alert or Fire Alarm, we should NOT be allowed to enter Provisioning Mode

## Incoming fire event

If we receive a Azure IoT  message with an event type denoting a fire event ("FIRE_ALARM"), the lanyard will:

- Initiate a double flashing red LED pattern (every 2 second)
- A distinct siren tone on the buzzer (every 15 seconds, for 5 seconds)
- Firm haptic alerts (pattern 118) (every 15 seconds, for 5 seconds)

The siren and haptics will operate in short bursts to reduce annoyance 

The lanyard will then change the feature/Bluetooth beacon scanning interval to the fast scan interval time in State and send the status update to layers/PubSub

The lanyard will not allow feature/NFC session sharing during a fire event. It will however only continue to check the status of the personal alert side buttons for presses (as this is a higher priority event).

The lanyard will only allow one particular mode/state at a time. So, if there is a personal alert raised whilst a fire event is present, the lanyard state will be "personal alert mode". If it's reset then the lanyard will resume the fire event mode. We can do this by having a secondary state variables isInFireEventMode, isInPersonalAlertMode

This is the incoming message which would trigger a fire alarm event

|   Field    |  Type  | Description                     |
| :--------: | :----: | :------------------------------ |
|     id     |  int   | unique identifier for the event |
|   event    | string | "FIRE_ALARM"                    |
| buildingId | string | UUID of the building            |
|   siteId   | string | UUID of the site                |
|  pointId   |  int   | Device address                  |
|   device   |  int   | Device address                  |
|    loop    |  int   | Loop number                     |
|    node    |  int   | Node number                     |
|   domain   |  int   | Domain number                   |
|   label    | string | Label of the device/point       |
| zoneLabel  | string | Label of the zone               |
| timestamp  | string | Timestamp of the event          |


These fire indications will remain active until we receive a fire alarm reset event, in which we return to quiescent mode:

|   Field    |  Type  | Description                     |
| :--------: | :----: | :------------------------------ |
|     id     |  int   | unique identifier for the event |
|   event    | string | "FIRE_ALARM_RESET"              |
| buildingId | string | UUID of the building            |
|   siteId   | string | UUID of the site                |
| timestamp  | string | Timestamp of the event          |


For both event types, we do not need to utilise anything other than the "event" property from the JSON payloads

## Personal Alert Activation

When both side buttons are long pressed the lanyard will enter Personal Alert Mode and trigger a pulsing purple LED pattern, an audible siren on the buzzer, and haptic alerts indefinitely. The siren and haptics will operate in short bursts to reduce annoyance to the user (perhaps every 5 seconds, in a code-3 style)

Immediately, an event payload will be transmitted to layers/PubSub explaining the personal alert and will also include the closest beacon at that moment. The structure as follows:

|      Field       |     Type      | Description                                                                     |
| :--------------: | :-----------: | :------------------------------------------------------------------------------ |
|        id        |      int      | unique identifier for the event                                                 |
|      event       |    string     | "PERSONAL_ALERT" |
|    sessionId     |    string     | UUID of the session                                                             |
|     deviceId     |    string     | UUID of the ILSS device                                                         |
|    errorCode     |      int      | error code for the error event                                                  |
|  primaryPointId  | string / null | identifier of the primary point                                                 |
| secondaryPointId | string / null | identifier of the secondary point                                               |
| tertiaryPointId  | string / null | identifier of the tertiary point                                                |
|  batteryStatus   |    string     | charging, low, normal, error                                                    |
|   batteryLevel   |      int      | 0-100                                                                           |


The lanyard will then change the feature/Bluetooth beacon scanning interval to the fast scan interval time in State and send the status update to layers/PubSub

The lanyard will only leave this mode if the layers/PubSub receives a command instructing it to clear the personal alert and return to quiescent operation with the type field "PERSONAL_ALERT_RESET" or "PERSONAL_ALERT_CLEAR"



## Inactivity Alerts
This feature is not yet available, and is not accepted for MVP. We will have a routine for Inactivty Alert which will involve a low level driver for an accelerometer. If the accelerometer doesn't have more than 5% change in movement over the period defined by a value in State (inactivity alert duration) we should send a message to Azure IoT

It will involve the monitoring of an accelerometer and beacon scanning data to determine whether a wearer is moving or has moved within a period of time. If they haven't, a PubSub event can be transmitted and therefore request investigation. Below is the outgoing message that would be sent via layers/PubSub
|      Field       |     Type      | Description                                                                     |
| :--------------: | :-----------: | :------------------------------------------------------------------------------ |
|        id        |      int      | unique identifier for the event                                                 |
|      event       |    string     | "INACTIVITY_ALERT" |
|    sessionId     |    string     | UUID of the session                                                             |
|     deviceId     |    string     | UUID of the ILSS device                                                         |
|    errorCode     |      int      | error code for the error event                                                  |
|  primaryPointId  | string / null | identifier of the primary point                                                 |
| secondaryPointId | string / null | identifier of the secondary point                                               |
| tertiaryPointId  | string / null | identifier of the tertiary point                                                |
|  batteryStatus   |    string     | charging, low, normal, error                                                    |
|   batteryLevel   |      int      | 0-100                                                                           |

We would then trigger the following audible-visual indications:
- Initiate a double flashing purple LED pattern (every 5 second)
- A distinct siren tone on the buzzer (every 15 seconds, for 5 seconds)
- Firm haptic alerts (pattern 118) (every 15 seconds, for 5 seconds)

Bluetooth scanning still operates at the rate defined in Quiescent Mode

These will continue until an incoming message of type  "INACTIVITY_ALERT_RESET" or "INACTIVITY_ALERT_CLEAR" arrives, where we resume Quiescent Mode. If a fire alarm event arrives the lanyard should enter Incoming Fire Event mode. If a personal alert is raised via the side buttons then that is also permissible and will clear the inactivity alert (sending a message with event "INACTIVITY_ALERT_CLEAR")


## Bidirectional communication requirements
We will use Azure IoT with devies authenticating via X.509 self signed certificates (already set up)



### Packed message (future - reducing packet size)

#### Endianness

Little-endian

#### Structure:

[START FRAME][MESSAGE LENGTH][MESSAGE ID][MESSAGE DESCRIPTOR][MESSAGE CODE][FROM ADDRESS][TO ADDRESS][DATA LENGTH][DATA][CRC16]

| Octets |       Field        | Length | Description |
| :----: | :----------------: | :----: | :---------- |
|   1    |    START FRAME     |   1    | 0xFE        |
|   2    |   MESSAGE LENGTH   |   1    |             |
|   3    |     MESSAGE ID     |   1    |             |
|   4    | MESSAGE DESCRIPTOR |   1    |             |
|   5    |    MESSAGE CODE    |   2    |             |
|  6-22  |    FROM ADDRESS    |   16   |             |
| 23-39  |     TO ADDRESS     |   16   |             |
|   40   |    DATA LENGTH     |   1    |             |
|   41   |        DATA        |   N    |             |
|  41+N  |       CRC16        |   2    |             |
|        |                    |        |             |

#### Supported Message codes

|  Code  | Category | Description                           |
| :----: | :------: | :------------------------------------ |
| 0x01FF | Command  | Non-acknowledge message               |
| 0x0101 | Command  | Acknowledge message                   |
| 0x0201 |  Event   | Periodic status update                |
| 0xA001 | Command  | Personal alert activated              |
| 0xA002 | Command  | Clear personal alert                  |
| 0xB001 |  Event   | Fire event                            |
| 0xB002 |  Event   | Fire event has been reset             |
| 0xC001 |  Event   | Inactivity threshold reached          |
| 0xC002 | Command  | Inactivity alert cleared by user      |
| 0xC003 | Command  | Inactivity alert cleared by responder |

A message can have a minimum length of 41 bytes, assuming the message has no data.
A message can have a maximum length of 255 bytes including the start frame and CRC

#### Acknowledgement

Each incoming message should be acknowledged using its message ID. If the sender does not receive an acknowledgement within 5 seconds it will be transmitted up to a maximum of 10 times.

If the receiver cannot process the incoming message for whatever reason, it should send a non-acknowledgement message. The sender is then expected to back off and retry after an interval and retry transmission up to a maximum of 10 times if it is appropriate for that kind of message.

#### CRC

CRC16-ARC shall be used
The CRC will be calculated from the start frame up to the end of the data

#### Examples

TODO
...
