#include "video-player.h"
#include "core/clock.h"
#include "logging/debug-log.h"
#include <stdio.h>
#include <string.h>

constexpr uint32_t PIXEL_BYTES = 2;   // RGB565
/// LVGL's refresh period while a video plays: never, so only the frames redraw the screen
constexpr uint32_t PAUSED_PERIOD_MS = UINT32_MAX;

void VideoPlayer::begin() {
    if (timer != nullptr) {
        return;
    }
    // A period of 0 runs the timer in every lv_timer_handler(), inside the screen's render().
    timer = lv_timer_create(tick, 0, this);
}

void VideoPlayer::tick(lv_timer_t* t) {
    static_cast<VideoPlayer*>(lv_timer_get_user_data(t))->step(nowUs());
}

bool VideoPlayer::translate(const DriverEvent& in, PinballEvents& out) {
    (void)in;
    out = {};
    return false;
}

bool VideoPlayer::update(uint32_t now, PinballEvents& out) {
    (void)now;
    out = {};
    if (!endedDue) {
        return false;
    }
    endedDue = false;
    out = {&ended, 1};
    return true;
}

bool VideoPlayer::playing(const lv_obj_t* in) const {
    return active && viewport == in;
}

void VideoPlayer::stop(const lv_obj_t* in) {
    if (playing(in)) {
        end();
    }
}

void VideoPlayer::reset() {
    end();
    endedDue = false;
}

bool VideoPlayer::play(lv_obj_t* in, const char* name, bool loops, bool withSound) {
    // Only the slide that shows plays a video, so a video that plays lies on the same slide and is
    // replaced.
    lv_obj_t* owner = screen.slideOf(in);
    if (owner == nullptr || owner != screen.top()) {
        DEBUG_LOG("video: %s refused, its viewport lies on no slide that shows\n", name);
        return false;
    }
    lv_obj_update_layout(in);
    lv_area_t a;
    lv_obj_get_content_coords(in, &a);
    if (a.x2 < a.x1 || a.y2 < a.y1 || a.x1 < 0 || a.y1 < 0 || a.x2 >= Display::WIDTH ||
        a.y2 >= Display::HEIGHT) {
        DEBUG_LOG("video: %s refused, its viewport is empty or reaches beyond the screen\n", name);
        return false;
    }
    if (strlen(name) > NAME_BYTES) {
        DEBUG_LOG("video: %s refused, the name is too long\n", name);
        return false;
    }
    const uint16_t width = uint16_t(lv_area_get_width(&a));
    const uint16_t height = uint16_t(lv_area_get_height(&a));
    const uint32_t bytes = uint32_t(width) * height * PIXEL_BYTES;

    // The rooms of the video that plays count as free, all but those a read still lands in.
    uint8_t* freed[FRAMES_AHEAD] = {};
    for (uint8_t i = 0; i < FRAMES_AHEAD; ++i) {
        freed[i] = active && holds[i] != Hold::READING ? rooms[i] : nullptr;
    }
    if (!screen.pictures.fits(FRAMES_AHEAD, bytes, freed, FRAMES_AHEAD)) {
        DEBUG_LOG("video: %s refused, the picture memory has no room for its frames\n", name);
        return false;
    }
    end();
    for (uint8_t i = 0; i < FRAMES_AHEAD; ++i) {
        rooms[i] = screen.pictures.take(bytes, true);
        held[i] = 0;
        holds[i] = Hold::NOTHING;
    }

    ++video;
    active = true;
    loop = loops;
    sound = withSound;
    frameBytes = bytes;
    frame = {};
    frame.header.magic = LV_IMAGE_HEADER_MAGIC;
    frame.header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
    frame.header.w = width;
    frame.header.h = height;
    frame.header.stride = uint16_t(width * PIXEL_BYTES);
    frame.data_size = bytes;
    viewport = in;
    slide = owner;
    // The image lies at the top left of the viewport's content and is empty until the first frame
    // has landed.
    image = lv_image_create(in);
    lv_obj_add_event_cb(image, gone, LV_EVENT_DELETE, this);

    char path[FILE_PATH_BYTES];
    snprintf(path, sizeof path, "video/%s.rgb", name);
    file = files.open(path);
    snprintf(wav, sizeof wav, "%s.wav", name);
    counted = false;
    count = 0;
    loopStart = 0;
    nextRead = 0;
    shownFrame = 0;
    anyShown = false;
    startUs = nowUs();
    // The frames redraw the screen while the video plays, so LVGL's own refresh pauses.
    lv_timer_set_period(lv_display_get_refr_timer(screen.lv), PAUSED_PERIOD_MS);
    soundFrame = 0;
    landed = 0;
    soundStarted = false;
    readAhead(0);
    return true;
}

// A video with sound follows its sound, so a sound that stalls holds the video with it. A video
// whose sound has run out, or one without, follows the clock from the frame it stands at.
uint32_t VideoPlayer::due(uint32_t now) {
    if (sound && !soundStarted) {
        // The first frames still come from the card, so the video stands at its first frame.
        startUs = now;
        return loopStart;
    }
    if (sound && sounds.videoSounding()) {
        uint32_t played = sounds.videoPlayed() / FRAME_SAMPLES;
        if (loop && counted && count > 0) {
            // The sound counts from the start of each pass, so a count that falls back starts the
            // video's next pass. A sound longer than the video holds its last frame.
            if (played < soundFrame) {
                loopStart += count;
            }
            soundFrame = played;
            played = played < count ? played : count - 1;
        }
        startUs = now - played * FRAME_US;
        return loopStart + played;
    }
    return loopStart + (now - startUs) / FRAME_US;
}

