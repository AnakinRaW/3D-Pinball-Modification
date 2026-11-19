// Tests on the computer whether the priority list keeps its entries highest priority first and
// each entry in the slot it was put in.
//
//   order   highest priority first, and the later of two equal priorities above the earlier
//   full    a list with no room refuses an entry and keeps the others
//   remove  removing entries keeps the order of the rest, and every pointer to an entry that
//           stays still reaches it
//
// Run: pio test -e computer, which runs in CI as well.

#include <priority-list.h>
#include <unity.h>

constexpr size_t ROOM = 4;   // the entries a test list holds

struct Item {
    uint8_t priority;
    char name;
    friend uint8_t priorityOf(const Item& i) { return i.priority; }
};

using List = PriorityList<Item, ROOM>;

void setUp() {}

void tearDown() {}

// Checks that the list holds exactly the names in order, highest first.
static void expectOrder(const List& list, const char* names) {
    size_t n = 0;
    while (names[n] != '\0') {
        ++n;
    }
    TEST_ASSERT_EQUAL_size_t(n, list.size());
    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_CHAR(names[i], list[i].name);
    }
}

// order: priorities 2, 5, 1 and a second 5 go in; the list holds the later 5 first.
static void testOrder() {
    List list;
    list.insert({2, 'a'});
    list.insert({5, 'b'});
    list.insert({1, 'c'});
    list.insert({5, 'd'});
    expectOrder(list, "dbac");
    const Item* top = list.find([](const Item& i) { return i.priority == 5; });
    TEST_ASSERT_NOT_NULL(top);
    TEST_ASSERT_EQUAL_CHAR('d', top->name);
}

// full: a fifth entry finds no room and changes nothing.
static void testFull() {
    List list;
    list.insert({1, 'a'});
    list.insert({3, 'b'});
    list.insert({2, 'c'});
    list.insert({4, 'd'});
    TEST_ASSERT_TRUE(list.full());
    TEST_ASSERT_NULL(list.insert({9, 'e'}));
    expectOrder(list, "dbca");
}

// remove: the top and a middle entry go, two new ones take their slots, and the pointers to the
// two that stayed still reach them.
static void testRemove() {
    List list;
    Item* a = list.insert({1, 'a'});
    list.insert({3, 'b'});
    Item* c = list.insert({2, 'c'});
    list.insert({4, 'd'});
    list.removeIf([](const Item& i) { return i.name == 'b' || i.name == 'd'; });
    expectOrder(list, "ca");
    list.insert({2, 'e'});
    list.insert({0, 'f'});
    expectOrder(list, "ecaf");
    TEST_ASSERT_EQUAL_CHAR('a', a->name);
    TEST_ASSERT_EQUAL_CHAR('c', c->name);
    list.clear();
    TEST_ASSERT_EQUAL_size_t(0, list.size());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(testOrder);
    RUN_TEST(testFull);
    RUN_TEST(testRemove);
    return UNITY_END();
}
