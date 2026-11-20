// Tests on the computer whether the byte ring hands its records back whole and in order.
//
//   wrap              records that run past the end of the buffer come back in order, the piece
//                     up to the end first and the rest after it
//   whole-or-nothing  a record larger than the free space is refused and leaves the ring as it was
//   power-of-two      a buffer of 1000 bytes holds 512. A ring of 0 bytes refuses every record
//
// Run: pio test -e computer, which runs in CI as well.

#include "core/byte-ring.h"
#include <string>
#include <unity.h>

constexpr size_t BUFFER_BYTES = 16;

void setUp() {}

void tearDown() {}

// Takes every byte the ring holds piece by piece and counts the pieces.
static std::string drain(ByteRing& ring, int& pieces) {
    std::string out;
    pieces = 0;
    for (;;) {
        size_t n = 0;
        const uint8_t* bytes = ring.peek(n);
        if (n == 0) {
            return out;
        }
        out.append(reinterpret_cast<const char*>(bytes), n);
        ring.consume(n);
        pieces = pieces + 1;
    }
}

// wrap: records across the end of the buffer come back in order, in two pieces.
static void testWrap() {
    uint8_t buffer[BUFFER_BYTES];
    ByteRing ring(buffer, sizeof(buffer));
    int pieces = 0;
    TEST_ASSERT_TRUE(ring.append("0123456789", 10));
    TEST_ASSERT_EQUAL_STRING("0123456789", drain(ring, pieces).c_str());
    TEST_ASSERT_TRUE(ring.append("abcdef", 6));
    TEST_ASSERT_TRUE(ring.append("ghijkl", 6));
    TEST_ASSERT_EQUAL_size_t(12, ring.used());
    TEST_ASSERT_EQUAL_STRING("abcdefghijkl", drain(ring, pieces).c_str());
    TEST_ASSERT_EQUAL_INT(2, pieces);
    TEST_ASSERT_EQUAL_size_t(22, ring.appended());
    TEST_ASSERT_EQUAL_size_t(22, ring.consumed());
}

// whole-or-nothing: a record that does not fit is refused. The ring keeps what it held.
static void testWholeOrNothing() {
    uint8_t buffer[BUFFER_BYTES];
    ByteRing ring(buffer, sizeof(buffer));
    int pieces = 0;
    TEST_ASSERT_TRUE(ring.append("0123456789", 10));
    TEST_ASSERT_FALSE(ring.append("abcdefg", 7));
    TEST_ASSERT_EQUAL_size_t(10, ring.used());
    TEST_ASSERT_TRUE(ring.append("abcdef", 6));
    TEST_ASSERT_FALSE(ring.append("x", 1));
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef", drain(ring, pieces).c_str());
}

// power-of-two: the ring takes the largest power of two of its buffer, none of an empty one.
static void testPowerOfTwo() {
    uint8_t buffer[1000];
    ByteRing ring(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_size_t(512, ring.size());
    ByteRing none(buffer, 0);
    TEST_ASSERT_EQUAL_size_t(0, none.size());
    TEST_ASSERT_FALSE(none.append("x", 1));
    TEST_ASSERT_FALSE(none.append("", 0));
    ByteRing missing(nullptr, sizeof(buffer));
    TEST_ASSERT_FALSE(missing.append("x", 1));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testWrap);
    RUN_TEST(testWholeOrNothing);
    RUN_TEST(testPowerOfTwo);
    return UNITY_END();
}
