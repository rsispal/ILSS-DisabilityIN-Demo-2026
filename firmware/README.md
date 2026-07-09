# ILSS Lanyard

## Project Description
>
>
>

## Configuration

The ILSS Lanyard application provides various configuration options through Kconfig. These can be configured through the Zephyr configuration system.

### Configuration Properties

| Configuration Option | Type | Default | Description | Status |
|---------------------|------|---------|-------------|---------|
| `ILSS_PREFERENCES_PERSONAL_ALERT_BUTTONS_ENABLED` | bool | y | Enable Personal Alert buttons functionality | ✅ Current |
| `ILSS_PREFERENCES_LED_INDICATORS_ENABLED` | bool | y | Enable LED indicators for visual feedback | ✅ Current |
| `ILSS_PREFERENCES_AUDIBLE_INDICATORS_ENABLED` | bool | y | Enable audible indicators (buzzer/speaker) | ✅ Current |
| `ILSS_PREFERENCES_HAPTICS_ENABLED` | bool | y | Enable haptic feedback (vibration) | ✅ Current |
| `ILSS_PREFERENCES_HONEYWELL_BEACON_SCANNING_ENABLED` | bool | y | Enable scanning for Honeywell-specific beacons | ✅ Current |
| `ILSS_PREFERENCES_INCOMING_FIRE_ALARM_EVENTS_ENABLED` | bool | y | Enable alerts for incoming fire alarm events | ✅ Current |
| `ILSS_PREFERENCES_WIFI_ENABLED` | bool | y | Enable Wi-Fi connectivity | ✅ Current |
| `ILSS_PREFERENCES_NFC_SESSION_SHARING_ENABLED` | bool | y | Enable NFC session sharing functionality | ✅ Current |
| `ILSS_PREFERENCES_QUIESCENT_MODE_SCAN_INTERVAL_MS` | int | 60000 | Quiescent mode beacon scanning interval (1-300s) | ✅ Current |
| `ILSS_PREFERENCES_FAST_SCAN_MODE_SCAN_INTERVAL_MS` | int | 20000 | Fast scan mode beacon scanning interval (1-60s) | ✅ Current |
| `ILSS_PREFERENCES_PERSONAL_ALERT_BUTTON_TRIGGER_DELAY_MS` | int | 5000 | Personal Alert button trigger delay (1-10s) | ✅ Current |

### Experimental Features

| Configuration Option | Type | Default | Description | Status |
|---------------------|------|---------|-------------|---------|
| `ILSS_PREFERENCES_EXPERIMENTAL_FEATURES_ENABLED` | bool | n | Master switch for experimental features | 🔬 Experimental |
| `ILSS_PREFERENCES_THIRD_PARTY_BEACON_SCANNING_ENABLED` | bool | n | Enable scanning for third-party beacons | 🔬 Experimental |
| `ILSS_PREFERENCES_INACTIVITY_ALERT_ENABLED` | bool | n | Enable inactivity alert functionality | 🔬 Experimental |

### Usage

To enable experimental features, set `ILSS_PREFERENCES_EXPERIMENTAL_FEATURES_ENABLED=y` in your configuration. This will make the experimental feature options available in the configuration menu.

### Configuration via Menuconfig

Run `west build -t menuconfig` to access the interactive configuration menu where you can modify these settings.


## Building firmware
Build command:
```sh
west build -b thingy53/nrf5340/cpuapp -- -Dilss_lanyard_SHIELD=nrf7002eb
```
