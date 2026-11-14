// Formats the card in the Teensy 4.1's built-in SD socket the way the SD Association's formatter
// does, through SdFat: FAT32 with 32 KB clusters for a card of 2 to 32 GB, exFAT above that.
// EVERYTHING ON THE CARD IS ERASED.
//
// The sketch waits for the serial monitor and formats only after a capital Y arrives on it.

#include <SdFat.h>

SdFs sd;

void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    if (!sd.begin(SdioConfig(FIFO_SDIO))) {
        // tells a card that does not answer from a card whose file system does not mount
        Serial.printf("no file system mounts, SdFat error 0x%02X, data 0x%02X\n", sd.sdErrorCode(), sd.sdErrorData());
        if (sd.cardBegin(SdioConfig(FIFO_SDIO)))
            Serial.printf("the card answers: %.1f GB\n", sd.card()->sectorCount() * 512.0 / 1e9);
        else
            Serial.printf("the card does not answer, SdFat error 0x%02X\n", sd.sdErrorCode());
        Serial.println("send Y to erase and format the card");
        while (Serial.read() != 'Y') {}
        if (!sd.format(&Serial)) { Serial.println("format failed"); return; }
        Serial.println("done");
        return;
    }
    Serial.printf("card %.1f GB, cluster now %lu bytes\n", sd.card()->sectorCount() * 512.0 / 1e9,
                  sd.bytesPerCluster());
    Serial.println("send Y to erase and format the card");
    while (Serial.read() != 'Y') {}

    if (!sd.format(&Serial)) {
        Serial.println("format failed");
        return;
    }
    sd.end();
    if (!sd.begin(SdioConfig(FIFO_SDIO))) {
        Serial.println("card did not mount after formatting");
        return;
    }
    Serial.printf("formatted: %s, cluster %lu bytes\n",
                  sd.fatType() == FAT_TYPE_EXFAT ? "exFAT" : "FAT32", sd.bytesPerCluster());
    Serial.println("done");
}

void loop() {}
