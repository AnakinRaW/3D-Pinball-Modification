// the scoop with its IR channel, its unsensed coil and its lamp
class Scoop : public Component {
public:
    // channel: the scoop's IR channel, as ir.channels in the config numbers it
    Scoop(SolenoidDriver& solenoids, IrSensing& ir, Lamps& lamps, uint8_t channel)
        : solenoids_(solenoids), ir_(ir), lamps_(lamps), channel_(channel) {}

    // turns the channel's Detected into BallInScoop and its Released into BallLeftScoop, which
    // carries the dwell the driver measured
    bool translate(const DriverEvent& in, PinballEvents& out) override {
        out = {};
        if (in.source != channel_) return false;           // another component's channel
        if (in.type == DriverEventType::Detected) {
            event_ = {in.time, EventType::BallInScoop, 0, 0};
        } else if (in.type == DriverEventType::Released) {
            event_ = {in.time, EventType::BallLeftScoop, 0, in.payload};   // the dwell in ms
        } else {
            return false;
        }
        out = {&event_, 1};
        return true;
    }

    // gets whether a ball rests in the scoop
    bool holding() const {
        uint32_t ms;
        return ir_.getState(channel_, ms) == IrSensing::State::Ball;
    }

    // gets how long the ball has rested there, in milliseconds; 0 without one
    uint32_t heldMs() const {
        uint32_t ms = 0;
        return ir_.getState(channel_, ms) == IrSensing::State::Ball ? ms : 0;
    }

    // pulls the coil once, whether a ball rests in the scoop or not; false while it is still on
    // or cooling down
    bool fire() { return solenoids_.fire(kCoil); }

    // gets the scoop lamp for the key and priority of the game's mode
    Lamp lamp(uint8_t key, uint8_t priority) { return lamps_.groups(key, priority).scoop; }

private:
    static constexpr uint8_t kCoil = 3;   // the unsensed solenoid, the fourth trigger pin

    SolenoidDriver& solenoids_;
    IrSensing&      ir_;
    Lamps&          lamps_;
    uint8_t         channel_;
    PinballEvent    event_;
};
