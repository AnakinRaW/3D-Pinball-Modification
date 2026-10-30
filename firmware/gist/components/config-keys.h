#pragma once
#include <stdint.h>
#include <stdio.h>

// every key of /config.yml, each written once; the part named beside it owns the setting, and its
// document gives the default and the range
namespace ConfigKey {
constexpr const char* kAdapterA      = "lighting.adapter_a";     // lighting driver
constexpr const char* kMusicVolume   = "audio.music_volume";     // sounds component
constexpr const char* kEffectsVolume = "audio.effects_volume";   // sounds component
constexpr const char* kServoStart    = "servo.start_deg";        // servo driver
constexpr const char* kServoEnd      = "servo.end_deg";          // servo driver
constexpr const char* kPullMs        = "solenoid.pull_ms";       // solenoid driver, a list of one value per coil
constexpr const char* kIrChannels    = "ir.channels";            // IR driver, a list of one entry per channel

// the fields of an ir.channels entry
constexpr const char* kIrChannel    = "channel";
constexpr const char* kIrClearBuild = "clear_build";
constexpr const char* kIrBallBuild  = "ball_build";
constexpr const char* kIrOrder      = "order";

// writes the key of entry i of a list into out, followed by field where one is given, such as
// "solenoid.pull_ms.2" or "ir.channels.0.order"; gets out
inline const char* entry(char (&out)[48], const char* list, uint8_t i, const char* field = nullptr) {
    if (field) snprintf(out, sizeof(out), "%s.%u.%s", list, unsigned(i), field);
    else       snprintf(out, sizeof(out), "%s.%u", list, unsigned(i));
    return out;
}
}
