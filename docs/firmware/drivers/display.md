# Display driver

The driver puts the picture on the display that [`docs/parts/display/design.md`](../../parts/display/design.md) describes. The [screen](../components/screen.md) and the [video player](../components/video-player.md) components build on it.

## Picture composition

The picture is 320 x 240 pixels. It is composed of three layers. From the front, they are `Front`, `Middle` and `Back`. Each pixel shows the front-most layer that has a colour there. A layer can be hidden. Where no layer has a colour, the pixel is black.

Each layer keeps a table of 256 colours. This table is the layer's palette. An entry of the palette holds a 16-bit RGB565 colour and an opacity in 32 steps. A pixel of the layer stores the number of its entry in the palette. This number is the pixel's index and takes one byte. Index 0 means transparent. Behind a partly opaque colour, the layers further back shine through.

Movies play inside a viewport on the back layer. The viewport is a rectangle that can be sized and placed. Outside the viewport, the back layer keeps showing its own pixels.

## Sending

The driver combines the three layers and a playing movie into one picture while it sends it.

The display keeps showing what it received last. The driver therefore sends only the rows that changed. A component marks its changed rows with `show()`. A row that changes again while it is being sent goes out a second time, so no change is lost.

Changing a palette colour does not update the pixels on the screen until `show()` sends the rows with that colour again.

## Movies

A fullscreen movie's viewport is the whole screen. Hiding the back layer hides the movie as well. The viewport's left edge and its width go in steps of 16 pixels, since the driver fills the bus 16 pixels at a time.

A movie is a file of raw frames, each as large as its viewport. Every pixel is two bytes of RGB565, high byte first. Every movie plays at 25 frames a second. [`media.md`](../../../assets/media.md) shows how FFmpeg makes such a file.

`play()` takes the movie as a `FileReader` and copies it. The caller may drop its `FileReader` right after the call. The driver reads the movie through a [file channel](../file-channel.md) using normal priority. A movie that loops reads on from the start of its file. A movie ends at once when its file is missing.

`play()` starts nothing and returns 0 when the left edge or the width of the viewport is no multiple of 16. The same holds for a viewport that reaches past the screen. A movie that is already playing goes on.

When a movie ends or is stopped, the back layer shows its own pixels in the viewport again.

## Start-up

`begin()` waits until 285 ms after power-up, as [`design.md`](../../parts/display/design.md#appendix-derivations) derives. It then resets the panel by command, since a restart of the Teensy can find the panel awake. It sets the panel to RGB565 in landscape and ends with a black screen.

## Performance & Memory Usage

Sending the whole combined picture once takes 12.8 ms.

While the driver sends, it takes an estimated 21.9 % of the processor. Partly opaque colours on the front and the middle layer raise this to 61.9 % at most. The sending keeps its speed even then.

The finished picture exists only on the display. No buffer in the Teensy holds it. The three layers take 75 KB each. The movie needs a buffer of 64 KB for the frames read ahead from the card. In all, the driver takes 289 KB of RAM2.

## Driver events

| Type | When | Payload |
|---|---|---|
| `MovieEnded` | A movie's last row has gone out, its file is missing, or a read of it has failed | The number `play()` returned |

The source is always `0`. The timestamp is the moment the movie ended. A full queue drops the event.

## Device faults

The panel never answers, since its read line RD is tied high. The driver therefore cannot tell whether the panel works and reports no fault.

## Existing libraries

[ILI9341_t3](https://github.com/PaulStoffregen/ILI9341_t3) and [ILI9341_T4](https://github.com/vindar/ILI9341_T4) drive the ILI9341 over SPI. This build wires the panel to the 8-bit bus.

[ILI948x_t41_p](https://github.com/david-res/ILI948x_t41_p) drives a panel on the 8-bit bus over FlexIO3, but it is written for the ILI9488 and sends from a whole picture in RGB565. That picture takes 150 KB, which RAM2 does not have left.

## Driver

```cpp
class Display : public Driver {
public:
    enum class Layer : uint8_t { Front, Middle, Back };   // from the front; Back shows a playing movie inside its viewport

    // sets up FlexIO3 and the panel and attaches to the driver tick; false when it is full
    bool begin() override;

    uint8_t* pixels(Layer layer);                            // gets a layer's palette indices, row by row
    void     palette(Layer layer, uint8_t index, uint16_t rgb565, uint8_t opacity);   // sets a colour of a layer's palette, opacity 0 to 255 in 32 steps
    void     palette(Layer layer, const uint8_t* entries);    // sets all 256 entries of a layer's palette from a picture file's table, entry 0 unused
    uint16_t colour(Layer layer, uint8_t index) const;       // gets the RGB565 colour of a palette entry, the back layer's with its opacity folded in
    bool     opaque(Layer layer, uint8_t index) const;       // gets whether a palette entry covers what lies behind it, every entry of the back layer
    void     show(uint16_t top, uint16_t bottom);            // marks rows top to bottom - 1 as drawn; returns at once
    void     shown(Layer layer, bool shown);                 // shows or hides a layer and marks every row

    uint32_t play(const FileReader& movie, uint16_t x, uint16_t y, uint16_t width, uint16_t height, bool loop);   // starts a movie in the viewport at x, y on the back layer; returns its number at once, 0 for a viewport off the 16-pixel steps or beyond the screen
    void     stopMovie();                                    // stops the movie; returns at once
    bool     moviePlaying() const;                           // gets whether a movie plays
};
```
