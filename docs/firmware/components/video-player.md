# Video player component

The video player plays videos in a viewport on a [slide](screen.md#slides) of the game. The viewport is an LVGL object that the game creates on the slide, sizes and places. A video may bring its sound, which then plays through the [sounds](sounds.md) over the music.

## Playing

`play()` plays a video in a viewport. The video takes the viewport's size and place when it starts. A viewport over the whole screen plays it fullscreen. LVGL draws the objects of a slide in the order the game created them. The video is drawn inside its viewport. Objects created before the viewport lie behind the video. A background larger than the viewport shows around the video. Objects created after the viewport lie in front of the video, such as a caption or a frame over its edge. The game overlay layer and the host's layer lie in front of every video. LVGL draws them over each frame.

A video named `jackpot` is the file `video/jackpot.rgb` in the game's directory. With `sound`, its sound `audio/jackpot.wav` plays as a [track](sounds.md#music) over every track of the game. The track that played is paused meanwhile. With `loop`, the video and its sound repeat.

The video player shows each frame as an LVGL image. It reserves memory for two frames in the screen's [picture memory](screen.md#pictures) when the video starts and frees it when the video ends or stops. A full-screen frame takes 150 KB. When the picture memory is full, `play()` plays nothing and returns false.

The video player runs as an LVGL timer. The [screen](screen.md)'s `render()` runs it in every pass of the main loop. The card reads each frame in one go into one of the two frames, while LVGL draws the frame in the other. When a frame is due, the video player makes LVGL redraw the screen right away, without waiting for LVGL's own refresh. The frame LVGL has drawn from then takes the next read. While a video plays, LVGL's own refresh pauses. The screen is then redrawn only with each frame of the video. A change elsewhere on the screen, such as a new score, appears with the next frame. A video with sound takes its time from its sound. It shows the frame that belongs to how far the sound has played, so a sound that stalls holds the video with it. When the sound ends before the video, the video switches to the clock and plays on from the frame it shows. A video without sound takes its time from its start. A frame that the card brings late stays on the screen longer. The video then skips to the frame that is due.

## Owner

A video belongs to the slide its viewport lies on. It plays only while that slide shows. `play()` plays nothing and returns false for a viewport on a hidden slide or on no slide. When another slide shows in place of the video's slide, the video stops with its sound. A mode that wants its video back starts it again once its slide shows. Objects and layers in front of the video leave it playing, however much of it they hide. When the viewport is deleted or its slide removed, the video stops as well. It then ends its sound and frees its frame in the picture memory.

One video plays at a time. Only the slide that shows plays one, so a new video replaces only a video of the same slide. `stop()` stops the video in a viewport and its sound. When a video with sound ends or stops, its sound ends with it. The paused track then resumes.

## End of a video

`playing()` tells whether a video still plays in a viewport. When it has played to its end, the video player ends its sound and makes `MovieEnded`. A video that loops never ends and makes none. A video that is stopped or replaced makes none either. The same holds for a video whose slide is hidden.

The timer detects the end. The video player hands `MovieEnded` to the game in its next `update()`, as every [component with timed work](../game-abstraction.md#playfield-components) does.

| Event | When | Payload |
|---|---|---|
| `MovieEnded` | A video has played to its end. Its source is the key of the mode that holds its slide | None |

## Existing libraries

Raw frames need no decoder. Motion JPEG decoded with bitbank2's [JPEGDEC](https://github.com/bitbank2/JPEGDEC) cuts the card's traffic to about a tenth. Its decoding would run in the main loop for every frame. The card carries raw frames with room to spare, as the [display's appendix](../../parts/display/design.md#appendix-derivations) measures.

An animated GIF holds at most 256 colors per frame. bitbank2's [AnimatedGIF](https://github.com/bitbank2/AnimatedGIF) and LVGL's `lv_gif` play such files. `lv_gif` also reads its file through LVGL's file interface in the main loop.

LVGL's `lv_lottie` plays vector animations, which it draws anew for every frame. It plays no recorded video. LVGL's `lv_ffmpeg` needs Linux.

Small animations of the interface step through pictures from the picture memory with LVGL's `lv_animimg`.

## Interface

```cpp
// the videos of the running game on its slides, with their sound over the music;
// every call returns at once
class VideoPlayer : public Component {
public:
    VideoPlayer(Screen& screen, FileSystem& files, Sounds& sounds);

    void begin();           // attaches its timer to LVGL, after the screen's begin()

    // hands the game the MovieEnded that the timer found, once
    bool update(uint32_t now, PinballEvents& out) override;

    // plays video/<name>.rgb in viewport, an LVGL object on the slide that shows, at the viewport's
    // size and place, over and over with loop, with sound audio/<name>.wav over the music;
    // false and nothing played for a viewport on a hidden slide, on no slide or beyond the screen,
    // or with the picture memory full
    bool play(lv_obj_t* viewport, const char* name, bool loop, bool sound);

    void stop(const lv_obj_t* viewport);            // stops the video in viewport and its sound; the paused music resumes
    bool playing(const lv_obj_t* viewport) const;   // gets whether a video plays in viewport, neither ended nor stopped

private:
    friend class GameHost;

    void reset();           // stops the video of the stopped game; the host calls it when it switches games
};
```
