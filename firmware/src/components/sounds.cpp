#include "sounds.h"
#include "logging/debug-log.h"
#include "logging/logger.h"
#include <math.h>
#include <stdio.h>

constexpr char AUDIO_DIRECTORY[] = "audio/";   // sounds.md: the game's audio directory

void Sounds::begin() {
    musicVolume(settings.musicVolume);
    effectsVolume(settings.effectsVolume);
}

float Sounds::gain(float percent) {
    return percent * percent;
}

FileReader Sounds::file(const char* name) const {
    char path[FILE_PATH_BYTES];
    const int n = snprintf(path, sizeof path, "%s%s", AUDIO_DIRECTORY, name);
    if (n < 0 || size_t(n) >= sizeof path) {
        DEBUG_LOG("sounds: %s is too long for a path\n", name);
        return FileReader();
    }
    return files.open(path);
}

uint8_t Sounds::pick() const {
    uint8_t oldest = MUSIC + 1;
    for (uint8_t c = MUSIC + 1; c < AudioDriver::CHANNELS; ++c) {
        if (!audio.playing(c)) {
            return c;
        }
        // The difference keeps the order across the count's wrap.
        if (int32_t(startedAs[c] - startedAs[oldest]) < 0) {
            oldest = c;
        }
    }
    return oldest;
}

void Sounds::play(const char* name) {
    const uint8_t c = pick();
    if (audio.playing(c)) {
        DEBUG_LOG("sounds: %s takes channel %u from an effect still playing\n", name, c);
    } else {
        DEBUG_LOG("sounds: %s plays on channel %u\n", name, c);
    }
    effects = effects + 1;
    startedAs[c] = effects;
    effectFiles[c] = file(name);
    effectNumbers[c] = audio.play(c, effectFiles[c]);
}

void Sounds::hold(Track& t) {
    if (t.number == 0 || t.number != onChannel) {
        return;
    }
    // The driver's position belongs to the last request, which was this track's.
    const AudioDriver::Position at = audio.position(MUSIC);
    if (at.empty()) {
        // The track has played to its end, and its SOUND_ENDED is on the way. It keeps its
        // number, so translate() ends it.
        return;
    }
    t.at = at;
    t.number = 0;
}

void Sounds::resume() {
    if (tracks.size() == 0) {
        if (onChannel != 0) {
            audio.stop(MUSIC);
            onChannel = 0;
        }
        return;
    }
    Track& t = tracks[0];
    if (t.number != 0) {
        if (t.number != onChannel && onChannel != 0) {
            // The track on top has played to its end and waits for its SOUND_ENDED, so the
            // channel stays silent until translate() ends it.
            audio.stop(MUSIC);
            onChannel = 0;
        }
        return;
    }
    t.number = t.at.empty() ? audio.play(MUSIC, t.file, t.loop) : audio.play(MUSIC, t.at);
    t.at = {};
    onChannel = t.number;
}

void Sounds::start(const Track& track) {
    // The track it replaces leaves first, so the new one lies above the tracks of equal priority.
    tracks.removeIf([&](const Track& t) { return t.same(track); });
    if (tracks.insert(track) == nullptr) {
        DEBUG_LOG("sounds: a track of key %u refused, %u modes hold one\n", track.key, MODES);
        resume();
        return;
    }
    if (tracks.size() > 1 && tracks[0].same(track)) {
        hold(tracks[1]);
    }
    resume();
}

template <typename Pred> void Sounds::drop(Pred pred) {
    const bool top = tracks.size() > 0 && pred(tracks[0]);
    tracks.removeIf(pred);
    if (top) {
        resume();
    }
}

void Sounds::music(Layer layer, const char* name, bool loop) {
    DEBUG_LOG("sounds: key %u starts the track %s\n", layer.key, name);
    Track track;
    track.key = layer.key;
    track.priority = layer.priority;
    track.file = file(name);
    track.loop = loop;
    start(track);
}

void Sounds::stopMusic(Layer layer) {
    drop([&](const Track& t) { return t.owner == Owner::MODE && t.key == layer.key; });
}

bool Sounds::musicEnded(Layer layer) const {
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].owner == Owner::MODE && tracks[i].key == layer.key) {
            return false;
        }
    }
    return true;
}

void Sounds::videoSound(const char* name, bool loop) {
    Track track;
    track.owner = Owner::VIDEO;
    track.file = file(name);
    track.loop = loop;
    start(track);
}

void Sounds::stopVideoSound() {
    drop([](const Track& t) { return t.owner == Owner::VIDEO; });
}

