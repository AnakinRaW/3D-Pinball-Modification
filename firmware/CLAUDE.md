# Firmware coding guidelines

## Run time and memory

- **No dynamic memory after the start.** No `new`, no `malloc()` and no container that grows, in the main loop or in an interrupt. Every buffer and every object is allocated statically or during the start.
- **An interrupt is short and never waits.** It calls no `Serial`, no `delay()` and allocates nothing. It calls FreeRTOS only through the `FromISR` functions, and only at a priority of 208 or higher, as [`general-design.md`](../docs/firmware/general-design.md#interrupts) sets. A variable shared with an interrupt is `volatile` or atomic.
- **The main loop never waits.** No `delay()` and no busy loop. A time comes from `nowUs()` and is compared with `before()` and the `elapsed` functions, which survive the counter's wrap.
- **A time in milliseconds that only counts on, such as the time in front of a log line, comes from `millis()`.** It counts the milliseconds since the start and wraps after 49.7 days, so a span taken as the difference of two readings stays right across the wrap.
- **No recursion and no variable-length arrays**, so every stack's depth can be computed.
- **A buffer names its memory**, `DMAMEM` or `EXTMEM`. Where DMA reads or writes a buffer the cache serves, the cache over it is cleaned or invalidated around the transfer.

## Tests

- **Code lands with its tests.** A new library gets its test program, and a changed behavior gets its row in [`testing.md`](../docs/firmware/testing.md) and its test, in the same step as the code. Before code is reported done, each public function it touched is named with the test that covers it. Coverage through another component's tests does not count.
- **Tests that need no hardware run on the computer**, in the `computer` environment, which PlatformIO builds with its platform `native`. CI runs them. Tests that need the Teensy run on the bench from the test image.
- **A new driver joins the register of `every-driver`** in `tests/teensy/src/every-driver/`, with a case for each row its rules reach.
- **Every test in the code has its row in testing.md.** [`tools/test-coverage.py`](tools/test-coverage.py) fails CI otherwise and counts the rows still without a test.

## Layout

All of the firmware's code stands in `src/`, in six folders that follow `docs/firmware/`:

| Folder | Holds |
|---|---|
| `core/` | what every part uses: the clock, the event queue, the device monitor, the priority list, the task lock, the firmware's version |
| `logging/` | the machine's log on the card, `DEBUG_LOG()` and `DEBUG_TRACE()` |
| `io/` | what reaches a device or a file for the parts above it, such as the card appender |
| `drivers/` | one driver per pair of files, the driver tick in `driver-base`, and the code a driver alone uses |
| `components/` | the components between the drivers and the game |
| `game/` | the game: modes and their layers |

Code taken from another project and changed here stands beside the code that uses it, as PWMServoT4 in `drivers/`. Libraries used unchanged come from PlatformIO's registry or Teensyduino, pinned in `platformio.ini`.

- **The firmware's headers are included in quotes, by their path from `src/`**, as `#include "drivers/lighting.h"`. A `.cpp` file includes the header of the same name by name. A library's header and a system header stand in angle brackets.
- **Every build reaches `src/` by its path.** The firmware builds it as its source. The computer tests build it with `test_build_src`, and `build_src_filter` in `[env:computer]` names the files that build without the Teensy, so a new file that does is added there. The test image and the bench programs take it as the library `firmware` through `firmware=symlink://${PROJECT_DIR}/../../src`. [`tools/firmware-src.py`](tools/firmware-src.py) gives that library every file but `main.cpp`, and a bench program names the files it needs in `custom_firmware_src`.
- **A fake for the computer tests stands at the path of the header it replaces**, below `tests/fakes/`, such as `tests/fakes/drivers/lighting.h`. The computer build looks up quoted headers in `tests/fakes` first.

## Structure

- **Fixed-width integers** such as `uint32_t`, `constexpr` instead of `#define`, `enum class` instead of a plain `enum`.
- **No number without a name.** A constant names the design document, the model figure or the datasheet page it comes from.
- **A constant is a `constexpr`, never a `#define`.** `#define` is kept for build switches such as `DEBUG` and for macros such as `DEBUG_LOG()`. A `constexpr` has a type and stays inside its file or class.
- **Constants stand at the top.** A constant only one `.cpp` file uses stands right below its includes. A constant of a class's interface stands at the top of the class in its header.
- **No pin number is typed into the code.** The pins come from one header generated from [`docs/pin-assignment.md`](../docs/pin-assignment.md), so the code and the allocation cannot disagree.
- **Only a driver touches its hardware.** Registers and pins belong to the driver that owns them, and everything above it goes through the driver's interface.
- **A library is used through its API.** Code never reaches past a library into its internals, whether registers, private members or raw sectors. A problem is solved with the library's settings and its correct use first.
- **No exceptions and no RTTI**, built with `-fno-exceptions` and `-fno-rtti`. A failure is a return value, and every return value of a call that reaches hardware is checked.
- **The firmware's version stands in [`src/core/version.h`](src/core/version.h)** and is raised by hand with every release build that goes into the machine. The log writes it at every start.
- **`src/` takes nothing from `gist/`.** A gist is a note to read. Code in `src/` neither includes a gist nor copies one.
- **A gist is deleted once its driver stands as code** and holds nothing that the code and the documents lack. Every reference to it then points to the code.

## Comments and names

- **A public member's comment states its contract.** The contract is what a caller sees: what the member does, what it needs and what it returns. What happens inside is written in the `.cpp`, at the code that does it. "Attaches the pin with the SER0049's range, sets no pulse" on `ServoDriver::begin()` describes the inside and goes into `servo-driver.cpp`.
- **Inside a function, a comment says why.** A register value names its datasheet section.
- **A function's comment opens with a plain full sentence saying what the function does.** A line for each parameter follows. Where the meaning of a value is not obvious, an example closes the comment, such as "Pair 0b10 gives the symbol of a 1, then the symbol of a 0."

  ```cpp
  // Gets a color dimmed by a brightness.
  // color is the color at full brightness.
  // brightness runs from 0 to 255. 0 gives black. 255 gives the color unchanged.
  CRGB dim(CRGB color, uint8_t brightness);
  ```

- **A comment is understood from the member's name and the comment alone.** It presumes no library. "As FastLED's scale8() scales it" sends the reader to FastLED, so the comment states the effect in plain words.
- **The contract names what a caller has to observe.** It names the precondition, such as a call after `begin()`. It says what an input outside its range does and what `false` or a failure value means. It names the task or the interrupt that may call the member, and whether the member waits.
- **A library explains only the job it does.** Why a caller needs something is written at the call site in the caller. Why the LED output's DMA needs interleaved words is written in the lighting driver, where it calls the encoder. The encoder describes only the words it computes.
- **An override documents only what differs from the contract in `Driver`.** `Driver` states once what `begin()` and `failed()` promise. An override's comment adds what this driver does beyond that, such as the cases in which its `begin()` returns `false`.
- **Unit and range live in the name and in a constant.** A quantity carries its unit in its name, such as `pullUs`, `TIMEOUT_MS` or `limitMa`. Its range stands in constants such as `SPEED_MIN_DEG_S` and `SPEED_MAX_DEG_S`. A setting's comment then names only its default and its config key, such as "servo.speed_deg_s's default".
- **A member's comment points into no file under `docs/`.** A source stands only beside a number, at the end of its line, as in `// pin-assignment.md: LED chain A`.
- **Production code holds no remark about tests.** A comment in `src/` names no test that runs the code and no test that needs it, such as "so the computer tests run it".
- **A name says what the code does and to what.** This holds for a library, a file, a type and a member. `ws2812-color-encoder` names the job and its object. `ws2812-symbols`, `encoding` and `pulses` name a by-product or nothing.
- **A file's opening comment and a `library.json` description say in plain words what the library does for its user.** The first sentence says what the code is and points to no document. A closing line such as "Design: docs/firmware/drivers/servo.md" may name the design document. A mechanism appears only where it explains a benefit to the user: "FlexIO2 times every bit in hardware, so the LEDs show the right colors however busy the processor is."
- **"Its own" and "on its own" are never written**, in a comment, a name or a description. The noun alone says it.

## Code style

The code follows the style of PJRC's Teensy core and of Arduino:

| What | Written as | Example |
|---|---|---|
| Class, struct, enum type | PascalCase | `SolenoidDriver` |
| Function, method, variable, parameter, member | camelCase, members without a prefix | `fire()`, `lastPullUs` |
| Constant, `constexpr`, enumerator, macro | UPPER_CASE | `PULL_US`, `FileStatus::PENDING` |
| File | lowercase with hyphens | `solenoid-driver.cpp` |

```cpp
class SolenoidDriver {
public:
    // Fires the coil once. Returns false while the coil cools down.
    bool fire(uint8_t coil);

private:
    static constexpr uint32_t PULL_US = 30000;   // the solenoid design's pull time
    uint32_t lastPullUs = 0;
};
```

- Four spaces, no tabs. Lines end at 100 characters.
- Braces open on the line of the statement. `if`, `for` and `while` always take braces.
- `public:` and `private:` stand at the class's indentation.
- A comment stands with `//` on a separate line above the code it explains. It is written in full, plainly built sentences, as [rule 10](../RULES.md#10-writing-style) asks. Only a comment at the end of a line may be a fragment of a few words, as for a constant's source. Doxygen tags such as `@param` or `\brief` are not used.
- [`.clang-format`](.clang-format) holds this layout, and CI checks every file of `src/`, `include/` and `tests/` against it with clang-format 18.

## Debug code

Debug code reaches only a debug build. The firmware that runs in the machine is a release build, and a release build is made only on purpose.

| Environment of `platformio.ini` | Builds | Defines |
|---|---|---|
| `debug` | the default under `default_envs`, with the debug code | `DEBUG`, through `build_src_flags` |
| `release` | the firmware for the machine, which every project in CI builds | `NDEBUG`, which removes every `assert()` |

`DEBUG` reaches the firmware's code and none of the libraries, because freertos-teensy names a constant `DEBUG`. `NDEBUG` reaches the libraries as well. A plain `python -m platformio run` or `-t upload` therefore builds the debug firmware, and the release firmware needs `-e release`.

Code that only serves debugging stands under `#if defined(DEBUG)`. Debug output goes through `DEBUG_LOG()` from [`src/logging/debug-log.h`](src/logging/debug-log.h), which the compiler removes without `DEBUG`, so no `Serial.print()` stays behind in the release firmware. The call stands behind `if (false)`, so a variable that only a log line reads raises no warning in the release build.

**No code in `src/` prints to `Serial` directly.** `Serial.printf()` waits for USB once its buffers are full. Two tasks printing at once corrupt its state, since the Teensy core's `usb_serial_write()` takes no lock. `DEBUG_LOG()` puts the line into a buffer. The main loop's `sendDebugLog()` hands USB only what it takes without waiting. Any task may call `DEBUG_LOG()`. In an interrupt the line is dropped and counted, so an interrupt notes an event with `DEBUG_TRACE()` from [`src/logging/debug-trace.h`](src/logging/debug-trace.h), which the main loop turns into a debug line.

**A log line formats no floating-point value.** On the Teensy 4.1 the C library took 200 bytes from the heap for the first two floats a task formatted with `%f`, so a float in a line breaks the rule against dynamic memory after the start. A value is written as an integer in a unit that keeps its digits, such as `%ld` mV, or as the text it was read from, such as a setting from the config file.

The machine's log on the card goes through the [logger](../docs/firmware/logging.md). A line worth keeping in the machine goes there, such as a setting out of its range. A debug build shows it on USB as well.

**Every refusal shows in a debug build.** A function that rejects its input or fails returns its error, and in a debug build it also names the refusal and its reason through `DEBUG_LOG()`. A silent `false` costs a search. `assert()` is kept for what can never happen, since a test that provokes a refusal has to run on.

## Tools

- **Warnings are errors for the firmware's code**, with `-Wall -Wextra -Werror` in `build_src_flags`. [`tools/quiet-libraries.py`](tools/quiet-libraries.py) hands those flags to `src/` where a project takes it as a library and to the tests as well, and compiles the other libraries and the Teensy core with `-w`, so a build reports only the warnings in the firmware's code.
- **clang-tidy checks the code in CI** with the checks [`.clang-tidy`](.clang-tidy) names, and any finding in `src/` or `include/` fails the run. [`tools/tidy.py`](tools/tidy.py) runs it on the release build, `--fix` applies the fixes it offers. A finding that is wrong for this code is silenced at its line with `// NOLINT(<check>)` and the reason.
- **Every dependency is pinned** to an exact version or commit.
