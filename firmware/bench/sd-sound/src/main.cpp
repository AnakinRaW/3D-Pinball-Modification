// Sound test for the Teensy 4.1: plays a WAV file from the card in the Teensy's own socket through
// the DFRobot DFR0954 to the speaker, in a loop. Wired as docs/parts/audio/design.md draws it:
// DIN to pin 2, LRC to pin 3, BCLK to pin 4, VCC to 5 V, GND to GND, R4 with 100 kOhm from GAIN
// to VCC, the speaker on SPK+ and SPK-.
//
// It plays /test.wav where the card holds it, and /tone.wav otherwise. Where /tone.wav is
// missing too, the program writes it first: three rising notes and a pause, 16-bit PCM at
// 44.1 kHz, mono, as assets/media.md makes a sound.
//
// A file reaches the card over USB without taking the card out. The computer sends "PUT <bytes>"
// and a newline, waits for "GO" and then sends the file's bytes. The program stops playing, writes
// them to /test.wav, answers with the bytes' FNV-1a checksum and plays the new file.
//
// PJRC's AudioPlaySdWav plays 16-bit PCM at 44.1 kHz, mono or stereo. It reads the card inside
// the audio update, which the audio driver rules out; a test without FreeRTOS takes it. The two
// channels are mixed at half each, then pass the master gain at its ceiling and leave on both
// channels of I2S2.
//
// A status line every five seconds gives the position in the file, the audio library's peak CPU
// load and its peak count of audio blocks.
//
// It starts once the serial monitor is open.

#include <Arduino.h>
#include <Audio.h>
#include <SD.h>
#include <math.h>

constexpr float GAIN_CAP = 0.786f;       // audio model g_cap: the speaker stays at 0.5 W
constexpr float CHANNEL_GAIN = 0.5f;     // left and right at half each, so their sum stays in range
constexpr uint8_t AUDIO_BLOCKS = 12;     // AudioPlaySdWav sends two per channel, the rest is headroom
constexpr uint32_t STATUS_MS = 5000;     // a status line this often
constexpr const char* TEST_FILE = "/test.wav";
constexpr const char* TONE_FILE = "/tone.wav";

// the tone file: A4, C#5 and E5, each 400 ms, then 400 ms of silence
constexpr uint32_t RATE_HZ = 44100;                  // assets/media.md
constexpr float NOTES_HZ[] = {440.0f, 554.37f, 659.26f};
constexpr uint32_t NOTE_SAMPLES = RATE_HZ * 4 / 10;  // 400 ms
constexpr uint32_t NOTE_COUNT = sizeof(NOTES_HZ) / sizeof(NOTES_HZ[0]);
constexpr uint32_t FILE_SAMPLES = NOTE_SAMPLES * (NOTE_COUNT + 1);
constexpr uint32_t FADE_SAMPLES = RATE_HZ / 200;     // 5 ms in and out, so a note does not click
constexpr float AMPLITUDE = 0.5f;                    // half of full scale, 6 dB below the cap

// the upload over USB
constexpr uint32_t CHUNK_BYTES = 32768;              // bytes written to the card at once
constexpr uint32_t QUIET_MS = 3000;                  // an upload with no byte for this long fails
constexpr uint32_t FNV_OFFSET = 2166136261u;         // FNV-1a, 32 bits
constexpr uint32_t FNV_PRIME = 16777619u;
constexpr uint32_t LINE_BYTES = 40;                  // the longest command line

AudioPlaySdWav wav;
AudioMixer4 mix;
AudioAmplifier level;
AudioOutputI2S2 out;
AudioConnection wireLeftIn(wav, 0, mix, 0);
AudioConnection wireRightIn(wav, 1, mix, 1);
AudioConnection wireLevel(mix, 0, level, 0);
AudioConnection wireLeft(level, 0, out, 0);
AudioConnection wireRight(level, 0, out, 1);

static uint8_t chunk[CHUNK_BYTES];
static char line[LINE_BYTES];
static uint32_t lineAt = 0;
static uint32_t plays = 0;
static uint32_t lastStatusMs = 0;
static const char* playing = TONE_FILE;

static void put16(uint8_t* at, uint16_t value) {
    at[0] = value & 0xFF;
    at[1] = value >> 8;
}

static void put32(uint8_t* at, uint32_t value) {
    put16(at, value & 0xFFFF);
    put16(at + 2, value >> 16);
}

// one sample of the tone file, as a signed 16-bit value
static int16_t sample(uint32_t n) {
    const uint32_t note = n / NOTE_SAMPLES;
    if (note >= NOTE_COUNT) {
        return 0;
    }
    const uint32_t i = n % NOTE_SAMPLES;
    const uint32_t edge = min(i, NOTE_SAMPLES - 1 - i);
    const float fade = edge < FADE_SAMPLES ? static_cast<float>(edge) / FADE_SAMPLES : 1.0f;
    const float phase = 2.0f * static_cast<float>(M_PI) * NOTES_HZ[note] * i / RATE_HZ;
    return static_cast<int16_t>(AMPLITUDE * fade * sinf(phase) * 32767.0f);
}

