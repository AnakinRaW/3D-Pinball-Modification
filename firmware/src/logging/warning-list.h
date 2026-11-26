// The warnings the machine wrote since its start, kept in memory for the screen. Every line of
// logWarning() enters the list, also before the log has begun and without a card. Equal warnings
// share one entry and count how often they came. The screen sees a change through version().
//
// Design: docs/firmware/logging.md
#pragma once
#include <atomic>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/// One warning of the list.
struct WarningEntry {
    static constexpr size_t TEXT_BYTES = 80;   // logging.md: the text a warning keeps

    const char* who = nullptr;    // the writer's name, as logWarning() got it
    char text[TEXT_BYTES] = {};   // the warning's text, cut to TEXT_BYTES - 1 characters
    uint16_t count = 0;           // how often it came, up to 65535
    uint32_t firstMs = 0;         // millis() when it came first
};

/// The first warnings since the start, in the order they came.
class WarningList {
public:
    static constexpr uint8_t ENTRIES = 16;   // logging.md: the warnings the list keeps

    /// Takes a warning into the list.
    ///
    /// A warning with the writer and the text of an entry raises that entry's count. Any other
    /// takes the next free entry. A warning that finds every entry taken is counted in dropped().
    ///
    /// @param who     Names the writer in a word. The list keeps the pointer, so it points to text
    ///                that lives as long as the firmware, such as a literal.
    /// @param ms      The time the warning came, from millis().
    /// @param format  The text's format, as for vprintf.
    /// @param args    The values the format takes.
    /// @note          Never waits. Any task, never an interrupt: a warning from an interrupt stays
    ///                out of the list.
    void add(const char* who, uint32_t ms, const char* format, va_list args)
        __attribute__((format(printf, 4, 0)));

    /// Gets a number that rises with every change of the list, a new entry or a higher count.
    uint32_t version() const { return changes.load(std::memory_order_acquire); }

    /// Gets how many entries the list holds.
    uint8_t count() const { return taken.load(std::memory_order_acquire); }

    /// Copies an entry.
    ///
    /// @param i    The entry's place, 0 for the warning that came first.
    /// @param out  Receives the entry.
    /// @return     False when @p i lies past the entries the list holds.
    /// @note       Any task, never an interrupt.
    bool read(uint8_t i, WarningEntry& out) const;

    /// Gets how many warnings found every entry taken.
    uint32_t dropped() const { return droppedTotal.load(std::memory_order_relaxed); }

private:
    WarningEntry entries[ENTRIES];
    std::atomic<uint8_t> taken{0};
    std::atomic<uint32_t> changes{0};
    std::atomic<uint32_t> droppedTotal{0};
};

/// The firmware's warnings, which logWarning() fills.
extern WarningList warningList;
