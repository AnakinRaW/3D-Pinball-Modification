// gets whether at least span microseconds have passed since t
inline bool elapsedUs(uint32_t t, uint32_t span) { return micros() - t >= span; }

// gets whether at least span milliseconds have passed since t
inline bool elapsedMs(uint32_t t, uint32_t span) { return millis() - t >= span; }

// gets whether time a lies before time b, also across the wrap of micros()
inline bool before(uint32_t a, uint32_t b) { return (int32_t)(a - b) < 0; }

EventQueue       events;
DriverTick       driverTick;             // the drivers attach to it in their begin()
DeviceMonitor    deviceMonitor;          // the drivers register with it in their begin()

// drivers
IrSensing        ir;
BreakBeam        drain;
SolenoidDriver   solenoids;
Storage          storage;
AudioDriver      audio;
Lighting         lights;
Display          display;
MagneticRotaryDriver rotary;
// other drivers

// game components
TopLanes         topLanes{ir, lights};   // a component, built on its drivers' parts
FileSystem       files{storage};         // the files of the games, the host and the logger
Sounds           sounds{audio};          // the running game's sounds
Screen           screen{display};        // the game's layer, background and movies, and the host's layer
Logger           logger{files};          // the machine's log on the card
// other game components

GameHost         host;
WDT_T4<WDT1>     watchdog;

extern uint8_t _ebss[], _estack[];          // PJRC's linker script: the end of the static variables, and the start of the stack
static StaticTask_t mainTask_;

static void machineTask(void*);             // the main task, below

namespace freertos { void setup_event_responder() {} }   // no timer for PJRC's MillisTimer

// blocks the 32 bytes at `at`, like PJRC's guard behind the static variables, so an overflowing
// interrupt stack crashes at once and the watchdog restarts the Teensy
static void guardStack(uintptr_t at) {
    SCB_MPU_RBAR = at | SCB_MPU_RBAR_REGION(15) | SCB_MPU_RBAR_VALID;   // 15 is free and wins over PJRC's RAM1 region
    SCB_MPU_RASR = SCB_MPU_RASR_TEX(0) | SCB_MPU_RASR_AP(0) | SCB_MPU_RASR_XN |
                   SCB_MPU_RASR_SIZE(4) | SCB_MPU_RASR_ENABLE;   // no access, 2^(4+1) = 32 bytes
    asm volatile("dsb");
    asm volatile("isb");
}

// the main task's stack is the rest of RAM1, between the guard behind the static variables and the
// guard below the interrupts' stack
void setup() {
    const uintptr_t bottom = reinterpret_cast<uintptr_t>(_ebss) + 32;
    const uintptr_t guard  = reinterpret_cast<uintptr_t>(_estack) - configMAIN_STACK_DEPTH - 32;   // 32-byte aligned
    guardStack(guard);
    xTaskCreateStatic(machineTask, "main", (guard - bottom) / sizeof(StackType_t), nullptr, 1,
                      reinterpret_cast<StackType_t*>(bottom), &mainTask_);
    vTaskStartScheduler();                  // sets every interrupt to priority 128 and never returns
}

void loop() {}                              // never runs, the main task carries the main loop

// the main task: sets the machine up, then runs the main loop
static void machineTask(void*) {
    NVIC_SET_PRIORITY(IRQ_TEMPERATURE_PANIC, 0);   // the core's priority, which the kernel's start reset
    NVIC_SET_PRIORITY(IRQ_GPIO6789, 96);    // sets the global priority of every pin interrupt

    // the watchdog before the first coil pulls, so a fault from here on ends in a restart; the idle
    // task feeds it while the start-up waits
    startWatchdog(watchdog);
    solenoids.begin();                      // pulls every coil once, as the stock machine does at power-on

    // storage next, so every later driver can read its settings from the card
    storage.begin();                        // starts the storage task and waits for the mount
    logger.begin();                         // finds where the log goes on, which needs the card
    logRestart(logger);                     // the cause of the last restart, error-handling.md

    // light ahead of ir for better calibration
    lights.begin();
    ir.begin();
    drain.begin();
   
    display.begin();
    rotary.begin();

    audio.begin();

    // other drivers...

    // every component, so the host routes its parts' driver events through it
    host.add(topLanes);
    host.add(sounds);
    host.add(screen);
    // other components...

    // the components, then the services no component owns
    static Machine machine{topLanes, files, sounds, screen, logger};
    host.begin(machine, display.touch());

    startingUp = false;                     // from here on only the main loop feeds the watchdog
    for (;;) {                              // the main loop
        DriverEvent batch[kBatch];
        const size_t n = events.read(batch, kBatch);
        for (size_t i = 0; i < n; ++i) host.dispatch(batch[i]);

        const uint32_t now = micros();
        host.update(now);
        logger.update(now);                 // writes the collected log lines once a second
        screen.update();                    // shows a picture once it has landed

        checkWatchdog();                    // feeds the watchdog while every guard holds
    }
}
