// Sends the colors of up to eight WS2812B LED chains from a Teensy 4.x, one chain per pin, all
// chains at once. FlexIO2 times every bit in hardware, so the LEDs show the right colors however
// busy the processor is.
//
// Each chain is a lane. Its bits leave through a FlexIO2 shifter, one timer clocks every lane, and
// DMA hands each shifter its next word, so no code runs while a frame goes out.
// firmware/lib/ws2812-color-encoder computes the words. After the longest lane the lines stay low
// for longer than a WS2812B needs to take over the frame.
//
// The DMA channel takes the highest priority of PJRC's DMAChannel channels, so a channel that asks
// without a pause cannot keep it from refilling the shifters.
//
// A lane may sit on any pin that reaches FlexIO2: 6 to 13 and 32 on the Teensy 4.0, 34 to 37
// besides on the 4.1, and 40 to 45 besides on the MicroMod. One FlexIO2 and one DMA channel serve
// every object of this class, so a program sends through one object at a time.
//
// Design: docs/firmware/drivers/lighting.md.
#pragma once
#include <Arduino.h>
#include <debug-log.h>

class Ws2812Flexio2 {
public:
    static constexpr uint8_t MAX_LANES = 8;   // RM FlexIO: FlexIO2 has eight shifters

    // one chain: the Teensy pin its data leaves on and its LEDs
    struct Lane {
        uint8_t pin;
        uint16_t leds;
    };

    // Takes up to MAX_LANES lanes. Each lane needs a different pin, and every pin has to reach
    // FlexIO2. Takes the frame's words from the heap, sets up FlexIO2 and its DMA channel once and
    // hands the pins of the lanes with LEDs to FlexIO2. A lane without LEDs leaves its pin as it
    // is. Returns false when a lane is refused, the heap has no room or no DMA channel is free.
    // It also returns false when FlexIO2's clock cannot run at 120 MHz, which the bit timing needs:
    // PLL3 runs at another rate, or FlexIO3 is enabled on the shared clock at another rate. A later
    // call keeps the lanes of the first and hands their pins over again.
    bool begin(const Lane* lanes, uint8_t count);

    // Gets whether a frame still goes out.
    bool busy() const;

    // Encodes each lane's colors, stored red, green, blue as FastLED's CRGB stores them and
    // scaled by the brightness, and starts sending them. rgb holds one pointer for each lane, in
    // the order begin() took the lanes. Returns at once. Returns false, and sends nothing, before
    // begin(), without LEDs and while the last frame still goes out.
    bool send(const uint8_t* const* rgb, uint8_t brightness);

private:
    bool allocated = false;
    uint8_t laneCount = 0;
    uint8_t pins[MAX_LANES] = {};
    uint8_t signals[MAX_LANES] = {};   // the FlexIO2 signal of each lane's pin
    uint16_t counts[MAX_LANES] = {};
    uint32_t* words = nullptr;   // the frame's words, the lanes' alternating, in RAM2
    uint32_t wordCount = 0;      // the words of the longest lane and the latch, per lane
};
