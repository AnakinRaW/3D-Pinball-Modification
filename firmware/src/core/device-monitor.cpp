#include "device-monitor.h"
#include "logging/logger.h"
#include "logging/debug-log.h"

/// The monitor's queue lies in RAM2. Its events are written before they are read, so it needs no
/// start value.
static DMAMEM DriverEvent monitorQueue[DeviceMonitor::DEPTH];

DeviceMonitor deviceMonitor(events, driverTick, monitorQueue);

bool DeviceMonitor::watch(Device device, const Driver& driver) {
    if (count == SLOTS) {
        logWarning("monitor", "all %u slots taken", SLOTS);
        return false;
    }
    if (out == nullptr) {
        EventQueue::Producer* attached = nullptr;
        if (!queue.attach(storage, DEPTH, attached)) {
            logWarning("monitor", "the event queue refused the monitor's queue");
            return false;
        }
        out = attached;
    }
    if (!checking) {
        if (!tick.attach([this] { check(); }, CHECK_US)) {
            logWarning("monitor", "the driver tick refused the check");
            return false;
        }
        checking = true;
    }
    // The check must not see a half-written slot.
    noInterrupts();
    slots[count] = Slot{device, &driver, 0};
    count = count + 1;
    interrupts();
    DEBUG_LOG("monitor: watches device %u\n", unsigned(device));
    return true;
}

void DeviceMonitor::check() {
    for (uint8_t i = 0; i < count; ++i) {
        Slot& s = slots[i];
        const Fault f = s.driver->failed();
        // Each round takes the lowest part that changed.
        for (uint32_t left = uint32_t(f.parts ^ s.reported); left != 0; left &= left - 1) {
            const uint8_t part = uint8_t(__builtin_ctz(left));
            const PartMask bit = PartMask(1u << part);
            const uint8_t source = faultSource(s.device, part);
            const bool published =
                (s.reported & bit) != 0
                    ? out->publish<DriverEventType::DEVICE_RECOVERED>(nowUs(), source, {})
                    : out->publish<DriverEventType::DEVICE_FAILED>(nowUs(), source, f.code);
            if (published) {
                s.reported = PartMask(s.reported ^ bit);
            }
        }
    }
}
