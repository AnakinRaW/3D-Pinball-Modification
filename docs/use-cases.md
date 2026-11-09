# Use cases

**Not covered** marks a use case that no document realizes yet, followed by what is missing.

## Playing a ball

| Use case | Realized in |
|---|---|
| At power-up the machine loads the game that ran last, or the first game when none ran before. | [Host](firmware/game-abstraction.md#host), [game host tests](firmware/testing.md#game-host) |
| A game is loaded anew once its post-game phase has ended. | [Host](firmware/game-abstraction.md#host), [game lifecycle](firmware/game-abstraction.md#game-lifecycle) |
| A loaded game starts once its start conditions hold, as a rule once every ball lies in the drain's store. | [Game lifecycle](firmware/game-abstraction.md#game-lifecycle) |
| A lane sensor sees even the fastest ball, one the plunger fires at full strength. | [IR ball sensing](parts/ir-reflective/design.md#requirements), [phase length](firmware/drivers/ir-sensing.md#phase-length) |
| The machine sees a ball resting at a position for as long as it rests there. The game learns how long it rested. | [IR ball sensing](parts/ir-reflective/design.md#requirements), [IR driver events](firmware/drivers/ir-sensing.md#driver-events) |
| Room light and the flicker of LED lamps never count as a ball. | [IR driver](firmware/drivers/ir-sensing.md#what-the-driver-does), [IR ball sensing](parts/ir-reflective/design.md#driver) |
| The game knows the order of everything the ball did and the time between two hits. | [Input handling](firmware/input-handling.md#requirements) |
| The game asks at any moment whether a ball sits at a sensor, in the beam or against a bumper. | [IR ball sensing](parts/ir-reflective/design.md#design-1), [break beam driver](firmware/drivers/break-beam.md#driver), [solenoid driver](firmware/drivers/solenoid.md#driver) |
| The game sees a ball roll through each top lane. | [Playfield components](firmware/game-abstraction.md#playfield-components) |
| A top bumper kicks a ball that touches it. A ball can bounce back and forth between the bumpers. | [Solenoids](parts/solenoid/design.md#requirements), [solenoid driver](firmware/drivers/solenoid.md#sensing-and-firing) |
| The bumpers kick only while a game is in play. | [Host](firmware/game-abstraction.md#host), [solenoid tests](firmware/testing.md#solenoids) |
| A ball resting against a bumper makes it kick once. | [Solenoid protection](firmware/drivers/solenoid.md#solenoid-protection) |
| The game sees each bumper hit. | **Not covered.** No component turns the driver's bumper hit into an event for the game, so the host drops it. |
| The game sees each hit on a bell once, however the contact bounces. | **Not covered.** The contact driver reports each hit once. No component passes it on to the game, so the host drops it. |
| The game sees a hit on each stand-up target. | **Not covered.** The stand-up targets have no design yet. |
| A shot that turns the rotating seal counts with its direction and its full turns. The game sets how far the seal has to turn for a shot. | [Rotating seal](firmware/components/rotating-seal.md) |
| Vibration of the machine and a nudge too small for a shot never count as a shot. | [Rotating seal](firmware/components/rotating-seal.md#requirements), [rotary sensor driver](firmware/drivers/magnetic-rotary.md#driver-events) |
| A ball still on the playfield carries on when the game changes. | [Host](firmware/game-abstraction.md#host) |

## Scoring

| Use case | Realized in |
|---|---|
| The game awards points for lanes, shots and hits by its own rules. | [Example game](firmware/game-abstraction.md#an-example-game) |
| The score stays on the screen over every mode's slide. | [Screen slides](firmware/components/screen.md#slides) |
| The post-game phase shows the score and keeps the high score. The high score survives a power cut. | [Game lifecycle](firmware/game-abstraction.md#game-lifecycle), [saved data](firmware/game-abstraction.md#saved-data) |
| The ball count shows on the lights of the shooter lane, where the stock ball count board sat. | [Light layout](parts/lighting/design.md#light-layout), [lamp groups](firmware/components/lamps.md#groups) |

## Drain and ball store

| Use case | Realized in |
|---|---|
| The machine notices every ball that leaves play through the drain. | [Break beam](parts/break-beam/design.md), [break beam driver](firmware/drivers/break-beam.md#ball-sensing) |
| The game learns when a drained ball has rolled into the store. Two balls that drain together still count as two. | [Ball store](firmware/components/drain.md#ball-store), [release](firmware/components/drain.md#release) |
| The machine knows how many balls the store holds, also right after power-up. | [Ball store](firmware/components/drain.md#ball-store) |
| On the game's request the ball separator lets exactly one ball into the plunger lane. It moves only between its two end positions, at a limited speed. | [Release](firmware/components/drain.md#release), [servo](parts/servo/design.md#requirements), [servo driver](firmware/drivers/servo.md#servo-positions) |
| A release that brings no ball into the plunger lane is reported. The game decides whether to try again. | [Release](firmware/components/drain.md#release) |
| The next ball comes into the plunger lane once the drained one is in the store. | [Example game](firmware/game-abstraction.md#an-example-game) |
| The game ends once its last ball has drained. | [Game lifecycle](firmware/game-abstraction.md#game-lifecycle) |
| The ball separator moves only while no coil pulls. | **Not covered.** The power budget relies on it. Nothing in the firmware enforces it. A multiball releases a ball while the bumpers can fire. |
| The drain lamp shows what the game sets. | [Drain lamp](firmware/components/drain.md#drain-lamp) |

## Scoop

| Use case | Realized in |
|---|---|
| The game learns when a ball enters the scoop and when it leaves. | [Scoop](firmware/components/scoop.md#pinball-events) |
| The game knows how long a ball has rested in the scoop, also for a ball that lay there at power-up. | [Ball in the scoop](firmware/components/scoop.md#ball-in-the-scoop) |
| The game throws the ball out of the scoop when its rules say so, or fires the coil for show. | [Eject](firmware/components/scoop.md#eject) |
| A new game empties the scoop during its intro. | [Game lifecycle](firmware/game-abstraction.md#game-lifecycle) |
| The scoop lamp shows what the game sets. | [Scoop lamp](firmware/components/scoop.md#scoop-lamp) |

## Modes

| Use case | Realized in |
|---|---|
| A ball save returns a ball that drains soon after its launch. | [Example game](firmware/game-abstraction.md#an-example-game) |
| A skill shot, a ball save and a multiball run as modes of a game, several at the same time. | [Modes](firmware/game-abstraction.md#modes) |
| Where two modes want the same hit, the mode of higher priority decides first. | [Modes](firmware/game-abstraction.md#modes) |
| A multiball puts the second ball into play while the first is still on the playfield. | [Modes](firmware/game-abstraction.md#modes), [ball store](firmware/components/drain.md#ball-store) |
| Each mode shows its own lights, its own slide and its own music. What the mode below set comes back when it ends. | [Overlapping effects](firmware/components/lamps.md#overlapping-effects), [screen slides](firmware/components/screen.md#slides), [music](firmware/components/sounds.md#music) |

## Lights

| Use case | Realized in |
|---|---|
| Every playfield light shows the color the game sets: steady, blinking, alternating, pulsing, fading or a run along several lights. | [Lamp effects](firmware/components/lamps.md#effects) |
| The ambient strips along the sides and over the diorama run effects and palettes along their whole length. | [Lamp groups](firmware/components/lamps.md#groups), [lamp effects](firmware/components/lamps.md#effects) |
| A short flash ends by itself. | [Overlapping effects](firmware/components/lamps.md#overlapping-effects) |
| The game dims the playfield lights and the ambient lights separately. | [Brightness](firmware/components/lamps.md#brightness) |
| The lights never draw more current than the adapter can give. The larger adapter lights every LED of the perimeters. | [Adapter profile](firmware/drivers/lighting.md#adapter-profile), [current ceiling](parts/lighting/design.md#current-ceiling) |
| Lights at the track, the targets, the rollovers and the scoop mark what the game asks for. | [Light layout](parts/lighting/design.md#light-layout) |
| Action lights on the playfield mark the goals of the game's modes. | **Not covered.** Their positions are not decided. |
| The machine runs an attract show while it waits for a player. | **Not covered.** A game's intro could hold until the player starts. No game and no document does so. |

## Sound and music

| Use case | Realized in |
|---|---|
| Background music plays during a game, once or over and over. A win or a loss plays its own music. | [Sounds](firmware/components/sounds.md#music-and-effects), [music](firmware/components/sounds.md#music) |
| Effects such as a bumper hit or the drain play over the music, several at once. | [Sounds](firmware/components/sounds.md#music-and-effects), [audio driver](firmware/drivers/audio.md#channels) |
| Each game brings its own sounds on the SD card. | [Sounds](firmware/components/sounds.md) |
| Music and effects each have their own volume, which the game sets. No volume overdrives the speaker. | [Volume](firmware/components/sounds.md#volume), [audio](parts/audio/design.md#requirements), [audio driver](firmware/drivers/audio.md#volume) |
| A sound starts, stops and changes its volume without a click. | [Audio driver](firmware/drivers/audio.md#volume) |
| No sound drops out while the card writes the log or a high score. | [Channel player](firmware/drivers/audio.md#channel-player), [file channel](firmware/file-channel.md) |
| A video's sound takes the place of the music. The music goes on where it stopped. | [Video player](firmware/components/video-player.md#playing), [music](firmware/components/sounds.md#music) |
| A game reacts to the toggle switch, such as by turning its background music off and on as the stock machine did. | [Controls driver](firmware/drivers/controls.md), [game abstraction](firmware/game-abstraction.md) |

## Display and video

| Use case | Realized in |
|---|---|
| A game shows pictures from the SD card, such as a background or icons. | [Pictures](firmware/components/screen.md#pictures) |
| A game shows small animations made of pictures. | [Video player](firmware/components/video-player.md#existing-libraries) |
| A game shows a message box over its own drawing. | [Layers](firmware/components/screen.md#layers) |
| A game plays a video in a part of the screen or over the whole screen, with or without its own drawings in front. | [Video player](firmware/components/video-player.md#playing), [display](parts/display/design.md#use-cases) |
| A full-screen video plays at its full frame rate while the sounds keep playing. A video with sound stays in step with its sound. | [Display](parts/display/design.md#use-cases), [video player](firmware/components/video-player.md#playing) |
| A video plays only while its mode's slide shows. | [Video owner](firmware/components/video-player.md#owner) |
| The game learns when a video has played to its end. | [End of a video](firmware/components/video-player.md#end-of-a-video) |
| The game reacts when the player taps or holds the screen, or presses a button on it. | [Touch](firmware/components/screen.md#touch), [touch driver](firmware/drivers/touch.md#driver-events) |
| The picture comes back after a supply dip has reset the panel. | [Display driver](firmware/drivers/display.md#device-faults), [reset](parts/display/design.md#reset) |

## Menu and settings

| Use case | Realized in |
|---|---|
| The player opens the menu over a running game and closes it again. | **Not covered.** The host's layer and its gestures exist. No document designs the menu. |
| The player picks another game in the menu. The game picked starts once the running one has stopped. | **Not covered.** The host can switch games. No document designs the menu that asks it to. |
| The player changes the volumes and other settings on the display by touch. A changed setting survives a restart. | **Not covered.** The config can change and save a setting. No document designs the menu that does it. |
| All games share settings such as the volumes, so no game needs code for them. | **Not covered.** The host is meant to provide them. No document says which settings or how. |
| The operator edits the machine's settings on a computer in the config file on the SD card. | [Config](firmware/components/config.md) |
| A missing or broken config file leaves every setting at its default. A value out of its range is held at the nearer end. Each case writes a line to the log. | [Config](firmware/components/config.md#reading) |

## Start-up and power

| Use case | Realized in |
|---|---|
| The machine runs from either of its two adapters. | [Power](parts/power/design.md#requirements) |
| A short or a lasting overload makes the adapter shut the power off. | [Adapter](parts/power/design.md#adapter) |
| Swapped polarity at the input damages nothing once the optional switch is fitted. | [Power](parts/power/design.md#requirements) |
| Every coil pulls once at power-up, one after another, so the machine shows at once that it starts. | [Main program](firmware/general-design.md#main-program), [solenoid driver](firmware/drivers/solenoid.md#sensing-and-firing) |
| The screen turns black at power-up, with no garbage on it. | [Display driver](firmware/drivers/display.md#start-up) |
| The ball sensors calibrate themselves at every start, also with a ball lying on one. | [IR driver](firmware/drivers/ir-sensing.md#at-start-up) |
| The ball separator moves slowly to its start position at power-up. | [Servo driver](firmware/drivers/servo.md#start-up) |
| A new game starts with no light, sound, picture or video left from the game before. | [Host](firmware/game-abstraction.md#host), [lamps](firmware/components/lamps.md#interface), [sounds](firmware/components/sounds.md#interface), [layers](firmware/components/screen.md#layers), [video player](firmware/components/video-player.md#interface) |
| The machine shows while the SD card is being written, so it is not switched off then. | **Not covered.** The storage driver reports when writing starts and ends. Nothing shows it. |

## Faults the machine shows

| Use case | Realized in |
|---|---|
| A device that fails leaves the game running without it. | [Device faults](firmware/error-handling.md#device-faults) |
| The machine plays without an SD card, without the display module or without the PSRAM. A card pulled during a game stops only the sounds, pictures and videos. | [Driver design](firmware/driver-design.md), [storage](firmware/drivers/storage.md#device-faults), [pictures](firmware/components/screen.md#pictures) |
| A covered or dead sensor, a ball stuck in the drain's beam, a ball resting against a bumper and a lost sensor bus are each reported. | [IR driver](firmware/drivers/ir-sensing.md#device-faults), [break beam driver](firmware/drivers/break-beam.md#device-faults), [solenoid driver](firmware/drivers/solenoid.md#device-faults), [contact driver](firmware/drivers/contacts.md#device-faults), [rotary sensor driver](firmware/drivers/magnetic-rotary.md#device-faults) |
| The machine lists its failed devices on the display, also over a running game. | **Not covered.** The host keeps the list. No document designs how it is shown. |
| The log records each device that fails and each that recovers. | **Not covered.** The host keeps the list for the menu. Nothing writes it to the log. |
| A game adapts to a failed device, such as skipping the ball save while the ball separator has failed. | **Not covered.** No game sees a fault. |
| A game goes on once the break beam has failed. | **Not covered.** A game ends a ball on the drain's event, which a failed beam never sends. |
| A coil is never held on, whatever the firmware does. | [Solenoid protection](firmware/drivers/solenoid.md#solenoid-protection), [watchdog](firmware/error-handling.md#watchdog), [drive](parts/solenoid/design.md#drive) |
| A firmware that hangs or crashes restarts by itself. The log names the cause. | [Watchdog](firmware/error-handling.md#watchdog), [restart causes](firmware/error-handling.md#restart-causes) |

## Maintenance

| Use case | Realized in |
|---|---|
| The operator records each ball sensor's values after a change to the playfield. | **Not covered.** The IR driver describes the build step and where it stores its results. No document says how the build step is started. |
| The operator retunes a sensor that misses balls by swapping one resistor on its board. | [Adjustment knobs](parts/ir-reflective/design.md#adjustment-knobs) |
| The operator sets the servo's end positions and speed, the coils' pull times, the adapter and the starting volumes in the config. | [Servo](firmware/drivers/servo.md#config), [solenoids](firmware/drivers/solenoid.md#config), [lighting](firmware/drivers/lighting.md#config), [sounds](firmware/components/sounds.md#config) |
| The operator checks at every start whether the seal's magnet sits at the right gap. | [Rotary sensor driver](firmware/drivers/magnetic-rotary.md#start-up) |
| The operator reads the machine's log on a computer. The log keeps the latest lines and never outgrows its limit. | [Logging](firmware/logging.md), [size limit](firmware/logging.md#size-limit) |
| The operator makes pictures, videos and sounds for a game from ordinary files. | [Media](../assets/media.md) |
| The operator adds a game, whose code is built into the firmware and whose files lie on the SD card. | [Game abstraction](firmware/game-abstraction.md) |
| The operator updates the firmware. | **Not covered.** The firmware's build and flash toolchain is not chosen yet. |
| The operator tests each subsystem on the bench before it goes into the machine. | [Testing](firmware/testing.md) |
| The operator restores the stock machine. | [Reversibility](../RULES.md#12-reversibility) |
