# General design

The rules here hold across the whole firmware. The figures they are checked against live in the design documents and are read from there: [`docs/parts/`](../docs/parts/), one directory per subsystem.

| Subsystem | File |
|---|---|
| IR ball sensing | [`ir-sensing.md`](ir-sensing.md) |
| Break beam | [`break-beam.md`](break-beam.md) |
| Bumpers | [`bumper.md`](bumper.md) |
| Lighting | [`lighting.md`](lighting.md) |
| Audio | [`audio.md`](audio.md) |
| Storage, the SD card | [`storage.md`](storage.md) |
| Hall rotary sensor | [`hall.md`](hall.md) |
| Controls | [`controls.md`](controls.md) |
| Servo | [`servo.md`](servo.md) |

## Firmware abstraction layers

The firmware keeps three layers apart. The drivers work the hardware in their own interrupts and publish what they detect as driver events. Game components represent logical playfield's elements, such as the top roll-over lanes. They may contain multiple different hardware parts. The games hold the rules and talk to the components. Games are organized and run by a game host, as [game-abstraction](game-abstraction.md) describes.

## Non-blocking coding

The main loop carries the host and the game logic. The drivers catch every event in their own interrupts, so a slow loop only delays the game's reaction. A loop that stays away longer lets the event queues fill up and eventually may lead to dropped events or and the watchdog restarts the machine. So nothing in the loop may block, and the loop has to come round quickly.

A driver hands its waiting to hardware. Where a transfer, a conversion or a frame takes time, a DMA channel or the peripheral itself moves the data and raises an interrupt when it is done, so the processor spends that time on something else. A job that cannot be handed over is split into pieces short enough that the loop still comes round in time.

## The event queue

Subsystems are not meant to communicate directly with each other. Instead they publish their events to a shared event queue. Each event is stamped with the moment of detection. The main loop drains that queue at a single point. A game then handles them as pinball events, which [game-abstraction](game-abstraction.md#playfield-components) describes.

[`input-handling.md`](input-handling.md) describes the event queue in more detail.

## Drivers

Drivers work in their own interrupts and do not rely on the main loop to update them.

Drivers report their state changes, not their current state. A state that persists over time produces no further event, and one physical change yields one event.

The current state stays queryable.

A driver whose `begin()` failed does nothing when called. This for example allows a game to run without an SD card or display installed.

## Interrupts

Detection may sit in an interrupt, the game logic never does.

What cannot wait for the loop stays inside its own subsystem and reports to the event queue afterwards. E.g., a top bumper is sensed and triggered by the interrupt. The game logic gets it through the event queue, as a pinball event of the bumpers' component.

An interrupt that has to be on time gets a higher priority than one that can wait. Only one handler runs at a time, and while it runs every interrupt of the same or lower priority waits for it to finish.

Every interrupt the drivers use sits at the priority below. A lower number is a higher priority, and the Teensy starts every interrupt at 128.

| Interrupt | Driver | Deadline | Priority |
|---|---|---|---|
| FlexPWM3.1 compare, starting the read block | IR ball sensing | microseconds, the read block has to end inside its phase | 64 |
| The interrupt after each SPI conversion | IR ball sensing | microseconds | 64 |
| Pin interrupts, one IRQ shared by every pin | Bumpers, break beam, controls | milliseconds | 96 |
| `IntervalTimer`, ending the solenoid pulls | Bumpers | milliseconds | 96 |

*Remarks: The pin interrupts and the bumpers' release timer share a level, so neither can interrupt the other and the bumper driver needs no lock. A further `IntervalTimer` therefore has to ask for 96 or a larger number.*

## The watchdog

The watchdog is a hardware timer in the i.MX RT that resets the Teensy unless the firmware feeds it within a specified time. It keeps counting when the firmware has crashed, so it also catches a failure that stops every line of code. The watchdog is to be used to guard against major malfunctions which are not recoverable, critical to the system or would harm hardware.

The watchdog time can be only as long as the shortest guard it has to ensure.

The main loop feeds it once per pass, and only while every condition it should guard holds. Feeding the watchdog belongs in the main loop because interrupts keep running while the loop hangs, and a watchdog fed from one would never fire.

The following components are guarded by the watchdog:

| Component | Condition for feeding | Shut-off Time Constraint |
|---|---|---|
| Bumper Driver | no coil has been on for longer than 51 ms, which `overdue()` checks, see [`bumper.md`](bumper.md) | ≤ 5 s |

```cpp
#include "Watchdog_t4.h"         // the WDT_T4 library, github.com/tonton81/WDT_T4

WDT_T4<WDT1> watchdog;           // the i.MX RT's WDOG1

void setup() {
    // ... every driver's begin() first, so a slow start-up cannot reset the Teensy
    WDT_timings_t config;
    config.timeout = 1;          // in seconds
    watchdog.begin(config);
}

void loop() {
    // ... the rest of the pass
    if (!bumpers.overdue()) watchdog.feed();   // each guarded component adds its condition
}
```

## The main program

The main file sets the machine up and runs the main loop. 

`setup()` first runs every driver's `begin()` in the correct order. Then it adds all game components to the game host and starts the host. Lastly, it starts the watchdog.

`loop()` knows no game. In each pass the event queue is dispatched to the host with `dispatch()` and updates the game host with the current time using `update()`. Lastly it feeds the watchdog given that every guard holds.

The following is a basic sketch of the machine's main file:

```cpp
EventQueue       events;

// drivers
IrSensing        ir;
BreakBeam        drain;
BumperDriver     bumpers;
Storage          storage;
AudioDriver      audio;
Lighting         lights;
Display          display;
HallRotaryDriver hallRotary;
// other drivers

// game components
TopLanes         topLanes{ir, lights};   // a component, built on its drivers' parts
// other game components

GameHost         host;
WDT_T4<WDT1>     watchdog;

void setup() {
    
    // storage first, so every later driver can read its settings from the card
    storage.begin(events.attach(storageQueue, kQueueDepth));

    // bumpers next so we can drive the coils at startup
    bumpers.begin(events.attach(bumperQueue, kQueueDepth));
    
    // light ahead of ir for better calibration
    lights.begin();
    ir.begin(events.attach(irQueue, kQueueDepth));
    drain.begin(events.attach(drainQueue, kQueueDepth));
   
    // display must be initialized before hall sensor
    const bool screen = display.begin(events.attach(displayQueue, kQueueDepth));
    hallRotary.begin(events.attach(hallRotaryQueue, kQueueDepth));

    audio.begin(events.attach(audioQueue, kQueueDepth));

    // other drivers...

    // every component, so the host routes its parts' driver events through it
    host.add(topLanes);
    // other components...

    // the components, then the services no component owns
    static Machine machine{topLanes, audio, storage, screen ? &display : nullptr};
    host.begin(machine, screen && display.touch());

    startWatchdog(watchdog);
}

void loop() {
    DriverEvent batch[kBatch];
    const size_t n = events.read(batch, kBatch);
    for (size_t i = 0; i < n; ++i) host.dispatch(batch[i]);

    host.update(micros());

    if (/** watchdog conditions **/) watchdog.feed();
}
```