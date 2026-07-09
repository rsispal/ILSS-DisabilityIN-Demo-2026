#pragma once

// Speech playback removed from digital-twin firmware build.
// Kept as a stub header so legacy includes compile if referenced.

enum class SpeechType {
    FIRE_ALARM = 0,
    OCCUPANT_ALERT = 1,
};

static constexpr uint32_t SPEECH_SAMPLE_RATE_HZ = 13000;

inline const uint8_t* getFireAlarmSpeech() { return nullptr; }
inline size_t getFireAlarmSpeechSize() { return 0; }
inline const uint8_t* getOccupantAlertSpeech() { return nullptr; }
inline size_t getOccupantAlertSpeechSize() { return 0; }
