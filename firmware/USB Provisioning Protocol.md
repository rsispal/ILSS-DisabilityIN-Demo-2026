# USB Command Interface

> [!IMPORTANT]
> **Canonical Documentation**: The live version of this document is maintained in the documentation repository at:
> `documentation/5-phase-3/5-1-lanyard/5-3-firmware/5-3-1-protocols-and-specs/USB-Provisioning-Protocol.md`
> Please ensure any updates are reflected in both locations to prevent divergence.

The USB Command Interface provides a JSON-based communication protocol for provisioning and controlling the ILSS Lanyard device via USB CDC connection. This interface uses versioned commands (currently USBCommandVersion1) to ensure future compatibility.

> **🔒 SECURITY NOTICE**: This interface is designed with security-first principles. All commands require **physical USB connection** and are never exposed over wireless networks. WiFi credentials and sensitive data are transmitted only over the local USB connection, significantly reducing attack vectors compared to network-based provisioning protocols.

## Overview

The USB Command Interface provides a **dedicated, separate endpoint** from the main logging interface. This ensures that device provisioning and control commands don't interfere with debug output and development logging.

### Dual USB Interface Architecture

The ILSS Lanyard exposes **two separate USB CDC endpoints**:

1. **Main Logging Interface** (Default CDC ACM)

   - **Device**: `/dev/ttyACM0` (or similar)
   - **Purpose**: Debug output, system logs, development feedback
   - **Usage**: Serial terminal, logging tools, development debugging

2. **Command Protocol Interface** (`CDC_ACM_1`)
   - **Device**: `/dev/ttyACM1` (or similar)
   - **Purpose**: JSON command/response protocol for device provisioning
   - **Usage**: WebUSB frontend, configuration tools, device management

### Communication Pattern

The interface uses a request-response pattern where commands are sent as JSON objects and responses include success status and data. All communication is performed over the dedicated command interface (`CDC_ACM_1`).

## 🔒 Security Considerations

### **Attack Vector Mitigation**

- **USB-Only Protocol**: All commands require **physical USB connection** - no remote access possible
- **No Network Exposure**: Command protocol never traverses wireless networks or internet
- **Physical Security Required**: Attackers must have direct physical access to the device and USB port
- **Isolated Interface**: Command interface is completely separate from WiFi/network interfaces

### **Credential Protection**

- **Local Transmission Only**: WiFi credentials are only transmitted over the local USB connection
- **No Persistent Logging**: Credentials are never stored in device logs or persistent storage
- **No Network Transmission**: SSIDs and passwords never leave the local USB connection
- **Immediate Processing**: Credentials are processed and discarded, not cached

### **Security Best Practices for Users**

1. **Physical Security**: Ensure device is in a secure location during provisioning
2. **Trusted Environment**: Use dedicated, secure computers for device setup
3. **Secure USB**: Avoid shared USB hubs or public charging stations
4. **Credential Management**: Never share or log WiFi credentials in plaintext
5. **Environment**: Perform provisioning in private, secure locations

**Protocol Security:**
The USB-only design eliminates the primary attack vectors that would exist with network-based provisioning:

- **No Man-in-the-Middle**: Credentials never traverse wireless networks
- **No Remote Exploitation**: Physical access required for all operations
- **No Credential Harvesting**: No network endpoints to attack
- **No Persistent Exposure**: Credentials exist only during the provisioning process

### ** Future Security Enhancements**

#### **End-to-End Encryption with Diffie-Hellman Key Exchange**

This protocol will in future be updated to offer session-based encryption using Diffie-Hellman key exchange to provide cryptographic protection beyond physical USB isolation.

**Why Useful:**

- **Enhanced Security**: Encrypts sensitive data even if physical access is compromised
- **Integrity Protection**: Prevents command tampering and ensures authenticity
- **Future Flexibility**: Enables secure network-based provisioning in later versions
- **Compliance**: Meets requirements for high-security environments

**Key Technical Considerations:**

- **Key Exchange**: Curve25519 for efficient DH key agreement
- **Encryption**: AES-256-GCM for authenticated encryption with integrity
- **Session Keys**: Perfect forward secrecy with per-session key derivation
- **Performance**: Minimal overhead for embedded device constraints

## Command Implementation Status

