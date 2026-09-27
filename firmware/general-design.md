# General design

The rules here hold across the whole firmware. The figures they are checked against live in the design documents and are read from there: [`docs/parts/`](../docs/parts/), one directory per subsystem.

What one driver alone has to keep sits with that driver:

| Subsystem | File |
|---|---|
| IR ball sensing | [`ir-sensing.md`](ir-sensing.md) |
| Break beam | [`break-beam.md`](break-beam.md) |
| Bumpers | [`bumper.md`](bumper.md) |
| Lighting | [`lighting.md`](lighting.md) |

## Nothing in the main loop may block

The main loop carries the game logic, the sensing, and IO such as displays, audio, LEDs. The sensing is the sensitive part, because an event can only be caught while it is happening. So nothing in the loop may block, the loop has to come round quickly.

**A driver hands its waiting to hardware.** Where a transfer, a conversion or a frame takes time, a DMA channel or the peripheral itself moves the data and raises an interrupt when it is done, so the processor spends that time on something else. A job that cannot be handed over is split into pieces short enough that the loop still comes round in time.

## The event queue

Subsystems are not meant to communicate directly with each other. Instead they publish their events to a shared event queue. Each event is stamped with the moment of detection. The main loop drains that queue at a single point. The game logic then decides how to handle the event.

[`input-handling.md`](input-handling.md) describes the event queue in more detail.

## Drivers

A driver reports state changes, not its current state. A state that persists over time produces no further event, and one physical change yields one event.

The current state stays queryable.

## Interrupts

Detection may sit in an interrupt, the game logic never does.

What cannot wait for the loop stays inside its own subsystem and reports to the event queue afterwards. E.g., a top bumper is sensed and triggered by the interrupt. The game logic gets the event through the event queue.

An interrupt that has to be on time gets a higher priority than one that can wait. Only one handler runs at a time, and while it runs every interrupt of the same or lower priority waits for it to finish.

Every interrupt the drivers use sits at the priority below. A lower number is a higher priority, and the Teensy starts every interrupt at 128.

| Interrupt | Driver | Deadline | Priority |
|---|---|---|---|
| FlexPWM3.1 compare, starting the read block | IR ball sensing | microseconds, the read block has to end inside its phase | 64 |
| The interrupt after each SPI conversion | IR ball sensing | microseconds | 64 |
| Pin interrupts, one IRQ shared by every pin | Bumpers, break beam | milliseconds | 96 |
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