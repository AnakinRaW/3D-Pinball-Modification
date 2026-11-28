// Decides whether the watchdog may be fed, and keeps the note of why it went unfed. The watchdog
// asks the guard before every feed, and its warning interrupt notes a hang through it. A note goes
// to the writer the watchdog names, which keeps it across the restart, so the next start can tell
// why the Teensy restarted.
//
// Design: docs/firmware/error-handling.md, Watchdog.
#pragma once
#include <Arduino.h>

/// What drives coils, such as the solenoid driver, shows the watchdog's guard through this.
class OverdueCoils {
public:
    /// Gets the coils that have been on for too long, one bit each.
    ///
    /// @return  0 while every coil is off or inside its pull, which lets the watchdog be fed.
    /// @note    The guard calls it with every interrupt blocked, so it only reads.
    virtual uint8_t overdue() const = 0;

protected:
    ~OverdueCoils() = default;
};

/// Why the watchdog went unfed.
enum class RestartCause : uint32_t {
    NONE = 0,   ///< no note, or the watchdog was fed after all
    COIL = 1,   ///< a coil stayed on too long. The detail holds the coils, one bit each
    HANG = 2,   ///< the firmware hung. The detail holds the address of the interrupted code
};

/// A note of the restart's cause, kept in memory that a restart leaves as it is.
struct RestartNote {
    uint32_t magic;       // NOTE_MAGIC once written
    RestartCause cause;   // why the watchdog went unfed
    uint32_t detail;      // the cause's detail
    uint32_t check;       // ties the three words above together
};

/// Writes a note that readNote() takes.
///
/// @param note    The note to write.
/// @param cause   Why the watchdog went unfed.
/// @param detail  The cause's detail.
void sealNote(RestartNote& note, RestartCause cause, uint32_t detail);

/// Gets the cause a note holds.
///
/// @return  NONE for a note sealNote() did not write, such as what memory holds after power-up.
RestartCause readNote(const RestartNote& note);

class WatchdogGuard {
public:
    /// Keeps a note across the restart, as the watchdog's memory and CrashReport do.
    using Writer = void (*)(RestartCause cause, uint32_t detail);

    /// Creates a guard that notes through @p write and guards no coils yet.
    ///
    /// @param write  Takes every note the guard writes or voids.
    explicit WatchdogGuard(Writer write) : write(write) {}

    /// Lets @p coils decide each feed from here on.
    ///
    /// @param coils  What drives the coils, such as the solenoid driver.
    /// @note         The main task calls it before the watchdog's begin().
    void watch(const OverdueCoils& coils) { this->coils = &coils; }

    /// Gets whether every guard holds, so the watchdog may be fed.
    ///
    /// A coil on for too long fails the guard and notes COIL with the coils. A guard that holds
    /// again voids the note with NONE, since the restart did not follow.
    ///
    /// @return  False while a coil has been on for too long.
    /// @note    One caller at a time, with the warning interrupt blocked.
    bool check();

    /// Notes why the watchdog goes unfed, where no note waits.
    ///
    /// The first note stands until check() voids it.
    ///
    /// @param cause   Why the watchdog goes unfed.
    /// @param detail  The cause's detail.
    /// @note          The warning interrupt calls it, and check().
    void note(RestartCause cause, uint32_t detail);

private:
    const OverdueCoils* coils = nullptr;   // nullptr while no coils are guarded
    Writer write;
    volatile bool noted = false;   // a note waits for its restart
};
