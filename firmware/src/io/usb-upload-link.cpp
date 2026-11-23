#include "usb-upload-link.h"
#include "logging/debug-log.h"
#include <Arduino.h>

size_t UsbUploadLink::read(uint8_t* to, size_t n) {
    // readBytes() waits only for bytes that have not arrived, so asking for the arrived ones alone
    // returns at once.
    const int arrived = Serial.available();
    if (arrived <= 0 || n == 0) {
        return 0;
    }
    const size_t take = size_t(arrived) < n ? size_t(arrived) : n;
    return Serial.readBytes(reinterpret_cast<char*>(to), take);
}

void UsbUploadLink::answer(const char* line, size_t n) {
    serialLine(line, n);
}