void VideoPlayer::step(uint32_t now) {
    for (Read& r : reads) {
        if (r.room != nullptr && r.request.status() != FileStatus::PENDING) {
            land(r);
        }
    }
    if (!active) {
        return;
    }
    if (screen.top() != slide) {
        // Another slide shows in place of the video's.
        end();
        return;
    }
    if (sound && !soundStarted && counted &&
        landed >= (count < START_FRAMES ? count : START_FRAMES)) {
        // The sound starts once the first frames have landed, so it does not run ahead of the
        // picture while the card brings them. A sound that loops runs on from its first sample
        // after its last, with no gap, and the video follows its passes.
        sounds.videoSound(wav, loop);
        soundStarted = true;
    }
    uint32_t frameDue = due(now);
    if (counted && (count == 0 || (!loop && frameDue >= loopStart + count))) {
        ended = {now, EventType::MOVIE_ENDED, screen.keyOf(slide), 0};
        endedDue = true;
        end();
        return;
    }
    if (counted && frameDue >= loopStart + count) {
        const uint32_t passes = (frameDue - loopStart) / count;
        loopStart += passes * count;
        startUs += passes * count * FRAME_US;
        frameDue = due(now);
    }
    show(frameDue);
    readAhead(frameDue);
}

void VideoPlayer::land(Read& r) {
    uint8_t* room = r.room;
    r.room = nullptr;
    if (!active || r.video != video) {
        // A read of a stopped video gives its room back once it has landed.
        screen.pictures.give(room);
        return;
    }
    if (!counted) {
        count = r.request.fileSize() / frameBytes;
        counted = true;
        if (count == 0) {
            DEBUG_LOG("video: %s holds no frame\n", file.name());
        }
    }
    const bool whole =
        r.request.status() == FileStatus::SUCCEEDED && r.request.bytes() == frameBytes;
    const bool passed = anyShown && held[r.slot] <= shownFrame;
    holds[r.slot] = whole && !passed ? Hold::FRAME : Hold::NOTHING;
    landed += whole ? 1 : 0;
}

// The latest frame that has landed and is due shows, so a frame the card brings late is skipped
// rather than slowing the video. The frame it replaces and every frame it passes free their rooms.
void VideoPlayer::show(uint32_t frameDue) {
    int best = -1;
    for (uint8_t i = 0; i < FRAMES_AHEAD; ++i) {
        if (holds[i] == Hold::FRAME && held[i] <= frameDue && (best < 0 || held[i] > held[best])) {
            best = i;
        }
    }
    if (best < 0) {
        return;
    }
    for (uint8_t i = 0; i < FRAMES_AHEAD; ++i) {
        if (holds[i] == Hold::SHOWN || (holds[i] == Hold::FRAME && held[i] < held[best])) {
            holds[i] = Hold::NOTHING;
        }
    }
    holds[best] = Hold::SHOWN;
    shownFrame = held[best];
    anyShown = true;
    frame.data = rooms[best];
    lv_image_set_src(image, &frame);
    // Draws the screen now, with every other change since the last frame.
    lv_refr_now(screen.lv);
}

// The frames after the last one asked for go into the rooms that hold nothing. A card that has
// fallen behind goes on with the frame that is due.
void VideoPlayer::readAhead(uint32_t frameDue) {
    uint8_t onTheWay = 0;
    for (const Read& r : reads) {
        if (r.room != nullptr && r.video == video) {
            ++onTheWay;
        }
    }
    if (nextRead < frameDue) {
        nextRead = frameDue;
    }
    while (onTheWay < READS_AT_ONCE) {
        // The first read tells the frames in the file, so the reads after it wait for it.
        if ((!counted && nextRead > 0) || (counted && !loop && nextRead >= loopStart + count)) {
            return;
        }
        const uint8_t slot = uint8_t(nextRead % FRAMES_AHEAD);
        if (holds[slot] != Hold::NOTHING) {
            return;
        }
        Read* r = nullptr;
        for (Read& candidate : reads) {
            if (candidate.room == nullptr && candidate.request.status() != FileStatus::PENDING) {
                r = &candidate;
                break;
            }
        }
        if (r == nullptr) {
            return;
        }
        const uint32_t inFile = counted ? nextRead % count : nextRead;
        holds[slot] = Hold::READING;
        held[slot] = nextRead;
        r->video = video;
        r->room = rooms[slot];
        r->slot = slot;
        file.read(inFile * frameBytes, rooms[slot], frameBytes, r->request);
        ++nextRead;
        ++onTheWay;
    }
}

void VideoPlayer::gone(lv_event_t* e) {
    VideoPlayer* self = static_cast<VideoPlayer*>(lv_event_get_user_data(e));
    if (lv_event_get_target(e) != self->image) {
        return;
    }
    // LVGL deletes the image itself, so end() lets go of it.
    self->image = nullptr;
    self->end();
}

void VideoPlayer::end() {
    if (!active) {
        return;
    }
    active = false;
    if (sound && soundStarted) {
        sounds.stopVideoSound();
    }
    sound = false;
    lv_obj_t* shown = image;
    image = nullptr;
    if (shown != nullptr) {
        lv_obj_delete(shown);
    }
    viewport = nullptr;
    slide = nullptr;
    lv_timer_t* refresh = lv_display_get_refr_timer(screen.lv);
    lv_timer_set_period(refresh, LV_DEF_REFR_PERIOD);
    lv_timer_resume(refresh);
    for (uint8_t i = 0; i < FRAMES_AHEAD; ++i) {
        // A room a read still lands in goes back once the read has landed.
        if (holds[i] != Hold::READING) {
            screen.pictures.give(rooms[i]);
        }
        rooms[i] = nullptr;
        holds[i] = Hold::NOTHING;
    }
}
