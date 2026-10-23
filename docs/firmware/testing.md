# Testing

Each test runs where the code it checks can run. Code that touches no register runs on the computer. Code that drives hardware runs on the Teensy at the bench. A test that needs the coils and a ball runs on the assembled machine.

A test on the Teensy judges what it can from inside the firmware and prints its result. A person carries out only the steps the firmware cannot, such as moving a ball. A person also answers what only a person can see or hear.

## Test programs

Each section below is one test program in `firmware/tests/`. The `computer` tests of a section build for the computer, and its other tests build for the Teensy. A test is named by its program and its row, such as `solenoid.pull-length`. A program runs its tests in the order of its table. The programs run in the order the sections stand, since a test relies on the sections above it.

Where a row links a figure, the test takes that figure from the firmware's own constant, so a changed figure changes the test with it.

A test build is the firmware built for one test program. It adds the counters and hooks its tests read, and the normal build carries none of them.

Code that touches no register is kept apart from code that does, so that it builds for the computer. There, fakes stand in for the hardware and for the drivers below the code under test. A fake has the interface of the part it replaces, records every call it gets, and answers as its test sets it up.

## Where a test runs

| Runs on | Setup |
|---|---|
| `computer` | The computer alone |
| `Teensy` | The [bench](#bench). The program carries out every step and judges every check, so nobody needs to be there |
| `bench` | The [bench](#bench), with a person who carries out the `DO` steps and answers the `ASK` questions |
| `machine` | The assembled machine with its coils connected and a ball on the playfield, with a person as on the bench |

## Results

A Teensy program prints its run to the serial monitor in these lines:

| Line | Meaning |
|---|---|
| `RUN <test>` | The test begins |
| `DO <test> <step>` | A person carries out the step. The test waits until it sees the step done, and fails when it does not see it in time |
| `ASK <test> <question>` | A person answers `y` or `n` over the serial port |
| `PASS <test>` | The test has passed |
| `FAIL <test> <seen> expected <wanted>` | The test has failed, with what it saw and what it expected |
| `SKIP <test> <reason>` | The test could not run, such as for a part that is not connected |
| `END <passed> <failed> <skipped>` | The program has run every test |

A computer test reports through its test framework, and its run fails when a single check fails.

## Bench

The bench is the Teensy on USB with the boards of the subsystems under test, the boards on a 5 V bench supply, and the [test card](#test-card) in. The coils stay disconnected, so a pull shows only on its trigger pin. Three boards are wired differently on the bench:

| Board | On the bench |
|---|---|
| Solenoid coil board | The Teensy's GND is joined with the board's GND, as its [supply](../parts/solenoid/design.md#supply) requires with the Teensy on USB |
| Servo board | The Teensy's GND is joined with the board's GND, as its [supply](../parts/servo/design.md#supply) requires |
| IR mainboard | It runs from its D24V5F3 on the bench supply, or from the Teensy's 3V3 pin while everything that pin feeds stays within the pin's [rating](../research/teensy-4.1.md#power) |

## Test card

The card is of Class A1 or A2. It holds two games, `/games/a/` and `/games/b/`, and nothing else. The test programs carry two test games with those ids, which print every call the host makes to them. Each game's directory holds these files:

| File | Content |
|---|---|
| `audio/music.wav` | music of at least a minute, in the [audio driver's format](drivers/audio.md#audio-files) |
| `audio/1.wav` to `audio/4.wav` | short effects in the same format, one for each channel |
| `audio/ramp.wav` | samples that count up by one from the first, so a test can place every sample |
| `audio/ramp-chunks.wav` | the same samples behind extra chunks ahead of the data chunk |
| `audio/full.wav` | a full-scale square wave |
| `audio/sync.wav` | a click every second, as long as `video/sync.rgb` |
| `video/loop.rgb` | a fullscreen movie in the [video format](../../assets/media.md#videos) |
| `video/viewport.rgb` | a movie for a viewport smaller than the screen, in the same format |
| `video/sync.rgb` | a movie that flashes white every second |
| `background.img` | a background picture in the [screen's format](components/screen.md#pictures) |
| `short.img` | the same picture, cut off halfway through its pixels |
| `icon.img` | a picture smaller than the screen with transparent pixels, in the same format |
| `settings` | a small settings file one driver reads in its `begin()` |

## Core

### Clock

The program `clock` checks the [time measurement](general-design.md#time-measurement).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `wrap` | computer | Compare times on both sides of the wrap of `nowUs()` with `before()`, `elapsedUs()` and `elapsedMs()` | Every answer equals the one the same span gives away from the wrap |
| `rate` | Teensy | Read `nowUs()` and the processor's cycle counter side by side for a minute | The two agree to within the time the reads take |

### Event queue

The program `event-queue` checks the [event queue](input-handling.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `oldest-first` | computer | Publish into several queues in turn, each queue in time order | `read()` gives every event once, oldest first across all queues |
| `across-wrap` | computer | Do the same with times on both sides of the wrap of `nowUs()` | The order holds across the wrap |
| `full` | computer | Publish into a queue until it is full, and once more | The last publish returns false and counts in `dropped()`, and the queued events come out unchanged |
| `payload` | computer | Publish an event of every type with `publish<Tag>()` | `payload<Tag>()` gives every payload back unchanged |
| `from-interrupts` | Teensy | Publish numbered events from two interrupts of different priorities for ten minutes while the main loop reads | Every event arrives once and whole, and each queue's events arrive in their order, while `dropped()` stays 0 |

### Driver tick

The program `driver-tick` checks the [driver tick](driver-design.md#driver-intervaltimer).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `periods` | Teensy | Attach callbacks with periods of one, two and twenty ticks and let them run for a minute | Each callback runs as often as its period gives, and every run lies a whole number of ticks after the one before |
| `refusals` | Teensy | Attach a period that is no multiple of the tick, then callbacks until every slot is taken, and one more | `attach()` returns false for the odd period and for the extra callback, and the attached callbacks keep running |

### Device monitor

The program `device-monitor` checks the [device monitor](error-handling.md#device-faults) on a fake driver.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `changes` | computer | Let the fake driver report parts failing and recovering, with error codes | Each change gives one `DeviceFailed` with the device, the part and the code, or one `DeviceRecovered`, and an unchanged answer gives nothing |
| `full-queue` | computer | Let parts fail while the monitor's queue is full, then empty it | Each part is reported at the first check after the queue has room |
| `source` | computer | Pack the source of every device and part | `faultDevice()` and `faultPart()` give back what `faultSource()` packed |

### PSRAM

The program `psram` checks the [PSRAM](../parts/psram/design.md) on the Teensy's underside.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `found` | Teensy | Read `external_psram_size`, which Teensyduino sets at start-up | It equals the [chip's size](../parts/psram/design.md#chip) in MB |
| `card` | Teensy | Read `background.img` through the [storage driver](drivers/storage.md) into the PSRAM, once straight and once at an address off the 32-byte steps | Both copies equal the file |
| `patterns` | Teensy | Write fixed patterns and a pseudo-random sequence into the whole PSRAM and read each back, as [PJRC's memory test](https://github.com/PaulStoffregen/teensy41_psram_memtest) does | Every byte reads back as written |

## Drivers

### Storage

The program `storage` checks the [storage driver](drivers/storage.md) against its [use cases](drivers/storage.md#requirements-and-use-cases).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `mount` | Teensy | Start with the card in | `begin()` returns true, and the card is not reported failed |
| `settings` | Teensy | Let a driver read its `settings` file in `begin()` | The values it prints match the file |
| `round-trip` | Teensy | Write a file, read it back, rewrite it shorter and read it again | Each read brings what was written last, and `fileSize()` follows |
| `missing` | Teensy | Read a file the card does not hold, then write it into a directory the card does not hold | The read brings 0 bytes, and the write creates the directory and the file |
| `past-end` | Teensy | Start a write past the end of a file | The write fails, and the file stays unchanged |
| `order` | Teensy | Start a write, then a read of the same place before the write has ended | The read brings the new data |
| `long-path` | Teensy | Open a file whose path is longer than the [driver allows](drivers/storage.md#files) | Every read and write of that file fails, and every other file works |
| `busy` | Teensy | Start a second operation on a request that is still `Pending` | Both requests fail |
| `scope-end` | Teensy | Let a request go out of scope while it is `Pending` | Its destructor returns only once the request has ended, and nothing writes into its buffer afterwards |
| `interrupts` | Teensy | Call `wait()` from an interrupt and from a task of the storage task's priority, and start a request from an interrupt more urgent than the [limit](general-design.md#interrupts) | Each `wait()` returns false at once while its request ends normally, and the urgent request fails at once |
| `loop-runs` | Teensy | Write a file of several megabytes and read it back while the main loop counts its passes | No pass of the main loop lasts as long as a driver tick |
| `writing` | Teensy | Start several writes back to back | One `WritingStarted` comes before the first, and one `WritingEnded` after the last |
| `yield` | Teensy | Attach an `EventResponder` to `yield()`, trigger it from an interrupt, and read a large file meanwhile | The responder runs in the main task once per pass and never in the storage task, and the read ends normally |
| `no-card` | Teensy | Start without the card | The start-up completes, every request fails at once, and the card is reported failed |
| `pulled` | bench | Pull the card while a long read runs | The read fails, every later request fails at once, and the card stays reported failed until a restart |
| `back` | bench | Put the card back and restart | The program's `Teensy` tests pass again |
| `readable` | bench | Put the card into the computer after the other tests | The computer's disk check finds no error |

### File channel

The program `file-channel` checks the [file channel](file-channel.md). Its computer tests read from a fake card that ends each read when the test lets it.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `whole-file` | computer | Read files of several sizes through rings of several chunk sizes | `take()` gives every byte once and in order, and `ended()` turns true after the last |
| `from-byte` | computer | Start a file at a byte inside it | The first byte taken is that byte |
| `loop` | computer | Read a file with `loop` set several times past its end | The bytes run on from the file's first byte after its last, with nothing missing or doubled |
| `slow-card` | computer | Hold the reads back while `take()` asks for more | `take()` gives fewer bytes and later the rest, with nothing missing or doubled |
| `restart` | computer | Start a new file while a read of the old one is on its way, then let that read end | No byte of the old file comes out |
| `stop` | computer | Call `stop()` while a read is on its way, then let it end | `take()` gives nothing, and `ended()` is true |
| `failed-read` | computer | Let a read fail partway through a file | The bytes before it come out, and `ended()` turns true after them |
| `skip` | computer | Take bytes with `to` set to `nullptr` | The bytes are skipped, and the next `take()` goes on behind them |
| `nothing` | computer | Start a file the card does not hold, and leave another channel unstarted | `ended()` is true at once for both |
| `high-first` | Teensy | Queue normal reads, then read through a channel of high priority | The channel's read ends before every normal read that had not started |

### Solenoids

The program `solenoid` checks the [solenoid driver](drivers/solenoid.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `pull-length` | Teensy | Fire every coil many times | Every pull lasts as long as the [protection rules](drivers/solenoid.md#solenoid-protection) allow, read back from the trigger pin |
| `cool-down` | Teensy | Call `fire()` during a pull, during the cool-down and right after it | `fire()` returns false during the pull and the cool-down, and true after |
| `hit` | bench | Arm the contacts and close each sensed solenoid's contact | Each contact fires its coil once and gives one `SolenoidHit` with its number |
| `held` | bench | Hold a contact closed, open it, and close it again several times | The held contact fires once, and every later pull follows an opening at least as long as the cool-down |
| `disarmed` | bench | Disarm the contacts, close each one, then call `fire()` | The contacts fire nothing and publish nothing, and `fire()` still pulls |
| `failed` | bench | Hold a contact closed for longer than the [fault time](drivers/solenoid.md#device-faults), then open it | The solenoid is reported failed, and recovered once the contact is open |
| `power-on` | machine | Switch the machine on | Every coil pulls once, one after another |
| `play-only` | machine | Shoot a ball at the bumpers before a game, during its Play phase and after it | The bumpers fire only during Play |

### Lighting

The program `lighting` checks the [lighting driver](drivers/lighting.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `chains` | bench | Run one lit device along each chain from its start, chain after chain, with the chain's number on the serial monitor | Each chain lights all its devices in order, and no device of another chain lights |
| `current` | bench | Limit the bench supply to just above the [budget](../parts/lighting/design.md#current-ceiling), connect every chain and set every device to full white | The supply never reaches its limit |

### IR ball sensing

The program `ir-sensing` checks the [IR driver](drivers/ir-sensing.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `phase-plan` | computer | Plan the phase for every channel count from one up | Every plan keeps the bounds of the [phase length](drivers/ir-sensing.md#phase-length), a count that does not fit is refused, and the count worked through there gives its figures |
| `calibration` | computer | Calibrate on fake averages near `clear_build`, near `ball_build` and near neither | Each channel gets the scale, `clear`, `ball`, `threshold` and `release` of the [start-up rules](drivers/ir-sensing.md#at-start-up), and the third counts as failed |
| `margin` | computer | Calibrate on fake readings whose noise misses the margin condition at every phase | The driver restarts once with a longer phase, then falls back to the stock phase, and reports the channel that still fails |
| `hysteresis` | computer | Feed differences that cross `threshold` once, cross it twice in a row, and hover between `release` and `threshold` | Only two readings in a row over `threshold` report a ball, only two in a row under `release` release it, and hovering gives nothing |
| `read-window` | Teensy | Note when each conversion runs in its phase for ten minutes, while the lights send frames and the display plays a movie | No channel is read before `t_inv` or after its phase, and the driver logs no miss |
| `overhead` | Teensy | Measure `t_ovh` as [reading sensor values](drivers/ir-sensing.md#reading-sensor-values) describes | The constraint stated there holds |
| `pass` | bench | Pass a ball over each channel in turn | Each pass gives one `Detected` and one `Released` from that channel and no other, and `getState()` follows them |
| `dwell` | bench | Hold the ball over a channel for a few seconds, then lift it | `Released` carries the milliseconds between its `Detected` and itself |
| `room-light` | bench | With no ball, switch the room light, and hold a flickering LED lamp and a phone's torch near the sensors | No channel reports a ball |
| `ball-at-start` | bench | Start with a ball lying on one channel, then lift it and pass it again | The channel is not reported failed, and the pass gives one `Detected` and one `Released` |
| `covered` | bench | Cover one sensor's emitter with opaque tape for longer than the [fault time](drivers/ir-sensing.md#device-faults), then uncover it | The channel is reported failed, and recovered once uncovered |
| `covered-at-start` | bench | Start with one sensor's emitter covered, then uncover it | The channel counts as failed until the next start |
| `noise` | machine | Start with the lights running and the room lit by a flickering LED lamp, as the [start-up calibration](drivers/ir-sensing.md#at-start-up) asks for σ | The σ it reports keeps the margin condition for every channel |
| `fast-ball` | machine | Shoot balls through every lane at the plunger's full strength | Each ball a person sees pass gives one `Detected` |

### Break beam

The program `break-beam` checks the [break beam driver](drivers/break-beam.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `pass` | bench | Pass a ball through the beam many times, slowly and fast | Each pass gives one `BallDrained`, and `blocked()` is true only while the ball is in the beam |
| `failed` | bench | Leave a ball in the beam for longer than the [fault time](drivers/break-beam.md#device-faults), then take it out | The beam is reported failed, and recovered once it is clear |

### I²C bus

The program `i2c-bus` checks the [I²C bus](io/i2c-bus.md) on `Wire`, with the rotary sensor and the port expander connected.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `turns` | Teensy | For a minute, request a read of the rotary sensor every tick and a new read of the port expander each time one ends | Every request is followed by one run and one call of its done function. A read of the rotary sensor never waits for more than one read of the port expander |
| `no-answer` | Teensy | Run a transfer to an address no device answers at, then a read of the rotary sensor | The first ends with the missing answer as its error. The second works |
| `probe` | Teensy | Run a transfer with neither part, once to the rotary sensor's address and once to an address no device answers at | The first ends without an error. The second ends with the missing answer |
| `held-low` | bench | Hold SDA at GND with a wire for a moment while transfers run, then let go | The transfers end with the line held low as their error. Transfers work again once SDA is free |

### Magnetic rotary sensor

The program `magnetic-rotary` checks the [rotary sensor driver](drivers/magnetic-rotary.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `step` | computer | Feed angles that cross from the top of a turn to 0 and back, in steps of every size up to half a turn | Each step comes out with its size and sign, and the position adds up over many turns without drift |
| `magnet` | bench | Start with the magnet in place | `magnet()` reports a magnet, neither too weak nor too strong, with the gain in mid-range |
| `turns` | bench | Turn the rod a counted number of whole turns one way, then back | `position()` moves by that many turns each way |
| `dead-band` | bench | Rock the seal within the rod's play, then turn it by a few degrees | The rocking gives no event, and the turn gives `RotorMoved` and then one `RotorStopped` with the position it rests at |
| `full-queue` | bench | Turn the rod while the main loop stops reading events, then let it read again | The last event carries the position `position()` gives |
| `unplugged` | bench | Disconnect the sensor while the program runs, then connect it again | The sensor is reported failed with error flags as its code, and recovered once connected, while the program keeps running |

### Port expander

The program `port-expander` checks the [port expander](io/port-expander.md). A test listener attached to all eight pins records every change it gets. A pin is pulled low on the bench with a wire to GND.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `changes` | bench | Pull each pin from E-1 to E-8 low in turn, then let it go | Each pull and each release gives one change with its pin and its new level. `levels()` shows the pin low while it is pulled |
| `together` | bench | Pull two pins low at the same moment | The listener gets the change of both pins |
| `failed-read` | bench | Pull a pin low while the test build fails the read that follows | The change still arrives with the time INT fell, after the driver tick's next [read](io/port-expander.md#reading-the-pins) |
| `restart` | bench | Restart the Teensy with a pin pulled low. Let it go while the Teensy starts | INT is high once the first `attach()` has returned. The next change arrives |
| `taken` | Teensy | Attach a second listener to a pin the first one holds | `attach()` returns false. The first listener keeps getting its changes |

### Contacts

The program `contacts` checks the [contact driver](drivers/contacts.md). A contact on the bench is a wire from GND to its pin.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `inputs` | bench | Close the contact on E-1 for a moment, then the one on E-2 | Each closure gives one `ContactClosed` with its pin as the source. `closed()` shows the pin while it is closed |
| `bounce` | bench | Tap a contact many times, slowly and as fast as possible | Each tap gives one `ContactClosed` |
| `bus-lost` | bench | Hold SDA at GND with a wire for longer than the [fault time](drivers/contacts.md#device-faults), then let go | The contacts are reported failed with error flags as their code, and recovered once SDA is free |

### Controls

The program `controls` checks the [controls driver](drivers/controls.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `position` | bench | Set the switch to 1, then to 0 | `position()` gives the position printed on the switch each time |
| `flips` | bench | Flip the switch many times, slowly and as fast as possible | Each flip gives one `ToggleSwitched` with the new position, and the last event always carries the position the switch rests in |

### Servo

The program `servo` checks the [servo driver](drivers/servo.md). A wire from the servo pin to a pin that no driver of the program drives lets its `Teensy` tests time every pulse.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `first-pulse` | Teensy | Time every pulse from before `begin()` until the servo stands at its start position | No pulse lies outside the [SER0049's range](drivers/servo.md#pwmservo), the first one included |
| `pulses` | Teensy | Move to each end position and time the pulses | Each pulse has the width of its end position's angle, once every frame |
| `ends` | bench | Move between the two end positions several times | The seesaw reaches each extreme, and the servo does not strain or hum against it |

### Audio

The program `audio` checks the [audio driver](drivers/audio.md). Its `Teensy` tests listen through a tap. A tap is a recording stage that the test build adds behind the master gain, so the program can compare the samples bound for the amplifier with the samples in the files.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `samples` | Teensy | Play `ramp.wav` and `ramp-chunks.wav` at full volume | The tap records every sample of each file once, in order, from the first, scaled by the master gain |
| `mix` | Teensy | Play `full.wav` on every channel at once | The tap records no sample beyond full scale times the [master gain](../parts/audio/design.md#level) |
| `loop` | Teensy | Play `ramp.wav` in a loop a hundred times | Each loop's first sample follows the last sample of the loop before, with no gap |
| `replace` | Teensy | Call `play()` on a channel that plays a sound | The old sound glides out without a jump, and the new one starts from its first sample |
| `glide` | Teensy | Start and stop a sound, and change its volume while it plays | Each change glides over one block, with no jump between neighboring samples |
| `volume` | Teensy | Set volumes inside and outside 0 to 1 | The tap records the samples scaled by the volume, and a value outside counts as the nearer end |
| `resume` | Teensy | Note `position()`, play another sound on the channel, then play on from the position | The first sound goes on from the sample where it stood |
| `ended` | Teensy | Let sounds play out, cut others off with `play()` and `stop()`, and let one loop | `SoundEnded` with the number `play()` returned comes once for every sound that played out, and for no other |
| `missing` | Teensy | Play a file the card does not hold, then start without the card and play | Each sound ends at once with `SoundEnded`, and the other channels play on |
| `starved` | Teensy | Start one write of several megabytes while a sound plays | The tap records silence while data is missing, then the sound going on from where it stopped, and no stray sample |
| `by-ear` | bench | Play the music and an effect on every other channel, restart one effect many times in a row, and let the music loop for several minutes | No sound drops out or clicks, every effect starts from its beginning, and the music loops |

### Display

The program `display` checks the [display driver](drivers/display.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `start` | bench | Start the Teensy from power-up, then restart it while the display is awake | The screen turns black both times, with no garbage and no white flash |
| `restart` | bench | Disconnect the 5 V of J-DISP for half a second while a picture shows, then connect it again | The picture returns within two seconds, and the Teensy keeps running |
| `picture` | bench | Send color bars, a ramp of every level of red, green and blue, and the words TOP LEFT in that corner | The colors are right, the ramps run smoothly, and the words stand upright at the top left |
| `rectangles` | Teensy | Send rectangles of several sizes and places, then one off the 16-pixel steps, one beyond the screen and one while another is on its way | Each rectangle on the steps lands where it was sent, the function handed in runs once after its last pixel, and the three others are refused |
| `refills` | Teensy | Send full screens for a minute while the main loop counts how often it runs | The refills take no more of the processor than the [estimate](drivers/display.md#performance--memory-usage), and the bus keeps its speed |

### Touch

The program `touch` checks the [touch driver](drivers/touch.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `touch` | bench | Touch the marks the test draws in the corners and the center, then drag a finger and rest it | Each touch lands inside the mark touched, and `TouchDown`, `TouchMoved`, `TouchHeld` and `TouchUp` come as the [events](drivers/touch.md#driver-events) describe |
| `no-touch` | bench | Start with the touch controller disconnected | `present()` is false, the picture works, no touch event comes, and no fault is reported |
| `touch-lost` | bench | Disconnect the touch controller while the machine runs | The controller is reported failed after 100 ms |

### Every driver

The program `every-driver` checks the [rules every driver follows](driver-design.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `begin-fails` | Teensy | Take every slot of the driver tick and of the device monitor, then call every driver's `begin()` | Each `begin()` that needs a slot returns false, and every later call of that driver answers at once with false or empty data |
| `at-once` | Teensy | Make every driver call that starts a long transfer, such as a large read, a full light frame, a full screen or a sound | Each call returns while its transfer still runs |

## Components

### File system

The program `file-system` checks the [file system component](components/file-system.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `paths` | computer | Open a path inside a game's directory, an empty path, one from the root, one with `..`, one too long, and one before any game is named | The first gives the file in `/games/<id>/`, and every other gives a file whose requests fail |
| `switch` | computer | Open a file, name another game with `use()`, and open again | The first file keeps its path, and the second lies in the other game's directory |

### Lamps

The program `lamps` checks the [lamps component](components/lamps.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `effects` | computer | Start every effect on a fake lighting driver and step the clock through it | Each lamp shows the color its [effect](components/lamps.md#effects) gives at every moment, and `run()` moves its pattern one lamp per step in its direction |
| `overlap` | computer | Blink a lamp with one key, lay two effects of higher priority over it, then remove their keys one by one | Each removal shows the effect below, and the blink goes on in its old rhythm |
| `six-keys` | computer | Set one lamp with six keys | The sixth effect is refused and counted, and the five before it stay |
| `frames` | Teensy | Run effects on every group for a minute | `update()` hands on one frame per frame period, and no `show()` waits for the frame before |
| `layout` | bench | Light each group and each named part in turn, with its name on the serial monitor | Each lights where the [lighting design](../parts/lighting/design.md#light-layout) places it, and nothing else lights |

### Rotating seal

The program `rotating-seal` checks the [rotating seal component](components/rotating-seal.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `shot` | computer | Feed `RotorStopped` events of movements short of the set angle and beyond it, both ways | Only the movements beyond the set angle give `SealShot`, each with its whole angle in degrees, positive clockwise seen from above |
| `first-rest` | computer | Start the component at a position, then feed a `RotorStopped` beyond the set angle from it | The `SealShot` carries the angle from the start position, and the start position itself is never reported |
| `turns` | computer | Feed shots that complete turns both ways, then call `reset()` | `completedTurn()` and `turns()` follow the shots, and `reset()` clears the turns and the progress toward the next one |
| `vibration` | machine | Nudge the machine while the bumpers fire, and turn the seal by less than the set angle | No `SealShot` comes |
| `ball` | machine | Shoot balls at the seal | Each shot that turns the seal past the set angle gives one `SealShot` in the direction it turned |

### Sounds

The program `sounds` checks the [sounds component](components/sounds.md) on a fake audio driver.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `channels` | computer | Play effects until every effect channel is busy, then one more | The music keeps its channel, each effect takes a free channel, and the extra one replaces the effect that started first |
| `interlude` | computer | Play music, then an interlude, and let the interlude end | The interlude takes the music's place, and the music goes on from where it stood |
| `interlude-ends` | computer | End interludes by a new track, by `stopMusic()` and by `stopInterlude()`, and start one with the music off | A new track and `stopMusic()` leave nothing to go on, `stopInterlude()` lets the music go on, and music that was off stays off |
| `music-ended` | computer | Let a track end, a looping track run, an older track's end arrive late, and an effect end | Only the track last started gives `MusicEnded`, once |
| `volume` | computer | Set both volumes across 0 to 1 and outside | The driver gets the square of each percentage, and a value outside counts as the nearer end |
| `names` | computer | Play names in the game's audio directory, and one too long | Each plays `/games/<id>/audio/<name>`, and the long one finds no file |
| `reset` | computer | Play on every channel, change the volumes, then call `reset()` | Every channel stops and gets the set volumes back |
| `resume` | bench | Play music, an interlude in the middle of it, and let the interlude end | The music goes on where it stopped, without a click |

### Screen

The program `screen` checks the [screen component](components/screen.md). Its computer tests run LVGL with a fake display driver.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `widen` | computer | Change objects that start and end at every column | Every rectangle the driver gets spans whole rows and covers the change |
| `gestures` | computer | Feed touch events of short and long touches, still and moving, onto a modal object, onto a button, onto no object and from beside a button onto it | A tap is a touch inside the [tap limits](components/screen.md#touch), a hold is `TouchHeld`, the button gets the touch that comes down on it and none that slides onto it, each tap and hold reaches the game only when no object took the touch, and `takeGesture()` gives the host every gesture once |
| `hide-layer` | computer | Hide the game's layer, change its objects, show it again, then hide it and start a new game | The changes show once the layer is shown, and the new game starts with its layer shown |
| `load` | Teensy | Load `background.img`, `short.img`, `icon.img` and a picture the card does not hold | `loading()` is true until each has loaded, the background and the icon show, and the short and the missing one make `failed()` true |
| `memory` | computer | Load a full-screen picture and smaller ones, release the full-screen one, reserve memory for a full-screen frame, then load until the memory is full | The frame gets the memory the full-screen picture freed, and a picture that finds the memory full makes `failed()` true |
| `show` | Teensy | Show `icon.img` in three images, delete the parent of one, then release the picture | The images show the same pixels, the other two stay when one is deleted, and `release()` deletes them |
| `layers` | bench | Create an object on the host's layer, a message box and an object on the game's layer, and an object on the background over `background.img`, then play a video there | The objects lie over one another in the [order of the layers](components/screen.md#layers), the message box between the host's layer and the game's, the video in front of the background's objects, and the picture behind them |
| `reset` | computer | Switch games while a picture and a frame of the stopped game still load | Every object and image of the stopped game is gone, and the new game starts only once both have loaded and the picture memory is empty |
| `redraw` | Teensy | Change the whole screen in every refresh, once without a video and once beside a full-screen video, while the main loop measures its passes | No pass [waits](components/screen.md#drawing) for the driver, and the longest pass is the time LVGL renders |

### Video player

The program `video-player` checks the [video player](components/video-player.md). Its computer tests run LVGL with a fake display driver, a fake card and fake sounds.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `play` | computer | Play a name with and without `sound` | The video player reads `video/<name>.rgb` frame by frame into its memory, and with `sound` the sounds get `audio/<name>.wav` as an interlude in the same call |
| `movie-ends` | computer | Let the video run out while its sound still plays | The sound ends, the music goes on, and `MovieEnded` with the video's number passes through the event queue to the game |
| `stop` | computer | Stop a video and its sound | Both stop, the music goes on, and the memory of the frame is free again |
| `no-room` | computer | Play a full-screen video while the picture memory is full, then a video in a viewport beyond the screen, both while another video plays | `play()` returns false both times, the sounds get no call, and the video playing goes on |
| `late-card` | computer | Hold a frame's read back past its 40 ms, in a video without sound | The frame on the screen stays, the frame due then follows, and the video keeps pace with its start |
| `sound-clock` | computer | Hold the sound back for a while as a video with sound plays, then let it go on | The frame on the screen stays while the sound waits, and the video then goes on with the frame that belongs to the sound |
| `frames` | Teensy | Play `loop.rgb` in a loop for ten minutes with a sound on every audio channel and a write to the card every second | Every frame is on the screen within its 40 ms, and no audio block plays silence for want of data |
| `viewport` | bench | Play `viewport.rgb` in a viewport beside a full-screen picture while the game draws beside it and over it | The video fills only its viewport, the picture shows around it, and the game's objects stay in front |
| `missing-movie` | Teensy | Play a video the card does not hold | It ends at once with `MovieEnded`, and nothing else changes |
| `sync` | bench | Play `sync` while music runs | Each flash comes with its click, and the music goes on where it stopped once the movie ends |

## Main program

### Logger

The program `logger` checks the [logger](logging.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `format` | computer | Write lines from several writers while the clock runs past its wrap | Each line starts with the milliseconds since the start, rising across the wrap, and the writer's name |
| `buffer-full` | computer | Write more lines than the buffer holds before the next write | Each line that does not fit is lost whole and counted in `lost()`, and the others stay intact |
| `one-write` | computer | Write lines at a high rate for several seconds into a fake card | The lines reach the card in one write a [second](logging.md#buffered-writes) |
| `failed-write` | computer | Let one write fail | Its lines are lost, and the next write goes where the failed one would have gone |
| `order` | Teensy | Write numbered lines, restart twice, then read `/log.txt` back | The lines stand in the order they were written, and the lines of each restart follow those before |
| `full` | Teensy | Put a `/log.txt` at the [size limit](logging.md#size-limit) on the card, then start and write | The file holds only the new lines |
| `no-card` | Teensy | Start without the card and write | `write()` returns false at once, and `lost()` counts the line |

### Watchdog

The program `watchdog` checks the [watchdog](error-handling.md#watchdog) and the [restart causes](error-handling.md#restart-causes) it leaves in the log.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `hang` | Teensy | Stop the main loop in an endless loop | The Teensy restarts within the watchdog's time, and the log names a hang at an address inside that loop |
| `coil` | Teensy | Fire a coil and stop the driver tick, so the coil stays on | The Teensy restarts within the watchdog's time, the log names that coil, and its trigger pin reads low after the restart before `begin()` drives it |
| `start-up` | Teensy | Let a test driver's `begin()` sleep for longer than the watchdog's time | The Teensy does not restart |
| `crash` | Teensy | Execute a trap instruction in the main loop | The Teensy restarts, and the log holds the crash report |
| `power-up` | bench | Cut the Teensy's power, restore it, then read the log | The log holds no restart report for that start |

### Start-up

The program `start-up` checks the [main program](general-design.md#main-program).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `order` | Teensy | Start, with every step of the start-up printing a line | The lines come in the order the main program lists |
| `priorities` | Teensy | Read the priority of every interrupt once the main loop runs | Each interrupt sits at the priority the [interrupt table](general-design.md#interrupts) lists |

### Game host

The program `game-host` checks the [host](game-abstraction.md#host) with the test games.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `lifecycle` | computer | Run a test game whose `done()` turns true in each phase | The host calls `start()`, `play()`, `end()` and `stop()` in the order of the [lifecycle](game-abstraction.md#game-lifecycle), and starts the game anew after its post-game phase |
| `stopped` | computer | Dispatch events and update while no game runs | No game gets a call |
| `dispatch` | computer | Dispatch driver events to fake components, some of which take them and make several pinball events of one | Each driver event goes to the components in order until one takes it, its pinball events reach the game in order, and an event no component takes reaches the game only when a service sent it |
| `faults` | computer | Dispatch `DeviceFailed` and `DeviceRecovered` | Neither reaches a component or the game, the host's list marks the part and clears it again, and a recovered touch controller calls the screen's `restart()` |
| `arming` | computer | Run a test game through its phases | The solenoid contacts are armed exactly while the game is in Play |
| `launch` | computer | Launch game b while a picture of game a still loads | Game a stops, the id of b is written, and b starts only once the screen has settled |
| `last-game` | Teensy | Start game b, then restart | Game b starts again |
| `first-start` | Teensy | Start with no record of a last game | The first game of the list starts |
| `saved-data` | Teensy | Let a test game write a file in `end()`, restart, and let it read the file in `start()` | The game reads what it wrote |
| `switch` | bench | Start a game, then switch to the other game while its background loads | Each background shows whole, and no picture of the stopped game stays on the screen |
| `menu` | bench | Open the menu during a game that keeps drawing, then close it | The menu opens over the game, and the game's layer reappears with every change drawn meanwhile |
| `menu-switch` | bench | Open the menu during a game, pick the other game and confirm | The picked game starts |
| `setting` | bench | Change a setting in the menu, change it again at once, then restart | No save starts while one is written, and after the restart the value saved last stands |
| `fault-list` | bench | Disconnect the rotary sensor during a game, then connect it again | The menu lists the sensor as failed and later as working, and the game goes on throughout |
| `card-pulled` | bench | Pull the card during a game that plays music and a movie | The game goes on, the music and the movie stop, and the menu lists the card as failed until a restart |
| `ball-carries-on` | machine | Switch games while a ball is on the playfield | The ball's events reach the new game |

## Whole machine

The program `whole-machine` runs the firmware with a game on the assembled machine.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `no-drops` | machine | Play with every subsystem at its busiest, with balls on the sensors, the seal turning, a movie, a sound on every channel and an effect on every light | `dropped()` stays 0 for every queue |
| `stacks` | machine | Play for half an hour, then read how deep every stack has been used | The main task's, the storage task's and the interrupts' stacks each stay within their size |
| `soak` | machine | Play for an hour | Nothing restarts, no device is reported failed, no log line is lost, and no event is dropped |
