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
    calibrate()
    deviceMonitor.watch(IrSensing, self)     # asks failed() every 100 ms from here on

calibrate():
    v, sigma = averages()          # every channel's mean and one clear channel's σ, from the same readings

    for c in channels:
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
note(c, diff):                     # after the ball logic, for every new difference
    if diff >= c.clear / 2:        # a ball only raises the difference
        c.lastGood = nowUs()

failed():                          # one bit per channel, for the device monitor
    parts = 0
    for c in channels:
        if c.calFailed or elapsedMs(c.lastGood, 1 s):  parts |= 1 << c
    return Fault(parts, 0)            # no error code
*/
