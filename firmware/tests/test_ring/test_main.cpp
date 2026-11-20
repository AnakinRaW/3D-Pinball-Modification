// Tests on the computer whether the ring hands its records back whole and in order.
//
//   wrap              records that run past the end of the buffer come back in order, the piece
//                     up to the end first and the rest after it
//   whole-or-nothing  a record larger than the free space is refused and leaves the ring as it was
//   power-of-two      a buffer of 1000 bytes holds 512. A ring of 0 bytes refuses every record
//   elements          a ring of structs counts elements, and records of several come back whole
//   begin             a ring without a buffer refuses every record, begin() gives it one and
//                     empties it
//
// Run: pio test -e computer, which runs in CI as well.

#include "core/ring.h"
#include <string>
#include <unity.h>

constexpr size_t BUFFER_BYTES = 16;
constexpr size_t ELEMENTS = 8;
constexpr size_t FIRST_RECORD = 3;     // a record that leaves the next one running past the end
constexpr size_t WRAP_RECORD = 6;      // a record of several elements across the end of the buffer
constexpr size_t BEGIN_ELEMENTS = 6;   // a buffer that is no power of two, of which begin() uses 4
constexpr size_t BEGIN_USED = 4;

// An element of several bytes, which a ring has to count as one.
struct Element {
    uint32_t number;
    uint16_t kind;
    uint8_t source;
    uint32_t payload;
};

void setUp() {}

void tearDown() {}

