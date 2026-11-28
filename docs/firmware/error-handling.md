# Error handling

The firmware handles errors on two levels. A device is the hardware a driver works, such as a sensor, the solenoids or the SD card. When a device stops working, its driver notices it, the fault is reported, and the machine goes on running without that device. When the firmware itself hangs, or would harm the hardware by keeping a coil on, the watchdog restarts the Teensy.

## Device faults

The device monitor is a firmware component that attaches to the [driver tick](driver-design.md#driver-intervaltimer) with a period of 100 ms and reports every fault. Each driver notes when its device parts last worked and counts the errors it notices. A part counts as failed once it has been out of order for longer than the driver allows, or has failed as often as the driver allows. A driver that counts time uses [`elapsedMs()`](general-design.md#time-measurement) to measure for how long the device part is inoperative.

The main task registers each watched driver with the monitor right after the driver's `begin()`. On every tick the device monitor calls `Fault failed()` of every registered driver. The driver returns a `Fault`, which holds a bitmask with one bit per failed part and the driver's error code. The bits count from the LSB, and the error code is 0 for a driver without error codes.

For every part whose bit changed, the monitor publishes `DEVICE_FAILED` or `DEVICE_RECOVERED` into its queue of the event queue. The event's source packs the device and the part. The device is an entry of [`Device`](driver-design.md#devices). The payload of `DEVICE_FAILED` is the driver's error code, and `DEVICE_RECOVERED` has no payload. When the queue is full, the part keeps the state it was last reported in, and the next check tries again. The monitor's queue lies in RAM2.

[`firmware/src/core/device-monitor.h`](../../firmware/src/core/device-monitor.h) holds the monitor.

## Watchdog

The watchdog is a hardware timer in the i.MX RT that resets the Teensy unless the firmware feeds it within a specified time. It keeps counting when the firmware has crashed, so it also catches a failure that stops every line of code. The watchdog is to be used to guard against major malfunctions which are not recoverable, critical to the system or would harm hardware.

The watchdog time can be only as long as the shortest guard it has to ensure.

The main loop feeds it once per pass, and only while every condition it should guard holds. Feeding the watchdog belongs in the main loop because interrupts keep running while the loop hangs, and a watchdog fed from one would never fire.

The watchdog starts right after the storage driver's mount, ahead of the solenoids. Until the main loop runs, FreeRTOS's idle task feeds it under the same conditions.

The following components are guarded by the watchdog:

| Component | Condition for feeding | Shut-off Time Constraint |
|---|---|---|
| Solenoid Driver | no coil has been on for longer than 31 ms, which `overdue()` checks, see [`solenoid.md`](drivers/solenoid.md) | ≤ 5 s |

A component joins the guards through `watch()`, which the main task calls before the watchdog's `begin()`. The solenoid driver joins as `OverdueCoils`, whose `overdue()` the guard asks before every feed. Without it, the watchdog guards against a hang alone.

[`firmware/src/core/watchdog.h`](../../firmware/src/core/watchdog.h) holds the watchdog, and [`firmware/src/core/watchdog-guard.h`](../../firmware/src/core/watchdog-guard.h) the guards that decide each feed.

### Restart causes

After every restart other than a power-up, the log on the card states its cause. PJRC's `CrashReport` tells which reset it was, such as the watchdog or a crash, and where a crash happened. Before the watchdog restarts the Teensy, the firmware notes in `CrashReport` why it went unfed. It writes the same note into RAM2, which a restart leaves as it is, so the next start writes the cause into the log in words.

| Cause | Note |
|---|---|
| A coil stays on too long | the coils |
| The firmware hangs | the address of the running code |

Right after the logger's `begin()`, the main task writes the report into the log.

On the bench a hang with interrupts on raised the warning 1.5 s after the last feed, and the report named the hang and its address. A hang with interrupts off restarted the Teensy without the warning, so its report names only the watchdog.
