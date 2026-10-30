/*
begin():
    out = events.attach(queue, kQueueDepth)
    csA, csB = HIGH, HIGH          # before SPI, or both converters answer at once
    ledGate  = LOW                 # never INPUT_PULLUP, the isolator reads that as a high
    SPI.begin()                    # the pins and the clock; the driver feeds LPSPI4 itself from here on
    attachLpspi4Receive()          # each conversion's reply, at the IR's priority after the kernel's start

    planPhase(n)                   # T and t_first from the channel count, or refuse to run
    startPhaseCounter()            # QuadTimer3 channels 2 and 3, free-running from here
    discard(SETTLING_CYCLES)       # sensors and the converters' reference settle
    readEntries()
    calibrate()
    deviceMonitor.watch(IrSensing, self)     # asks failed() every 100 ms from here on

readEntries():                     # ir.channels, the build step's results
    for i in 0 .. config.count(ConfigKey::kIrChannels) - 1:
        e = config.get(entry(kIrChannels, i, kIrChannel), 0, 15), ... clear_build and ball_build 0 to 1023, order 0 to 15
        if any field unset or e.channel taken or e.ball_build <= e.clear_build:
            log("ir.channels.i breaks the rules and is left out");  continue
        entries[e.channel] = e

calibrate():
    v, sigma = averages()          # every channel's mean and one clear channel's σ, from the same readings

    for c in channels:
        b = entries[c]                 # the build step's clear_build, ball_build and read order
        if   b missing:             c.calFailed = true;  continue
        if   v[c] near c.clear_build:  c.scale = v[c] / c.clear_build
        elif v[c] near c.ball_build:   c.scale = v[c] / c.ball_build
        else:                          c.scale = 1;  c.calFailed = true

        c.clear, c.ball = c.clear_build * c.scale, c.ball_build * c.scale
        c.threshold     = (c.clear + c.ball) / 2
        c.release       = c.threshold - m_r * sigma
        c.lastGood    = nowUs()

    if not marginHolds():          # (ball - clear) / 2 >= (m_r + 2) * sigma
        raisePhase() or fallBackToStockPhase() or report(Degraded)
        restart()
*/

/*
build():                           # the program ir-build, with the serial monitor open
    ask("clear the track, then Enter")
    clear, sigma = averages()
    for c in channels:
        ask("ball on channel c, then Enter")
        ball[c] = averages()[c]
    kept = [c for c in channels if clear[c] >= 1.5 * sigma and ball[c] > clear[c]]
    print(channels not in kept)
    order = kept ranked by ball - clear, the weakest last

    config.remove(ConfigKey::kIrChannels)
    for i, c in kept:
        config.set(entry(kIrChannels, i, kIrChannel), c), likewise clear_build, ball_build and order
    config.save()                  # the game firmware takes the entries at its next start
    print(the entries)
*/

/*
note(c, diff):                     # after the ball logic, for every new difference
    if diff >= c.clear / 2:        # a ball only raises the difference
        c.lastGood = nowUs()

failed():                          # one bit per channel, for the device monitor
    parts = 0
    for c in channels:
        if c.calFailed or elapsedMs(c.lastGood, 1 s):  parts |= 1 << c
    return Fault(parts, 0)            # no error code
*/
