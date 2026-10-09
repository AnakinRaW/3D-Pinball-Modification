// gets the microseconds GPT2 has counted since setup() started it
inline uint32_t nowUs() { return GPT2_CNT; }

// gets whether at least span microseconds have passed since t, a time from nowUs()
inline bool elapsedUs(uint32_t t, uint32_t span) { return nowUs() - t >= span; }

// gets whether at least span milliseconds have passed since t, a time from nowUs()
inline bool elapsedMs(uint32_t t, uint32_t span) { return nowUs() - t >= span * 1000; }

// gets whether time a lies before time b, also across the wrap of nowUs()
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
Watchdog         watchdog;               // the i.MX RT's WDOG1, error-handling.md

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

// starts GPT2 counting microseconds from the 24 MHz crystal, free-running across 2^32. Its clock
// gate is off after a reset, and a register access before the gate is on stops the Teensy
static void startClock() {
    CCM_CCGR0 |= CCM_CCGR0_GPT2_BUS(CCM_CCGR_ON) | CCM_CCGR0_GPT2_SERIAL(CCM_CCGR_ON);
    GPT2_CR = 0;
    GPT2_PR = GPT_PR_PRESCALER24M(7) | GPT_PR_PRESCALER(2);   // 24 MHz / 8 / 3 = 1 MHz
    GPT2_CR = GPT_CR_EN_24M | GPT_CR_CLKSRC(5) | GPT_CR_FRR | GPT_CR_WAITEN | GPT_CR_ENMOD;
    GPT2_CR |= GPT_CR_EN;                   // ENMOD clears the counter as it starts
}

// the main task's stack is the rest of RAM1, between the guard behind the static variables and the
// guard below the interrupts' stack
void setup() {
    startClock();                           // before anything takes a time
    const uintptr_t bottom = reinterpret_cast<uintptr_t>(_ebss) + 32;
    const uintptr_t guard  = reinterpret_cast<uintptr_t>(_estack) - configMAIN_STACK_DEPTH - 32;   // 32-byte aligned
    guardStack(guard);
    xTaskCreateStatic(machineTask, "main", (guard - bottom) / sizeof(StackType_t), nullptr, 1,
                      reinterpret_cast<StackType_t*>(bottom), &mainTask_);
    vTaskStartScheduler();                  // sets every interrupt to priority 128 and never returns
}

void loop() {}                              // never runs, the main task carries the main loop

static volatile bool startingUp = true;     // until the main loop runs

// FreeRTOS's idle task calls it while the other tasks wait: feeds the watchdog until the main loop
// runs. freertos-teensy v11.2.0_v4 sets configUSE_IDLE_HOOK 1 and defines a weak, empty hook
extern "C" void vApplicationIdleHook() {
    if (startingUp) watchdog.check();
}

// the main task: sets the machine up, then runs the main loop
static void machineTask(void*) {
    NVIC_SET_PRIORITY(IRQ_TEMPERATURE_PANIC, 0);   // the core's priority, which the kernel's start reset
    NVIC_SET_PRIORITY(IRQ_GPIO6789, 96);    // sets the global priority of every pin interrupt

    // the watchdog before the first coil pulls, so a fault from here on ends in a restart; the idle
    // task feeds it while the start-up waits
    watchdog.begin();
    solenoids.begin();                      // pulls every coil once, as the stock machine does at power-on

    // storage next, so every later driver can read its settings from the card
    storage.begin();                        // starts the storage task and waits for the mount
    logger.begin();                         // finds where the log goes on, which needs the card
    watchdog.report(logger);                // the cause of the last restart, which needs the log

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

        const uint32_t now = nowUs();
        host.update(now);
        logger.update(now);                 // writes the collected log lines once a second
        screen.update();                    // shows a picture once it has landed

        watchdog.check();                   // feeds the watchdog while every guard holds
    }
}
