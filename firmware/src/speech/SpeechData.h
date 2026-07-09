#pragma once

#include <stdint.h>
#include <stddef.h>

// Speech audio data embedded in flash
// Format: Unsigned 8-bit PCM, 13000 Hz sample rate, little endian

// Symbols created by EMBED_FILES in CMakeLists.txt
extern const uint8_t fire_alarm_raw_start[] asm("_binary_FIRE_ALARM_raw_start");
extern const uint8_t fire_alarm_raw_end[] asm("_binary_FIRE_ALARM_raw_end");
extern const uint8_t occupant_alert_raw_start[] asm("_binary_OCCUPANT_ALERT_raw_start");
extern const uint8_t occupant_alert_raw_end[] asm("_binary_OCCUPANT_ALERT_raw_end");

// Helper functions to get speech data
inline const uint8_t* getFireAlarmSpeech() {
    return fire_alarm_raw_start;
}

inline size_t getFireAlarmSpeechSize() {
    return fire_alarm_raw_end - fire_alarm_raw_start;
}

inline const uint8_t* getOccupantAlertSpeech() {
    return occupant_alert_raw_start;
}

inline size_t getOccupantAlertSpeechSize() {
    return occupant_alert_raw_end - occupant_alert_raw_start;
}

// Speech sample rate
constexpr uint32_t SPEECH_SAMPLE_RATE_HZ = 13000;

// Speech types for playback
enum class SpeechType {
    FIRE_ALARM,
    OCCUPANT_ALERT
};

