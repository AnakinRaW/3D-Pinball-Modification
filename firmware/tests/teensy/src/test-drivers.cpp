// The instances of test-drivers.h, made in the order of the firmware's main.cpp.

#include "test-drivers.h"
#include "components/file-system.h"
#include "components/screen.h"
#include "components/sounds.h"
#include "components/video-player.h"
#include "core/event-queue.h"
#include "drivers/audio.h"
#include "drivers/break-beam.h"
#include "drivers/display.h"
#include "drivers/lighting.h"
#include "drivers/storage.h"
#include "drivers/touch.h"
#include "io/i2c-bus.h"

namespace test {

AudioDriver audio(events);
FileSystem files(storage);
Sounds sounds(audio, files);
Display display;
Touch touch(io::wire2Bus, events, driverTick);
Screen screen(display);
VideoPlayer videoPlayer(screen, files, sounds);
Lighting lighting;
BreakBeamDriver breakBeam(events);

}   // namespace test