// Appends the characters of text as one record.
static bool append(Ring<uint8_t>& ring, const std::string& text) {
    return ring.append(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

// Takes every byte the ring holds piece by piece and counts the pieces.
static std::string drain(Ring<uint8_t>& ring, int& pieces) {
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

// Gets the element numbered k, whose fields all follow from k.
static Element element(uint32_t k) {
    return Element{k, uint16_t(k + 1), uint8_t(k + 2), k * 3};
}

// Checks that e is the element numbered k.
static void checkElement(uint32_t k, const Element& e) {
    TEST_ASSERT_EQUAL_UINT32(k, e.number);
    TEST_ASSERT_EQUAL_UINT16(uint16_t(k + 1), e.kind);
    TEST_ASSERT_EQUAL_UINT8(uint8_t(k + 2), e.source);
    TEST_ASSERT_EQUAL_UINT32(k * 3, e.payload);
}

// wrap: records across the end of the buffer come back in order, in two pieces.
static void testWrap() {
    uint8_t buffer[BUFFER_BYTES];
    Ring<uint8_t> ring(buffer, sizeof(buffer));
    int pieces = 0;
    TEST_ASSERT_TRUE(append(ring, "0123456789"));
    TEST_ASSERT_EQUAL_STRING("0123456789", drain(ring, pieces).c_str());
    TEST_ASSERT_TRUE(append(ring, "abcdef"));
    TEST_ASSERT_TRUE(append(ring, "ghijkl"));
    TEST_ASSERT_EQUAL_size_t(12, ring.used());
    TEST_ASSERT_EQUAL_STRING("abcdefghijkl", drain(ring, pieces).c_str());
    TEST_ASSERT_EQUAL_INT(2, pieces);
    TEST_ASSERT_EQUAL_size_t(22, ring.appended());
    TEST_ASSERT_EQUAL_size_t(22, ring.consumed());
}

// whole-or-nothing: a record that does not fit is refused. The ring keeps what it held.
static void testWholeOrNothing() {
    uint8_t buffer[BUFFER_BYTES];
    Ring<uint8_t> ring(buffer, sizeof(buffer));
    int pieces = 0;
    TEST_ASSERT_TRUE(append(ring, "0123456789"));
    TEST_ASSERT_FALSE(append(ring, "abcdefg"));
    TEST_ASSERT_EQUAL_size_t(10, ring.used());
    TEST_ASSERT_TRUE(append(ring, "abcdef"));
    TEST_ASSERT_FALSE(append(ring, "x"));
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef", drain(ring, pieces).c_str());
}

// power-of-two: the ring takes the largest power of two of its buffer, none of an empty one.
static void testPowerOfTwo() {
    uint8_t buffer[1000];
    Ring<uint8_t> ring(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_size_t(512, ring.size());
    Ring<uint8_t> none(buffer, 0);
    TEST_ASSERT_EQUAL_size_t(0, none.size());
    TEST_ASSERT_FALSE(append(none, "x"));
    TEST_ASSERT_FALSE(append(none, ""));
    Ring<uint8_t> missing(nullptr, sizeof(buffer));
    TEST_ASSERT_FALSE(append(missing, "x"));
}

// elements: the ring counts structs, and a record of several runs past the end in two pieces.
static void testElements() {
    Element buffer[ELEMENTS];
    Ring<Element> ring(buffer, ELEMENTS);
    TEST_ASSERT_EQUAL_size_t(ELEMENTS, ring.size());
    Element record[WRAP_RECORD];
    for (uint32_t k = 0; k < FIRST_RECORD; ++k) {
        record[k] = element(k);
    }
    TEST_ASSERT_TRUE(ring.append(record, FIRST_RECORD));
    size_t n = 0;
    const Element* first = ring.peek(n);
    TEST_ASSERT_EQUAL_size_t(FIRST_RECORD, n);
    for (uint32_t k = 0; k < FIRST_RECORD; ++k) {
        checkElement(k, first[k]);
    }
    ring.consume(n);
    for (uint32_t k = 0; k < WRAP_RECORD; ++k) {
        record[k] = element(FIRST_RECORD + k);
    }
    TEST_ASSERT_TRUE(ring.append(record, WRAP_RECORD));
    TEST_ASSERT_EQUAL_size_t(WRAP_RECORD, ring.used());
    // The record does not fit a second time.
    TEST_ASSERT_FALSE(ring.append(record, WRAP_RECORD));
    uint32_t k = FIRST_RECORD;
    int pieces = 0;
    for (const Element* e = ring.peek(n); n > 0; e = ring.peek(n)) {
        for (size_t i = 0; i < n; ++i) {
            checkElement(k, e[i]);
            k = k + 1;
        }
        ring.consume(n);
        pieces = pieces + 1;
    }
    TEST_ASSERT_EQUAL_UINT32(FIRST_RECORD + WRAP_RECORD, k);
    TEST_ASSERT_EQUAL_INT(2, pieces);
    TEST_ASSERT_EQUAL_size_t(FIRST_RECORD + WRAP_RECORD, ring.appended());
    TEST_ASSERT_EQUAL_size_t(FIRST_RECORD + WRAP_RECORD, ring.consumed());
}

// begin: a ring without a buffer refuses, and begin() gives it a buffer and empties it.
static void testBegin() {
    uint8_t buffer[BEGIN_ELEMENTS];
    Ring<uint8_t> ring;
    TEST_ASSERT_EQUAL_size_t(0, ring.size());
    TEST_ASSERT_FALSE(append(ring, "x"));
    ring.begin(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_size_t(BEGIN_USED, ring.size());
    TEST_ASSERT_TRUE(append(ring, "abc"));
    ring.begin(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_size_t(0, ring.used());
    TEST_ASSERT_EQUAL_size_t(0, ring.appended());
    TEST_ASSERT_EQUAL_size_t(0, ring.consumed());
    int pieces = 0;
    TEST_ASSERT_TRUE(append(ring, "wxyz"));
    TEST_ASSERT_EQUAL_STRING("wxyz", drain(ring, pieces).c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testWrap);
    RUN_TEST(testWholeOrNothing);
    RUN_TEST(testPowerOfTwo);
    RUN_TEST(testElements);
    RUN_TEST(testBegin);
    return UNITY_END();
}
