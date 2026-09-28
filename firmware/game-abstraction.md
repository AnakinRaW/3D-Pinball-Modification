# Game abstraction layer

The games are kept apart from the machine they run on. A game holds the rules of play, such as what a lane is worth or how long the ball save lasts. It sees the playfield only through components, such as the lanes, the scoop and the drain. Between the games and the machine sits the host, which lets one firmware carry several games. It gives every game the same interface to the machine and runs each through the same lifecycle. The host also carries the menu, where the player picks a game and changes its settings. A new game therefore needs no change to a driver, and every game reuses the same components.

## A game

A game holds all the rules and conditions for the player to collect points.

A game is driven two ways. `event()` is the game's handler for every event of a game component, and for the events of a service such as the audio driver's `SoundEnded`. `update()` runs once per pass of the main loop and gets the current time. Both return as soon as their work is done, since [nothing in the loop may block](general-design.md#nothing-in-the-main-loop-may-block).

A game may bring its own files on the SD card, under `/games/<id>`. It may also declare its own game options which can be changed by interacting with the touch display or the toggle button.

A `Machine` typed struct is passed to the game, allowing it to access game components and hardware devices.

## The host

The host runs the games. It hands every game the same `Machine`, passes it the components' events and the current time, and moves it through its [lifecycle](#game-lifecycle). At start-up it starts the game that ran last, or a default game. Once the running game has finished, its post-game phase included, the host starts it anew on its own.

At every start of a game, including machine power-up, the host performs the same sequence:

1. It stops the running game and runs a clean-up procedure.
2. It writes changed settings and the id of the new game.
3. It prepares the new game by reading its settings and initializing things such as audio, display or lighting.
4. It starts the new game.

A ball still on the playfield carries on in the new game.

The host also manages the user menu on the touch display. There the player switches games, changes a game's settings or configures the machine.

## Game lifecycle

A game is organized into phases and the host is responsible for their transition. The game signals that it is ready for such a phase transition.

| Phase | What the game does | Begins with | Ends with |
|---|---|---|---|
| Stopped | gets no calls | boot, or `stop()` | `start()`, once the host has read the game's settings |
| Intro | sets itself up afresh and runs its intro through `update()`, such as emptying the scoop and playing an intro sound | `start()`, which hands it the machine and its options | `done()` returning true, such as once the intro sound has ended with the scoop empty. The host then calls `play()` |
| Play | plays through `event()` and `update()` | `play()` | `done()` returning true, for instance once its last ball drains. The host then calls `end()` |
| Post-game | shows its score and saves its high score | `end()` | `done()` returning true. The host then stops the game and starts it anew |

## Playfield components

Every element of the playfield is a component, and a game talks to the components, rather than to a hardware driver. A component can map multiple hardware elements together, such as a line consists of an IR sensor, a light and solenoid. An element with a single part, such as the drain's break beam, is a component too, so no event of a playfield driver reaches a game.

The host hands each driver event to the component that owns its source. The component updates its state and turns the event into one of its own, and the host passes that event on to the game. IR channel 9, for instance, becomes `LanePassed` for lane 2, stamped with the moment the IR driver saw the ball.

## Saved data

A game's settings, assets, high scores and other data may be stored in its directory on the SD card. Each of them is a file that the [storage driver](storage.md) reads and writes. A game reaches the card only through that driver.

The card is written only while no game is in play. A game keeps its live score in RAM and writes it in `end()`, and the host writes changed settings between stopping one game and starting the next.


## Code sketch

### Interfaces

```cpp
// what a game may use; the host hands it over in start()
struct Machine {
    XXXComponent& component;  // for each component and each service, one struct field
};

// one element of the playfield, built on the drivers' parts
class Component {
public:
    virtual ~Component() = default;

    // turns a driver event of its own parts into its own event; false for any other
    virtual bool translate(const PinballEvent& in, PinballEvent& out) = 0;
};

// the three top roll-over lanes, each with its IR channel and its lamp
class TopLanes : public Component {
public:
    // an IR event of a lane's channel becomes LanePassed, with the lane as its source
    bool     translate(const PinballEvent& in, PinballEvent& out) override;
    uint32_t passedAt(uint8_t lane) const;       // gets when a ball last rolled through a lane
    Color    color(uint8_t lane) const;          // gets a lane's lamp color
    void     light(uint8_t lane, Color c);       // sets a lane's lamp color, black for off
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
    void begin(Machine& machine, bool touch);  // starts the game that ran last, or the default game
    void dispatch(const PinballEvent& e);      // hands a driver event to its component, and that one's event to the game
    void update(uint32_t now);                 // updates the running game, and moves it on once its phase is done
    void launch(Game* next);                   // the sequence every start takes; the menu calls it on OK as well

private:
    enum Phase { Stopped, Intro, Play, PostGame };

    void  cleanUp();                 // resets the components after a game, such as the lamps
    void  prepare(Game* next);       // roots the storage driver in next's directory, reads its settings, sets audio, display and lighting up
    Game* lastGame();                // gets the game that ran last, or the default game
    void  saveSettings(Game* next);  // writes changed settings and the id of next to the card

    Machine* machine_ = nullptr;
    Game*    game_    = nullptr;
    Phase    phase_   = Stopped;
    bool     touch_   = false;
    File     settings_;       // next's settings, read in step 3
    bool     settingsRead_ = false;   // their FileRead has arrived
    static constexpr uint8_t kSettings = 0;   // the tag of the settings read; games use other tags
};
```

### The host

```cpp
void GameHost::begin(Machine& machine, bool touch) {
    machine_ = &machine;
    touch_   = touch;
    launch(lastGame());
}

void GameHost::dispatch(const PinballEvent& e) {
    if (e.type == EventType::FileRead && e.source == kSettings) {   // step 3 is done
        settingsRead_ = true;
        return;
    }
    // ... a driver event to the component that owns it, and that one's event to the game
}

void GameHost::update(uint32_t now) {
    if (phase_ == Stopped) {                    // 4. Stopped to Intro, once step 3 has read the settings
        if (!settingsRead_) return;
        game_->start(*machine_);
        phase_ = Intro;
        return;
    }
    game_->update(now);
    if (!game_->done()) return;
    switch (phase_) {
        case Intro:    phase_ = Play;     game_->play(); break;   // Intro to Play
        case Play:     phase_ = PostGame; game_->end();  break;   // Play to Post-game
        case PostGame: launch(game_);                    break;   // a new game of the same kind
        default:                                         break;
    }
}

void GameHost::launch(Game* next) {
    if (game_) {                            // 1. any phase to Stopped
        game_->stop();
        cleanUp();
    }
    saveSettings(next);                     // 2. changed settings and the id of next
    settingsRead_ = false;
    prepare(next);                          // 3. next's settings, audio, display and lighting
    game_  = next;
    phase_ = Stopped;                       // update() starts it once its settings are read
}
```

### An example game

```cpp
// an example game: three balls, 100 points a lane
class Classic : public Game {
public:
    const char* id() const override    { return "classic"; }
    const char* title() const override { return "Classic"; }

    // Stopped to Intro
    void start(Machine& m) override {
        m_ = &m; phase_ = Intro; done_ = false; played_ = false; score_ = 0; balls_ = 3;
        best_     = 0;
        bestFile_ = m.storage.open("save/best");
        loaded_   = false;
        bestFile_.read(&best_, sizeof best_, kBest);   // the high score, there before the intro ends
        m.scoop.eject();                        // empty the scoop
        kicked_ = micros();
        m.audio.music(intro_);                  // play the intro
    }

    // Intro to Play
    void play() override {
        phase_ = Play; done_ = false;
        m_->audio.music(background_);
    }

    // Play to Post-game: plays the end music, keeps the high score, shows the score
    void end() override {
        phase_ = PostGame; done_ = false; played_ = false;
        m_->audio.music(outro_);
        if (score_ > best_) { best_ = score_; bestFile_.seek(0); bestFile_.write(&best_, sizeof best_); }
        // ... draws score_ into the game's layer
    }

    void stop() override { m_->audio.stopMusic(); }

    void event(const PinballEvent& e) override {
        if (e.type == EventType::SoundEnded && e.source == 0) played_ = true;   // the music channel
        if (e.type == EventType::FileRead && e.source == kBest) loaded_ = true;  // the high score is in
        if (phase_ != Play) return;
        if (e.type == EventType::LanePassed) score_ += 100;
        if (e.type == EventType::Drained && --balls_ == 0) done_ = true;       // the last ball: Play is done
    }

    // carries the intro and the post-game phase on; the timed work of play goes here too
    void update(uint32_t now) override {
        if (phase_ == Intro) {
            if (m_->scoop.holding()) {                                  // a ball still inside
                if (now - kicked_ >= kKickGapUs) { m_->scoop.eject(); kicked_ = now; }
            } else if (played_ && loaded_) {
                done_ = true;                                           // the intro is done
            }
        } else if (phase_ == PostGame && played_) {
            done_ = true;                                               // the end music has played
        }
    }

    bool done() const override { return done_; }

private:
    enum Phase { Intro, Play, PostGame };
    static constexpr uint32_t kKickGapUs = 500000;   // between two tries to empty the scoop
    static constexpr uint8_t  kBest      = 1;        // the tag of the high score's read

    Machine* m_      = nullptr;
    Phase    phase_  = Intro;
    bool     done_   = false;                       // the current phase is done
    bool     played_ = false;                       // the sound on the music channel has ended
    bool     loaded_ = false;                       // the high score's FileRead has arrived
    uint32_t kicked_ = 0;
    uint32_t score_  = 0;
    uint32_t best_   = 0;                           // the high score, read from the card
    File     bestFile_;                             // the high score's file on the card
    uint8_t  balls_  = 0;
    SoundId  intro_;
    SoundId  background_;
    SoundId  outro_;
};
```