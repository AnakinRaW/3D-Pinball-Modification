// A lock that keeps every other task out of a short piece of code, so two tasks never change the
// same data at once. While it is held, FreeRTOS switches no task and no interrupt that calls
// FreeRTOS runs. The more urgent interrupts of the drivers keep running, so the data it guards is
// never touched from an interrupt.
#pragma once
#include <FreeRTOS.h>
#include <task.h>

// Holds the lock from its construction to the end of its scope. Locks nest. Before FreeRTOS has
// started, only one task runs. The lock then does nothing.
// Any task may take it. An interrupt never takes it, since an interrupt of a driver can stop the
// code that holds it.
class TaskLock {
public:
    TaskLock() : held(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        if (held) {
            taskENTER_CRITICAL();
        }
    }

    ~TaskLock() {
        if (held) {
            taskEXIT_CRITICAL();
        }
    }

    TaskLock(const TaskLock&) = delete;
    TaskLock& operator=(const TaskLock&) = delete;

private:
    const bool held;
};

// Gets whether the code runs in an interrupt rather than in a task.
inline bool inInterrupt() {
    return xPortIsInsideInterrupt() == pdTRUE;
}
