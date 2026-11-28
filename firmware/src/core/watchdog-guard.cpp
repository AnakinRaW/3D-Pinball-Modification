#include "watchdog-guard.h"

constexpr uint32_t NOTE_MAGIC = 0x57444F47;   // "WDOG", a note sealNote() wrote

/// Gets the word that ties a note's other words together.
static uint32_t checkOf(const RestartNote& note) {
    return ~(note.magic ^ uint32_t(note.cause) ^ note.detail);
}

void sealNote(RestartNote& note, RestartCause cause, uint32_t detail) {
    note.magic = NOTE_MAGIC;
    note.cause = cause;
    note.detail = detail;
    note.check = checkOf(note);
}

RestartCause readNote(const RestartNote& note) {
    if (note.magic != NOTE_MAGIC || note.check != checkOf(note)) {
        return RestartCause::NONE;
    }
    return note.cause;
}

bool WatchdogGuard::check() {
    const uint8_t overdue = coils != nullptr ? coils->overdue() : 0;
    if (overdue != 0) {
        note(RestartCause::COIL, overdue);
        return false;
    }
    if (noted) {
        // Fed after all, so the restart the note waited for does not come.
        noted = false;
        write(RestartCause::NONE, 0);
    }
    return true;
}

void WatchdogGuard::note(RestartCause cause, uint32_t detail) {
    if (noted) {
        return;
    }
    noted = true;
    write(cause, detail);
}
