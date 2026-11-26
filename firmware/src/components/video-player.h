// Plays videos in a viewport on a slide of the game, with their sound over the music. A video is a
// file of raw RGB565 frames at 25 frames a second. The card reads one second of frames ahead into
// the screen's picture memory, so a card that stalls while it writes leaves the picture running.
// A video with sound takes its time from how far its sound has played, so picture and sound stay
// together.
//
// Design: docs/firmware/components/video-player.md.
#pragma once
#include "components/file-system.h"
#include "components/screen.h"
#include "components/sounds.h"
#include "game/component.h"

class VideoPlayer : public Component {
public:
    static constexpr uint8_t FRAMES_AHEAD = 25;   // display model: n_video_ahead

    /// Creates the video player of a screen, which plays the game's files and sounds.
    VideoPlayer(Screen& screen, FileSystem& files, Sounds& sounds)
        : screen(screen), files(files), sounds(sounds) {}

    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;

    /// Attaches the video player to LVGL, whose timers the screen's render() runs.
    ///
    /// A second call does nothing.
    ///
    /// @note  The main task calls it at the start, after the screen's begin().
    void begin();

    /// Takes no driver event.
    ///
    /// @return  Always false.
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    /// Hands the game MOVIE_ENDED once a video has played to its end, once.
    ///
    /// @return  False when no video has ended since the last call.
    bool update(uint32_t now, PinballEvents& out) override;

    /// Plays a video in a viewport and returns at once.
    ///
    /// The video takes the viewport's size and place at the call and lies inside it, in front of
    /// the objects created before the viewport's content and behind those created after. It
    /// replaces the video that plays. With @p sound, its sound plays over every track of the game,
    /// which waits paused meanwhile.
    ///
    /// @param viewport  An LVGL object on the slide that shows.
    /// @param name      The video's name. "jackpot" plays video/jackpot.rgb of the game's directory
    ///                  and, with @p sound, audio/jackpot.wav.
    /// @param loop      Plays the video and its sound over and over, so it never ends.
    /// @param sound     Plays the video's sound with it.
    /// @return          False, with nothing played and the video that plays left playing, for a
    ///                  viewport on a hidden slide, on no slide, empty or reaching beyond the
    ///                  screen, and when the picture memory has no room for one second of frames.
    /// @note            The main loop calls it.
    bool play(lv_obj_t* viewport, const char* name, bool loop, bool sound);

    /// Stops the video in a viewport and its sound.
    ///
    /// The paused track goes on. A stopped video makes no MOVIE_ENDED.
    ///
    /// @param viewport  The video's viewport. Any other object is left alone.
    void stop(const lv_obj_t* viewport);

    /// Gets whether a video plays in a viewport, neither ended nor stopped.
    bool playing(const lv_obj_t* viewport) const;

private:
    static constexpr uint32_t FRAME_RATE_HZ = 25;   // display model: f_movie

    friend class GameHost;

    static constexpr uint32_t US_PER_S = 1000000;
    static constexpr uint32_t FRAME_US = US_PER_S / FRAME_RATE_HZ;
    static constexpr uint32_t SOUND_RATE_HZ = 44100;   // audio model: f_s
    static constexpr uint32_t FRAME_SAMPLES = SOUND_RATE_HZ / FRAME_RATE_HZ;
    /// The reads of one video on their way at once, so the card starts the next frame as soon as
    /// it ends one
    static constexpr uint8_t READS_AT_ONCE = 2;
    /// The reads the player carries, so a replaced video's reads can still land
    static constexpr uint8_t READS = 2 * READS_AT_ONCE;
    static constexpr size_t NAME_BYTES = 32;   // a video's name, which a path has room for
    /// The frames that land before the video's sound starts, so the sound starts with a picture
    /// to follow: four frames take the card 72 ms, display model: t_card_frame
    static constexpr uint32_t START_FRAMES = 4;

    /// What a room of the read-ahead holds.
    enum class Hold : uint8_t { NOTHING, READING, FRAME, SHOWN };

    /// A read of a frame into a room.
    struct Read {
        FileRequest request{FilePriority::RAISED};
        uint32_t video = 0;        // the play() the read belongs to
        uint8_t* room = nullptr;   // where it lands
        uint8_t slot = 0;          // the room's place in the read-ahead
    };

    /// Stops the video of the stopped game. The host calls it when it switches games.
    void reset();

    /// Takes the landed reads, shows the frame that is due and starts the next reads.
    ///
    /// @note  LVGL's timer calls it in every pass of the main loop.
    void step(uint32_t now);

    static void tick(lv_timer_t* t);

    /// Ends the video once its image is deleted with its viewport or its slide.
    static void gone(lv_event_t* e);

    /// Gets the frame due now, counted from the video's start across its loops.
    uint32_t due(uint32_t now);

    /// Takes a read that has landed.
    void land(Read& r);

    /// Starts the reads of the frames ahead.
    void readAhead(uint32_t now);

    /// Shows the latest frame that has landed and is due, and frees the frames it passes.
    void show(uint32_t now);

    /// Ends the video and its sound and gives its rooms back, a room a read still lands in once the
    /// read has landed.
    void end();

    Screen& screen;
    FileSystem& files;
    Sounds& sounds;
    lv_timer_t* timer = nullptr;
    Read reads[READS];
    uint8_t* rooms[FRAMES_AHEAD] = {};   // the read-ahead in the picture memory
    uint32_t held[FRAMES_AHEAD] = {};    // the frame each room holds or receives
    Hold holds[FRAMES_AHEAD] = {};
    lv_image_dsc_t frame = {};   // the frame on the screen
    lv_obj_t* image = nullptr;   // inside the viewport, filling it
    lv_obj_t* viewport = nullptr;
    lv_obj_t* slide = nullptr;   // the slide the viewport lies on, which owns the video
    FileReader file;
    char wav[NAME_BYTES + sizeof(".wav")] = {};
    uint32_t video = 0;   // counts the plays, so a read of a stopped video is known
    uint32_t frameBytes = 0;
    uint32_t count = 0;        // the frames in the file, once the first read has landed
    uint32_t loopStart = 0;    // the frame the running pass started at
    uint32_t soundFrame = 0;   // the frame of its pass the sound stood at last
    uint32_t landed = 0;       // the frames of the video that have landed whole
    uint32_t nextRead = 0;     // the next frame to read
    uint32_t shownFrame = 0;   // the frame on the screen
    uint32_t startUs = 0;      // nowUs() when frame loopStart was due, by the clock
    bool active = false;       // a video plays
    bool counted = false;      // count is known
    bool anyShown = false;     // a frame of the video is on the screen
    bool loop = false;
    bool sound = false;
    bool soundStarted = false;   // the video's sound has been handed to the sounds
    bool endedDue = false;       // a video has ended, and update() has not handed it out yet
    PinballEvent ended = {};
};
