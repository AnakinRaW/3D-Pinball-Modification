# Video player component

The video player plays videos in a viewport on the back layer the [display](../drivers/display.md#movies) provides. The viewport is a rectangle that the game sizes and places. A video may bring its sound, which then plays through the [sounds](sounds.md) in place of the music.

## Playing

`play()` plays a video in a viewport. A viewport over the whole screen plays it fullscreen. The viewport's left edge and its width go in steps of 16 pixels. For a viewport off those steps or beyond the screen, `play()` returns false and plays nothing. The game's layer and the host's layer lie in front of every video.

A video named `jackpot` is the file `video/jackpot.rgb` in the game's directory. With `sound`, its sound `audio/jackpot.wav` interrupts the music currently playing. With `loop`, the video and its sound repeat.

One video plays at a time, and a new one replaces the one playing. `stop()` stops the video and its sound. When a video with sound ends or stops, the music it stopped before continues.

## End of a video

`playing()` tells whether the video last started still plays. When it has played to its end, the video player sends `MovieEnded` and ends its sound with it. A video that loops never ends and sends none.

| Event | When | Payload |
|---|---|---|
| `MovieEnded` | A video has played to its end | The number the driver gave the video |

## Interface

```cpp
// the videos of the running game on the background, with their sound in place of the music;
// every call returns at once
class VideoPlayer : public Component {
public:
    VideoPlayer(Display& display, FileSystem& files, Sounds& sounds);

    // turns the end of the video last started into MovieEnded and ends its sound with it
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    // plays video/<name>.rgb in the viewport at x, y on the background, over and over with loop, with
    // sound audio/<name>.wav in place of the music; false and nothing played for a viewport whose x or
    // width is no multiple of 16 or that reaches past the screen
    bool play(const char* name, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool loop, bool sound);

    void stop();            // stops the video and its sound; the music goes on where it stopped
    bool playing() const;   // gets whether the video last started has neither ended nor been stopped

private:
    friend class GameHost;

    void reset();           // stops the video of the stopped game; the host calls it when it switches games
};
```