| ID  | Command                              | Status             | Command Format                                                                            | Expected Response                                                                                                           | Notes                                                                                                |
| --- | ------------------------------------ | ------------------ | ----------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| 1   | `GET_INFO`                           | ✅ **IMPLEMENTED** | `{"cmd": 1}`                                                                              | `{"cmd": 1, "success": true, "data": {"hw":"0.1.1","sw":"0.1.0","model":"HON-ILSS-WIFI","serial":78187493520,"manu":1318}}` | Consolidated device information (hardware, software, model, serial, manufacturer)                    |
| 2   | `RESET_DEVICE`                       | 🟡 **TO DO**       | `{"cmd": 2}`                                                                              | `{"cmd": 2, "success": true, "data": "Device reset completed"}`                                                             | Reset device settings, clear user data, restart                                                      |
| 3   | `SCAN_WIFI_NETWORKS`                 | ✅ **IMPLEMENTED** | `{"cmd": 3}`                                                                              | `{"cmd": 3, "success": true, "data": [{"s":"SSID","r":-45,"sec":1,"b":0,"c":1,"mac":"aa:bb:cc:dd:ee:ff"}]}`                 | Returns filtered networks with size limits (max 914 bytes). **Blocked if already connected to WiFi** |
| 4   | `CONNECT_WIFI`                       | ✅ **IMPLEMENTED** | `{"cmd": 4, "data": {"ssid": "network_name", "password": "network_password", "band": 2}}` | `{"cmd": 4, "success": true, "message": "Connected to SSID"}`                                                               | Connect to specified WiFi network with credentials                                                   |
| 5   | `GET_WIFI_STATUS`                    | ✅ **IMPLEMENTED** | `{"cmd": 5}`                                                                              | `{"cmd": 5, "success": true, "data": {"connected": true, "ssid": "51MD_Wi-Fi", "mode": "STA"}}`                             | Get current WiFi connection status and SSID                                                          |
| 6   | `DISCONNECT_WIFI`                    | ✅ **IMPLEMENTED** | `{"cmd": 6}`                                                                              | `{"cmd": 6, "success": true, "data": "WiFi disconnected"}`                                                                  | Disconnect from current WiFi network                                                                 |
| 7   | `SET_SESSION_ID`                     | ✅ **IMPLEMENTED** | `{"cmd": 7, "data": {"session_id": "user_session_123"}}`                                  | `{"cmd": 7, "success": true, "data": "Session ID set"}`                                                                     | Store session ID in device state for user identification                                             |
| 8   | `SET_USER_FIRST_NAME`                | ✅ **IMPLEMENTED** | `{"cmd": 8, "data": {"user_first_name": "John"}}`                                         | `{"cmd": 8, "success": true, "data": "First name set"}`                                                                     | Store user's first name in device state                                                              |
| 9   | `SET_USER_LAST_NAME`                 | ✅ **IMPLEMENTED** | `{"cmd": 9, "data": {"user_last_name": "Doe"}}`                                           | `{"cmd": 9, "success": true, "data": "Last name set"}`                                                                      | Store user's last name in device state                                                               |
| 10  | `SET_USER_PERSONA`                   | ✅ **IMPLEMENTED** | `{"cmd": 10, "data": {"user_persona": "admin"}}`                                          | `{"cmd": 10, "success": true, "data": "Persona set"}`                                                                       | Parse persona enum and store in device state                                                         |
| 11  | `GET_BATTERY_STATUS`                 | 🟡 **TO DO**       | `{"cmd": 11}`                                                                             | `{"cmd": 11, "success": true, "data": {"charge_level": 85, "is_charging": false, "voltage_mv": 3800}}`                      | Get actual battery status from hardware                                                              |
| 12  | `ENABLE_PERSONAL_ALERT_BUTTONS`      | 🟡 **TO DO**       | `{"cmd": 12}`                                                                             | `{"cmd": 12, "success": true, "data": "Personal alert buttons enabled"}`                                                    | Enable personal alert button functionality                                                           |
| 13  | `DISABLE_PERSONAL_ALERT_BUTTONS`     | 🟡 **TO DO**       | `{"cmd": 13}`                                                                             | `{"cmd": 13, "success": true, "data": "Personal alert buttons disabled"}`                                                   | Disable personal alert button functionality                                                          |
| 14  | `ENABLE_HAPTICS`                     | 🟡 **TO DO**       | `{"cmd": 14}`                                                                             | `{"cmd": 14, "success": true, "data": "Haptics enabled"}`                                                                   | Enable haptic feedback functionality                                                                 |
| 15  | `DISABLE_HAPTICS`                    | 🟡 **TO DO**       | `{"cmd": 15}`                                                                             | `{"cmd": 15, "success": true, "data": "Haptics disabled"}`                                                                  | Disable haptic feedback functionality                                                                |
| 16  | `ENABLE_BUZZER`                      | 🟡 **TO DO**       | `{"cmd": 16}`                                                                             | `{"cmd": 16, "success": true, "data": "Buzzer enabled"}`                                                                    | Enable buzzer/sound functionality                                                                    |
| 17  | `DISABLE_BUZZER`                     | 🟡 **TO DO**       | `{"cmd": 17}`                                                                             | `{"cmd": 17, "success": true, "data": "Buzzer disabled"}`                                                                   | Disable buzzer/sound functionality                                                                   |
| 18  | `ENABLE_LED_INDICATIONS`             | 🟡 **TO DO**       | `{"cmd": 18}`                                                                             | `{"cmd": 18, "success": true, "data": "LED indications enabled"}`                                                           | Enable LED status indicators                                                                         |
| 19  | `DISABLE_LED_INDICATIONS`            | 🟡 **TO DO**       | `{"cmd": 19}`                                                                             | `{"cmd": 19, "success": true, "data": "LED indications disabled"}`                                                          | Disable LED status indicators                                                                        |
| 20  | `ENABLE_INACTIVITY_ALERTS`           | 🟡 **TO DO**       | `{"cmd": 20}`                                                                             | `{"cmd": 20, "success": true, "data": "Inactivity alerts enabled"}`                                                         | Enable inactivity monitoring and alerts                                                              |
| 21  | `DISABLE_INACTIVITY_ALERTS`          | 🟡 **TO DO**       | `{"cmd": 21}`                                                                             | `{"cmd": 21, "success": true, "data": "Inactivity alerts disabled"}`                                                        | Disable inactivity monitoring and alerts                                                             |
| 22  | `ENABLE_NFC_SESSION_SHARING`         | 🟡 **TO DO**       | `{"cmd": 22}`                                                                             | `{"cmd": 22, "success": true, "data": "NFC session sharing enabled"}`                                                       | Enable NFC-based session sharing                                                                     |
| 23  | `DISABLE_NFC_SESSION_SHARING`        | 🟡 **TO DO**       | `{"cmd": 23}`                                                                             | `{"cmd": 23, "success": true, "data": "NFC session sharing disabled"}`                                                      | Disable NFC session sharing                                                                          |
| 24  | `SET_TIME`                           | 🟡 **TO DO**       | `{"cmd": 24, "data": {"timestamp": 1640995200}}`                                          | `{"cmd": 24, "success": true, "data": "Time set to timestamp"}`                                                             | Parse Unix timestamp and set device system time                                                      |
| 25  | `SET_QUIESCENT_BEACON_SCAN_INTERVAL` | 🟡 **TO DO**       | `{"cmd": 25, "data": {"quiescent_scan_interval_ms": 60000}}`                              | `{"cmd": 25, "success": true, "data": "Quiescent beacon scan interval updated"}`                                            | Set quiescent mode beacon scanning interval (1000-300000ms)                                          |
| 26  | `SET_FAST_BEACON_SCAN_INTERVAL`      | 🟡 **TO DO**       | `{"cmd": 26, "data": {"fast_scan_interval_ms": 20000}}`                                   | `{"cmd": 26, "success": true, "data": "Fast beacon scan interval updated"}`                                                 | Set fast scan mode beacon scanning interval (1000-60000ms)                                           |
| 27  | `SET_PERSONAL_ALERT_BUTTON_DELAY`    | 🟡 **TO DO**       | `{"cmd": 27, "data": {"personal_alert_delay_ms": 5000}}`                                  | `{"cmd": 27, "success": true, "data": "Personal alert button delay updated"}`                                               | Set personal alert button trigger delay (1000-10000ms)                                               |

