// The storage driver reads and writes the Teensy 4.1's SD card without making a caller wait for
// the card. The storage task does the card's work with SdFat
// and takes turns with the main task every millisecond, so a card that hangs never stops the main
// loop. A missing or failing card ends every request at once as failed. The same card comes back
// once it answers again.
//
// The requests, the files and their order come from drivers/storage-service.h.
//
// Design: docs/firmware/drivers/storage.md.
#pragma once
#include "drivers/driver-base.h"
#include "drivers/storage-service.h"

class Storage : public Driver, public StorageService {
public:
    static constexpr uint32_t STACK_BYTES = 8192;   // storage.md: the storage task's stack

    Storage();

    // Starts the storage task and waits until it has mounted the card. The main task calls it once
    // before the main loop. Returns false when no card sits in the socket or its file system does
    // not read. Every request then fails at once until a restart.
    bool begin() override;

    // Reports the card as failed part 0 with SdFat's error code once begin() has tried the mount.
    Fault failed() const override;

    // Gets the least stack the storage task had left since it started, in bytes. It is 0 before
    // begin(). Any task.
    uint32_t stackLeftBytes() const;

private:
    bool callerMayStart() const override;
    bool callerMayWait() const override;
    void wake() override;
    void pause() override;
    uint32_t lock() override;
    void unlock(uint32_t held) override;

    // Runs the storage task. It mounts the card, then serves the requests and tries a failed card
    // again once a second.
    static void run(void* self);

    volatile bool mountTried = false;
};

// storage is the firmware's one storage driver.
extern Storage storage;
