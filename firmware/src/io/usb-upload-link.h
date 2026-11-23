// The card upload's link to the computer over the Teensy's USB serial port. It takes the bytes
// that have arrived without waiting, and its answers leave with the debug lines from the main
// loop.
#pragma once
#include "components/card-upload.h"

class UsbUploadLink final : public UploadLink {
public:
    size_t read(uint8_t* to, size_t n) override;
    void answer(const char* line, size_t n) override;
};