### Status Legend

- ✅ **IMPLEMENTED**: Command fully implemented and functional
- 🟡 **TO DO**: Command structure exists but core functionality needs implementation
- ❌ **NOT OFFERED**: Command not yet implemented

### Implementation Priority

**High Priority (Core Functionality):**

- WiFi connection management (commands 6, 7, 8)
- Battery status (command 13)
- Feature toggles (commands 14-25)

**Medium Priority (User Configuration):**

- Session management (command 9)
- User preferences (commands 10-12, 27)
- Time synchronization (command 26)

**Low Priority (Advanced Features):**

- Device reset (command 4) - requires careful implementation

## Command Format

### Numeric Command Optimization

The command interface uses **numeric command IDs** instead of string commands for efficiency:

- **Size reduction**: `"command":"scan_wifi_networks"` (25 chars) → `"cmd":5` (7 chars) = **72% smaller**
- **Faster parsing**: Integer comparison vs string comparison
- **Easier validation**: Range checking (1-25) vs string matching
- **Consistent format**: All commands use same field names

### Request Format

**Numeric Format (Recommended):**

```json
{
  "cmd": 5,
  "data": "optional_data_string"
}
```

**For commands with parameters, use the data object pattern:**

```json
{
  "cmd": 4,
  "data": {
    "ssid": "network_name",
    "password": "network_password",
    "band": 2
  }
}
```