// writes the tone file with its RIFF, fmt and data headers; returns false where a write fails
static bool writeTone() {
    File file = SD.open(TONE_FILE, FILE_WRITE_BEGIN);
    if (!file) {
        return false;
    }
    const uint32_t dataBytes = FILE_SAMPLES * 2;
    uint8_t head[44];
    memcpy(head, "RIFF", 4);
    put32(head + 4, 36 + dataBytes);
    memcpy(head + 8, "WAVEfmt ", 8);
    put32(head + 16, 16);              // the fmt chunk's size
    put16(head + 20, 1);               // PCM
    put16(head + 22, 1);               // mono
    put32(head + 24, RATE_HZ);
    put32(head + 28, RATE_HZ * 2);     // bytes per second
    put16(head + 32, 2);               // bytes per sample
    put16(head + 34, 16);              // bits per sample
    memcpy(head + 36, "data", 4);
    put32(head + 40, dataBytes);
    bool ok = file.write(head, sizeof(head)) == sizeof(head);

    const uint32_t chunkSamples = CHUNK_BYTES / 2;
    for (uint32_t n = 0; ok && n < FILE_SAMPLES; n += chunkSamples) {
        const uint32_t count = min(chunkSamples, FILE_SAMPLES - n);
        for (uint32_t i = 0; i < count; i++) {
            put16(chunk + 2 * i, static_cast<uint16_t>(sample(n + i)));
        }
        ok = file.write(chunk, count * 2) == count * 2;
    }
    file.close();
    return ok;
}

// takes size bytes from USB into /test.wav and answers with their checksum
static void receive(uint32_t size) {
    wav.stop();
    SD.remove(TEST_FILE);
    File file = SD.open(TEST_FILE, FILE_WRITE_BEGIN);
    if (!file) {
        Serial.println("ERR cannot create the file");
        return;
    }
    Serial.println("GO");
    uint32_t hash = FNV_OFFSET;
    uint32_t got = 0;
    uint32_t fill = 0;
    uint32_t lastByteMs = millis();
    const uint32_t startMs = millis();
    bool ok = true;
    while (ok && got < size) {
        const int ready = Serial.available();
        if (ready <= 0) {
            if (millis() - lastByteMs > QUIET_MS) {
                ok = false;
            }
            continue;
        }
        const uint32_t take = min(min(static_cast<uint32_t>(ready), CHUNK_BYTES - fill), size - got);
        const uint32_t n = Serial.readBytes(reinterpret_cast<char*>(chunk + fill), take);
        for (uint32_t i = 0; i < n; i++) {
            hash = (hash ^ chunk[fill + i]) * FNV_PRIME;
        }
        fill += n;
        got += n;
        lastByteMs = millis();
        if (fill == CHUNK_BYTES || got == size) {
            ok = file.write(chunk, fill) == fill;
            fill = 0;
        }
    }
    file.close();
    if (!ok) {
        Serial.printf("ERR after %lu of %lu bytes\n", static_cast<unsigned long>(got),
                      static_cast<unsigned long>(size));
        return;
    }
    Serial.printf("OK %lu bytes in %lu ms, fnv %08lx\n", static_cast<unsigned long>(got),
                  static_cast<unsigned long>(millis() - startMs), static_cast<unsigned long>(hash));
    playing = TEST_FILE;
}

// reads a command line from USB without waiting; PUT starts an upload
static void command() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c != '\n') {
            if (lineAt < LINE_BYTES - 1) {
                line[lineAt++] = c;
            }
            continue;
        }
        line[lineAt] = 0;
        lineAt = 0;
        if (strncmp(line, "PUT ", 4) == 0) {
            receive(strtoul(line + 4, nullptr, 10));
            return;
        }
        Serial.printf("ERR unknown command: %s\n", line);
    }
}

static void listRoot() {
    File root = SD.open("/");
    if (!root) {
        Serial.println("  cannot open the root directory");
        return;
    }
    for (File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
        Serial.printf("  %s  %lu bytes\n", entry.name(), static_cast<unsigned long>(entry.size()));
        entry.close();
    }
    root.close();
}

static void halt() {
    while (true) {
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
    }

    AudioMemory(AUDIO_BLOCKS);
    mix.gain(0, CHANNEL_GAIN);
    mix.gain(1, CHANNEL_GAIN);
    level.gain(GAIN_CAP);

    Serial.println("sound test through I2S2 to the DFR0954");
    if (!SD.begin(BUILTIN_SDCARD)) {
        Serial.println("no card found in the Teensy's socket");
        halt();
    }
    Serial.println("card found, root directory:");
    listRoot();
    if (!SD.exists(TONE_FILE)) {
        Serial.printf("writing %s ... ", TONE_FILE);
        if (!writeTone()) {
            Serial.println("failed");
            halt();
        }
        Serial.println("done");
    }
    if (SD.exists(TEST_FILE)) {
        playing = TEST_FILE;
    }
}

void loop() {
    command();

    if (!wav.isPlaying()) {
        if (!wav.play(playing)) {
            Serial.printf("%s does not play: not a WAV the library reads\n", playing);
            playing = TONE_FILE;
            return;
        }
        plays = plays + 1;
        // the library parses the header in the next updates; the length follows from it
        delay(10);
        if (!wav.isPlaying()) {
            Serial.printf("%s stopped at once: only 16-bit PCM at 44.1 kHz plays\n", playing);
            playing = TONE_FILE;
            return;
        }
        Serial.printf("play %lu: %s, %lu ms long\n", static_cast<unsigned long>(plays), playing,
                      static_cast<unsigned long>(wav.lengthMillis()));
    }

    const uint32_t nowMs = millis();
    if (nowMs - lastStatusMs >= STATUS_MS) {
        lastStatusMs = nowMs;
        Serial.printf("  at %lu ms, CPU peak %.1f %%, blocks peak %u of %u\n",
                      static_cast<unsigned long>(wav.positionMillis()),
                      static_cast<double>(AudioProcessorUsageMax()),
                      static_cast<unsigned>(AudioMemoryUsageMax()),
                      static_cast<unsigned>(AUDIO_BLOCKS));
    }
}
