// A fake of the firmware's task lock for the tests that run on the computer. The tests run in one
// thread, so the lock only counts how often it was taken. A test sets fakeInInterrupt to
// make the code believe it runs in an interrupt.
#pragma once
#include <cstdint>

inline uint32_t fakeLocksTaken = 0;
inline bool fakeInInterrupt = false;

class TaskLock {
public:
    TaskLock() { fakeLocksTaken = fakeLocksTaken + 1; }

    TaskLock(const TaskLock&) = delete;
    TaskLock& operator=(const TaskLock&) = delete;
};

inline bool inInterrupt() {
    return fakeInInterrupt;
}