**Legacy String Format (Deprecated):**

```json
{
  "command": "scan_wifi_networks",
  "data": "optional_data_string"
}
```

### Response Format

**Numeric Format (Recommended):**

```json
{
  "cmd": 5,
  "success": true,
  "data": "response_data"
}
```

**Legacy String Format (Deprecated):**

```json
{
  "command": "scan_wifi_networks",
  "success": true,
  "data": "response_data"
}
```

**Error Response:**

```json
{
  "cmd": 5,
  "success": false,
  "error_message": "error_details"
}
```

## Available Commands

### Quick Reference - Numeric Command IDs

| ID  | Command                              | Description                                                                           |
| --- | ------------------------------------ | ------------------------------------------------------------------------------------- |
| 1   | `GET_INFO`                           | Get consolidated device information (hardware, software, model, serial, manufacturer) |
| 2   | `RESET_DEVICE`                       | Reset device to factory settings                                                      |
| 3   | `SCAN_WIFI_NETWORKS`                 | Scan for available WiFi networks                                                      |
| 4   | `CONNECT_WIFI`                       | Connect to a WiFi network                                                             |
| 5   | `GET_WIFI_STATUS`                    | Get current WiFi connection status                                                    |
| 6   | `DISCONNECT_WIFI`                    | Disconnect from current WiFi network                                                  |
| 7   | `SET_SESSION_ID`                     | Set user session identifier                                                           |
| 8   | `SET_USER_FIRST_NAME`                | Set user's first name                                                                 |
| 9   | `SET_USER_LAST_NAME`                 | Set user's last name                                                                  |
| 10  | `SET_USER_PERSONA`                   | Set user's persona/role                                                               |
| 11  | `GET_BATTERY_STATUS`                 | Get battery level and status                                                          |
| 12  | `ENABLE_PERSONAL_ALERT_BUTTONS`      | Enable personal alert button functionality                                            |
| 13  | `DISABLE_PERSONAL_ALERT_BUTTONS`     | Disable personal alert button functionality                                           |
| 14  | `ENABLE_HAPTICS`                     | Enable haptic feedback functionality                                                  |
| 15  | `DISABLE_HAPTICS`                    | Disable haptic feedback functionality                                                 |
| 16  | `ENABLE_BUZZER`                      | Enable buzzer/sound functionality                                                     |
| 17  | `DISABLE_BUZZER`                     | Disable buzzer/sound functionality                                                    |
| 18  | `ENABLE_LED_INDICATIONS`             | Enable LED status indicators                                                          |
| 19  | `DISABLE_LED_INDICATIONS`            | Disable LED status indicators                                                         |
| 20  | `ENABLE_INACTIVITY_ALERTS`           | Enable inactivity monitoring and alerts                                               |
| 21  | `DISABLE_INACTIVITY_ALERTS`          | Disable inactivity monitoring and alerts                                              |
| 22  | `ENABLE_NFC_SESSION_SHARING`         | Enable NFC-based session sharing                                                      |
| 23  | `DISABLE_NFC_SESSION_SHARING`        | Disable NFC session sharing                                                           |
| 24  | `SET_TIME`                           | Set device system time                                                                |
| 25  | `SET_QUIESCENT_BEACON_SCAN_INTERVAL` | Set quiescent mode beacon scanning interval                                           |
| 26  | `SET_FAST_BEACON_SCAN_INTERVAL`      | Set fast scan mode beacon scanning interval                                           |
| 27  | `SET_PERSONAL_ALERT_BUTTON_DELAY`    | Set personal alert button trigger delay                                               |

