# General design

Rules that hold across the whole firmware. The figures they are checked against live in the design documents and are read from there: [`docs/parts/`](../docs/parts/), one directory per subsystem.

What one driver alone has to keep sits with that driver:

| Subsystem | File |
|---|---|
| IR ball sensing | [`ir-sensing.md`](ir-sensing.md) |
| Lighting | [`lighting.md`](lighting.md) |

## Nothing in the main loop may block

The main loop carries the game logic, the sensing, and IO such as displays, audio, LEDs. The sensing is the sensitive part, because an event can only be caught while it is happening. So nothing in the loop may block, the loop has to come round quickly.

**A driver's waiting belongs to hardware, not to the firmware.** Where a transfer, a conversion or a frame takes time, the peripheral or a DMA channel carries it and the core is told when it is done. What cannot be handed over is split into pieces short enough to fit between two passes.

## The message bus

Subsystems are not meant to communicate directly with each other. Instead they send messages and events to a shared message bus. Each event is stamped with the moment of detection. The main loop drains the bus at a single point. The game logic then decides how to handle the event.

[`input-handling.md`](input-handling.md) describes the message bus in more detail.

## Interrupts

Detection may sit in an interrupt, the game rules never do.

- **What cannot wait for the loop stays inside its own subsystem** and reports to the bus afterwards. E.g., a top bumper is sensed and triggered by the interrupt. The game logic gets the event through the message bus.
- **An interrupt that has to be on time gets a higher priority than one that can wait.** Only one handler runs at a time, and while it runs every interrupt of the same or lower priority waits for it to finish. Each subsystem's file names its own deadlines.