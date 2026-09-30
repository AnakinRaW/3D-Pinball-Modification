# Rotating seal component

The component turns the rod's movements into the event a game uses of the rotating seal. The [magnetic rotary sensor driver](../magnetic-rotary.md) reports where the rod stands and when it comes to rest, and the host hands those events to this component alone, as [`game-abstraction.md`](../game-abstraction.md#playfield-components) describes.

## Requirements

The game has the following requirements to this component: 

The game wants to ...
- know whether the seal moved as the result of a ball shot. Non-shot-based movements such as caused from vibration should be ignored.
- know the direction of movement
- set the minimum angle a rotation is treated as a shot
- know whether the seal completed a full turn and how many turns there had been in the past.
- reset the turn counter and current turn completion level

The machine's vibration, the rod's play and a nudge short of the set angle count for nothing.

## Pinball event

| Event | When | Payload |
|---|---|---|
| `SealShot` | The seal stands still again after a movement from rest that turned it at least the set angle | The whole angle of that movement, in whole degrees, positive clockwise and negative counterclockwise, seen from above |

A movement begins when the seal leaves its resting position and ends with the driver's `RotorStopped`. The component reads each movement from the `RotorStopped` event. Therefore a shot's angle is the difference between the resting position that `RotorStopped` carries and the one reported before. The component is initialized with the driver's start-up position, which it sets as the first reported resting position without ever reporting it. 

After a `SealShot`, `completedTurn()` tells whether that shot completed a full turn. The turn always runs in the shot's own direction, so the sign of the `SealShot` gives it. `turns()` counts the full turns of all shots since `reset()`.

The game sets the angle a shot needs with `setShot()`. Vibration inside the driver's dead band starts no movement, and a movement short of the set angle is dropped, so neither adds to the turns.

## Interface

```cpp
// the seal on the rotating assembly, read by the magnetic rotary sensor
class RotatingSeal : public Component {
public:
    // turns each RotorStopped whose movement reached the set angle into a SealShot
    bool     translate(const DriverEvent& in, PinballEvents& out) override;
    void     setShot(uint16_t degrees);      // sets the angle a movement has to turn the seal to count as a shot
    bool     completedTurn() const;          // gets whether the last shot completed a full turn
    int32_t  turns() const;                  // gets the full turns of all shots since reset(), positive clockwise
    void     reset();                        // clears the turns, the progress toward the next turn and a shot in progress
};
```
