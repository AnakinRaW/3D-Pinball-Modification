# Features

The modification keeps every feature of the stock machine and builds on it.

*A bold text marks something that is new to the machine or changes it markedly.*

## Games

| Feature | Stock machine | Modification |
|---|---|---|
| **Multiball** | None. | **Two balls may be in play at the same time.** [Drain](firmware/components/drain.md) |
| **Ball save** | None. | **A ball that drains soon after its launch comes back** into the plunger lane. [Example game](firmware/game-abstraction.md#an-example-game) |
| **Modes** | | A game splits its rules into **modes**, such as **a skill shot**, the ball save or the multiball. **Several modes run at the same time**, each with its own lights, slide and music. [Modes](firmware/game-abstraction.md#modes) |
| Rules | One built-in game, which the player wins or loses. | **Several games** can be built into the firmware, each with its own rules, sounds and pictures. Each game **keeps its high score**. [Game abstraction](firmware/game-abstraction.md) |

## Playfield

| Feature | Stock machine | Modification |
|---|---|---|
| **Scoop** | None. | **A scoop catches the ball** and holds it until the game **throws it out with a coil**. [Scoop](firmware/components/scoop.md) |
| **Rotating seal** | The seal in the middle of the playfield turns when the ball hits it. | **A shot that turns the seal scores**, by how far and in which direction it turns. [Magnetic rotary sensor](parts/magnetic-rotary/design.md) |
| **Stand-up targets** | None. | The playfield gets **stand-up targets**, and the game sees every hit on one. [Hit targets](parts/contacts/design.md#stand-up-targets) |
| **Tracks** | Three of the four tracks see the ball and score it. | **All four tracks see the ball.** [IR ball sensing](parts/ir-reflective/design.md) |
| **Top lanes** | None. | **Three top lanes** each have their own sensor and their own light. The game sees every ball that rolls through one. [Light layout](parts/lighting/design.md#light-layout) |
| **Inlanes** | Two inlanes run beside the flippers on each side. | **Each of the four inlanes gets a light and a sensor**, so the game sees every ball that rolls through one. |
| Bells | The ball rings two bells. | Each bell becomes **a hit target**, and the game sees every hit on it. [Hit targets](parts/contacts/design.md#hit-targets) |

## Display

| Feature | Stock machine | Modification |
|---|---|---|
| **Screen** | None. | **A 2.8 inch touch display** shows the score, pictures, animations and **videos**, over the whole screen or in a part of it. [Display](parts/display/design.md), [video player](firmware/components/video-player.md) |
| **Touch** | None. | The player **taps the screen**, and a game reacts to it. **A menu** over the running game switches games and changes settings. |

## Lights

| Feature | Stock machine | Modification |
|---|---|---|
| Lamps | Single-colored LEDs light the playfield, switched only in groups. | Every light is **an RGB LED whose color the game sets on its own**. **New lights** mark the tracks, the scoop, the targets, the top lanes and the goals of the game's modes. [Lighting](parts/lighting/design.md) |
| **Effects** | | A light **blinks, pulses, fades or runs along several lights**. The effects of several modes **overlap**. [Lamps](firmware/components/lamps.md) |
| Ambient light | White strips light the sides of the playfield and the ceiling of the diorama. | **RGB strips** at the same places run **effects and palettes** along their whole length. |

## Sound

| Feature | Stock machine | Modification |
|---|---|---|
| Speaker | One sound plays at a time. An effect cuts off the background music, which then starts again from its beginning. | **The music and up to three effects play at once**. The music **goes on where an effect interrupted it**. [Sounds](firmware/components/sounds.md) |
| Sounds | Seven built-in sounds. | **Each game brings its own music and effects** as files on the SD card. **Each mode of a game can play its own music**. |

[`use-cases.md`](use-cases.md) lists every use case in detail, each with the document that realizes it.
