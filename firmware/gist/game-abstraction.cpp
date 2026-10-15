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

private:
    enum Phase { Stopped, Intro, Play, PostGame };

    void  cleanUp();                 // resets the components after a game, such as the lamps
    void  prepare(Game* next);       // sets audio, display and lighting up for next
    Game* lastGame();                // gets the game that ran last from a file outside the games' directories, or the default game
    void  saveLastGame(Game* next);  // writes the id of next, which lastGame() reads at the next start-up
    void  start();                   // starts the game launch() chose, once the screen has cleared the stopped game's layers
    bool  service(const DriverEvent& in, PinballEvent& out);   // copies a service's event into the pinball event of the same name; false for any other

    Machine* machine_ = nullptr;
    Game*    game_    = nullptr;
    Phase    phase_   = Stopped;
    bool     starting_ = false;      // launch() chose game_, and start() has not started it yet
    Game*    first_    = nullptr;    // the game begin() read, which the first update() launches
    bool     touch_   = false;
    FailedDevices failed_;   // the failed parts of every device, for the menu
    FileWriter  lastFile_;   // "/last-game", which keeps the id of the game that ran last
    FileRequest lastSaved_;  // the write of that id, which saveLastGame() starts
    char        lastId_[Storage::kPath] = {};   // the id lastGame() reads at start-up; saveLastGame() rewrites the file from next's id
    Component* components_[kComponents];   // the components add() was given
    size_t     componentCount_ = 0;
};

void GameHost::begin(Machine& machine, bool touch) {
    machine_ = &machine;
    touch_   = touch;
    first_   = lastGame();                      // read before the main loop; the first update() launches it
}

void GameHost::dispatch(const DriverEvent& e) {
    if (e.type == DriverEventType::DeviceFailed)    { failed_.set(faultDevice(e.source), faultPart(e.source), e.payload); return; }     // the list the menu shows; no game sees it
    if (e.type == DriverEventType::DeviceRecovered) { failed_.clear(faultDevice(e.source), faultPart(e.source)); return; }
    PinballEvents t;
    PinballEvent  copy;
    bool taken = false;
    for (size_t i = 0; i < componentCount_ && !taken; ++i) taken = components_[i]->translate(e, t);
    if (!taken && service(e, copy)) t = {&copy, 1};   // a service's event goes on; any other untaken one is dropped
    if (phase_ == Stopped) return;                     // a stopped game gets no calls
    for (uint8_t i = 0; i < t.count; ++i) game_->event(t.events[i]);
}

bool GameHost::service(const DriverEvent& in, PinballEvent& out) {
    EventType type;
    switch (in.type) {
        case DriverEventType::ToggleSwitched: type = EventType::ToggleSwitched; break;   // controls
        default:                              return false;
    }
    out = {in.time, type, in.source, in.payload};
    return true;
}

void GameHost::update(uint32_t now) {
    if (first_) {                               // the game begin() read
        launch(first_);
        first_ = nullptr;
        return;
    }
    if (phase_ == Stopped) {                    // a stopped game gets no calls
        start();                                // the game launch() chose, once the screen has cleared; its first update() comes in the next pass, with a fresh now
        return;
    }
    game_->update(now);
    if (!game_->done()) return;
    switch (phase_) {
        case Intro:    phase_ = Play;     solenoids.arm(true);  game_->play(); break;   // Intro to Play, bumpers armed
        case Play:     phase_ = PostGame; solenoids.arm(false); game_->end();  break;   // Play to Post-game, bumpers disarmed
        case PostGame: launch(game_);                    break;   // a new game of the same kind
        default:                                         break;
    }
}

void GameHost::launch(Game* next) {
    if (game_ && phase_ != Stopped) {       // 1. any phase to Stopped; a game whose start still waits never started
        game_->stop();
        solenoids.arm(false);               // the bumpers fire only in Play
        cleanUp();
    }
    phase_ = Stopped;
    machine_->files.use(next->id());        // makes next the running one
    machine_->sounds.reset();               // the stopped game's sounds end with it
    machine_->screen.reset();               // and its layer and background
    machine_->videoPlayer.reset();          // and its video
    saveLastGame(next);                     // 2. the id of next
    prepare(next);                          // 3. audio, display and lighting
    game_     = next;
    starting_ = true;
    start();                                // 4. Stopped to Intro, at once unless a picture of the stopped game still lands
}

// a picture of the stopped game still landing in a layer would cover what the new game draws in start()
void GameHost::start() {
    if (!starting_ || !machine_->screen.settled()) return;
    starting_ = false;
    game_->start(*machine_);
    phase_ = Intro;
}

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
        m.scoop.eject();                        // empty the scoop
        kicked_ = nowUs();
        m.sounds.music("intro.wav");            // play the intro
    }

    // Intro to Play
    void play() override {
        phase_ = Play; done_ = false;
        m_->sounds.music("background.wav", true);   // the background music, over and over
    }

    // Play to Post-game: plays the end music, keeps the high score, shows the score
    void end() override {
        phase_ = PostGame; done_ = false;
        m_->sounds.music("outro.wav");
        const bool known = load_.status() == FileStatus::Succeeded;   // a failed read leaves the high score on the card unknown
        if (known && score_ > best_) {
            best_ = score_;
            bestFile_.rewrite(&best_, sizeof best_, save_);   // best_ stays unchanged until save_ has ended
        }
        // ... draws score_ into the game's layer
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
            if (m_->scoop.holding()) {                                  // a ball still inside
                if (now - kicked_ >= kKickGapUs) { m_->scoop.eject(); kicked_ = now; }
            } else if (!m_->sounds.musicPlaying() && load_.status() != FileStatus::Pending) {
                done_ = true;                                           // the intro is done
            }
        } else if (phase_ == PostGame) {
            if (save_.status() != FileStatus::Pending && !m_->sounds.musicPlaying()) {
                done_ = true;                                           // the end music has played and the write has ended
            }
        }
    }

    bool done() const override { return done_; }

private:
    enum Phase { Intro, Play, PostGame };
    static constexpr uint32_t kKickGapUs = 500000;   // between two tries to empty the scoop

    Machine* m_      = nullptr;
    Phase    phase_  = Intro;
    bool     done_   = false;                       // the current phase is done
    uint32_t kicked_ = 0;
    uint32_t score_  = 0;
    uint32_t best_   = 0;                           // the high score, read from the card
    FileWriter  bestFile_;                          // the high score's file on the card
    FileRequest load_;                              // the read of the high score
    FileRequest save_;                              // the write of a new high score
    uint8_t  balls_  = 0;
};
