// Every key of the machine config stands here once. The part named beside a key owns the setting.
// Its header gives the default and the range.
//
// Design: docs/firmware/components/config.md.
#pragma once

namespace ConfigKey {
constexpr const char* ADAPTER_A = "lighting.adapter_a";          // lighting driver
constexpr const char* MUSIC_VOLUME = "audio.music_volume";       // sounds component
constexpr const char* EFFECTS_VOLUME = "audio.effects_volume";   // sounds component
constexpr const char* SERVO_START_DEG = "servo.start_deg";       // servo driver
constexpr const char* SERVO_END_DEG = "servo.end_deg";           // servo driver
constexpr const char* SERVO_SPEED_DEG_S = "servo.speed_deg_s";   // servo driver
constexpr const char* TOUCH_LEFT_EDGE = "touch.left_edge";       // touch driver
constexpr const char* TOUCH_RIGHT_EDGE = "touch.right_edge";     // touch driver
constexpr const char* TOUCH_TOP_EDGE = "touch.top_edge";         // touch driver
constexpr const char* TOUCH_BOTTOM_EDGE = "touch.bottom_edge";   // touch driver
constexpr const char* BOOT_FILM = "host.boot_film";              // game host
}   // namespace ConfigKey
