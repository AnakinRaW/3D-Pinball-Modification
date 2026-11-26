# Testing

Each test runs where the code it checks can run. Code that touches no register runs on the computer. Code that drives hardware runs on the Teensy at the bench. A test that needs the coils and a ball runs on the assembled machine.

A test on the Teensy judges what it can from inside the firmware and prints its result. A person carries out only the steps the firmware cannot, such as moving a ball. A person also answers what only a person can see or hear.

## Test programs

Each section below is one test program. Its tests run in the order of its table. The programs run in the order of the sections, since a test relies on the sections above it. A test is named by its program and its row, such as `solenoid.pull-length`. Where a row links a figure, the test takes the figure from the firmware's constant.

A `computer` test builds from `firmware/tests/test_<program>` in the `computer` environment. Code that touches no register is kept apart from code that does, so that it builds there. Fakes stand in for the hardware and for the drivers below the code under test. A fake has the interface of the part it replaces, records every call it gets and answers as its test sets it up.

Every other test is part of the test image `firmware/tests/teensy`, one firmware that holds the Teensy tests of every program in `src/<program>`. The image adds the counters and hooks its tests read, which the normal build lacks. It runs its tests in a FreeRTOS task, as the firmware runs its main loop.

The runner `firmware/tools/teensy-tests.py` flashes the image once and sends it the tests one at a time over USB serial. The Teensy restarts before each program, so every program starts from power-up. Every check is a condition in the test's code. The runner reads only the result lines. A quick run, the default, shortens every duration a row states to a few seconds. `--soak` takes the durations as stated, with the same checks. `--bench` adds the `bench` tests, which need a person at the bench.

A row counts as covered once the code holds a test of its name that runs where the row says: `test<Row>` in its computer test, such as `testOldestFirst` for `oldest-first`, or `{"<program>.<row>", RunsOn::...}` in its program's folder of the image. `firmware/tools/test-coverage.py` counts the covered rows in CI and fails the run where the code and this document disagree.

A program that has a computer test folder needs a test for each of its `computer` rows. A row that describes code still to be written is marked `computer, planned` in its Runs on column and only counts against the coverage, as do the rows of a program without a computer test folder. The mark comes off with the test, since a planned row that has a test fails the run as well.

## Where a test runs

