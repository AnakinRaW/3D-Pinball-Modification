# Game abstraction layer

The games are kept apart from the machine they run on. A game holds the rules of play, such as what a lane is worth or how long the ball save lasts. It sees the playfield only through components, such as the lanes, the scoop and the drain. Between the games and the machine sits the host, which lets one firmware carry several games. It gives every game the same interface to the machine and runs each through the same lifecycle. The host also carries the menu, where the player picks a game. A new game therefore needs no change to a driver, and every game reuses the same components.

## A game

A game holds all the rules and conditions for the player to collect points.

A game is driven two ways. `event()` is the game's handler for every event of a game component, and for the events of a service such as the controls driver's `ToggleSwitched`. `update()` runs once per pass of the main loop and gets the current time. Both return as soon as their work is done, since [nothing in the loop may block](general-design.md#execution-model).

A game may bring its own files on the SD card, under `/games/<id>`.

A `Machine` typed struct is passed to the game, allowing it to access game components and hardware devices.

## Host

The host runs the games and is responsible for switching and starting them. It hands every game the same `Machine`, passes it the components' events and the current time, and moves it through its [lifecycle](#game-lifecycle). At start-up it reads which game ran last, and its first `update()` starts that game or a default game. Once the running game has finished its post-game phase, the host starts it anew.

The host performs the same sequence at every start of a game, machine power-up included:

1. It stops the running game and runs a clean-up procedure.
2. It writes the id of the new game.
3. It prepares the new game by initializing things such as audio, display or lighting.
4. It starts the new game once the screen has cleared the stopped game's objects and pictures.

A ball still on the playfield carries on in the new game.

The bumpers fire only while a game is in its [Play](#game-lifecycle) phase. The host arms the contacts of the [solenoid driver](drivers/solenoid.md) when the phase begins and disarms them when it ends.

The host also manages the user menu on the touch display. There the player switches games or configures the machine.

The host can provide what all games share, such as settings the player changes in the menu. A game then needs no code for them.

The host catches every [device fault](error-handling.md#device-faults) and keeps the list of failed devices and handles them itself. Components and games should not see a fault. When the touch controller recovers, the host also calls `restart()` of the [display driver](drivers/display.md#device-faults), because the panel was reset with it.

## Game lifecycle

A game is organized into phases and the host is responsible for their transition. The game signals that it is ready for such a phase transition.

| Phase | What the game does | Begins with | Ends with |
|---|---|---|---|
| Stopped | gets no calls | boot, or `stop()` | `start()` |
| Intro | sets itself up afresh and runs its intro through `update()`, such as emptying the scoop and playing an intro sound | `start()`, which hands it the machine | `done()` returning true, such as once the intro sound has ended with the scoop empty. The host then calls `play()` |
| Play | plays through `event()` and `update()` | `play()` | `done()` returning true, for instance once its last ball drains. The host then calls `end()` |
| Post-game | shows its score and saves its high score | `end()` | `done()` returning true. The host then stops the game and starts it anew |

## Modes

A game splits its rules into modes. A mode is a part of the rules that runs for a while, such as a skill shot after the launch, a ball save or a multiball. The game starts it. It ends when the game stops it or when it has done its part. A base mode holds the rules that last the whole of Play, such as the score and the next ball.

The game keeps the modes that run in a mode stack. All modes in the stack run at the same time. Each mode has a priority, which sets its place in the stack. The place matters only where two modes want the same event: the higher one decides first whether it takes it. Between equal priorities the mode started later lies higher. The stack is a [priority list](general-design.md#priority-lists). A mode cleans up after itself: its `stop()` removes what it started.

The game hands every event to its modes from the highest priority down. A mode that takes an event returns true from `event()`, and the modes below it never see that event. The game's `update()` reaches every mode.

A mode ends itself by returning true from `done()`. The game stops it after the call in which it said so, thus no mode leaves the stack while the game walks it. The game's `stop()` ends every mode.

The host sees no modes. It hands its events and the time to the game, and the game hands them to its stack.

## Playfield components

Every element of the playfield is a component, and a game talks to the components, rather than to a hardware driver. A component can map multiple hardware elements together, such as one that combines an IR sensor, a light and a solenoid. An element with a single part is a component too, such as the drain's break beam. No event of a playfield driver therefore reaches a game. A service a game uses can be a component as well, such as the [file system](components/file-system.md) on the SD card or the game's [sounds](components/sounds.md).

A driver publishes a `DriverEvent`, and a game gets a `PinballEvent`. Both carry the moment of detection, a type, a source and a payload.

The host dispatches each driver event to the components by calling their `translate()` one after the other, until the first of them takes it. `translate()` gets the driver event as the `in` parameter and the translated pinball events as `out` parameter. The return value indicates whether the component took it.
The component however is not required to convert every driver event to a pinball event. The host therefore is required to check whether it needs to pass events to the game.

A component may make several pinball events of one driver event. It keeps them in an array that is big enough for the most it can make at once. The `out` parameter contains the pointer and size to that array.

A component with timed work, such as the [drain](components/drain.md)'s wait for a released ball, does it in `update()`. The host calls `update()` of every component once per pass of the main loop, before the game's `update()`, and in every phase of the game. The host passes the pinball events of both `translate()` and `update()` to the game. A component without timed work keeps the default, which makes none.

Components are described under [`components/`](components/).

## Saved data

A game's assets, high scores and other data may be stored in its directory on the SD card. Assets are files the [storage driver](drivers/storage.md) only reads. A game writes its high scores into ordinary files of its directory, as [storage](drivers/storage.md#writing) describes. A file missing on the card is created by its first write.

A game writes to the card only outside Play. A game keeps its live score in RAM and writes it in `end()`.

## Interfaces

```cpp
// the events a game gets: those the components make, and those of the services
enum class EventType : uint16_t {
    // List of events
};

// what a game gets, with the fields of a DriverEvent
struct PinballEvent {
    uint32_t  time;      // nowUs() at detection, taken over from the driver event
    EventType type;
    uint8_t   source;    // lane or channel inside its component or service
    uint32_t  payload;   // interpreted according to the type
};

// one payload per type
template <EventType Tag> struct PayloadOf;
template <EventType Tag> typename PayloadOf<Tag>::type payload(const PinballEvent&);

// what a game may use; the host hands it over in start()
struct Machine {
    XXXComponent& component;  // for each component and each service, one struct field
};

// the pinball events a component made of one driver event; valid until its next translate()
struct PinballEvents {
    const PinballEvent* events = nullptr;   // points into the component's own list
    uint8_t             count  = 0;         // how many it holds this time
};

// one element of the playfield, or a service such as the sounds, built on the drivers' parts
class Component {
public:
    virtual ~Component() = default;

    // takes a driver event of the component's parts and points out at the pinball events it made of it, if any; false for any other
    virtual bool translate(const DriverEvent& in, PinballEvents& out) = 0;

    // runs the component's timed work and points out at the pinball events it made, if any; false
    // when it made none. The host calls it once per pass of the loop, before the game's update()
    virtual bool update(uint32_t now, PinballEvents& out) { return false; }
};

// the three top roll-over lanes, each with its IR channel
class TopLanes : public Component {
public:
    // an IR event of a lane's channel becomes LanePassed, with the lane as its source
    bool     translate(const DriverEvent& in, PinballEvents& out) override;
    uint32_t passedAt(uint8_t lane) const;       // gets when a ball last rolled through a lane
};

// every game implements this; the host calls it through the lifecycle
class Game {
public:
    virtual ~Game() = default;

    virtual const char* id() const = 0;             // its directory on the card, /games/<id>
    virtual const char* title() const = 0;          // the name the player sees

    virtual void start(Machine& machine) = 0;       // starts a new game: Stopped to Intro
    virtual void play() = 0;                        // begins play: Intro to Play
    virtual void end() = 0;                         // ends the game: Play to Post-game
    virtual void stop() = 0;                        // stops the game: any phase to Stopped

    virtual void event(const PinballEvent& e) = 0;  // handles an event, in every phase but Stopped
    virtual void update(uint32_t now) = 0;          // once per pass of the loop, in every phase but Stopped
    virtual bool done() const = 0;                  // gets whether the current phase is done
};

// a part of a game's rules that runs for a while; the game runs it
class Mode {
public:
    virtual ~Mode() = default;

    virtual uint8_t priority() const = 0;           // its place in the stack

    virtual void start(Machine& machine) = 0;       // sets the mode up
    virtual void stop() {}                          // ends the mode and removes what it started
    virtual bool event(const PinballEvent& e) = 0;  // handles an event; true takes it, and the modes below never see it
    virtual void update(uint32_t now) {}            // once per pass of the loop
    virtual bool done() const { return false; }     // true once the mode has ended itself
};

// the modes of a game that run, ordered by priority
class ModeStack {
public:
    void start(Mode& mode, Machine& machine);   // starts a mode and puts it at its priority; nothing when it already runs
    void stop(Mode& mode);                      // ends a mode
    void stopAll();                             // ends every mode, the game's stop() calls it
    bool running(const Mode& mode) const;       // gets whether a mode runs

    void event(const PinballEvent& e);          // hands an event down from the highest priority until a mode takes it, then stops those whose done() is true
    void update(uint32_t now);                  // runs every mode, then stops those whose done() is true
};

// the games this firmware carries; the first one is the default
extern Game* const kGames[];
extern const size_t kGameCount;

class GameHost {
public:
    void add(Component& component);            // routes the driver events of the component's parts through it
    void begin(Machine& machine, bool touch);   // starts the sounds at their starting volumes and reads which game ran last, or the default game; the first update() launches it
    void dispatch(const DriverEvent& e);       // hands a driver event to its component and that one's events to the game, or a service's event copied to the game; keeps a fault for the logging or display
    void update(uint32_t now);                 // updates every component, then the running game, and moves the game on once its phase is done
    void launch(Game* next);                   // the sequence every start takes; the menu calls it on OK as well
};
```

## An example game

```cpp
// an example game: three balls, 100 points a lane, and a ball save after each launch
class Classic : public Game {
public:
    const char* id() const override    { return "classic"; }
    const char* title() const override { return "Classic"; }

    // Stopped to Intro
    void start(Machine& m) override {
        m_ = &m; phase_ = Intro; done_ = false; score_ = 0; balls_ = 3; saveDue_ = false;
        bestFile_ = m.files.open("best");
        if (load_.status() != FileStatus::Pending) bestFile_.read(0, &best_, sizeof best_, load_);   // the high score, there before the intro ends; a read still on its way after a quick restart brings the same file
        m.sounds.music("intro.wav");            // play the intro
    }

    // Intro to Play: starts the base mode and the ball save, and launches the first ball
    void play() override {
        phase_ = Play; done_ = false;
        modes_.start(base_, *m_);
        modes_.start(ballSave_, *m_);
        m_->drain.release();
    }

    // Play to Post-game: ends every mode, keeps the high score
    void end() override {
        phase_ = PostGame; done_ = false;
        modes_.stopAll();
        const bool known = load_.status() == FileStatus::Succeeded;   // a failed read leaves the high score on the card unknown
        if (known && score_ > best_) {
            best_ = score_;
            bestFile_.rewrite(&best_, sizeof best_, save_);   // best_ stays unchanged until save_ has ended
        }
    }

    void stop() override { modes_.stopAll(); }

    // hands the events of Play to the modes, then starts the ball save the base mode asked for
    void event(const PinballEvent& e) override {
        if (phase_ != Play) return;
        modes_.event(e);
        if (saveDue_) { saveDue_ = false; modes_.start(ballSave_, *m_); }
    }

    // carries the intro and the post-game phase on, and runs the modes during Play
    void update(uint32_t now) override {
        if (phase_ == Intro) {
            if (!m_->sounds.musicPlaying() && load_.status() != FileStatus::Pending) {
                done_ = true;                                           // the intro is done
            }
        } else if (phase_ == Play) {
            modes_.update(now);
        } else if (phase_ == PostGame) {
            if (save_.status() != FileStatus::Pending) {
                done_ = true;                                           // the write has ended
            }
        }
    }

    bool done() const override { return done_; }

private:
    // the rules of all of Play: 100 points a lane, and the next ball once one drains
    class Base : public Mode {
    public:
        explicit Base(Classic& game) : game_(game) {}
        uint8_t priority() const override { return 1; }
        void start(Machine& m) override   { m_ = &m; waiting_ = false; }
        bool event(const PinballEvent& e) override {
            if (e.type == EventType::LanePassed) { game_.score_ += 100; return true; }
            if (e.type == EventType::Drained) {
                if (--game_.balls_ == 0) game_.done_ = true;               // the last ball: Play is done
                else waiting_ = true;                                       // the next ball, once it is in the store
                return true;
            }
            if (e.type == EventType::BallStored && waiting_ && m_->drain.release()) {
                waiting_ = false;
                game_.saveDue_ = true;                                      // a new ball gets a new ball save
                return true;
            }
            return false;
        }
    private:
        Classic& game_;
        Machine* m_       = nullptr;
        bool     waiting_ = false;                       // a ball has drained and the next waits for the store
    };

    // the ball save: for a while after a launch, a drained ball comes back
    class BallSave : public Mode {
    public:
        uint8_t priority() const override { return 2; }
        void start(Machine& m) override {
            m_ = &m; started_ = false; over_ = false; waiting_ = false;
            m.drain.lamp(priority(), priority()).set(Color::green);               // lit while the save holds; its priority serves as its key
        }
        void stop() override { m_->lamps.remove(priority()); }                    // the lamp shows what the modes below set
        bool event(const PinballEvent& e) override {
            if (e.type == EventType::Drained) { waiting_ = true; return true; }   // the base mode never sees this ball
            if (e.type == EventType::BallStored && waiting_ && m_->drain.release()) {
                waiting_ = false; over_ = true;                             // one save per launch
                return true;
            }
            return false;
        }
        void update(uint32_t now) override {
            if (!started_) { since_ = now; started_ = true; }
            if (!waiting_ && now - since_ >= kSaveMs * 1000) over_ = true;
        }
        bool done() const override { return over_; }
    private:
        static constexpr uint32_t kSaveMs = 10000;       // how long after a launch the save holds
        Machine* m_       = nullptr;
        uint32_t since_   = 0;                           // nowUs() at the first update
        bool     started_ = false;
        bool     waiting_ = false;                       // a saved ball waits for the store
        bool     over_    = false;
    };

    enum Phase { Intro, Play, PostGame };

    Machine* m_      = nullptr;
    Phase    phase_  = Intro;
    bool     done_   = false;                       // the current phase is done
    uint32_t score_  = 0;
    uint32_t best_   = 0;                           // the high score, read from the card
    FileWriter  bestFile_;                          // the high score's file on the card
    FileRequest load_;                              // the read of the high score
    FileRequest save_;                              // the write of a new high score
    uint8_t  balls_  = 0;
    bool     saveDue_ = false;                      // the base mode launched a ball, so a ball save starts after the walk
    Base      base_{*this};
    BallSave  ballSave_;
    ModeStack modes_;                               // the modes of this game that run
};
```