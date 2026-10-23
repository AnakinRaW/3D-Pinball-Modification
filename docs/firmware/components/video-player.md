# Video player component

The video player plays videos in a viewport on the game's background, which the [screen](screen.md) provides. The viewport is a rectangle that the game sizes and places. A video may bring its sound, which then plays through the [sounds](sounds.md) in place of the music.

## Playing

`play()` plays a video in a viewport. A viewport over the whole screen plays it fullscreen. The video lies in front of the game's background objects. The game's layer and the host's layer lie in front of every video, and LVGL draws them over each frame.

A video named `jackpot` is the file `video/jackpot.rgb` in the game's directory. With `sound`, its sound `audio/jackpot.wav` interrupts the music currently playing. With `loop`, the video and its sound repeat.

The video player shows each frame as an LVGL image. It reserves memory for one frame in the screen's [picture memory](screen.md#pictures) when the video starts and frees it when the video ends or stops. A full-screen frame takes 150 KB. When the picture memory is full, `play()` plays nothing and returns false.

The video player runs as an LVGL timer. The [screen](screen.md)'s `update()` runs it in every pass of the main loop. When a frame is due, the video player makes LVGL redraw the screen right away, without waiting for LVGL's own refresh. It then reads the next frame from the card in one go into its memory. While a video plays, LVGL's own refresh pauses. The screen is then redrawn only with each frame of the video. A change elsewhere on the screen, such as a new score, appears with the next frame. A video with sound takes its time from its sound. It shows the frame that belongs to how far the sound has played, so a sound that stalls holds the video with it. A video without sound takes its time from its start. A frame that the card brings late stays on the screen longer. The video then skips to the frame that is due.

One video plays at a time, and a new one replaces the one playing. `stop()` stops the video and its sound. When a video with sound ends or stops, the music it stopped before continues.

## End of a video

`playing()` tells whether the video last started still plays. When it has played to its end, the video player ends its sound and publishes `MovieEnded`. A video that loops never ends and publishes none. A video that the game stops with `stop()` or replaces with another one publishes none either.

The timer detects the end and publishes `MovieEnded` to the [event queue](../input-handling.md#concept), as a driver would. The host hands the event back to the video player's `translate()`, which passes it on to the game.

| Event | When | Payload |
|---|---|---|
| `MovieEnded` | The video last started has played to its end | None |

## Existing libraries

Raw frames need no decoder. Motion JPEG decoded with bitbank2's [JPEGDEC](https://github.com/bitbank2/JPEGDEC) cuts the card's traffic to about a tenth. Its decoding would run in the main loop for every frame. The card carries raw frames with room to spare, as the [display's appendix](../../parts/display/design.md#appendix-derivations) estimates.

An animated GIF holds at most 256 colors per frame. bitbank2's [AnimatedGIF](https://github.com/bitbank2/AnimatedGIF) and LVGL's `lv_gif` play such files. `lv_gif` also reads its file through LVGL's file interface in the main loop.

LVGL's `lv_lottie` plays vector animations, which it draws anew for every frame. It plays no recorded video. LVGL's `lv_ffmpeg` needs Linux.

Small animations of the interface step through pictures from the picture memory with LVGL's `lv_animimg`.

## Interface

```cpp
// the videos of the running game on the background, with their sound in place of the music;
// every call returns at once
class VideoPlayer : public Component {
public:
    VideoPlayer(Screen& screen, FileSystem& files, Sounds& sounds);

    void begin();           // attaches its queue to the event queue and its timer to LVGL, after the screen's begin()

    // hands the MovieEnded it published to the game
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    // plays video/<name>.rgb in the viewport at x, y on the background, over and over with loop,
    // with sound audio/<name>.wav in place of the music; false and nothing played for a viewport
    // beyond the screen or with the picture memory full
    bool play(const char* name, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool loop, bool sound);

    void stop();            // stops the video and its sound; the music goes on where it stopped
    bool playing() const;   // gets whether the video last started has neither ended nor been stopped

private:
    friend class GameHost;

    void reset();           // stops the video of the stopped game; the host calls it when it switches games
};
```
