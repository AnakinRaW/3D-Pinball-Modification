// The drivers and components the test programs share, one instance of each, as the firmware's
// main.cpp holds them. The Teensy restarts before each program, so a program finds each of them
// constructed and not begun. A test that needs a driver never begun, or one whose begin() fails,
// keeps that further instance in its program. The drivers publish into the firmware's events.
// The classes are only named here, so a program includes the headers of those it uses.
#pragma once

class AudioDriver;
class Display;
class FileSystem;
class Lighting;
class Screen;
class Sounds;
class Touch;
class VideoPlayer;

namespace test {

extern AudioDriver audio;
extern FileSystem files;
extern Sounds sounds;
extern Display display;
extern Touch touch;
extern Screen screen;
extern VideoPlayer videoPlayer;
extern Lighting lighting;

}   // namespace test
