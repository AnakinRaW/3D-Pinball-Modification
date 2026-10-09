# General design

This document describes the general concepts of the firmware, which apply to all subsystems of this machine.

| Subsystem | File |
|---|---|
| IR ball sensing | [`drivers/ir-sensing.md`](drivers/ir-sensing.md) |
| Break beam | [`drivers/break-beam.md`](drivers/break-beam.md) |
| Solenoids | [`drivers/solenoid.md`](drivers/solenoid.md) |
| Lighting | [`drivers/lighting.md`](drivers/lighting.md) |
| Audio | [`drivers/audio.md`](drivers/audio.md) |
| Storage, the SD card | [`drivers/storage.md`](drivers/storage.md) |
| Magnetic rotary sensor | [`drivers/magnetic-rotary.md`](drivers/magnetic-rotary.md) |
| Controls | [`drivers/controls.md`](drivers/controls.md) |
| Servo | [`drivers/servo.md`](drivers/servo.md) |

## Firmware abstraction layers

The firmware keeps three layers apart.

The drivers work the hardware in their own interrupts and publish what they detect as driver events.

The main loop carries the whole game logic. It lays out the game rules and orchestrates most of the hardware in reaction to what happens on the playfield. Game components represent logical elements of the playfield, such as the top roll-over lanes, and may combine several hardware parts. They run in the main loop as well.

A service a game uses is a component too, such as the [file system](components/file-system.md) on the SD card. The games hold the rules and talk to the components. A game host organises and runs the games, as [game-abstraction](game-abstraction.md) describes.

## Event queue

Every driver publishes its events, each stamped with the moment of detection, to one shared queue. The main loop drains it at one point. [`input-handling.md`](input-handling.md) describes the queue.

## Drivers

[`driver-design.md`](driver-design.md) describes how every driver works.

## Execution model

The ball moves fast, so nothing may hold up the detection and reaction to these events. Every driver therefore works in its interrupts. An interrupt notes an event the moment it happens and publishes it, whatever the main loop is doing, so a slow main loop only delays the game's reaction and never the detection.

The SD card driver is one exception because the library SdFat internally waits for every call until the card has finished. The SD specification allows a card up to 500 ms for a write and such a wait should not happen in either a driver interrupt or the main loop.

The solution to this is using [freertos-teensy](https://github.com/tsandmann/freertos-teensy), a small real-time scheduler for microcontrollers. FreeRTOS runs several tasks side by side on the one processor and organizes scheduling using SysTick and PendSV.

The firmware runs two tasks, the main task with priority 1 for the main loop and the storage task with priority 2 for the SD card (In FreeRTOS a lower priority number means lower priority). While SdFat waits for the card, the main loop runs, and the card's interrupt wakes the storage task the moment a transfer ends. The drivers run in neither task, and their interrupts run ahead of both. Starting FreeRTOS sets every interrupt to priority 128, so each driver sets its priorities in its `begin()`.

## Interrupts

A lower number is a higher priority, and a handler makes every interrupt of the same or a lower priority wait.

| Driver and job | Interrupt | Deadline | Calls FreeRTOS | Priority |
|---|---|---|---|---|
| Watchdog, warning before a restart | WDOG1's warning interrupt | none, the restart follows | no | 32 |
| IR ball sensing, starting a sensor reading | QuadTimer3 compare | microseconds, the read block has to end inside its phase | no | 64 |
| IR ball sensing, taking each conversion's result | LPSPI4's receive interrupt | microseconds | no | 64 |
| Solenoids, break beam and controls, reacting to a contact, the beam or the switch | the pin interrupt, one for every pin | milliseconds | no | 96 |
| Rotary sensor, solenoids, device monitor and display, their periodic work | `IntervalTimer`, the driver tick | its next tick | no | 96 |
| Audio, refilling the half of the output buffer that has played | I²S2 DMA | half an audio block | no | 128 |
| Lighting, refilling the half of the LED bit buffer that has gone out | OctoWS2811's DMA | the next half of the bit buffer | no | 128 |
| FreeRTOS's limit, from which on an interrupt may call FreeRTOS | | | | 208 |
| Audio, computing the next block of samples | the Audio library's update | the next audio block | yes | 208 |
| Display, refilling its bus | FlexIO3 | none, a late refill only pauses the bus | yes | 224 |
| Storage, waking the storage task at the end of a transfer | the SD controller's interrupt | none, the storage task waits for it | yes | 240 |
| FreeRTOS, its tick and its task switch | SysTick and PendSV | its next tick, 1 ms | yes | 240 |

FreeRTOS holds back the interrupts that call it for a moment while it changes its task lists. The more urgent interrupts never wait for it.

## Exclusive peripherals

Each peripheral below belongs to one driver, and no other code or library may use it. [`pin-assignment.md`](../pin-assignment.md#shared-resources) lists their pins.

| Peripheral | Owner | What the driver does with it |
|---|---|---|
| SPI | IR ball sensing | reads the converters |
| QuadTimer3, all four channels | IR ball sensing | counts the sensor phases and starts the read block |
| QuadTimer4 | Lighting | OctoWS2811 times the LED data |
| I²S2 | Audio | the Audio library hands every block to the amplifier |
| Wire | Magnetic rotary sensor | runs the bus itself after `begin()` |
| FlexPWM1.1 | Servo | PWMServo repeats the servo pulse |
| FlexIO3 | Display | clocks the 8-bit bus out to the display |
| GPT2 | Time measurement | counts the microseconds that `nowUs()` reads |

The IR driver takes all four channels of QuadTimer3, because they share one interrupt ([i.MX RT1060 reference manual](https://www.pjrc.com/teensy/IMXRT1060RM_rev3.pdf), Rev. 3, interrupt 135).

## Time measurement

The firmware takes its times from GPT2, a hardware counter that counts microseconds from the board's crystal. `setup()` starts it before anything takes a time, and `nowUs()` reads it. Time measurement in this firmware goes through `elapsedUs()` or `elapsedMs()`, and every comparison of two times goes through `before()`. 

The two `elapsed` functions read the noted time before the current one, so an interrupt in between cannot make the difference negative. `nowUs()` wraps every 71.6 minutes. `before()` keeps two times in order across the wrap.

```cpp
// gets the microseconds GPT2 has counted since setup() started it
inline uint32_t nowUs() { return GPT2_CNT; }

// gets whether at least span microseconds have passed since t, a time from nowUs()
inline bool elapsedUs(uint32_t t, uint32_t span) { return nowUs() - t >= span; }

// gets whether at least span milliseconds have passed since t, a time from nowUs()
inline bool elapsedMs(uint32_t t, uint32_t span) { return nowUs() - t >= span * 1000; }

// gets whether time a lies before time b, also across the wrap of nowUs()
inline bool before(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }
```

## Main program

`setup()` starts the [clock](#time-measurement), creates the main task and starts FreeRTOS. The main task then runs, in this order:

1. the [watchdog](error-handling.md#watchdog), so it guards the coils from their first pull;
2. the solenoids' `begin()`, which pulls every coil once, so the machine shows at once that it starts;
3. the storage driver's `begin()`, so later drivers can read their settings from the card;
4. the logger's `begin()`, and then the [report of the last restart](error-handling.md#restart-causes);
5. the `begin()` of every other driver, the lighting's ahead of the IR's;
6. the game host's `begin()`;
7. the main loop, which hands the queued events to the host, updates the host, the logger and the screen, and feeds the watchdog.
