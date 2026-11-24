# Slide test

The test shows whether slides described in YAML work on the screen. It runs in the Arcade, with the firmware's screen code compiled to WebAssembly. The format is the one [`game-files.md`](../firmware/game-files.md#screen) gives. The test is the third step of the first stage of [`game-files-plan.md`](game-files-plan.md#step-3-the-slides).

## Step 1: the slide class

- Widget structs that mirror the entries of `game.yml` one to one: `label`, `image`, `video`, `animation` and `rect`. Each holds its place and its size. `animate` names a property, a start, a target and a time.
- A class that builds LVGL objects from a list of widgets, on `slide(layer)` or on `overlay()`. When its mode ends, it deletes its overlay widgets and releases its pictures.
- Placeholders such as `"{score}"` bind to variables through LVGL's subjects. A narrow interface maps a name to a subject until the variable store exists.
- The labels take LVGL's built-in font.
- Computer tests beside `test_screen` with its fake display, and their rows in [`testing.md`](../firmware/testing.md).

## Step 2: test YAMLs

1. The score as a bound label that counts up.
2. A picture with a text that fades in.
3. A video with a caption in front of it.
4. A sequence of pictures.

## Step 3: the screen in the Arcade

The WebAssembly module in `firmware/sim` takes LVGL, the screen component, the picture memory, the video player and the file system on a card in memory. A simulated display driver copies LVGL's rectangles into a frame of 320 × 240 pixels, which the Arcade draws into a canvas. The whole game needs the same view later.

## Step 4: test program in the module

The program reads the test YAMLs with libyaml, fills the structs from them and shows the slides one after another while the score counts up. This reading serves the test alone. The game's loader reads through the [config component](../firmware/components/config.md)'s tree, and the test program goes once that loader exists. Since the program is C++, it runs on the Teensy unchanged.

## Step 5: evaluation

What works and what fails on the screen goes into `game-files.md` and into the loader's design.