### Device Information

#### Get Device Info (Consolidated)

**Command:** `{"cmd": 1}`  
**Data:** None  
**Response:** All device information in one response

**Example:**

```json
{ "cmd": 1 }
```

**Response:**

```json
{
  "cmd": 1,
  "success": true,
  "data": {
    "hw": "0.1.1",
    "sw": "0.1.0",
    "model": "HON-ILSS-WIFI",
    "serial": 78187493520,
    "manu": 1318
  }
}
```

**Field Descriptions:**

- **`hw`**: Hardware version (e.g., "0.1.1")
- **`sw`**: Software version (e.g., "0.1.0")
- **`model`**: Model identifier (e.g., "HON-ILSS-WIFI")
- **`serial`**: Device serial number (numeric)
- **`manu`**: Manufacturer ID (numeric)

#### Scan WiFi Networks

**Command:** `{"cmd": 3}`  
**Data:** None  
**Response:** Array of WiFi networks with compact JSON format

**Response Format:**

```json
{
  "cmd": 3,
  "success": true,
  "data": [
    {
      "s": "51MD_Wi-Fi",
      "r": -39,
      "sec": 1,
      "b": 0,
      "c": 2,
      "mac": "1a:27:f5:f1:e6:ba"
    }
  ]
}
```

**Field Descriptions:**

- `s`: SSID (network name)
- `r`: RSSI (signal strength in dBm)
- `sec`: Security type (0=Open, 1=WPA/WPA2, 2=WEP, 3=WPA3)
- `b`: Frequency band (0=2.4GHz, 1=5GHz)
- `c`: Channel number
- `mac`: BSSID (access point MAC address)

**Size Limits:**

- Maximum response size: 914 bytes (to fit within 1024-byte USB buffer)
- Networks are added dynamically until size limit is reached
- Response includes detailed logging of size calculations

#### Connect to WiFi Network

**Command:** `{"cmd": 4}`  
**Data:** JSON object with connection parameters in data field  
**Response:** Connection success/failure status

**Request Format:**

```json
{
  "cmd": 4,
  "data": {
    "ssid": "network_name",
    "password": "network_password",
    "band": 2
  }
}
```

**Data Parameters:**

- `ssid`: Network name (required)
- `password`: Network password (required for secured networks)
- `band`: Frequency band (0=auto, 2=2.4GHz, 5=5GHz, optional)

**Response Format:**

```json
{
  "cmd": 4,
  "success": true,
  "message": "Connected to network_name"
}
```

**Note on Response Security:**
The response message intentionally includes the SSID for user confirmation. This is **secure by design** because:

- **Local Only**: Response is only visible on the local USB connection
- **No Network Transmission**: SSID confirmation never leaves the USB interface
- **User Verification**: Allows users to confirm they connected to the intended network
- **Physical Security**: Requires physical USB access to view the response

**Error Response:**

```json
{
  "cmd": 4,
  "success": false,
  "error": "Connection failed"
}
```

** SECURITY CONSIDERATIONS:**

**Attack Vector Mitigation:**

- **USB-Only Protocol**: This command is **exclusively available via USB CDC interface** and cannot be accessed remotely
- **No Network Exposure**: The command protocol is never transmitted over WiFi or any network interface
- **Physical Access Required**: Attackers must have physical access to the device and USB port

**Security Best Practices for Provisioning Applications:**

1. **Secure USB Connection:**

   - Use dedicated, isolated USB connections for provisioning
   - Avoid shared USB hubs or public charging stations
   - Ensure the provisioning computer is secure and malware-free