bool Sounds::videoSounding() const {
    for (size_t i = 0; i < tracks.size(); ++i) {
        if (tracks[i].owner == Owner::VIDEO) {
            return true;
        }
    }
    return false;
}

bool Sounds::videoPlayed(uint32_t& samples) const {
    samples = 0;
    // The video's sound lies on top whenever it is there, so the channel plays it.
    if (tracks.size() == 0 || tracks[0].owner != Owner::VIDEO || tracks[0].number == 0 ||
        tracks[0].number != onChannel) {
        return false;
    }
    // A channel that has played its sound's last sample holds no position, though the sound's end
    // may still be on its way.
    const AudioDriver::Position at = audio.position(MUSIC);
    if (at.empty()) {
        return false;
    }
    samples = at.played();
    return true;
}

void Sounds::musicVolume(float percent) {
    musicPercent = fminf(fmaxf(percent, VOLUME_MIN), VOLUME_MAX);
    balance();
}

void Sounds::effectsVolume(float percent) {
    effectsPercent = fminf(fmaxf(percent, VOLUME_MIN), VOLUME_MAX);
    balance();
}

void Sounds::balance() {
    float music = gain(musicPercent);
    float effects = gain(effectsPercent);
    // A full-scale sample of the music and one of an effect add up on the mixer. Dividing both by
    // their sum keeps that within full scale and keeps the ratio between them.
    const float sum = music + effects;
    if (sum > FULL_SCALE) {
        music = music * FULL_SCALE / sum;
        effects = effects * FULL_SCALE / sum;
    }
    musicGain = music;
    effectsGain = effects;
    audio.volume(MUSIC, musicGain);
    for (uint8_t c = MUSIC + 1; c < AudioDriver::CHANNELS; ++c) {
        audio.volume(c, effectsGain);
    }
}

bool Sounds::translate(const DriverEvent& in, PinballEvents& out) {
    if (in.type != DriverEventType::SOUND_ENDED) {
        return false;
    }
    out = {};
    const SoundEnd end = payload<DriverEventType::SOUND_ENDED>(in);
    const uint32_t number = end.number;
    if (end.code != SoundEndCode::PLAYED && number != 0) {
        warnFailed(in.source, end);
    }
    if (in.source != MUSIC || number == 0) {
        return true;
    }
    if (number == onChannel) {
        onChannel = 0;
    }
    // A track stopped or replaced reports no end, so only a track still held can match.
    const Track* t = tracks.find([&](const Track& x) { return x.number == number; });
    if (t == nullptr) {
        return true;
    }
    const bool top = t == &tracks[0];
    const Track done = *t;
    tracks.removeIf([&](const Track& x) { return x.number == number; });
    if (done.owner == Owner::MODE) {
        DEBUG_LOG("sounds: the track of key %u has ended\n", done.key);
        ended = {in.time, EventType::MUSIC_ENDED, done.key, number};
        out = {&ended, 1};
    }
    if (top) {
        resume();
    }
    return true;
}

void Sounds::warnFailed(uint8_t channel, SoundEnd end) {
    const char* name = nullptr;
    if (channel == MUSIC) {
        const Track* t = tracks.find([&](const Track& x) { return x.number == end.number; });
        name = t != nullptr ? t->file.name() : nullptr;
    } else if (channel < AudioDriver::CHANNELS && effectNumbers[channel] == end.number) {
        name = effectFiles[channel].name();
    }
    if (name == nullptr || name[0] == '\0') {
        // A name too long for a path leaves the file without one.
        name = "a sound";
    }
    switch (end.code) {
    case SoundEndCode::NO_FILE:
        logWarning("sounds", "%s is missing or empty", name);
        break;
    case SoundEndCode::NOT_WAV:
        logWarning("sounds", "%s is no WAV of 16-bit PCM at 44.1 kHz, mono", name);
        break;
    case SoundEndCode::CUT_SHORT:
        logWarning("sounds", "%s ends ahead of its samples", name);
        break;
    case SoundEndCode::READ_FAILED:
        logWarning("sounds", "%s stopped, a read of the card failed", name);
        break;
    case SoundEndCode::PLAYED:
        break;
    }
}

void Sounds::reset() {
    tracks.clear();
    onChannel = 0;
    for (uint8_t c = 0; c < AudioDriver::CHANNELS; ++c) {
        audio.stop(c);
        audio.volume(c, c == MUSIC ? musicGain : effectsGain);
    }
}
