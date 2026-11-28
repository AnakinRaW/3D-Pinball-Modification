// Restarts the Teensy when the firmware hangs or keeps a coil on too long. The i.MX RT's WDOG1
// counts down 2 s and restarts the Teensy unless it is fed, and it keeps counting when every line
// of code has stopped. The main loop feeds it once per pass while every guard holds, and FreeRTOS's
// idle task feeds it under the same guards until the main loop runs.
//
// Before the restart, the watchdog notes why it went unfed. The next start reads the note, and
// report() writes the cause and PJRC's CrashReport into the log.
//
// Design: docs/firmware/error-handling.md, Watchdog.
#pragma once
#include <Arduino.h>
#include "core/watchdog-guard.h"

extern "C" void vApplicationIdleHook();
extern "C" void watchdogWarned(const uint32_t* frame);

class Watchdog {
public:
    static constexpr uint32_t TIMEOUT_S = 2;   // model/firmware/figures.py, t_wdt

    /// Creates a watchdog that guards no coils yet, only against a hang.
    Watchdog();

    /// Lets @p coils decide each feed from here on, besides the hang.
    ///
    /// @param coils  What drives the coils, such as the solenoid driver.
    /// @note         The main task calls it before begin().
    void watch(const OverdueCoils& coils) { guard.watch(coils); }

    /// Reads the note of the last restart, starts WDOG1 with its warning and lets FreeRTOS's
    /// idle task feed it.
    ///
    /// From here on the Teensy restarts TIMEOUT_S after the last feed. WDOG1 cannot be stopped
    /// again. A second call does nothing.
    ///
    /// @note  The main task calls it once, after the storage driver's mount and before the
    ///        solenoids' begin().
    void begin();

    /// Ends the feeds from FreeRTOS's idle task, so the main loop alone feeds the watchdog.
    ///
    /// @note  The main task calls it once, right before the main loop.
    void loopStarts();

    /// Feeds the watchdog where every guard holds, and notes the cause where one fails.
    ///
    /// Does nothing before begin(). It blocks every interrupt for the few instructions the guards
    /// and the feed take.
    ///
    /// @note  The main loop calls it at the end of every pass.
    void check();

    /// Writes the cause of the last restart and PJRC's CrashReport into the log.
    ///
    /// Writes nothing after a power-up. CrashReport is empty once it has been written.
    ///
    /// @note  The main task calls it once, right after the logger's begin().
    void report();

    /// Gets why the watchdog restarted the Teensy before this start, as begin() read it.
    ///
    /// @return  NONE where the last restart was no restart by the watchdog, and before begin().
    RestartCause lastCause() const { return previousCause; }

    /// Gets the detail of lastCause(): the coils, one bit each, or the address of the code that
    /// hung.
    uint32_t lastDetail() const { return previousDetail; }

private:
    friend void ::vApplicationIdleHook();
    friend void ::watchdogWarned(const uint32_t* frame);

    /// Keeps a note in CrashReport's breadcrumbs and in the note that the next begin() reads.
    static void write(RestartCause cause, uint32_t detail);

    WatchdogGuard guard;
    bool begun = false;
    volatile bool idleFeeds = false;   // FreeRTOS's idle task feeds the watchdog
    bool starving = false;             // the last check() fed nothing
    bool unbegunNoted = false;         // a check() before begin() has been logged
    RestartCause previousCause = RestartCause::NONE;
    uint32_t previousDetail = 0;
};