2. **Credential Handling:**

   - Never log WiFi credentials in plaintext
   - Use secure input methods (password fields, encrypted storage)
   - Implement proper credential sanitization before transmission
   - Consider using temporary, single-use credentials for initial setup

3. **Provisioning Environment:**

   - Perform provisioning in secure, private locations, on known-secure computers
   - Avoid public WiFi networks during device setup
   - Ensure firewalls are in place and antivirus is in use
   - Ensure provisioning applications are up to date

4. **Response Security:**
   - **Protocol Design**: The response message intentionally includes the SSID for user confirmation
   - **Local Only**: This response is only visible on the local USB connection
   - **No Network Transmission**: SSID is never sent over WiFi or stored in network logs

**Why This Protocol is Secure:**

- **Attack Surface Minimization**: USB-only access significantly reduces attack vectors
- **No Network Exposure**: Credentials never traverse wireless networks during provisioning
- **Physical Security**: Requires direct physical access to the device
- **Immediate Confirmation**: User can verify connection success with SSID confirmation
- **No Persistent Storage**: Credentials are not stored in device logs or responses

**End User Responsibilities:**

- Ensure physical security of the device during provisioning
- Use secure, private locations for initial setup
- Verify the provisioning application is from a trusted source
- Follow organizational cybersecurity policies for device provisioning

#### Get WiFi Status

**Command:** `get_wifi_status` (ID: 7)  
**Data:** None  
**Response:** Current WiFi connection status and information

**Response Format:**

```json
{
  "command": "get_wifi_status",
  "success": true,
  "data": {
    "connected": true,
    "ssid": "51MD_Wi-Fi",
    "mode": "STA"
  }
}
```

**Status Fields:**

- `connected`: Boolean indicating if connected to a network
- `ssid`: Current network SSID (empty if not connected)
- `mode`: Current WiFi mode ("STA" for station/client, "AP" for access point)

**Example:**

```json
{ "cmd": 1 }
```

**Response:**

```json
{
  "cmd": 1,
  "success": true,
  "data": {
    "hw": "0.1.1",
    "sw": "0.1.0",
    "model": "HON-ILSS-WIFI",
    "serial": 78187493520,
    "manu": 1318
  }
}
```

### Device Control

#### Factory Reset

**Command:** `factory_reset`  
**Data:** None  
**Response:** Boolean indicating success

**Example:**

```json
{ "command": "factory_reset" }
```

**Response:**

```json
{
  "command": "factory_reset",
  "success": true,
  "data": "true"
}
```

### WiFi Operations

#### Scan WiFi Networks

**Command:** `scan_wifi_networks`  
**Data:** None  
**Response:** JSON array of discovered networks

**Example:**

```json
{ "command": "scan_wifi_networks" }
```

**Response:**

```json
{
  "command": "scan_wifi_networks",
  "success": true,
  "data": "[{\"s\":\"MyNetwork\",\"r\":-45,\"sec\":2,\"b\":1,\"c\":6,\"mac\":\"12:34:56:78:9a:bc\"}]"
}
```

**Network Object Structure (Shortened Keys for Space Efficiency):**

- `s`: Network name (SSID)
- `r`: Signal strength in dBm (RSSI)
- `sec`: Security type (0=Open, 1=WEP, 2=WPA, 3=WPA2, 4=WPA3)
- `b`: Frequency band (0=Unknown, 1=2.4GHz, 2=5GHz)
- `c`: WiFi channel number
- `mac`: MAC address of access point (BSSID)

**Note:** Dynamically adds as many visible networks (with non-empty SSIDs) as can fit within the 1024-byte response limit. The system automatically stops adding networks when approaching the size limit to prevent truncation.

**Connection Check:** The scan command is blocked if the device is already connected to a WiFi network. In this case, the command returns `success: false` with the message "WiFi scan blocked: Device already connected to network 'SSID_NAME'".

#### Connect to WiFi

**Command:** `connect_wifi`  
**Data:** JSON string with connection parameters  
**Response:** Boolean indicating success

**Example:**

```json
{
  "command": "connect_wifi",
  "data": "{\"ssid\":\"MyNetwork\",\"password\":\"MyPassword\",\"band\":2}"
}
```

