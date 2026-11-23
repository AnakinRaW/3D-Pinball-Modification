// Puts pictures on the ILI9341 of the Adafruit 2090 over its 8-bit bus. A caller hands over a
// rectangle of RGB565 pixels and goes on at once. FlexIO3 clocks the bytes out 32 at a time while
// an interrupt refills it, so a whole screen takes 12.8 ms on the bus and the processor stays free
// for other work meanwhile. The pixels go out in the direction the panel builds its picture, so a
// moving picture seldom shows half of the frame before.
//
// The driver also sets the panel up again after the module's supervisor has reset it, without the
// caller waiting for the panel.
//
// Design: docs/firmware/drivers/display.md.
#pragma once
#include <Arduino.h>
#include "drivers/driver-base.h"

class Display : public Driver {
public:
    using Sent = teensy::inplace_function<void(void), 16>;   // a lambda capturing a pointer fits

    static constexpr uint16_t WIDTH = 320;    // the panel's pixels across in landscape
    static constexpr uint16_t HEIGHT = 240;   // its rows down
    static constexpr uint16_t STEP_PX =
        16;   // the pixels of one burst, a rectangle's x and width step
    /// The bus time of a whole screen, 320 by 240 pixels of two bytes at 83.3 ns a byte.
    static constexpr uint32_t FRAME_BUS_US = 12800;
    /// The processor clocks one refill of a burst costs the caller at most, measured.
    static constexpr uint32_t REFILL_CLOCKS = 255;
    /// The processor clocks one burst lasts on the bus, 2.67 µs at 600 MHz.
    static constexpr uint32_t BURST_CLOCKS = 1600;

    /// Sets up FlexIO3 and the panel, leaves the screen black and attaches to the driver tick.
    ///
    /// It waits until 285 ms after power-up, when the module has let go of the panel, and takes
    /// about 0.3 s more. A second call returns what the first returned.
    ///
    /// @return  False when the driver tick is full, or when FlexIO3's clock cannot run at 120 MHz:
    ///          PLL3 runs at another rate, or FlexIO2 shares the clock at another rate.
    bool begin() override;

    /// Sets the panel up again after the module's supervisor has reset it.
    ///
    /// restarting() is true from the call until the panel shows again, which takes about 0.25 s.
    /// A rectangle sent meanwhile waits and goes out once the panel is back. A rectangle on its
    /// way finishes first. Does nothing before begin() has succeeded.
    ///
    /// @note  Returns at once. The main loop calls it.
    void restart();

    /// Gets whether a restart still holds the panel.
    bool restarting() const;

    /// Sends a rectangle of pixels to the screen and returns at once.
    ///
    /// The pixels lie row by row, each pixel two bytes of RGB565, high byte first. The screen
    /// keeps them until a later rectangle covers them.
    ///
    /// @param x       The left edge, a multiple of STEP_PX.
    /// @param y       The top edge.
    /// @param width   The width, a multiple of STEP_PX.
    /// @param height  The height.
    /// @param pixels  The pixels. They stay in place and unchanged until @p sent runs.
    ///                From the PSRAM a rectangle goes out slower than its bus time, so they
    ///                lie in RAM1 or RAM2.
    /// @param sent    Runs in the driver's interrupt once the last pixel has left the bus. It has
    ///                to be short and must never wait.
    /// @return        False, with nothing sent, before begin() has succeeded, for a rectangle off
    ///                the 16-pixel steps, empty or beyond the screen, and while a rectangle is
    ///                still on its way.
    /// @note          The main loop calls it.
    bool send(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* pixels,
              Sent sent);

private:
    enum class Step : uint8_t { IDLE, RESET, SLEEP_OUT };   // where a restart stands

    /// Takes the next step of a restart.
    ///
    /// @return  Whether the restart still holds the panel.
    /// @note    Runs in the FlexIO3 interrupt.
    bool stepRestart();

    /// Loads the rectangle's next burst into FlexIO3.
    ///
    /// @note  Runs in the FlexIO3 interrupt.
    void loadNext();

    /// Runs in the FlexIO3 interrupt: starts a rectangle, refills its bursts and ends it.
    void serve();
    static void serveActive();

    /// Raises the FlexIO3 interrupt while a step of a restart waits.
    ///
    /// @note  Runs on the driver tick.
    void tick();

    bool begun = false;
    bool begunOk = false;

    // the rectangle; send() writes it while sending is false, and the interrupt reads it after
    const uint8_t* column = nullptr;   // the top pixel of the column that goes out
    const uint8_t* at = nullptr;       // the next pixel to load
    uint32_t rowsLeft = 0;             // the pixels still to load of that column
    uint32_t left = 0;                 // the bursts still to load
    uint16_t rectX = 0;
    uint16_t rectY = 0;
    uint16_t rectWidth = 0;
    uint16_t rectHeight = 0;
    Sent sent;
    volatile bool sending = false;   // a rectangle waits or is on its way
    volatile bool running = false;   // its bursts are on the bus

    volatile uint32_t restarts = 0;        // restart() counts it up, the main loop alone
    volatile uint32_t restartsTaken = 0;   // the interrupt takes it up to restarts
    volatile Step step = Step::IDLE;
    uint32_t stepUs = 0;   // nowUs() when the step began

    static Display* active;   // the display FlexIO3's interrupt serves
};
