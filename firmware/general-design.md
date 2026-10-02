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
| Magnetic rotary sensor | [`magnetic-rotary.md`](magnetic-rotary.md) |
| Controls | [`controls.md`](controls.md) |
| Servo | [`servo.md`](servo.md) |

## Firmware abstraction layers

The firmware keeps three layers apart. The drivers work the hardware in their own interrupts and publish what they detect as driver events. Game components represent logical playfield's elements, such as the top roll-over lanes. They may contain multiple different hardware parts. The games hold the rules and talk to the components. Games are organized and run by a game host, as [game-abstraction](game-abstraction.md) describes.

## Non-blocking coding

The main loop carries the host and the game logic. The drivers catch every event in their own interrupts, so a slow loop only delays the game's reaction. A loop that stays away longer lets the event queues fill up and eventually may lead to dropped events and the watchdog restarts the machine. So nothing in the loop may block, and the loop has to come round quickly.

## The event queue

Subsystems are not meant to communicate directly with each other. Instead they publish their events to a shared event queue. Each event is stamped with the moment of detection. The main loop drains that queue at a single point. A game then handles them as pinball events, which [game-abstraction](game-abstraction.md#playfield-components) describes.

[`input-handling.md`](input-handling.md) describes the event queue in more detail.

## Drivers

[`driver-design.md`](driver-design.md) describes how every driver works: its interrupts and the driver tick.

## Interrupts

An interrupt that has to be on time gets a higher priority than one that can wait. Only one handler runs at a time, and while it runs every interrupt of the same or lower priority waits for it to finish.

Every interrupt the drivers use sits at the priority below. A lower number is a higher priority, and the Teensy starts every interrupt at 128.

| Interrupt | Driver | Deadline | Priority |
|---|---|---|---|
| FlexPWM3.1 compare, starting the read block | IR ball sensing | microseconds, the read block has to end inside its phase | 64 |
| The interrupt after each SPI conversion | IR ball sensing | microseconds | 64 |
| Pin interrupts, one IRQ shared by every pin | Bumpers, break beam, controls | milliseconds | 96 |
| `IntervalTimer`, the driver tick | Magnetic rotary sensor, bumpers, device monitor, storage | its next tick | 96 |
| I²S2 DMA, handing a block to the amplifier | Audio | the next audio block | 128 |
| The Audio library's update, computing a block | Audio | the next audio block | 208 |
| The SD controller's interrupt, ending one card transfer and starting the next | Storage | before a stream's buffer runs dry | 240 |

Two interrupts are shared by several drivers, the pin interrupt and the `IntervalTimer` interrupt. All pin interrupts run on one IRQ, and `setup()` sets its priority once. All four `IntervalTimer`s share one interrupt as well, which runs at the highest priority any of them asks for. The [driver tick](driver-design.md#driver-intervaltimer) is the only `IntervalTimer` the firmware uses.

## Time measurement

Every time measurement compares a noted time with the current time with `micros()` or `millis()`. If the current time is read before the note, an interrupt in between can note a later time. The difference then turns negative, and as an unsigned number it reads as a very long time. `elapsedUs()` and `elapsedMs()` get the note first and read the current time after it, so the difference cannot turn negative. Every measurement goes through one of them.

```cpp
// gets whether at least span microseconds have passed since t
inline bool elapsedUs(uint32_t t, uint32_t span) { return micros() - t >= span; }

// gets whether at least span milliseconds have passed since t
inline bool elapsedMs(uint32_t t, uint32_t span) { return millis() - t >= span; }
```

## The main program

The main file sets the machine up and runs the main loop. 

`setup()` first runs every driver's `begin()` in the correct order. Then it adds all game components to the game host and starts the host. Lastly, it starts the [watchdog](error-handling.md#the-watchdog).

`loop()` knows no game. In each pass the event queue is dispatched to the host with `dispatch()` and updates the game host with the current time using `update()`. Lastly it feeds the watchdog given that every guard holds.

The following is a basic sketch of the machine's main file:

```cpp
EventQueue       events;
DriverTick       driverTick;             // the drivers attach to it in their begin()
DeviceMonitor    deviceMonitor;          // the drivers register with it in their begin()

// drivers
IrSensing        ir;
BreakBeam        drain;
BumperDriver     bumpers;
Storage          storage;
AudioDriver      audio;
Lighting         lights;
Display          display;
MagneticRotaryDriver rotary;
// other drivers

// game components
TopLanes         topLanes{ir, lights};   // a component, built on its drivers' parts
// other game components

GameHost         host;
WDT_T4<WDT1>     watchdog;

void setup() {
    NVIC_SET_PRIORITY(IRQ_GPIO6789, 96);    // sets the global priority of every pin interrupt

    // storage first, so every later driver can read its settings from the card
    storage.begin();

    // bumpers next so we can drive the coils at startup
    bumpers.begin();
    
    // light ahead of ir for better calibration
    lights.begin();
    ir.begin();
    drain.begin();
   
    const bool screen = display.begin();
    rotary.begin();

    audio.begin();

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