**Data Structure:**

- `ssid`: Network name
- `password`: Network password
- `band`: Frequency band (0=auto, 2=2.4GHz, 5=5GHz)

**Response:**

```json
{
  "command": "connect_wifi",
  "success": true,
  "data": "true"
}
```

### User Configuration

#### Set Session ID

**Command:** `{"cmd": 7}`  
**Data:** JSON object with session_id parameter  
**Response:** Boolean indicating success

**Example:**

```json
{
  "cmd": 7,
  "data": {
    "session_id": "session_12345"
  }
}
```

#### Set User First Name

**Command:** `{"cmd": 8}`  
**Data:** JSON object with user_first_name parameter  
**Response:** Boolean indicating success

**Example:**

```json
{
  "cmd": 8,
  "data": {
    "user_first_name": "John"
  }
}
```

#### Set User Last Name

**Command:** `{"cmd": 9}`  
**Data:** JSON object with user_last_name parameter  
**Response:** Boolean indicating success

**Example:**

```json
{
  "cmd": 9,
  "data": {
    "user_last_name": "Doe"
  }
}
```

#### Set User Persona

**Command:** `{"cmd": 10}`  
**Data:** JSON object with user_persona parameter  
**Response:** Boolean indicating success

**Example:**

```json
{
  "cmd": 10,
  "data": {
    "user_persona": "admin"
  }
}
```

**Persona Values:**

- `"unknown"`: Unknown
- `"visitor"`: Visitor
- `"staff"`: Staff
- `"admin"`: Admin
- `"emergency_responder"`: Emergency Responder

### Device Status

#### Get Battery Status

**Command:** `get_battery_status`  
**Data:** None  
**Response:** JSON string with battery information

**Example:**

```json
{ "command": "get_battery_status" }
```

**Response:**

```json
{
  "command": "get_battery_status",
  "success": true,
  "data": "{\"charge_level\":85,\"is_charging\":false,\"voltage_mv\":3800}"
}
```

**Battery Status Structure:**

- `charge_level`: Battery percentage (0-100)
- `is_charging`: Boolean indicating charging state
- `voltage_mv`: Battery voltage in millivolts

### Feature Toggles

#### Enable/Disable Personal Alert Buttons

**Command:** `enable_personal_alert_buttons` / `disable_personal_alert_buttons`  
**Data:** None  
**Response:** Boolean indicating success

**Example:**

```json
{ "command": "enable_personal_alert_buttons" }
```

#### Enable/Disable Haptics

**Command:** `enable_haptics` / `disable_haptics`  
**Data:** None  
**Response:** Boolean indicating success

**Example:**

```json
{ "command": "disable_haptics" }
```

#### Enable/Disable Buzzer

**Command:** `enable_buzzer` / `disable_buzzer`  
**Data:** None  
**Response:** Boolean indicating success

**Example:**

```json
{ "command": "enable_buzzer" }
```

#### Enable/Disable LED Indications

**Command:** `enable_led_indications` / `disable_led_indications`  
**Data:** None  
**Response:** Boolean indicating success

**Example:**

```json
{ "command": "disable_led_indications" }
```

#### Enable/Disable Inactivity Alerts

**Command:** `enable_inactivity_alerts` / `disable_inactivity_alerts`  
**Data:** None  
**Response:** Boolean indicating success

**Example:**

```json
{ "command": "enable_inactivity_alerts" }
```

#### Enable/Disable NFC Session Sharing

**Command:** `enable_nfc_session_sharing` / `disable_nfc_session_sharing`  
**Data:** None  
**Response:** Boolean indicating success

**Example:**

```json
{ "command": "disable_nfc_session_sharing" }
```

### System Configuration

#### Set Time

**Command:** `{"cmd": 24}`  
**Data:** JSON object with timestamp parameter  
**Response:** Boolean indicating success

**Example:**

```json
{
  "cmd": 24,
  "data": {
    "timestamp": 1640995200
  }
}
```

#### Set Quiescent Beacon Scan Interval

**Command:** `{"cmd": 25}`  
**Data:** JSON object with quiescent scan interval parameter  
**Response:** Boolean indicating success

**Example:**

```json
{
  "cmd": 25,
  "data": {
    "quiescent_scan_interval_ms": 60000
  }
}
```

