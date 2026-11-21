// The firmware's time measurement. GPT2 counts microseconds from the board's crystal, and nowUs()
// reads it. Every time in the firmware comes from nowUs(),
// every span goes through elapsedUs() or elapsedMs(), and every comparison of two times through
// before(). nowUs() wraps every 71.6 minutes, and all three keep their answers across the wrap.
//
// Design: docs/firmware/general-design.md, Time measurement.
#pragma once
#include <Arduino.h>

/// Starts GPT2 counting microseconds.
///
/// @note  setup() calls it before anything takes a time.
void startClock();

/// Gets the microseconds GPT2 has counted since startClock().
inline uint32_t nowUs() {
    return GPT2_CNT;
}

/// Gets whether at least @p spanUs microseconds have passed since @p t.
///
/// @param t       A time from nowUs().
/// @param spanUs  The span in microseconds.
inline bool elapsedUs(uint32_t t, uint32_t spanUs) {
    return nowUs() - t >= spanUs;
}

/// Gets whether at least @p spanMs milliseconds have passed since @p t.
///
/// @param t       A time from nowUs().
/// @param spanMs  The span in milliseconds.
inline bool elapsedMs(uint32_t t, uint32_t spanMs) {
    return nowUs() - t >= spanMs * 1000;
}

/// Gets whether time @p a lies before time @p b, also across the wrap of nowUs().
///
/// @param a  The first time.
/// @param b  The second time.
inline bool before(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) < 0;
}