| Runs on | Setup |
|---|---|
| `computer` | The computer alone |
| `Teensy` | The [bench](#bench). The program carries out every step and judges every check, so nobody needs to be there |
| `bench` | The [bench](#bench), with a person who carries out the `DO` steps and answers the `ASK` questions |
| `machine` | The assembled machine with its coils connected and a ball on the playfield, with a person as on the bench |

## Results

The test image prints its run to the serial port in these lines:

| Line | Meaning |
|---|---|
| `RUN <test>` | The test begins |
| `DO <test> <step>` | A person carries out the step. The test waits until it sees the step done, and fails when it does not see it in time |
| `ASK <test> <question>` | A person answers `y` or `n` over the serial port |
| `CHECK <test> <tag> <count>` | The runner checks the debug lines of the tag that the test sent, numbered from 0, and answers `checked <good> <total> <lost>`: the lines that came whole and in order, every line of the tag, and the lines and notes the debug output's loss notices state |
| `PASS <test>` | The test has passed |
| `FAIL <test> <seen> expected <wanted>` | The test has failed, with what it saw and what it expected |
| `SKIP <test> <reason>` | The test could not run, such as for a part that is not connected |
| `END <passed> <failed> <skipped>` | The image has run every test the command named |
| `RESTART <test>` | The test restarts the Teensy and goes on after `resume` |
| `TEST <test> <runs on> <quick s> <soak s>` | The image holds the test. It ends within the seconds given for a quick and for a soak run |
| `READY <count>` | The list of tests has ended. The count gives the tests run since the start |
| `BLANK` | The display is set up and black |
| `CRASH <text>` | One line of the crash report that a crash left before the restart |
| `ERROR <text>` | A command or a test broke these rules, such as a test that reported twice |

The image takes these commands, one to a line:

| Command | Effect |
|---|---|
| `list` | A `TEST` line for every test, then `READY` |
| `run <pattern> [soak]` | Runs every test whose name matches the pattern, in the order of this document, then prints `END`. A `*` stands for any text. Several patterns may follow `run` |
| `resume <test> [soak]` | Runs the part of the test that follows its restart, then prints `END` |
| `reset` | Restarts the Teensy |
| `blank` | Sets the display up, which leaves it black, then prints `BLANK` |

The image keeps no state across a restart. The runner sends `resume` after a `RESTART` line. A restart can leave a stripe of stray bytes on the display, which stays awake through it. The runner therefore restarts the image after its last test and sends `blank`.

The runner fails a test that prints `FAIL` or `ERROR`. It also fails a test that runs past its time from the `TEST` line, and a test during which the serial port goes away without a `RESTART`. A crash report after such a loss marks the failure as a crash. A test past its time stops the run, since the image no longer answers. The runner exits with 0 when every test passed or skipped, and with 1 otherwise.

A computer test reports through its test framework, and its run fails when a single check fails.

## Bench

The bench is the Teensy on USB with the boards of the subsystems under test, the boards on a 5 V bench supply, and the [test card](#test-card) in. The coils stay disconnected, so a pull shows only on its trigger pin. These boards are wired differently on the bench:

| Board | On the bench |
|---|---|
| Solenoid coil board | The Teensy's GND is joined with the board's GND, as its [supply](../parts/solenoid/design.md#supply) requires with the Teensy on USB |
| Servo board | The Teensy's GND is joined with the board's GND, as its [supply](../parts/servo/design.md#supply) requires |
| Lighting boards | The Teensy's GND is joined with each board's GND, as their [supply](../parts/lighting/design.md#supply) requires |
| IR board | It runs from its D24V5F3 on the bench supply, or from the Teensy's 3V3 pin while everything that pin feeds stays within the pin's [rating](../research/teensy-4.1.md#power) |

## Test card

The card is of Class A1 or A2. It is formatted FAT32 with 32 KB clusters and holds two games, `/games/a/` and `/games/b/`, and nothing else. The test programs carry two test games with those ids, which print every call the host makes to them. Each game's directory holds these files:

| File | Content |
|---|---|
| `audio/music.wav` | music of at least a minute, in the [audio driver's format](drivers/audio.md#audio-files) |
| `audio/1.wav` to `audio/4.wav` | short effects in the same format, one for each channel |
| `settings` | a small settings file one driver reads in its `begin()` |

The programs `sounds`, `screen` and `video-player` write their sounds, pictures and videos into `/games/tests/` themselves.

## Core

### Clock

The program `clock` checks the [time measurement](general-design.md#time-measurement).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `wrap` | computer | Compare times on both sides of the wrap of `nowUs()` with `before()`, `elapsedUs()` and `elapsedMs()` | Every answer equals the one the same span gives away from the wrap |
| `rate` | Teensy | Read `nowUs()` and the processor's cycle counter side by side for a minute | The two agree to within the time the reads take |

### Task lock

The program `task-lock` checks the lock that keeps the other tasks out of a short piece of code, [`core/task-lock.h`](../../firmware/src/core/task-lock.h).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `exclusive` | Teensy | Let the test task and a task above its priority count one counter up for two seconds each, by reading it, pausing and writing it back. Run once without the lock and once with it | Without the lock some counts get lost. With the lock none get lost |

### Debug output

The program `debug-output` checks the [debug output](logging.md#debug-output) and the form of a [line](logging.md#lines).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `format` | computer | Write lines whose text ends in one line break, in none and in two | Each line starts with the milliseconds since the start and the letter `D` and ends in exactly one line break |
| `cut` | computer | Write a line twice as long as the longest line | It is cut to the longest line and ends in `...` |
| `full` | computer | Write lines until the buffer is full, then one more and one twice as long | Each new line takes the place of as many of the oldest lines as it needs. The lines between come out intact, followed by a notice of how many lines were dropped |
| `half-sent` | computer | Fill the buffer, send part of the oldest line, then write one more | The rest of the begun line goes first, then as many whole lines as the new one needs. The new line comes out whole |
| `interrupt` | computer | Write a line while posing as an interrupt | The line is dropped, and the next send states it in a notice. The lock is never taken |
| `room` | computer | Send with USB serial taking no bytes, then ten, then any number | `sendDebugLog()` hands over no more than USB serial takes. The next call goes on where the last stopped |
| `send-time` | Teensy | Write a burst of 300 lines into a debug output and send them while the runner reads | Every line leaves. No call of `send()` takes a millisecond |
| `interrupt-line` | Teensy | Add a line to a debug output from an `IntervalTimer` 100 times | The output drops and counts all 100 lines |
| `round-trip` | Teensy | Write 200 numbered lines through `DEBUG_LOG()` and let the runner check what arrives over USB | The runner counts all 200 lines, each whole and in its order |

### Debug trace

The program `debug-trace` checks the [debug trace](logging.md#debug-output), which notes events from interrupts.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `order` | computer | Note two events with their times, then note and take one at a time past the end of the records | Every note comes out in the order it was noted, with its label, its numbers and its time |
| `full` | computer | Note one event more than the trace holds, take one and note again | The extra note is refused and counted. The note after the take fits |
| `lost` | computer | Note two events more than the trace holds, then take every note | The kept notes come first, then a notice of the two lost notes |
| `format` | computer | Turn a note into a line | The line holds the time, `D trace`, the label, both numbers and the note's time in microseconds |
| `from-interrupts` | Teensy | Note numbered events from two interrupts of different priorities for two seconds while the test task takes them | Every note arrives once and whole, each interrupt's notes in their order. None is lost |
| `round-trip` | Teensy | Note 100 numbered events with `DEBUG_TRACE()` from an `IntervalTimer` and let the runner check the debug lines that arrive over USB | The runner counts all 100 lines, each whole and in its order |
| `lost-notice` | Teensy | Note 300 numbered events with `DEBUG_TRACE()` while nothing sends, then let the runner check the debug lines | The runner counts the first 256 notes, each whole and in its order, and a notice of 44 lost notes |

### Ring

The program `ring` checks the ring that the debug output and the card appender keep their lines in and that each queue of the [event queue](input-handling.md) keeps its events in.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `wrap` | computer | Append records of bytes that run past the end of the buffer and read them back | They come back in order, the piece up to the buffer's end first and the rest after it |
| `whole-or-nothing` | computer | Append a record larger than the free space | It is refused. The ring keeps what it held |
| `power-of-two` | computer | Make rings of 1000 bytes, of 0 bytes and of no buffer | The first holds 512 bytes, the others refuse every record |
| `elements` | computer | Append records of several structs to a ring of 8, the second running past the end, and read them back | `size()`, `used()` and `peek()` count structs. Every struct comes back whole and in order, the second record in two pieces |
| `begin` | computer | Append to a ring made without a buffer, then give it a buffer of 6 with `begin()`, append, and call `begin()` again | The first record is refused. The ring holds 4 elements, and the second `begin()` leaves it empty with both counts at 0 |

### Card appender

The program `card-appender` checks the [card appender](../../firmware/src/io/card-appender.h) on fake files whose writes run until the test ends them, and once on the card through the storage driver.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `period` | computer | Append records and let the time run to a second | Nothing is written before the second, then one write takes every record |
| `half-full` | computer | Fill the buffer to half | The write starts at once |
| `flush` | computer | Append a record and call `flush()` | The next `update()` writes it |
| `retry` | computer | Let a write fail, then fill half the buffer and call `flush()` | The next write waits a second and takes the failed write's records to the same place, with the new ones behind them |
| `start` | computer | Append to one file, `start()` a second, append and `start()` a third at once | The first file gets the records before the start. The second is started anew with the rest. The third start is refused until the second has its records |
| `lost` | computer | Append records past the free space | Each is refused and counted. `takeLost()` gives the count once |
| `no-file` | computer | Append before any file, then `start()` one and append | The first record is refused and counted, the next starts the file anew |
| `wrap` | computer | Append records that run past the end of the buffer and let a second pass | They reach the card in two writes, the second right after the first |
| `card-retry` | Teensy | Give the appender a file on the card that is shorter than it believes, append a record, then lengthen the file | The first write fails and keeps the record. A second later the record reaches the card at the place the failed write would have taken, and none is lost |

### Priority list

The program `priority-list` checks the [priority list](general-design.md#priority-lists).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `order` | computer | Put entries of three priorities into a list, two of them at equal priority | The list holds them highest first, the later of the two equal ones above the earlier |
| `full` | computer | Put one entry more into a list than it has room for | `insert()` refuses the extra entry, and the list keeps the others in their order |
| `remove` | computer | Remove the top entry and one from the middle, then put two new ones in, while holding pointers to the entries that stay | The others keep their order, and each pointer still reaches its entry |

### LED color encoder

The program `ws2812-color-encoder` checks the encoding the [lighting driver](drivers/lighting.md#how-a-frame-goes-out) sends its bits with.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `symbols` | computer | Read the highs, the lows and the length of a 0 and a 1 | Each lies inside the windows of the older WS2812B revision and of the WS2812B-2020 datasheet |
| `order` | computer | Encode one LED and decode its words | The LED goes out green, red, blue, each most significant bit first |
| `lanes` | computer | Encode two LEDs into one lane of three alternating lanes | The words land every third word, and the other lanes' words stay |
| `padding` | computer | Encode fewer LEDs than the lane holds, then more | Zero words follow the last LED, and an LED that does not fit whole stays out |
| `scale` | computer | Encode colors at a brightness under full | Every color is scaled as FastLED's `scale8()` scales it |

### Event queue

The program `event-queue` checks the [event queue](input-handling.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `oldest-first` | computer | Publish into several queues in turn, each queue in time order | `read()` gives every event once, oldest first across all queues |
| `across-wrap` | computer | Do the same with times on both sides of the wrap of `nowUs()` | The order holds across the wrap |
| `full` | computer | Publish into a queue until it is full, and once more | The last publish returns false and counts in `dropped()`, and the queued events come out unchanged |
| `drop-warning` | computer | Let a queue drop two events, then read twice with a log begun | The log holds one warning that names the queue and the two dropped events |
| `payload` | computer | Publish an event of every type with `publish<Tag>()` | `payload<Tag>()` gives every payload back unchanged |
| `refused` | computer | Attach a depth of three, a depth of zero and no storage, then queues until every one is taken, and one more. Publish through each refused queue | `attach()` returns false for each refusal, and the queue it points at drops the event and counts it in `dropped()` |
| `from-interrupts` | Teensy | Publish numbered events from two interrupts of different priorities for ten minutes while the main loop reads | Every event arrives once and whole, and each queue's events arrive in their order, while `dropped()` stays 0 |

### Driver tick

The program `driver-tick` checks the [driver tick](driver-design.md#driver-intervaltimer). Its `computer` tests step a fake `IntervalTimer` one tick at a time.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `due` | computer | Attach callbacks with periods of one, two and twenty ticks and step forty ticks | Each callback runs on exactly the ticks its period gives |
| `refused` | computer | Attach a period of no ticks, of half a tick and of one and a half ticks, then callbacks until every slot is taken, and one more | `attach()` returns false for each refusal, and the attached callbacks run on the next tick |
| `timer-start` | computer | Attach two callbacks | The first `attach()` starts the timer with the tick's period and priority, and the second leaves it as it is |
| `no-timer` | computer | Let the fake timer refuse to start, then let it start | The first `attach()` returns false and keeps no callback, and the second starts the timer |
| `periods` | Teensy | Attach callbacks with periods of one, two and twenty ticks and let them run for a minute | Each callback runs as often as its period gives, and every run lies a whole number of ticks after the one before |
| `refusals` | Teensy | Attach a period that is no multiple of the tick, then callbacks until every slot is taken, and one more | `attach()` returns false for the odd period and for the extra callback, and the attached callbacks keep running |

### Device monitor

The program `device-monitor` checks the [device monitor](error-handling.md#device-faults) on a fake driver.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `changes` | computer | Let the fake driver report parts failing and recovering, with error codes | Each change gives one `DEVICE_FAILED` with the device, the part and the code, or one `DEVICE_RECOVERED`, and an unchanged answer gives nothing |
| `full-queue` | computer | Let parts fail while the monitor's queue is full, then empty it | Each part is reported at the first check after the queue has room |
| `source` | computer | Pack the source of every device and part | `faultDevice()` and `faultPart()` give back what `faultSource()` packed |

### PSRAM

The program `psram` checks the [PSRAM](../parts/psram/design.md) on the Teensy's underside.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `found` | Teensy | Read `external_psram_size`, which Teensyduino sets at start-up | It equals the [chip's size](../parts/psram/design.md#chip) in MB |
| `card` | Teensy | Read `background.img` through the [storage driver](drivers/storage.md) into the PSRAM, once straight and once at an address off the 32-byte steps | Both copies equal the file |
| `patterns` | Teensy | Send the waiting debug lines, then write fixed patterns and a pseudo-random sequence into the whole PSRAM and read each back, as [PJRC's memory test](https://github.com/PaulStoffregen/teensy41_psram_memtest) does | Every byte reads back as written |

## Drivers

### Storage

The program `storage` checks the [storage driver](drivers/storage.md) against its [use cases](drivers/storage.md#requirements-and-use-cases).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `service-order` | computer | Queue reads of each priority in mixed order | A higher priority goes before a lower one, each priority in the order its requests came |
| `busy` | computer | Start a second operation on a request that is still pending | Both requests fail, and the request starts afresh once it has ended |
| `long-path` | computer | Open a file whose path is longer than the [driver allows](drivers/storage.md#files), and one of the longest length | Every request on the first fails at once, and the second works |
| `read-ends` | computer | Read across the end of a file, at its end and from a missing file | The read stops at the end. The other two bring 0 bytes and succeed |
| `past-end` | computer | Start a write past the end of a file, then one at its end | The first fails without counting as a card error and leaves the file, the second appends |
| `open-files` | computer | Read one file more than the driver keeps open, then write a file it read | The file used longest ago is closed, and the write opens its file anew for writing |
| `card-errors` | computer | Let transfers fail, one at a time and three in a row, and let the card vanish | One error starts the card anew. Three in a row, or a card that does not come back, make it count as failed, and every request then fails at once |
| `recover` | computer | Let a failed card answer again, first as another card, then as the same | Another card stays unused, the same card works again |
| `refused` | computer | Start requests as a caller that may not start one, and before the mount | Each fails at once, and a pending request stays as it is |
| `wait-ends` | computer | Wait for requests, let one go out of scope while pending, and wait where waiting is refused | `wait()` and the destructor return once the request has ended. The refused `wait()` returns false at once |
| `cache-path` | computer | Ask for reads into RAM1 and into cached memory on and off cache lines | Only RAM1 and whole cache lines at a sector of the file take the bytes straight |
| `card-file` | computer | Let the card appender write through the storage driver's file, once with a failing card | The records reach the file, and the failed write keeps them |
| `mount` | Teensy | Start with the card in | `begin()` returns true, and the card is not reported failed |
| `round-trip` | Teensy | Write a file, read it back, rewrite it shorter, read it again and append | Each read brings what was written last, and `fileSize()` follows |
| `missing` | Teensy | Read a file the card does not hold, then write one into a directory the card does not hold | The read brings 0 bytes, and the write creates the directory and the file |
| `order` | Teensy | Start a write, then a read of the same place before the write has ended | The read brings the new data |
| `scope-end` | Teensy | Let a request go out of scope while it is pending | Its destructor returns once the request has ended, and nothing writes into its buffer afterwards |
| `interrupts` | Teensy | Start a request from an interrupt more urgent than the [limit](general-design.md#interrupts), then one from the limit that calls `wait()` | The urgent request fails at once. The other is pending, its `wait()` returns false at once, and it ends normally |
| `loop-runs` | Teensy | Write a file of several megabytes and read it back while the test task polls like the main loop | No pass lasts as long as a driver tick |
| `yield` | Teensy | Call `yield()` from the test task and from an interrupt while a large read runs | Each call returns at once, and the read ends normally |
| `cached-reads` | Teensy | Read into RAM2 and the PSRAM, on cache lines and off them | Each read brings the right bytes, and the bytes around the buffer stay unchanged |
| `stack` | Teensy | Read how much of its stack the storage task has used after the tests above | A quarter of the stack stays untouched |
| `card-back` | bench | Pull the card while the program reads, then put it back | The reads fail, every request fails at once, and the card works again once it is back. A quarter of the storage task's stack stays untouched after the card errors |
| `no-card` | bench | Pull the card and let the Teensy restart without it | `begin()` returns within a tenth of a second, every request fails at once, and the card is reported failed |
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

### LED output

The program `ws2812-flexio2` checks the [FlexIO2 output](drivers/lighting.md#how-a-frame-goes-out) the lighting driver sends its frames through. It needs no wiring.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `wave` | Teensy | Send one frame of ten LEDs over six lanes on the lighting's pins, each LED a different color, and read the pins back through FlexIO2's pin register while the frame goes out | Every lane carries exactly its LEDs' bits, each high lies within the reading's tolerance of the encoder's high for a 0 or a 1, and every line is low before and after the frame |
| `two-lanes` | Teensy | Do the same with two lanes, chain F's pin first and chain A's second | The same |
| `busy` | Teensy | Send a frame, send a second one right after it, and send a third after the first has gone out | The output is busy while the first frame goes out and refuses the second at once. Then it is free again and takes the third |
| `refusals` | Teensy | Start outputs with no lane, with one lane more than FlexIO2 has shifters, with a pin that does not reach FlexIO2 and with one pin for two lanes, then send on one of them, and ask for the FlexIO2 signal of that pin | Each start and the send return false, and the pin has no signal |
| `long` | Teensy | Send one frame of the longest chain the lighting allows on chain F, where no LED is wired, and check each bit as it comes back | Every bit of the frame comes back as sent, each high within the reading's tolerance, and the line is low after the frame |
| `load` | Teensy | Do the same frame after frame, at the lamps' frame period, while a second DMA channel copies between two buffers in RAM2 without a pause. A soak run sends ten times as many frames | Every frame comes back whole |
| `flexio3-load` | Teensy | Do as `load`, while the processor writes FlexIO3's eight shifter buffers once every display burst, as a sending [display](drivers/display.md) does, with FlexIO3 disabled | Every frame comes back whole |
| `latch` | Teensy | Send a short frame on chain F and the next one the moment the output is free again | The line stays low between the two frames for longer than the reset time that ends a frame for the LEDs |
| `clock` | Teensy | Move the clock root FlexIO2 and FlexIO3 share to another rate. Start an output with FlexIO3 enabled, then one with FlexIO3 off | The first start is refused and leaves the root alone. The second sets the root back to the rate the bit timing needs and starts, and a frame goes out afterwards |
| `shared` | Teensy | Send a frame through one output, then send through a second output while the frame goes out, and again once it is out | The second output is busy while the first one's frame goes out and refuses its send at once. Afterwards its send goes through |
| `setup` | Teensy | Start an output and read FlexIO2's timer registers back | The registers hold the timer setup the start wrote |

### Lighting

The program `lighting` checks the [lighting driver](drivers/lighting.md). Its tests up to `layout-6a` need LEDs on chain A alone.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `profiles` | Teensy | Start a driver without chains with `lighting.adapter_a` absent, below, inside and above its range | Each start selects the larger adapter's profile from that adapter's rating on, and the smaller adapter's profile below it |
| `begin` | Teensy | Start the driver without `configure()`, then show | `begin()` returns true with the smaller adapter's profile and no chain, FastLED holds the strip's power model, and `show()` returns false |
| `counts` | Teensy | Configure chain A with more LEDs than a chain may hold and the other chains with none, start the driver, then configure three LEDs and start it again | Chain A holds the bound's LEDs, all dark, the other chains hold none, and the second start keeps the chains of the first |
| `colors` | bench | Show red, green, blue and white on chain A, then a rainbow | Each LED shows each color, and the cap dims none of them |
| `limits` | bench | Show white at the profile's playfield brightness and at its ambient brightness, then its perimeter pattern | The LEDs step down to the ambient brightness, and the pattern lights the LEDs the profile's perimeter step names |
| `cap` | Teensy | Work out the brightness of three LEDs at full white under the profile's cap, then under a cap they exceed | Under the profile's cap the brightness is full. Under the lower one it is the brightness the program's sum gives, and the LEDs' draw at it stays within that cap |
| `layout-3a` | Teensy | Sum the whole light layout at full white within the limits of the smaller adapter's profile with FastLED's power functions | The sum stays under the profile's cap, so the driver dims nothing |
| `layout-6a` | Teensy | Do the same with the larger adapter's profile | The same |
| `chains` | bench | Run one lit device along each chain from its start, chain after chain, with the chain's number on the serial monitor | Each chain lights all its devices in order, and no device of another chain lights |
| `current` | bench | Limit the bench supply to just above the [budget](../parts/lighting/design.md#current-ceiling), connect every chain and set every device to full white | The supply never reaches its limit |

### IR ball sensing

The program `ir-sensing` checks the [IR driver](drivers/ir-sensing.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `phase-plan` | computer | Plan the phase for every channel count from one up | Every plan keeps the bounds of the [phase length](drivers/ir-sensing.md#phase-length), a count that does not fit is refused, and the count worked through there gives its figures |
| `calibration` | computer | Calibrate on fake averages near `clear_build`, near `ball_build` and near neither | Each channel gets the scale, `clear`, `ball`, `threshold` and `release` of the [start-up rules](drivers/ir-sensing.md#at-start-up), and the third counts as failed |
| `entries` | computer | Start with `ir.channels` entries that keep the [rules](drivers/ir-sensing.md#config), one whose ball value lies under its clear value and two of the same channel | The good entries are taken, the others are left out with a log line each, and their channels fail calibration |
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
| `no-queue` | computer | Begin a driver on an event queue that takes no further queue | `begin()` returns false, the log holds a warning that names the queue, the pin keeps no interrupt, and the driver answers with empty data |
| `at-start` | computer | Begin with the beam broken, then let the [fault time](drivers/break-beam.md#device-faults) pass | The pin is an input with hysteresis and an interrupt on both edges, no `BALL_DRAINED` comes, and the beam fails once the fault time has passed since `begin()` |
| `crossing` | computer | Break the beam after it was clear for a second, then clear it | The break gives one `BALL_DRAINED` from gate 0 with the time of the break, the clear gives none, and `blocked()` follows the pin |
| `flicker` | computer | Clear the beam between two breaks for just under the [flicker window](drivers/break-beam.md#ball-sensing), then for exactly the window, then read a break twice | Only the break after the full window gives a second `BALL_DRAINED`, and the break read twice gives none |
| `stays-broken` | computer | Keep the beam broken for just under the fault time with a flicker in between, then for the fault time, then clear it | The beam fails only once it has stayed broken for the fault time since the flicker, with code 0, and works again once clear |
| `wrap` | computer | Run the flicker window and the fault time across the wrap of `nowUs()` | Both give the answers they give away from the wrap |
| `second-driver` | computer | Begin a second driver while the first holds the pin, and call it before and after | Its `begin()` returns false, the log holds a warning that names the held pin, `blocked()` is false and no part fails, and the first driver keeps its events |
| `pad` | Teensy | Begin the driver | The pin's pad has hysteresis without pull-up or keeper, its interrupt is enabled on both edges, and `blocked()` follows the pin |
| `pass` | bench | Pass a ball through the beam ten times, slowly and fast | The ten passes give ten `BALL_DRAINED` and no more, and `blocked()` is false once the ball is out |
| `failed` | bench | Hold a ball in the beam for longer than the [fault time](drivers/break-beam.md#device-faults), then take it out | The beam is reported failed within a second past the fault time, and recovered once it is clear |

### I²C bus

The program `i2c-bus` checks the [I²C bus](io/i2c-bus.md). The rows on `Wire` need the rotary sensor and the port expander connected, and the rows named `wire2-` need the display module's touch controller on `Wire2`.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `turns` | Teensy | For a minute, request a read of the rotary sensor every tick and a new read of the port expander each time one ends | Every request is followed by one run and one call of its done function. A read of the rotary sensor never waits for more than one read of the port expander |
| `no-answer` | Teensy | Run a transfer to an address no device answers at, then a read of the rotary sensor | The first ends with the missing answer as its error. The second works |
| `probe` | Teensy | Run a transfer with neither part, once to the rotary sensor's address and once to an address no device answers at | The first ends without an error. The second ends with the missing answer |
| `held-low` | bench | Hold SDA at GND with a wire for a moment while transfers run, then let go | The transfers end with the line held low as their error. Transfers work again once SDA is free |
| `wire2-probe` | Teensy | Run a transfer with neither part on `Wire2`, once to the touch controller's address and once to an address no device answers at | The first ends without an error. The second ends with the missing answer |
| `wire2-turns` | Teensy | Request a read of the touch controller's five touch bytes every driver tick, and a read of its first register each time the first read ends | Every request is followed by one run and one call of its done function, and no transfer ends with an error |
| `wire2-repeat` | Teensy | Request a read three times before the bus starts it, then three times more while it runs | The read runs twice |
| `wire2-held-low` | bench | Request a read of the touch controller every driver tick, hold SCL on `Wire2` at GND with a wire for about a second, then let go | The reads end with the line held low as their error. Reads work again once SCL is free |

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
| `inputs` | bench | Close the contact on each pin from E-1 to E-4 for a moment, one after the other | Each closure gives one `ContactClosed` with its pin as the source. `closed()` shows the pin while it is closed |
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
| `defaults` | Teensy | Start with neither angle in the config, move to each end position and time the pulses | The pulses name the two defaults of the [config](drivers/servo.md#config) |
| `speed` | Teensy | Set `servo.speed_deg_s` below and above one degree per frame, move between the end positions and time the pulses | The pulse's angle changes at the set speed and stops on the end position |

### Audio

The program `audio` checks the [audio driver](drivers/audio.md). Its computer tests play the channels and the mixer from a fake card. Its `Teensy` tests write their sounds to the card and listen through a tap. A tap is a recording stage that the program connects behind the master gain, so it can compare the samples bound for the amplifier with the samples in the files. The sounds are a ramp of samples that count up by one, the same ramp behind extra chunks ahead of the data chunk, a falling ramp, a constant level and a full-scale square wave.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `wav-chunks` | computer | Play files of several lengths, with odd-sized and long chunks ahead of the data chunk and behind it | Every sample of the data chunk comes out once and in order, the first block gliding in, and the sound ends with its number |
| `wav-format` | computer | Play a stereo file, a file at another sample rate, a RIFF file that is no WAV, a file without a fmt chunk, a missing file, a file with no name, and a file whose read fails partway | Each ends with its number. The failed read ends behind the samples before it |
| `wav-ffmpeg` | computer | Play a sound that the command of [`media.md`](../../assets/media.md#sounds) made from a stereo MP3 at another sample rate | Every sample of its data chunk comes out once and in order, the first block gliding in, and the sound ends with its number |
| `wav-loop` | computer | Play a file with chunks around its data chunk in a loop for forty blocks | After the first block the samples run through the file again and again with no gap, and no end comes |
| `card-waits` | computer | Play a file while the card brings one chunk every fifty blocks | The blocks without data are silent, and every sample comes once and in order |
| `position` | computer | Note a sound's position, play another, play on from the position, and do the same in the bytes behind the samples of a sound that loops. Read the position right after each request | The sound goes on at the sample noted, and the loop starts over at its first sample. Right after a request, the position is the one the request starts from, and empty for silence |
| `glides` | computer | Start a sound, set volumes inside and outside 0 to 1, replace the sound and stop it | Each change glides linearly across one block, a volume outside counts as the nearer end, and a replaced or stopped sound reports no end |
| `ends` | computer | Let a sound play out, replace one, stop one, and replace a request before its block | Only the sound that played out reports its number, once |
| `mix-sum` | computer | Add blocks of two channels below full scale | The sum comes out exactly, one block late, also into one of the inputs |
| `mix-limit` | computer | Add four full-scale blocks behind a quiet one, then quiet blocks again | No sample passes full scale. The quiet block glides down ahead of the loud ones without a jump, and the gain climbs back to 1 within 100 ms |
| `samples` | Teensy | Play the ramp and the ramp behind extra chunks at full volume | The tap records every sample of each file once, in order, from the first, scaled by the master gain |
| `mix` | Teensy | Play the square wave on every channel at once | The tap records no sample beyond full scale times the [master gain](../parts/audio/design.md#level) |
| `loop` | Teensy | Play the ramp in a loop a hundred times | Each loop's first sample follows the last sample of the loop before, with no gap |
| `replace` | Teensy | Call `play()` on a channel that plays a sound | The old sound glides out without a jump, and the new one starts from its first sample |
| `glide` | Teensy | Start and stop a sound, and change its volume while it plays | Each change glides over one block, with no jump between neighboring samples |
| `volume` | Teensy | Set volumes inside and outside 0 to 1 | The tap records the samples scaled by the volume, and a value outside counts as the nearer end |
| `resume` | Teensy | Note `position()`, play another sound on the channel, then play on from the position | The first sound goes on from the sample where it stood |
| `ended` | Teensy | Let sounds play out, cut others off with `play()` and `stop()`, and let one loop | `SOUND_ENDED` with the number `play()` returned comes once for every sound that played out, and for no other |
| `missing` | Teensy | Play a file the card does not hold while another channel plays | The sound ends at once with `SOUND_ENDED`, and the other channel plays on |
| `starved` | Teensy | Write several megabytes to the card while a sound plays | The tap records silence while data is missing, then the sound going on from where it stopped, and no stray sample |
| `by-ear` | bench | Play looping tones and a blip on every other channel, restart one blip many times in a row, and let the tones loop for several minutes | No sound drops out or clicks, every effect starts from its beginning, and the music loops |

### Display

The program `display` checks the [display driver](drivers/display.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `start` | bench | Start the Teensy from power-up, then restart it while the display is awake | The screen is white until the panel is set up and then turns black both times, with no garbage |
| `restart` | bench | Disconnect the 5 V of J-DISP for half a second while a picture shows, then connect it again | The picture returns within two seconds, and the Teensy keeps running |
| `picture` | bench | Send color bars, a ramp of every level of red, green and blue, and the words TOP LEFT in that corner | The colors are right, the ramps run smoothly, and the words stand upright at the top left |
| `rectangles` | Teensy | Send rectangles of several sizes and places, then one off the 16-pixel steps, one beyond the screen and one while another is on its way | The function handed in runs once per rectangle on the steps, after the rectangle's bus time and close behind it, and the three others are refused |
| `refills` | Teensy | Send the screen in bands from RAM2 without a pause for a minute while a loop counts its passes, in turns with the same loop while nothing is sent | The refills take no more of the processor than the [measured share](drivers/display.md#performance--memory-usage), and a screen takes its bus time within 5 % |
| `restart-steps` | Teensy | Send a screen, call `restart()` while it goes out, then send a second screen during the restart | The first screen finishes. `restarting()` is true from the call until the panel shows again, after the panel's waits after its reset and after Sleep Out and at most two of the display's ticks more. The second screen goes out only then |
| `setup` | Teensy | Send two thousand small rectangles, then call `restart()` ten times, and read FlexIO3's registers back after each rectangle and each restart | Every register holds what the driver writes for bursts after a rectangle and for single writes after a restart |
| `repeated-restarts` | bench | Call `restart()` twenty times in a row and send a green screen after each restart | The screen turns green after every restart |
| `with-lighting` | Teensy | Start the [lighting](drivers/lighting.md) with the longest chain on chain F beside the display, and send a dark light frame every frame period of the [lamps](components/lamps.md) while the screen goes out in bands without a pause | No light frame is refused, and a screen takes its bus time within 5 % |

### Touch

The program `touch` checks the [touch driver](drivers/touch.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `touch` | bench | Touch the marks the test draws in the corners and the center, then drag a finger and rest it | Each touch lands inside the mark touched, and `TouchDown`, `TouchMoved`, `TouchHeld` and `TouchUp` come as the [events](drivers/touch.md#driver-events) describe |
| `touch-lost` | bench | Show a picture, then pull the 5 V lead from the module's Vin for about two seconds and plug it back in. On `DEVICE_RECOVERED`, restart the display as the host does and send the picture again | The controller is reported failed with the error flags of its failed reads, then recovered. The picture shows again, and the Teensy keeps running |
| `tick` | computer | Begin with the controller answering, then step the driver tick | A read is requested once every tick of the touch driver |
| `orientation` | computer | Answer reads with the readings measured at the picture's edges, between them and beyond them | Each reading lands where the [mapping](drivers/touch.md#reading) puts it, a reading beyond an edge on that edge, in `finger()` and in the events |
| `events` | computer | Answer reads for a touch that moves by less and by more than 10 pixels, rests and leaves, then for one that rests within 10 pixels of where it came down | `TouchMoved` comes only for a move of 10 pixels from the position last reported, a touch that strayed brings no `TouchHeld`, the resting one brings one `TouchHeld` within a read of 1 s, and `TouchUp` carries the last position |
| `failed-read` | computer | Let reads fail with a finger on the screen, for less and for more than 100 ms, then let one work | Past 100 ms the controller is reported failed with the flags of the last failed read, one `TouchUp` releases the finger, and the working read recovers it |
| `absent` | computer | Begin with the controller not answering, then step the driver tick | `present()` is false, no read is requested, no fault is reported and no event comes |
| `calibration` | computer | Configure other readings at the picture's edges, readings above 4095 beside a twin at 4095, and two readings of each axis less than 100 apart | The edges map onto the new readings, the first driver behaves as its twin, and the axes that lie too close keep their defaults |
| `reads` | Teensy | Begin with the display module connected and let the driver read for two seconds without a touch | `present()` is true, no fault is reported, no event comes and `finger()` shows no finger |

### Every driver

The program `every-driver` checks the [rules every driver follows](driver-design.md) on every driver in `firmware/src/drivers/`. Each driver stands in the program's register with a case for each row, and a row runs every driver's case. A driver without a case for a row is named in the run's output.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `before-begin` | Teensy | Call each driver's members before its `begin()` | Every call answers at once with false or empty data |
| `out-of-range` | Teensy | Configure each driver with settings past both ends of their ranges, beside a twin configured at the nearer ends | Each driver behaves as its twin |
| `at-once` | Teensy | Make every driver call that starts a long transfer or a move, such as a large read, a full light frame, a full screen or a sound | Each call returns while its transfer still runs, and a second light frame right after the first returns false at once |
| `begin-fails` | Teensy | Take every slot of the driver tick, then call every driver's `begin()` | Each `begin()` that needs a slot returns false, and every later call of that driver answers at once with false or empty data |

## Components

### File system

The program `file-system` checks the [file system component](components/file-system.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `paths` | computer | Open a path inside a game's directory, an empty path, one from the root, one with `..`, one too long, one before any game is named, and a full path for the host | The first gives the file in `/games/<id>/`, the last the file at the card's root, and every other a file whose requests fail |
| `switch` | computer | Open a file, name another game with `use()`, and open again | The first file keeps its path, and the second lies in the other game's directory |
| `root` | computer | Name the card's root with `useRoot()`, open a path, an empty path, one from the root, one with `..` and one too long, then name a game and open again | The first gives the file at the card's root, every other a file whose requests fail, and the last lies in the game's directory |

### Config

The program `config` checks the [config component](components/config.md). It hands the component the text of a file instead of reading the card, and reads keys it makes up itself.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `defaults` | computer | Start without a file and read several keys with and without a default, then start with a file that does not parse | Every read gets its default or no value, and no file is written. With the broken file, the log names the line that broke, and the file stays as it is |
| `values` | computer | Start with a file that holds a value inside its range for every key read, in sections, in a list of values and in a list of entries | Every read gets the file's value, `count()` gets the length of each list, and the log stays empty |
| `clamp` | computer | Read a value above its range and one below | Each gets the nearer end of its range, with one log line each |
| `wrong` | computer | Read a value that is text, with a default and without one | The read with a default gets the default, the other gets no value, and each writes a log line |
| `save` | computer | Start with a key nothing reads, remove a list, set a new one of entries, save and read the file back | The file holds the unknown key, the new list one entry to a line, and no trace of the old list |
| `save-busy` | computer | Save, and save again before the first write has ended | The second save is refused, and the file holds the first |

### Lamps

The program `lamps` checks the [lamps component](components/lamps.md). Its computer tests run on a fake lighting driver and a stand-in of FastLED's colors, palettes and curves.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `effects` | computer | Start every effect on a fake lighting driver and step the clock through it | Each lamp shows the color its [effect](components/lamps.md#effects) gives at every moment, `run()` moves its pattern one lamp per step in its direction, and a brightness outside 0 to 1 counts as the nearer end |
| `overlap` | computer | Blink a lamp for one mode, lay effects of two modes of higher priority over it, then remove those modes one by one | Each removal shows the effect below, and the blink goes on in its old rhythm |
| `six-modes` | computer | Set one lamp for six modes | The sixth effect is refused and counted, and the five before it stay |
| `separate` | computer | Give two playfield lamps of one mode a blink and a steady color, then clear the steady one | Each lamp shows the effect set on it, and the blink goes on in its rhythm after the other lamp's clear |
| `spans` | computer | Configure the lamps of two playfield sections with lamps of several LEDs, a count past a chain's end, a count of 0 and fewer counts than LEDs, and set each lamp to a different color | Each LED shows its lamp's color: a long count is cut at its chain's end, a 0 takes one LED, and the LEDs past the counts are one lamp each |
| `chains` | computer | Place the playfield's lower section on chain A, the ceiling on a chain past F and the upper section on chain A as well, then light the lower section's lamps | `chainLeds()` refuses the two misplaced groups, they hold no lamps, and the lower section lights on chain A |
| `gaps` | computer | Configure the right perimeter with two hidden stretches, run a chase along it, then run one after `skipGaps()` | No hidden LED lights. The first chase takes the same time for every LED, hidden or not. The chase after `skipGaps()` lights the visible LEDs one after the other at an even pace. The smaller adapter's profile takes every second visible LED |
| `sync` | computer | Configure both perimeters with 79 LEDs and hidden stretches of different length and place, and start a chase on both in one pass, on both profiles | In every frame where both perimeters light an LED, they light the same LED. Both light their last lamp in the same frame |
| `heights` | computer | Give the perimeters of `sync` heights that make the right one run sideways for a stretch, with one height out of order on the left, and start a chase on both in one pass | In every frame, the LEDs both perimeters light lie less than one LED spacing apart in height. Both light their last LED in the same frame. The height out of order is left out |
| `path-gaps` | computer | Run a chase along `perimeter` over the perimeters of `sync` | The chase lights the visible LEDs up the left and down the right one after the other at an even pace, without a pause at either hidden stretch |
| `playfield-gaps` | computer | Configure a playfield section with a hidden LED and a hidden stretch, cut into lamps of 2, 1 and 2 LEDs, and give every lamp a different color | Every hidden LED stays dark, a lamp starts after a gap, the lamp that meets a gap ends there, and the LEDs past the counts are one lamp each |
| `groups` | computer | Configure chains of different lengths, read every group's size on both profiles, take parts of the playfield at and past its end, and run a wipe along `ambient` | Each group holds the lamps its chains and the profile's perimeter step give, a part past the end is cut there, and the wipe starts at the left perimeter's first lamp |
| `millis-wrap` | computer | Start a blink, a fade and a color with `lasting()` on three playfield lamps shortly before `millis()` wraps | The blink keeps its rhythm, the fade its pace and the lasting color its end across the wrap |
| `since` | computer | Date a pulse on a lamp and on a strip back by half a period, date a pulse ahead of now, date a fade back by half its time, and date a color back with `lasting()` after it | Each pulse dated back shows the color of half a period at once. The pulse dated ahead starts now. The fade shows half its way from the color the lamp showed when it began. The color ends where `lasting()` set its end |
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

The program `sounds` checks the [sounds component](components/sounds.md). Its computer tests run it on a fake audio driver, whose channels are the driver's channels and play from a fake card. Its `Teensy` and `bench` tests run it on the audio driver, and its `Teensy` test listens through a tap behind the master gain, as the [audio](#audio) program does.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `channels` | computer | Play effects until every effect channel is busy, then one more | The music keeps its channel, each effect takes a free channel, and the extra one replaces the effect that started first |
| `video-sound` | computer | Play music, then a video's sound, and let it end | The video's sound takes the music's place, the music goes on from where it stood, and no `MUSIC_ENDED` comes for the video's sound |
| `video-sound-top` | computer | Start a mode's track at the highest priority and stop the track below while a video's sound plays, then let the video's sound end, and play a video's sound with no track | The video's sound plays on, the mode's track starts from its beginning once the video's sound has ended, and with no track the music stays off |
| `tracks` | computer | Start tracks for three modes, two of them at equal priority, start a second track for one mode, then stop the track on top and let the next one end | Only the track of the highest priority plays, between the equal ones the later, a covered track is paused and resumes where it was paused, a second track of a mode replaces the mode's first, and a track below that is stopped leaves the track on top playing |
| `same-pass` | computer | Play a track and cover it with a second, then stop the second and start a third on top before the driver plays a block | The first track waits paused at the sample it was to go on from, and goes on there once the third stops |
| `music-ended` | computer | Let a track end, a looping track run, a track end right before another covers it, the end of a replaced track arrive late, and an effect end | Each track that played to its end gives `MUSIC_ENDED` once, with the key of its mode as the source, also when it is covered by then. A covered track that has ended never starts again. The looping track, the replaced track and the effect give none |
| `volume` | computer | Set both volumes across 0 to 1 and outside, their squares adding up to 1 at most | The driver gets the square of each percentage, and a value outside counts as the nearer end |
| `balance` | computer | Set music and effects whose squares add up to more than 1, then lower the effects, then call `reset()` | The driver gets both squares divided by their sum. Once the effects leave room, the music returns to the square of the volume it was set to. `reset()` hands the driver the scaled volumes again |
| `start-volumes` | computer | Start with the default starting volumes, then with the music at 1 and the effects above 1, then with the music below 0 | The defaults reach the driver squared and unscaled, the second start gives both a half, and the third gives the music 0 and the effects 1 |
| `names` | computer | Play names in the game's audio directory, and one too long | Each plays `/games/<id>/audio/<name>`, and the long one finds no file |
| `reset` | computer | Play on every channel with tracks waiting below, change the volumes, then call `reset()` | Every channel stops and gets the set volumes back, and no track goes on |
| `handover` | Teensy | Play a mode's track, lay a video's sound over it, and let the video's sound end | The tap records the track from its first sample up to where it was paused, the video's sound from its first sample, then the track going on from the sample where it was paused. The track glides out without a jump between neighboring samples, each sound glides in across its first block, and no `MUSIC_ENDED` comes |
| `resume` | bench | Play music, a video's sound in the middle of it, and let the video's sound end | The music goes on where it stopped, without a click |

### Screen

The program `screen` checks the [screen component](components/screen.md). Its computer tests run LVGL with a fake display driver. Its Teensy tests write `background.img`, a full-screen picture, `short.img`, the same picture cut off halfway through its pixels, and `icon.img`, a picture smaller than the screen with transparent pixels.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `widen` | computer | Change objects that start and end at every column, then two objects far apart in one refresh | Every rectangle the driver gets spans whole rows and covers the change, and a refresh hands the driver one rectangle |
| `gestures` | computer, planned | Feed touch events of short and long touches, still and moving, onto a modal object, onto a button, onto no object and from beside a button onto it | A tap is a touch inside the [tap limits](components/screen.md#touch), a hold is `TouchHeld`, the button gets the touch that comes down on it and none that slides onto it, each tap and hold reaches the game only when no object took the touch, and `takeGesture()` gives the host every gesture once |
| `slides` | computer | Take slides for three modes, two of them at equal priority, ask again for one mode, then remove the slide on top, while a button lies on each slide. Start an animation with `animate()` on two slides and one more on the covered slide | Only the slide of the highest priority shows, between the equal ones the later, the second call gets the same slide, only the button of the slide that shows takes a touch, the animations of covered slides stand still, and the next slide shows once the top one is removed, its animation going on where it stood |
| `hide-overlay` | computer, planned | Hide the game overlay layer, change its objects, show it again, then hide it and start a new game | The changes show once the layer is shown, and the new game starts with its overlay layer shown |
| `load` | Teensy | Load `background.img`, `short.img`, `icon.img` and a picture the card does not hold | `loading()` is true until each has loaded, the background and the icon show, and the short and the missing one make `failed()` true |
| `memory` | computer | Load a full-screen picture and smaller ones, release the full-screen one, reserve memory for a full-screen frame, then load until the memory is full | The frame gets the memory the full-screen picture freed, and a picture that finds the memory full makes `failed()` true |
| `show` | Teensy | Show `icon.img` in three images, delete the parent of one, then release the picture | The images show the same pixels, the other two stay when one is deleted, and `release()` deletes them |
| `layers` | bench | Create an object on the host's layer, a message box and an object on the game overlay layer, an object on a slide and an object on the background over `background.img`, then play a video on the slide | The objects lie over one another in the [order of the layers](components/screen.md#layers), the message box between the host's layer and the game overlay layer, the video and the slide's object in front of the background's objects, and the picture behind them |
| `reset` | computer | Switch games while a picture and a frame of the stopped game still load | Every object, slide and image of the stopped game is gone, and the new game starts only once both have loaded and the picture memory is empty |
| `redraw` | Teensy | Change the whole screen in every refresh, once without a video and once beside a full-screen video, while the main loop measures its passes | No pass [waits](components/screen.md#drawing) for the driver, and the longest pass is the time LVGL renders |
| `restart` | computer | Call `restart()`, change an object while the fake display restarts, then let the display come back | The display driver's `restart()` is called once, no rectangle goes out while the display restarts, and the first refresh after it sends the whole screen once, with the change in it |
| `covered` | computer | Lay an object that fills the screen on a slide over an object that fills the background, then change the whole screen | LVGL draws the object on the slide and leaves out the one it covers. Once the slide is removed, the background's object is drawn again |
| `picture-covers` | computer | Show a full-screen picture without opacity on a slide over an object that fills the background, with an object in front of the picture, and change the whole screen. Then do the same with a picture that holds opacity | LVGL draws the picture and the object in front of it and leaves out the background's object, for an image created before the picture has loaded and for one created after. Behind the picture with opacity the background's object is drawn |
| `power-lost` | bench | Show `background.img`, then pull the 5 V lead from the module's Vin for about two seconds and plug it back in. On `DEVICE_RECOVERED` of the touch controller, call `restart()` as the host does | `restarting()` of the display is true from the call until the panel is back, the picture shows again, and the Teensy keeps running |

### Video player

The program `video-player` checks the [video player](components/video-player.md). Its computer tests run LVGL with a fake display driver, a fake card and the sounds on a fake audio driver. Its Teensy tests write `loop.rgb`, a full-screen movie of four seconds whose top left pixel holds the frame's number, with `loop.wav` as long as the movie, `viewport.rgb`, a smaller movie, and `sync.rgb`, which flashes white every second, with `sync.wav`, a click every second.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `play` | computer | Play a name with and without `sound` | The video player reads `video/<name>.rgb` frame by frame into its memory, one second ahead, and with `sound` the sounds get `audio/<name>.wav` as the video's track once the first frames have landed |
| `movie-ends` | computer | Let the video run out while its sound still plays | The sound ends, the music goes on, and `MOVIE_ENDED` with the key of the mode that holds the video's slide reaches the game from the video player's next `update()` |
| `stop` | computer | Stop a video and its sound | Both stop, the music goes on, and the memory of the frame is free again |
| `no-room` | computer | Play a full-screen video while the picture memory is full, then a video in a viewport beyond the screen, both while another video plays | `play()` returns false both times, the sounds get no call, and the video playing goes on |
| `owner` | computer | Play a video on the slide that shows, then one in a viewport on a hidden slide and one on no slide, then a second one on the slide that shows | `play()` returns false for the hidden slide and for no slide while the first video goes on, and the second video on the slide that shows replaces the first |
| `hidden` | computer | Show a slide of higher priority over a playing video's slide, then one of equal priority, then remove a playing video's slide, and lay an object over the whole viewport of a playing video | The video and its sound stop each of the first three times, the music goes on, and no `MOVIE_ENDED` comes, while the video under the object plays on |
| `late-card` | computer | Hold a frame's read back past its 40 ms, in a video without sound | The frame on the screen stays, the frame due then follows, and the video keeps pace with its start |
| `short-sound` | computer | Play a video whose sound ends before its last frame, and one whose sound file is missing | Each video plays on by the clock from the frame it stands at, the screen keeps refreshing, and each ends with `MOVIE_ENDED` |
| `sound-clock` | computer | Hold the sound back for a while as a video with sound plays, then let it go on | The frame on the screen stays while the sound waits, and the video then goes on with the frame that belongs to the sound |
| `covers` | computer | Play a video that fills the screen over an object that fills the background, then stop the video | LVGL draws the object in none of the video's frames and draws it again once the video has stopped |
| `frames` | Teensy | Write a file of several megabytes, then play `loop.rgb` with its sound in a loop for ten minutes. Meanwhile play a sound on each of the other audio channels, write to the card every second and run the game's load: a dark light frame at the lamps' frame period on chain F, where no LED is wired, and the touch driver's reads | From the second frame on, every frame follows the one before within its 40 ms, none is skipped, and each is the frame that belongs to how far its sound has played. No channel's sound misses a sample for want of data, and the loops of the sound leave no gap |
| `viewport` | bench | Play `viewport.rgb` in a viewport on a slide over a full-screen picture on the same slide, with a caption created after the viewport | The video fills only its viewport, the picture shows around it, and the caption lies in front of the video |
| `missing-movie` | Teensy | Play a video the card does not hold | It ends at once with `MOVIE_ENDED`, and nothing else changes |
| `sync` | bench | Play `sync` while music runs | Each flash comes with its click, and the music goes on where it stopped once the movie ends |

## Main program

### Logger

The program `logger` checks the [logger](logging.md).

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `format` | computer | Start an empty log, then write lines from two writers at two times | The head line comes first, then the banner with the machine's name, the firmware's version and the kind of build, debug or release. Each line starts with the milliseconds since the start, then the level's letter and the writer's name |
| `buffer-full` | computer | Write more lines than the buffer holds before the next write, then one line after it | Each line that does not fit is lost whole and counted in `lost()`. The others stay intact. A line states the count ahead of the next line |
| `one-write` | computer | Write lines at a high rate for several seconds into a fake card | The lines reach the card in one write a [second](logging.md#buffer) |
| `failed-write` | computer | Let one write fail, then write a line | The failed write's lines reach the card with the next write, at the place the failed one would have taken |
| `head` | computer | Start with both files full and numbered, then with a file that has no head line | The log goes on in the file with the higher number. Without a head line it goes on in the first file |
| `turn` | computer | Write a line that fits into a nearly full file, then one that does not | The second line starts the other file anew, behind a head line one number higher. The full file stays within its limit |
| `no-card` | computer | Write two lines without `begin()`, as the firmware does without a card | Both lines are lost and counted |
| `interrupt` | computer | Write a line while posing as an interrupt | The line is lost and counted. The lock is never taken |
| `free-lines` | computer | Begin a log, then write a line through `logInfo()` and one through `logWarning()` | Both lines stand in that log with their level and writer |
| `two-tasks` | Teensy | Write numbered lines from a task above the test task's priority in bursts while the test task writes its own and runs `update()`, into files in RAM | Every line stands whole in the file, each writer's lines in their order. None is lost |
| `order` | Teensy | Write numbered lines, restart twice, then read the log back | The lines stand in the order they were written. The lines of each restart follow those before |
| `files` | Teensy | Put a log file at the [size limit](logging.md#files) on the card, then start and write | The other file starts anew with a higher number. The full file stays as it was |
| `card-missing` | bench | Start without the card and write | The log does not begin. `write()` returns false at once. `lost()` counts the line |

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
| `heap` | Teensy | Take a block with `malloc()` in the constructor of a static object, and a second block in the main task | Both blocks lie in RAM2, and they do not overlap |

### Game host

The program `game-host` checks the [host](game-abstraction.md#host) with the test games.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `lifecycle` | computer | Run a test game whose `done()` turns true in each phase | The host calls `start()`, `play()`, `end()` and `stop()` in the order of the [lifecycle](game-abstraction.md#game-lifecycle), and starts the game anew after its post-game phase |
| `stopped` | computer | Dispatch events and update while no game runs | No game gets a call |
| `mode-end` | computer | Start two test modes whose `stop()` is empty, each lighting a lamp, taking a slide with a video and playing a track, then let one end itself and stop the other | Each time, the lamp shows the effect below, the slide below shows, the video stops and the track below goes on |
| `dispatch` | computer | Dispatch driver events to fake components, some of which take them and make several pinball events of one | Each driver event goes to the components in order until one takes it, its pinball events reach the game in order, and an event no component takes reaches the game only when a service sent it |
| `faults` | computer | Dispatch `DEVICE_FAILED` and `DEVICE_RECOVERED` | Neither reaches a component or the game, the host's list marks the part and clears it again, and a recovered touch controller calls the screen's `restart()` |
| `arming` | computer | Run a test game through its phases | The solenoid contacts are armed exactly while the game is in Play |
| `launch` | computer | Launch game b while a picture of game a still loads | Game a stops, and b starts only once the screen has settled |
| `choice` | Teensy | Start with a card holding two games, then start without a card | With the card, the game choice shows the built-in game's card first. Without it, the warning shows and the built-in game starts |
| `saved-data` | Teensy | Let a test game write a file in `end()`, restart, and let it read the file in `start()` | The game reads what it wrote |
| `boot-film` | bench | Start with `/video/intro.rgb` and `/audio/intro.wav` at the card's root, then start once more without the two files | The film plays once on the whole screen with its sound before the host's first screen, and without the files that screen shows at once |
| `switch` | bench | Start a game, then switch to the other game while its background loads | Each background shows whole, and no picture of the stopped game stays on the screen |
| `menu` | bench | Open the menu during a game that keeps drawing, then close it | The menu opens over the game, and the game overlay layer reappears with every change drawn meanwhile |
| `menu-switch` | bench | Open the menu during a game, pick the other game and confirm | The picked game starts |
| `setting` | bench | Change a setting in the menu, change it again at once, then restart | No save starts while one is written, and after the restart the value saved last stands |
| `fault-list` | bench | Disconnect the rotary sensor during a game, then connect it again | The menu lists the sensor as failed and later as working, and the game goes on throughout |
| `card-pulled` | bench | Pull the card during a game that plays music and a movie | The game goes on, the music and the movie stop, and the menu lists the card as failed until the same card is back |
| `ball-carries-on` | machine | Switch games while a ball is on the playfield | The ball's events reach the new game |

## Whole machine

The program `whole-machine` runs the firmware with a game on the assembled machine.

| Test | Runs on | Steps | Passes when |
|---|---|---|---|
| `no-drops` | machine | Play with every subsystem at its busiest, with balls on the sensors, the seal turning, a movie, a sound on every channel and an effect on every light | `dropped()` stays 0 for every queue |
| `stacks` | machine | Play for half an hour, then read how deep every stack has been used | The main task's, the storage task's and the interrupts' stacks each stay within their size |
| `soak` | machine | Play for an hour | Nothing restarts, no device is reported failed, no log line is lost, and no event is dropped |