**Parameters:**

- `quiescent_scan_interval_ms`: Quiescent mode beacon scanning interval (1000-300000ms, default: 60000)

#### Set Fast Beacon Scan Interval

**Command:** `{"cmd": 26}`  
**Data:** JSON object with fast scan interval parameter  
**Response:** Boolean indicating success

**Example:**

```json
{
  "cmd": 26,
  "data": {
    "fast_scan_interval_ms": 20000
  }
}
```

**Parameters:**

- `fast_scan_interval_ms`: Fast scan mode beacon scanning interval (1000-60000ms, default: 20000)

#### Set Personal Alert Button Delay

**Command:** `{"cmd": 27}`  
**Data:** JSON object with personal alert delay parameter  
**Response:** Boolean indicating success

**Example:**

```json
{
  "cmd": 27,
  "data": {
    "personal_alert_delay_ms": 5000
  }
}
```

**Parameters:**

- `personal_alert_delay_ms`: Personal alert button trigger delay (1000-10000ms, default: 5000)

## Usage Examples

### Connecting to Both Interfaces

When you plug your Lanyard into a computer, you'll see **two separate USB devices**:

```bash
# Terminal 1: Logging interface (debug output)
screen /dev/ttyACM0 115200

# Terminal 2: Command interface (JSON protocol)
screen /dev/ttyACM1 115200
```

### Testing the Command Interface

1. **Connect to the command interface** (`/dev/ttyACM1`)
2. **Send a JSON command**:
   ```json
   { "cmd": 1 }
   ```
3. **Receive the response**:
   ```json
   {
     "cmd": 1,
     "success": true,
     "data": { "hw": "0.1.1", "sw": "0.1.0", "model": "HON-ILSS-WIFI", "serial": 78187493520, "manu": 1318 }
   }
   ```

### WebUSB Integration

Your React frontend can:

- **Connect specifically** to the command interface (`/dev/ttyACM1`)
- **Send commands** without interfering with logging
- **Maintain separate sessions** for each purpose

## Implementation Details

### Low-Level Driver

The `UsbLowLevelDriver` class provides generic USB CDC communication methods:

- `sendData()` / `receiveData()` - String-based communication
- `sendBytes()` / `receiveBytes()` - Raw byte communication
- `waitForConnection()` - Wait for USB connection
- `isConnected()` - Check connection status

### Command Processing

The `USB` class handles command parsing and execution:

- `executeCommand()` - Parse JSON and execute command
- `createResponse()` - Generate JSON response
- Individual command handlers for each operation

### JSON Handling

Uses manual JSON string building for embedded compatibility:

- Lightweight implementation suitable for embedded systems
- Manual parsing for command structure
- Consistent response formatting
- No external library dependencies

## Usage Examples

### Basic Command Flow

```cpp
USB usb(logger);
usb.begin();

// Process incoming commands
usb.processCommands();
```

### Custom Command Sending

```cpp
// Send a command
std::string command = "{\"cmd\":1}";
usb.sendCommand(command, 1000);

// Receive response
std::string reply;
if (usb.receiveReply(reply, 1000)) {
    // Process reply
}
```

### WebUSB Integration

The interface is designed to work seamlessly with WebUSB:

- JSON-based communication
- Consistent response format
- Error handling for robust operation
- Timeout support for reliable communication

## Error Handling

### Common Error Scenarios

- **Invalid JSON**: Malformed command structure
- **Unknown Command**: Unsupported command name
- **Missing Data**: Required data field not provided
- **Execution Failure**: Command execution error

### Error Response Format

**Numeric Format (Recommended):**

```json
{
  "cmd": 3,
  "success": false,
  "data": "Detailed error description"
}
```

**Legacy String Format (Deprecated):**

```json
{
  "command": "command_name",
  "success": false,
  "error_message": "Detailed error description"
}
```

## Future Enhancements

- **Command Versioning**: Support for multiple command versions
- **Batch Operations**: Execute multiple commands in sequence
- **Event Streaming**: Real-time event notifications
- **Authentication**: Secure command execution
- **Command Validation**: Schema-based command validation

---

_This interface provides a robust foundation for device provisioning and control via USB CDC, enabling seamless integration with WebUSB applications._
