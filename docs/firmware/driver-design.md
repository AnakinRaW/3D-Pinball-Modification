# Driver design

Drivers handle the machine's electrical hardware. They work in their own interrupts and do not rely on the main loop to update them. 

The storage driver works in a [task](general-design.md#execution-model) instead because the underlying sdfat driver is blocking.

A driver hands its waiting to hardware. Where a transfer, a conversion or a frame takes time, a DMA channel or the peripheral itself moves the data and raises an interrupt when it is done, so the processor spends that time on something else. A job that cannot be handed over is split into pieces short enough that the loop still comes round in time.

Drivers report their state changes, not their current state. A state that persists over time produces no further event, and one physical change yields one event. Events are published to the [event queue](input-handling.md).

The current state stays queryable regardless.

Device faults reach the event queue through the [device monitor](error-handling.md#device-faults). It asks every driver every 100 ms which parts of its device have failed, and it publishes `DeviceFailed` and `DeviceRecovered` whenever that answer changes.

`begin()` returns false when it fails. Every later call to that driver answers at once with either `false` or empty data. This lets a game run without an SD card or a display installed, for example.

## Driver interface

Every driver implements a `Driver` interface. A driver that reports events holds its queue and attaches it to the [event queue](input-handling.md) in its `begin()` method. `begin()` is called by the [main task](general-design.md#main-program) before the main loop. A driver whose device the [device monitor](error-handling.md#device-faults) watches overrides `failed()` and registers itself in `begin()`.

```cpp
// the interface every driver implements
class Driver {
public:
    // sets the driver up; false when that fails
    virtual bool begin() = 0;

    // gets which parts of the driver's device have failed; the default reports none, for a
    // driver that notes no fault
    virtual Fault failed() const;
};
```

## Devices

`Device` names the unique devices of the machine. A device may contain multiple parts or channels of the same kind, such as the three sensed solenoids or the individual IR reflective channels. However, those parts are not represented by this enum.

```cpp
// the devices of the machine; a device of one part has only part 0
enum class Device : uint8_t {
    RotarySensor,
    BreakBeam,
    Solenoids,
    IrSensing,
    SdCard,
    // ... more devices
    Count,        // the number of devices
};
```

## Interrupts

A driver detects its events inside its interrupts.

Things that cannot wait for the game loop to pick up are handled directly inside their own subsystem and get reported to the event queue afterwards. E.g., the interrupt of a sensed solenoid's contact triggers its coil. The game logic gets the hit through the event queue, as a pinball event of the bumpers' component.

Every interrupt's priority is set in the table of [general-design](general-design.md#interrupts).

### Driver IntervalTimer

The driver tick is one dedicated `IntervalTimer` that runs every 5 ms and serves every driver that needs a regular period. A driver can attach to this driver tick in its own `begin()`, together with a callback function and the period it needs, a multiple of 5 ms.

For example, on every tick the rotary sensor reads values, the solenoid driver checks whether a pull is due to end and the [device monitor](error-handling.md#device-faults) asks the drivers for failed parts every 100 ms. 

The timer starts on the very first attached driver. Attaching does not need to be in any special order. The driver tick calls only the callbacks that are due.

```cpp
// one IntervalTimer, shared by every driver that needs a regular period
class DriverTick {
public:
    using Callback = teensy::inplace_function<void(void), 16>;   // a lambda capturing this fits

    static constexpr uint32_t kPeriodUs = 5000;    // tick period

    // runs fn every periodUs; before the main loop only, and the first call starts the timer. False
    // when every slot is taken, periodUs is no multiple of kPeriodUs or no IntervalTimer is free
    bool attach(Callback fn, uint32_t periodUs);
};

extern DriverTick driverTick;   // the one driver tick, defined in the main file
```
