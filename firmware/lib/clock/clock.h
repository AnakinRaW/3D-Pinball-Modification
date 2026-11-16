// The firmware's time measurement of docs/firmware/general-design.md. GPT2 counts microseconds
// from the board's crystal, and nowUs() reads it. Every time in the firmware comes from nowUs(),
// every span goes through elapsedUs() or elapsedMs(), and every comparison of two times through
// before(). nowUs() wraps every 71.6 minutes, and all three keep their answers across the wrap.
#pragma once
#include <Arduino.h>

// Starts GPT2 counting microseconds. setup() calls it before anything takes a time.
void startClock();

// gets the microseconds GPT2 has counted since startClock()
inline uint32_t nowUs() {
    return GPT2_CNT;
}

// gets whether at least spanUs microseconds have passed since t, a time from nowUs()
inline bool elapsedUs(uint32_t t, uint32_t spanUs) {
    return nowUs() - t >= spanUs;
}

// gets whether at least spanMs milliseconds have passed since t, a time from nowUs()
inline bool elapsedMs(uint32_t t, uint32_t spanMs) {
    return nowUs() - t >= spanMs * 1000;
}

// gets whether time a lies before time b, also across the wrap of nowUs()
inline bool before(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) < 0;
}
