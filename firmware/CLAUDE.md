# Firmware coding guidelines

## Debug code

Debug code reaches only a debug build. The firmware that runs in the machine is a release build, and a release build is made only on purpose.

The firmware's `platformio.ini` has two environments:

| Environment | Builds | Defines |
|---|---|---|
| `debug` | the default under `default_envs`, with the debug code | `DEBUG` |
| `release` | the firmware for the machine | `NDEBUG`, which removes every `assert()` |

```ini
[platformio]
default_envs = debug

[env:debug]
build_src_flags = -D DEBUG

[env:release]
build_flags = ${teensy.build_flags} -D NDEBUG
```

`DEBUG` is set with `build_src_flags`, which reach only the firmware's own code, because freertos-teensy names a constant `DEBUG`. `NDEBUG` reaches the libraries as well.

A plain `python -m platformio run` or `-t upload` therefore builds the debug firmware. The release firmware needs `-e release`.

Code that only serves debugging stands under `#if defined(DEBUG)`. Debug output goes through `DEBUG_LOG()`, which the compiler removes without `DEBUG`, so no `Serial.print()` stays behind in the release firmware. The call stands behind `if (false)`, so a variable that only a log line reads raises no warning in the release build:

```cpp
#if defined(DEBUG) && defined(NDEBUG)
#error "DEBUG and NDEBUG are both defined"
#endif

#if defined(DEBUG)
#define DEBUG_LOG(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_LOG(...) do { if (false) { Serial.printf(__VA_ARGS__); } } while (0)
#endif
```

[`lib/driver-base/debug-log.h`](lib/driver-base/debug-log.h) holds the macro.

**Every refusal shows in a debug build.** A function that rejects its input or fails returns its error, and in a debug build it also names the refusal and its reason through `DEBUG_LOG()`. A silent `false` costs a search. `assert()` is kept for what can never happen, since a test that provokes a refusal has to run on.

CI builds and tests the release code: every project in CI builds with `-e release`.

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
- A comment stands on its own line above the code it explains, as full sentences. A comment at the end of a line is kept to a few words, as for a constant's source.
- [`.clang-format`](.clang-format) holds this layout, and CI checks every file of `src/` and `lib/` against it with clang-format 18.

## Run time and memory

- **No dynamic memory after the start.** No `new`, no `malloc()` and no container that grows, in the main loop or in an interrupt. Every buffer and every object is allocated statically or during the start.
- **An interrupt is short and never waits.** It calls no `Serial`, no `delay()` and allocates nothing. It calls FreeRTOS only through the `FromISR` functions, and only at a priority of 208 or higher, as [`general-design.md`](../docs/firmware/general-design.md#interrupts) sets. A variable shared with an interrupt is `volatile` or atomic.
- **The main loop never waits.** No `delay()` and no busy loop. A time comes from `nowUs()` and is compared with `before()` and the `elapsed` functions, which survive the counter's wrap.
- **No recursion and no variable-length arrays**, so every stack's depth can be computed.
- **A buffer names its memory**, `DMAMEM` or `EXTMEM`. Where DMA reads or writes a buffer the cache serves, the cache over it is cleaned or invalidated around the transfer.

## Structure

- **Fixed-width integers** such as `uint32_t`, `constexpr` instead of `#define`, `enum class` instead of a plain `enum`.
- **A quantity carries its unit in its name**, such as `pullUs`, `TIMEOUT_MS` or `limitMa`.
- **No number without a name.** A constant names the design document, the model figure or the datasheet page it comes from.
- **A constant is a `constexpr`, never a `#define`.** `#define` is kept for build switches such as `DEBUG` and for macros such as `DEBUG_LOG()`. A `constexpr` has a type and stays inside its file or class.
- **Constants stand at the top.** A constant only one `.cpp` file uses stands right below its includes, where a `constexpr` stays inside that file. A constant of a class's interface stands at the top of the class in its header.
- **No pin number is typed into the code.** The pins come from one header generated from [`docs/pin-assignment.md`](../docs/pin-assignment.md), so the code and the allocation cannot disagree.
- **Only a driver touches its hardware.** Registers and pins belong to the driver that owns them, and everything above it goes through the driver's interface.
- **No exceptions and no RTTI**, built with `-fno-exceptions` and `-fno-rtti`. A failure is a return value, and every return value of a call that reaches hardware is checked.
- **`src/` takes nothing from `gist/`.** A gist is a note to read. Code in `src/` neither includes a gist nor copies one, and is written for the firmware itself.
- **A gist is deleted once its driver stands as code.** When a driver runs as code in `firmware/` and its gist holds nothing that the code and the documents lack, the gist goes, and every reference to it points to the code.
- **A library is used through its API.** Code never reaches past a library into its internals, whether registers, private members or raw sectors.
- **A comment says why.** A register value names its datasheet section.

## Tools

- **Warnings are errors for the firmware's own code**, with `-Wall -Wextra -Werror`. The libraries and the Teensy core compile with `-w`, which [`tools/quiet-libraries.py`](tools/quiet-libraries.py) adds, so a build reports only the firmware's own warnings.
- **Static analysis** with clang-tidy runs in CI.
- **Tests that need no hardware run on the computer.** The `computer` tests of [`testing.md`](../docs/firmware/testing.md) build in a `native` environment and run in CI. Tests that need the Teensy run on the bench.
- **Every dependency is pinned** to an exact version or commit.
