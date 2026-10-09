# Solenoid driver

The driver watches the contacts of the three sensed solenoids, fires the coil of whichever one a ball reached, and publishes the hit. A fourth channel drives a solenoid without a contact, an unsensed solenoid, which the game logic fires. The top bumpers are built on the sensed solenoids and the scoop on the unsensed one.

## Sensing and firing

A ball that closes the circuit between the conductive foil and a sensed solenoid's shell pulls that solenoid's sense line high. The driver runs on a pin interrupt on both edges of each sense line. When a line goes high, the handler raises the coil's trigger pin and then publishes the hit.

Firing the sensed solenoids belongs in the handler because a sensed solenoid has to answer the ball quickly and cannot wait for game logic to pick up the event.

The game logic can fire every solenoid as well, and the unsensed one only that way. It can only request a pull with `fire()`, which returns false while the coil is still on or cooling down. It cannot hold a coil on or release it directly. `fire()` blocks every interrupt for the few instructions a pull's start takes, because the pin interrupts start pulls as well.

At start-up `begin()` pulls every coil once, as the stock machine does at power-on. It pulls them one after another, so the supply carries a single coil at a time.

The contacts fire their coils only while they are armed. After `begin()` they are disarmed, and the [host](../game-abstraction.md#host) arms them while a game is played. A disarmed contact publishes no hit either, and `fire()` works either way.

## Driver events

| Field | Content |
|---|---|
| Type | `DriverEventType::SolenoidHit` |
| Source | The sensed solenoid, `0` to `2` |
| Timestamp | `nowUs()` in the handler, the moment the contact closed |
| Payload | None |

## Solenoid protection

A rising edge on a sense line pulls its coil once, even when the contact stays closed. A released coil can fire again only after the cool-down, and a sensed solenoid's contact has to have been open for that long as well.

| Rule | Value |
|---|---|
| Duration of a solenoid pull | ~50 ms |
| Duration of the cool-down phase after a solenoid pull | ~10 ms |

When a coil switches on, the driver notes the time. Every 5 ms the [driver tick](../driver-design.md#driver-intervaltimer) checks how long each coil has been on and switches it off after 45 ms, so a pull lasts 45 to 50 ms. The tick also notes when it switched the coil off, and the cool-down counts from that time. 

The driver always writes the time first and switches the coil second. Whoever looks at a coil then finds the time that belongs to its state.

If the firmware itself fails, the [watchdog](../error-handling.md#watchdog) resets the Teensy. The main loop feeds it only while `overdue()` reports that no coil has been on for longer than 51 ms.

At a firmware reset, the Teensy no longer drives the trigger pins, and the pull-down resistors on the board switch every coil off.

## Device faults

A sensed solenoid counts as failed once its contact has stayed closed for 2 s. This might be caused by a ball resting against a shell or a sense line touching the foil.

## Driver

```cpp
class SolenoidDriver : public Driver {
public:
    // attaches the queue, attaches the release to the driver tick and registers with the
    // device monitor; false when the driver tick or the device monitor is full, and the
    // driver then never energises a coil
    bool begin() override;

    // starts a pull of the coil; false while it is still on or cooling down
    bool fire(uint8_t coil);

    // arms the contacts of the sensed solenoids, or disarms them with false; a disarmed contact
    // neither fires its coil nor publishes a hit. Disarmed after begin()
    void arm(bool armed);

    // gets the coils that have been on for longer than 51 ms, one bit each; the main loop feeds
    // the watchdog only while this is 0
    uint8_t overdue() const;

    // the state the game logic may query between events
    bool ballOn(uint8_t solenoid) const;

    // gets which sensed solenoids have failed, one bit each: a contact closed for 2 s; no error code
    Fault failed() const override;
};
```
