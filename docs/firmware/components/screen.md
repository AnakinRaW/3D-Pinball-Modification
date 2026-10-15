# Screen component

The screen component hands out the layers of the display as canvases to draw on. A game gets two layers: a background layer and an overlay layer. The host gets a third one for the menu and for its pages on faults and debugging. The host layer lies on top of the other layers. 

The screen component marks every changed row for the [display driver](../drivers/display.md). The driver sends only these rows to the display. The screen component also loads pictures from the card into a layer and turns touches into taps and holds.

## Layers

Each layer of the screen is one layer of the [display driver](../drivers/display.md#picture-composition). The host's layer is `Front`. The game's layer is `Middle`. The game's background is `Back`. A video plays in a viewport on the background through the [video player](video-player.md).

`layer()` gets the game's layer. `background()` gets the game's background. `showLayer()` shows or hides the game's layer. A hidden layer keeps its pixels. A game may go on drawing into it. The drawing appears once the layer is shown again.

The host gets its own layer with `hostLayer()`. `showHost()` shows or hides it.

When the host starts a game, the component clears the stopped game's layer and background. The new game starts with its layer shown. The host starts the new game once the clear is done.
    
## Drawing

A `Canvas` stands for one layer. It is an [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library) canvas, so it draws text, lines, rectangles, circles and bitmaps. Every pixel of a canvas holds a palette index. Index 0 is transparent. `palette()` sets the 255 colors a layer uses besides index 0. Each color also has an opacity from 0 to 255. The display shows the opacity in 32 steps. A partly opaque color lets what lies behind it shine through. `nearest()` gets the index of the layer's opaque color closest to a color. A game finds the color of its text this way.

Drawing changes the layer in memory. `show()` brings the changes to the screen and returns at once. A game first draws everything that changes and then calls `show()` once. A new color from `palette()` also appears with the next `show()`.

A game loads its background picture once and draws only what changes into its own layer. Where the game erases its layer back to 0, the background shows through.

## Pictures

A picture is a file on the card, opened through the [file system](file-system.md). It starts with 256 palette entries. An entry takes four bytes: the color in RGB565 with its high byte first, its opacity and an unused byte. Entry 0 holds the width and the height instead, each in two bytes with the high byte first. Then come the pixels row by row from the top left, one byte each with the pixel's palette index. The pictures of one layer share one palette. [`media.md`](../../../assets/media.md) shows how the tool makes them from PNGs.

`load()` reads a full-screen picture into a layer with its colors and returns at once. `update()` shows it once it has been read from the card. Until then, `loading()` is true and drawing into the layer is lost, because the picture overwrites it. A second `load()` replaces a picture still on its way. While a picture lands, the screen can briefly show it in the layer's old colors. A file that is missing, short or smaller than the screen leaves the layer empty.

`colors()` gives a layer only the palette of a picture file. A layer of smaller pictures and text gets its colors this way. The palette loads like a picture.

A smaller picture is read once into a `Picture` and drawn from there as often as needed. The game gives the `Picture` a buffer with room for the pixels. `draw()` puts the picture into a layer at a position, in the layer's colors. Its transparent pixels leave the layer as it is. Parts outside the layer are cut off. While the picture loads, `draw()` draws nothing, so a game loads its pictures in `start()` and waits for them in its intro. A file that is missing, short or larger than the buffer draws nothing.

## Touch

The component turns the touch driver's events into gestures. A tap is a touch that leaves the screen within 500 ms and within 10 pixels of where it came down. A hold is a finger resting for 1 s, which the touch driver reports as `TouchHeld`. `finger()` gives where a finger touches.

A touch sinks from the front layer to the back. A hidden layer lets it pass. A shown layer takes it when one of its controls from the [UI control library](screen-architecture.svg) lies under the finger. A modal layer takes every touch, as the host's layer does while its menu is open. The first layer that takes a touch keeps it until the finger leaves the screen. A touch that no layer takes becomes a tap or a hold, which reaches the game as a pinball event. The host also takes every tap and hold with `takeGesture()`, so it can react to a gesture over a running game.

| Event | When | Payload |
|---|---|---|
| `ScreenTapped` | A tap that no layer took | x in the lower 16 bits and y in the upper 16 |
| `ScreenHeld` | A hold that no layer took | x and y as for `ScreenTapped` |

## Interface

```cpp
// a picture smaller than the screen, read once into a buffer the game gives and then drawn as
// often as needed into a layer with the colors of its set
class Picture {
public:
    template <size_t N>
    explicit Picture(uint8_t (&pixels)[N]);  // the buffer, with room for N pixels

    void    load(const FileReader& picture); // reads a picture file into the buffer and returns at once; replaces one still on its way
    bool    loading() const;                 // gets whether the picture still loads, and starts a load that waits for the one before
    int16_t width() const;                   // gets the picture's width, 0 while it loads and when the file is missing, short or too large
    int16_t height() const;                  // gets the picture's height, 0 as for width()
};

// a layer of the picture as an Adafruit_GFX canvas of palette indices, index 0 transparent
class Canvas : public Adafruit_GFX {
public:
    Canvas(Display& display, Display::Layer layer);

    void drawPixel(int16_t x, int16_t y, uint16_t index) override;                  // sets one pixel's palette index
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t index) override;   // fills a rectangle row by row
    void fillScreen(uint16_t index) override;                                        // fills the whole layer
    void draw(const Picture& picture, int16_t x, int16_t y);                            // draws a picture with its top left corner at x, y; its transparent pixels leave the layer as it is
    void palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t opacity = 255);   // sets a color of the layer's palette, 255 opaque, and marks every row
    void show();                                                                     // hands the rows drawn since the last show() to the driver
    void load(const FileReader& picture);                                            // reads a full-screen picture file into the layer with its colors and returns at once
    void colors(const FileReader& picture);                                         // takes the palette of a picture file for the layer and returns at once

    bool    loading() const;                                                         // gets whether a picture or a palette still loads
    uint8_t nearest(uint8_t r, uint8_t g, uint8_t b) const;                          // gets the index of the layer's opaque color closest to r, g, b, 0 when it has none
};

// the picture: the game's layer and background, and the host's layer over them
class Screen : public Component {
public:
    Screen(Display& display, Touch& touch);

    // takes every touch event; a touch sinks from the front layer to the back, and one that no
    // layer takes becomes ScreenTapped or ScreenHeld
    bool translate(const DriverEvent& in, PinballEvents& out) override;

    // shows a picture once it has landed, clears a layer once a picture still on its way has, and
    // redraws the controls that changed; the main loop calls it in every pass
    void update();

    Canvas& layer();                         // gets the game's layer
    Canvas& background();                    // gets the game's background, which a playing video covers inside its viewport
    void    showLayer(bool shown);           // shows or hides the game's layer; hidden, it keeps what is drawn
    View&   view();                          // gets the controls on the game's layer

    Finger finger() const;                   // gets where a finger touches the screen

private:
    friend class GameHost;

    enum class Gesture : uint8_t { None, Tap, Hold };

    Canvas& hostLayer();                     // gets the host's layer
    View&   hostView();                      // gets the controls on the host's layer
    bool    settled() const;                 // gets whether the game's layer and background neither load nor wait for a clear
    void    showHost(bool shown);            // shows or hides the host's layer
    Gesture takeGesture(uint16_t& x, uint16_t& y);   // gets the host the last gesture once, None when there is none
    void    reset();                         // clears the game's layer and background, and shows the game's layer
};
```
