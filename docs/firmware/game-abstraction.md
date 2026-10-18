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
4. It starts the new game once the screen has cleared the stopped game's layers.

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

## Playfield components

Every element of the playfield is a component, and a game talks to the components, rather than to a hardware driver. A component can map multiple hardware elements together, such as one that combines an IR sensor, a light and a solenoid. An element with a single part is a component too, such as the drain's break beam. No event of a playfield driver therefore reaches a game. A service a game uses can be a component as well, such as the [file system](components/file-system.md) on the SD card or the game's [sounds](components/sounds.md).

A driver publishes a `DriverEvent`, and a game gets a `PinballEvent`. Both carry the moment of detection, a type, a source and a payload.

The host dispatches each driver event to the components by calling their `translate()` one after the other, until the first of them takes it. `translate()` gets the driver event as the `in` parameter and the translated pinball events as `out` parameter. The return value indicates whether the component took it.
The component however is not required to convert every driver event to a pinball event. The host therefore is required to check whether it needs to pass events to the game.

A component may make several pinball events of one driver event. It keeps them in an array that is big enough for the most it can make at once. The `out` parameter contains the pointer and size to that array.

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

// the games this firmware carries; the first one is the default
extern Game* const kGames[];
extern const size_t kGameCount;

class GameHost {
public:
    void add(Component& component);            // routes the driver events of the component's parts through it
    void begin(Machine& machine, bool touch);   // reads which game ran last, or the default game; the first update() launches it
    void dispatch(const DriverEvent& e);       // hands a driver event to its component and that one's events to the game, or a service's event copied to the game; keeps a fault for the logging or display
    void update(uint32_t now);                 // updates the running game, and moves it on once its phase is done
    void launch(Game* next);                   // the sequence every start takes; the menu calls it on OK as well
};
```

## An example game

```cpp
// an example game: three balls, 100 points a lane
class Classic : public Game {
public:
    const char* id() const override    { return "classic"; }
    const char* title() const override { return "Classic"; }

    // Stopped to Intro
    void start(Machine& m) override {
        m_ = &m; phase_ = Intro; done_ = false; score_ = 0; balls_ = 3;
        bestFile_ = m.files.open("best");
        if (load_.status() != FileStatus::Pending) bestFile_.read(0, &best_, sizeof best_, load_);   // the high score, there before the intro ends; a read still on its way after a quick restart brings the same file
        m.sounds.music("intro.wav");            // play the intro
    }

    // Intro to Play
    void play() override {
        phase_ = Play; done_ = false;
    }

    // Play to Post-game: keeps the high score
    void end() override {
        phase_ = PostGame; done_ = false;
        const bool known = load_.status() == FileStatus::Succeeded;   // a failed read leaves the high score on the card unknown
        if (known && score_ > best_) {
            best_ = score_;
            bestFile_.rewrite(&best_, sizeof best_, save_);   // best_ stays unchanged until save_ has ended
        }
    }

    void stop() override {}

    void event(const PinballEvent& e) override {
        if (phase_ != Play) return;
        if (e.type == EventType::LanePassed) score_ += 100;
        if (e.type == EventType::Drained && --balls_ == 0) done_ = true;       // the last ball: Play is done
    }

    // carries the intro and the post-game phase on; the timed work of play goes here too
    void update(uint32_t now) override {
        if (phase_ == Intro) {
            if (!m_->sounds.musicPlaying() && load_.status() != FileStatus::Pending) {
                done_ = true;                                           // the intro is done
            }
        } else if (phase_ == PostGame) {
            if (save_.status() != FileStatus::Pending) {
                done_ = true;                                           // the write has ended
            }
        }
    }

    bool done() const override { return done_; }

private:
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
};
```