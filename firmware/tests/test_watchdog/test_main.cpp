// Tests on the computer whether the watchdog's guard decides each feed from the coils it watches,
// and whether it writes and voids the note of a restart's cause as it should. A test's coils stand
// in for the solenoid driver and report the overdue coils the test sets.
//
//   no-coils        a guard that watches no coils lets the watchdog be fed and notes nothing
//   guards-hold     with no coil overdue, the watchdog is fed and nothing noted
//   coil-overdue    an overdue coil stops the feeds and notes COIL with that coil once
//   first-note      a hang noted while a coil's note waits leaves the coil's note
//   fed-after-note  once no coil is overdue, the watchdog is fed and the note voided once
//   note-seal       a sealed note reads back, and a broken one reads as NONE
//
// Run: pio test -e computer, which runs in CI as well.

#include "core/watchdog-guard.h"
#include <unity.h>

constexpr uint8_t COIL = 2;                // the coil the tests keep on
constexpr uint8_t OTHER_COIL = 0;          // coil-overdue: a second coil that goes overdue later
constexpr uint32_t HANG_PC = 0x6000a5c0;   // an address in the flash, where code runs
constexpr uint32_t DETAIL = 0x12345678;    // note-seal: a detail with every nibble different
constexpr uint8_t NOTES = 8;

// Coils that report the overdue coils a test sets.
class TestCoils : public OverdueCoils {
public:
    uint8_t overdue() const override { return coils; }

    uint8_t coils = 0;   // the overdue coils, one bit each
};

static TestCoils coils;

// The notes a guard wrote, oldest first.
struct Written {
    RestartCause cause;
    uint32_t detail;
};
static Written written[NOTES];
static uint8_t writes = 0;

static void writeNote(RestartCause cause, uint32_t detail) {
    TEST_ASSERT_TRUE(writes < NOTES);
    written[writes] = {cause, detail};
    writes = writes + 1;
}

// Gets a guard that watches the test's coils.
static WatchdogGuard watching() {
    WatchdogGuard guard(writeNote);
    guard.watch(coils);
    return guard;
}

void setUp() {
    writes = 0;
    coils.coils = 0;
}

void tearDown() {}

static void testNoCoils() {
    WatchdogGuard guard(writeNote);
    coils.coils = 1u << COIL;
    TEST_ASSERT_TRUE(guard.check());
    TEST_ASSERT_TRUE(guard.check());
    TEST_ASSERT_EQUAL_UINT8(0, writes);
}

static void testGuardsHold() {
    WatchdogGuard guard = watching();
    TEST_ASSERT_TRUE(guard.check());
    TEST_ASSERT_TRUE(guard.check());
    TEST_ASSERT_EQUAL_UINT8(0, writes);
}

static void testCoilOverdue() {
    WatchdogGuard guard = watching();
    coils.coils = 1u << COIL;
    TEST_ASSERT_FALSE(guard.check());
    TEST_ASSERT_EQUAL_UINT8(1, writes);
    TEST_ASSERT_EQUAL_UINT32(uint32_t(RestartCause::COIL), uint32_t(written[0].cause));
    TEST_ASSERT_EQUAL_HEX32(1u << COIL, written[0].detail);
    // The coil stays on, and another one goes overdue: no feed, and the note stands as written.
    coils.coils |= 1u << OTHER_COIL;
    TEST_ASSERT_FALSE(guard.check());
    TEST_ASSERT_FALSE(guard.check());
    TEST_ASSERT_EQUAL_UINT8(1, writes);
}

static void testFirstNote() {
    WatchdogGuard guard = watching();
    coils.coils = 1u << COIL;
    TEST_ASSERT_FALSE(guard.check());
    // The warning comes while the coil's note waits.
    guard.note(RestartCause::HANG, HANG_PC);
    TEST_ASSERT_EQUAL_UINT8(1, writes);
    TEST_ASSERT_EQUAL_UINT32(uint32_t(RestartCause::COIL), uint32_t(written[0].cause));
}

static void testFedAfterNote() {
    WatchdogGuard guard = watching();
    coils.coils = 1u << COIL;
    TEST_ASSERT_FALSE(guard.check());
    coils.coils = 0;
    TEST_ASSERT_TRUE(guard.check());
    TEST_ASSERT_EQUAL_UINT8(2, writes);
    TEST_ASSERT_EQUAL_UINT32(uint32_t(RestartCause::NONE), uint32_t(written[1].cause));
    TEST_ASSERT_EQUAL_HEX32(0, written[1].detail);
    TEST_ASSERT_TRUE(guard.check());
    TEST_ASSERT_EQUAL_UINT8(2, writes);
    // A later failure is noted afresh.
    guard.note(RestartCause::HANG, HANG_PC);
    TEST_ASSERT_EQUAL_UINT8(3, writes);
    TEST_ASSERT_EQUAL_UINT32(uint32_t(RestartCause::HANG), uint32_t(written[2].cause));
    TEST_ASSERT_EQUAL_HEX32(HANG_PC, written[2].detail);
}

static void testNoteSeal() {
    RestartNote note{};
    // Memory as power-up may leave it.
    TEST_ASSERT_EQUAL_UINT32(uint32_t(RestartCause::NONE), uint32_t(readNote(note)));
    for (const RestartCause cause : {RestartCause::NONE, RestartCause::COIL, RestartCause::HANG}) {
        sealNote(note, cause, DETAIL);
        TEST_ASSERT_EQUAL_UINT32(uint32_t(cause), uint32_t(readNote(note)));
        TEST_ASSERT_EQUAL_HEX32(DETAIL, note.detail);
    }
    // One word changed after the seal breaks it.
    sealNote(note, RestartCause::COIL, DETAIL);
    note.detail = DETAIL ^ 1u;
    TEST_ASSERT_EQUAL_UINT32(uint32_t(RestartCause::NONE), uint32_t(readNote(note)));
    sealNote(note, RestartCause::COIL, DETAIL);
    note.cause = RestartCause::HANG;
    TEST_ASSERT_EQUAL_UINT32(uint32_t(RestartCause::NONE), uint32_t(readNote(note)));
    sealNote(note, RestartCause::HANG, DETAIL);
    note.magic = note.magic ^ 1u;
    TEST_ASSERT_EQUAL_UINT32(uint32_t(RestartCause::NONE), uint32_t(readNote(note)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testNoCoils);
    RUN_TEST(testGuardsHold);
    RUN_TEST(testCoilOverdue);
    RUN_TEST(testFirstNote);
    RUN_TEST(testFedAfterNote);
    RUN_TEST(testNoteSeal);
    return UNITY_END();
}
