// What the firmware's code takes from the Teensy core's Arduino.h, for the simulator: the fake of
// the computer tests, whose GPT2 counter and fakeMillis the simulator sets before every pass of the
// main loop. FastLED's WebAssembly header declares millis() with C linkage, so the fake's inline
// millis() takes another name here and millis() is the C function fastled-platform.cpp defines.
#pragma once
#include <cstdint>
#define millis fakeArduinoMillis
#include "../../tests/fakes/Arduino.h"
#undef millis

extern "C" uint32_t millis();
