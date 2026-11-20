// What FastLED takes from its platform, for the simulator. FastLED's time is the simulated clock,
// and its tasks find a runtime without coroutines. FastLED's WebAssembly platform needs threads,
// which the simulator does without.
// FastLED.h first, since FastLED's platform headers take their macros from it
#include <FastLED.h>
#include <platforms/coroutine_runtime.h>
#include <platforms/time_platform.h>
#include "core/clock.h"

namespace fl {
namespace platforms {

namespace {

constexpr fl::u32 US_PER_MS = 1000;

// a runtime with no coroutines, so pumping them does nothing
class NoCoroutines : public ICoroutineRuntime {
public:
    void pumpCoroutines(fl::u32) FL_NO_EXCEPT override {}
};

}   // namespace

fl::u32 millis() FL_NO_EXCEPT {
    return nowUs() / US_PER_MS;
}

fl::u32 micros() FL_NO_EXCEPT {
    return nowUs();
}

ICoroutineRuntime& ICoroutineRuntime::instance() FL_NO_EXCEPT {
    static NoCoroutines runtime;
    return runtime;
}

}   // namespace platforms
}   // namespace fl
