# General design

Rules that hold across the whole firmware. The figures they are checked against live in the design documents and are read from there: [`docs/parts/`](../docs/parts/), one directory per subsystem.

What one driver alone has to keep sits with that driver:

| Subsystem | File |
|---|---|
| IR ball sensing | [`ir-sensing.md`](ir-sensing.md) |
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

**What cannot wait for the loop stays inside its own subsystem** and reports to the event queue afterwards. E.g., a top bumper is sensed and triggered by the interrupt. The game logic gets the event through the event queue.

**An interrupt that has to be on time gets a higher priority than one that can wait.** Only one handler runs at a time, and while it runs every interrupt of the same or lower priority waits for it to finish. Each subsystem's file names its own deadlines.