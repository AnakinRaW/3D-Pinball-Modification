// IR channel model: the computation, with no reference to the document.
// Every entry point takes a parameter object p holding the inputs, so the same call can be made
// for a phase length or a pull-down the user has not selected.
//
// p = {N, R, clk, fspi, tovh, jit, vref, amb, ambself, mains, mod, railmv, nconv, D, k,
//      iph, iclr, taumodel, taufix, CH, perMode}
// bounds = {step, min, max} for the phase length, as the control offers it.

const IRTM = (function () {

  // t_r/t_f read off Figure 6, V_CE = 5 V, in microseconds
  const FIG6 = [[1, 51.6], [1.5, 52.1], [2, 56.0], [3, 61.7], [4, 68.5], [4.7, 70.9],
                [5, 72.1], [6, 77.0], [8, 85.6], [10, 93.7]];
  function fig6(R) {
    const lr = Math.log10(Math.max(0.3, R));
    let a = FIG6[0], b = FIG6[FIG6.length - 1];
    for (let i = 0; i < FIG6.length - 1; i++) { if (R >= FIG6[i][0] && R <= FIG6[i + 1][0]) { a = FIG6[i]; b = FIG6[i + 1]; break; } }
    if (R < FIG6[0][0]) { a = FIG6[0]; b = FIG6[1]; } if (R > b[0]) { a = FIG6[FIG6.length - 2]; b = FIG6[FIG6.length - 1]; }
    const la = Math.log10(a[0]), lb = Math.log10(b[0]);
    const f = (lr - la) / (lb - la);
    return Math.exp(Math.log(a[1]) + f * (Math.log(b[1]) - Math.log(a[1])));
  }

  function qtail(z) { // upper tail of the normal, Zelen & Severo
    if (z < 0) return 1 - qtail(-z);
    const t = 1 / (1 + 0.2316419 * z), phi = Math.exp(-z * z / 2) / Math.sqrt(2 * Math.PI);
    return phi * t * (0.319381530 + t * (-0.356563782 + t * (1.781477937 + t * (-1.821255978 + t * 1.330274429))));
  }

  const GOOD_S = 3.15e7, THIN_S = 86400;          // one false report a year, and one a day
  // Not chosen for what a player would notice: a day of quiet already passes that. They are chosen
  // against the estimates being wrong. A noise floor 20 % worse than guessed takes the year down to
  // eight hours, which still holds, and the day down to seven minutes, which does not. Amber therefore
  // means the configuration works and would not survive a bad guess.
  function marginFor(sec, N, T, k) {
    // rate = N / (2T) * p^k per second; solve for the margin whose tail gives rate = 1 / sec
    const want = Math.pow(2 * T / (N * 1e6 * sec), 1 / k);
    if (!(want < 0.5)) return 0;
    let lo = 0, hi = 24;
    for (let i = 0; i < 44; i++) { const mid = (lo + hi) / 2; if (qtail(mid / 2) > want) lo = mid; else hi = mid; }
    return (lo + hi) / 2;
  }

  function every(sec) {
    if (!isFinite(sec) || sec > 3.15e10) return 'never';
    if (sec < 1) return '< 1 s';
    if (sec < 90) return Math.round(sec) + ' s';
    if (sec < 5400) return Math.round(sec / 60) + ' min';
    if (sec < 1.73e5) return (sec / 3600).toFixed(1) + ' h';
    if (sec < 3.15e7) return Math.round(sec / 86400) + ' d';
    return (sec / 3.15e7).toFixed(1) + ' yr';
  }

  const snrOf = (st, n) => n > 0 ? st / n : Infinity;
  const r2 = n => Math.round(n * 100) / 100, r1 = n => Math.round(n * 10) / 10;

  const SELF = 20;
  const SCENES = {
    dark: { room: 0, mod: 0 },
    led: { room: 60, mod: 60 },
    bulb: { room: 250, mod: 10 },
    day: { room: 500, mod: 0 },
    sun: { room: 900, mod: 0 }
  };

  const START = 10;                              // microseconds the read block's start is quantised to
  const BALL_MM = 9;                             // the ball crosses a sensor over its own diameter

  // The signal follows tau exponentially, so how hard a wrong tau hits depends on where the read sits
  // on that curve. At half the swing an error in tau costs the same fraction of the signal: 30 % out
  // on tau, 30 % off the signal. Below half it amplifies, 31 % of swing turning a 30 % error into 42 %
  // and 10 % turning it into 90 %. Above half it damps, 90 % of swing giving back only 11 %.
  // tau is not measured here, and the two candidates for it stand a factor of 3.5 apart, so the floor
  // is a requirement rather than a preference. It comes out once tau is measured on the built board.
  const PERF_MIN = 0.50;

  // Three values, because a swap is a swap: 4.7 kΩ is what the board carries, 2.2 kΩ is the step
  // down a channel takes when ambient light already fills its range, and 1.5 kΩ is one more of the
  // same. Every other E12 value between them would be a resistor nobody stocks for this build.
  const PULLDOWNS = [1.5, 2.2, 4.7];

  function tauOf(p, R) {
    const m = p.taumodel;
    if (m === 'linear') return 100 * R / 2.2;
    if (m === 'fig6max') return 100 * (fig6(R) / fig6(1)) / 2.2;
    if (m === 'fig6typ') return fig6(R) / 2.2;
    return p.taufix * (fig6(R) / fig6(p.R));
  }

  // One pair of currents per sensor, as wired. The block reads them strongest first, which is the only
  // order worth having: the first slot has settled least, so it goes to the part that can spare it and
  // the weakest part gets the last slot, where the phase has run furthest. Every other order hands that
  // slot to a part that needs it more.
  function newChannels(p, keep) {
    const n = 16, b = p.iph, c = Math.min(p.iclr, p.iph), R = p.R, tau = tauOf(p, R), old = keep || [];
    const out = [];
    for (let i = 0; i < n; i++) out.push({
      name: (old[i] && old[i].name) || ('S' + (i + 1)),
      ball: b, clear: c, R, tau: +tau.toFixed(1)
    });
    return out;
  }

  function chOrder(p) {
    const src = p.perMode === 'all'
      ? Array.from({ length: p.N }, () => ({ ball: p.iph, clear: Math.min(p.iclr, p.iph), R: p.R, tau: tauOf(p, p.R) }))
      : p.CH.slice(0, p.N);
    const use = src.map((c, i) => ({
      ...c, src: i, contrast: Math.max(0, c.ball - c.clear) * c.R,
      name: p.perMode === 'each' ? ((p.CH[i] && p.CH[i].name) || ('S' + (i + 1))) : String(i + 1)
    }));
    use.sort((a, b) => b.contrast - a.contrast);   // strongest into the earliest slot
    return use;
  }

  function model(p, T) {
    const tconv = p.clk / p.fspi;
    const tbud = p.N * (tconv + p.tovh);
    const guard = p.jit;                        // the block has to finish this far before the phase ends
    // The driver arms a timer at a round number, so the instant is taken down to the step below the
    // ideal one. Down rather than up: an earlier start reads a less settled channel and costs signal,
    // a later one walks the tail of the block into the phase boundary and costs a channel its reading.
    const tfirst = Math.floor((T - tbud - guard) / START) * START, tlast = T - guard - (tconv + p.tovh);
    const lsb = p.vref / 1024 * 1000;
    const ambRoom = p.amb, ambTot = ambRoom + p.ambself;
    const w = 2 * Math.PI * p.mains, amp = ambRoom * p.mod / 100;
    const Ts = T * 1e-6;
    const flick = p.mains === 0 ? 0 : amp * (1 - Math.cos(w * Ts));
    // Samples at t-Ts, t, t+Ts. mean(dark) - lit = A sin(wt) [cos(w Ts) - 1], so the peak residue is
    // A (1 - cos(w Ts)), capped at 2A. Its small-angle form 0.5 A w^2 Ts^2 held here until the phase
    // passed a millisecond: at 600 us it runs 1 % high, at 1200 us 5 %, and at the stock board's 3 ms
    // 36 %, where it reports 64 steps against the exact 47. Unbounded growth pushed the optimiser to
    // short phases for a reason that was arithmetic rather than physical.
    const bus = p.N * 10.6;                      // mA, N emitters at worst case
    // Derived, the constant is the midpoint of the datasheet's typical and worst case and the residue is the
    // half-spread around it, a fifth of the value. Measured on the board, what is left is the meter: 1 mV of
    // resolution on each of two readings of a 3300 mV rail, so 0.06 % of the dark reading.
    const railRel = p.railmv / 3300;            // the step as entered, against the 3.3 V rail
    const railPred = 0.0125 * p.N / 16 * 3300;  // what the datasheet gives at this bus current
    const residFrac = railRel * 0.20;           // the midpoint of the datasheet band leaves a fifth open
    const railStep = railRel * 3300;            // mV between lit and dark
    const refRes = residFrac * ambTot;
    const noise = Math.hypot(p.nconv, refRes, flick);
    // Every channel sees the same noise; what differs is its part, its place and how far its own
    // reading has settled by the time the block reaches it. The design has to hold for the worst one.
    const step = tconv + p.tovh;
    const chans = chOrder(p).map((c, i) => {
      const t = tfirst + i * step, xi = Math.exp(-T / c.tau);
      const f = Math.max(0, 1 - 2 * Math.exp(-t / c.tau) / (1 + xi));
      const tz = c.tau * Math.log(2 / (1 + xi));
      const clr = Math.min(c.clear, c.ball);
      const A = (c.ball - clr) * c.R, Amid = (c.ball + clr) / 2 * c.R;
      const st = A * f / lsb;
      return {
        i, src: c.src, name: c.name, t, f, tau: c.tau, R: c.R, x: xi, tz, ball: c.ball, clear: c.clear,
        steps: st, thr: Amid * f / lsb, full: c.ball * c.R / lsb, mg: noise > 0 ? st / noise : 0,
        ballS: c.ball * c.R * f / lsb, clrS: clr * c.R * f / lsb, perStep: lsb / c.R,
        sw: tt => 1 - 2 * Math.exp(-tt / c.tau) / (1 + xi)
      };
    });
    const blank = {
      steps: 0, thr: 0, mg: 0, full: 0, i: 0, src: 0, t: tfirst, f: 0, tau: tauOf(p, p.R), R: p.R,
      x: Math.exp(-T / tauOf(p, p.R)), tz: 0, sw: () => 0
    };
    const worst = chans.reduce((a, b) => b.mg < a.mg ? b : a, chans[0] || { ...blank, ballS: 0, clrS: 0, perStep: lsb / p.R });
    const steps = worst.steps, thr = worst.thr, A = worst.steps * lsb;
    const peak = chans.reduce((a, b) => b.full > a.full ? b : a, chans[0] || blank).full;
    const tau = Math.max(...chans.map(c => c.tau), 0) || blank.tau;   // the slowest sensor sets the bounds
    const Rmax = Math.max(...chans.map(c => c.R), 0) || p.R;
    const x = worst.x, sw = worst.sw, tzero = worst.tz;
    const cycle = 2 * T, reads = Math.floor(p.D * 1000 / cycle);
    const duty = tbud / T;                      // share of the core the read block holds
    // p is the Gaussian tail beyond half the gap, the threshold sitting midway between the clear
    // track and the ball. Nothing has measured that the noise really is Gaussian out at 4 sigma, and
    // the two readings are treated as independent, which flicker is not. The interval that follows is
    // the mean of a Poisson process, so it has no minimum, and it moves by decades on small changes
    // in the noise: at sixteen channels a 10 % worse noise floor takes 21 hours down to 50 minutes.
    const goodMargin = marginFor(GOOD_S, p.N, T, p.k), thinMargin = marginFor(THIN_S, p.N, T, p.k);
    const tail = qtail(Math.max(0, snrOf(steps, noise)) / 2);
    const rate = p.N * (1e6 / (2 * T)) * Math.pow(tail, p.k);
    const falseEvery = rate > 0 ? 1 / rate : Infinity;
    const ovhCeil = (T - guard - tau * Math.LN2) / p.N - tconv;
    return {
      T, tconv, tbud, tfirst, tlast, tau, x, sw, tzero, A, lsb, steps, thr, chans, worst, peak,
      flick, noise, refRes, residFrac, ovhCeil, bus, railRel, railStep, railPred, ambTot,
      snr: snrOf(steps, noise), falseEvery, goodMargin, thinMargin, cycle, reads,
      signOK: chans.every(c => c.t > c.tz), fitOK: tfirst > 0, readOK: reads >= p.k,
      perfOK: worst.f >= PERF_MIN,
      acqWin: 1.5 / p.fspi, acqNeed: (Rmax + 1) * 0.02 * 6.9, guard, duty,
      swFirst: worst.f, swLast: chans.length ? chans[chans.length - 1].f : 0, mV: worst.steps * lsb
    };
  }

  // A configuration is usable when every one of these holds. Anything that fails one is not ranked.
  const usable = m => m.fitOK && m.signOK && m.readOK && m.perfOK;

  // The firmware picks four things. Two of them are searched here. The read order is fixed at strongest
  // first, and where the threshold sits between the two readings is fixed at the midpoint: a shift buys
  // one kind of error at the other's expense, and the midpoint is where the worse of the two is
  // smallest. It moves only if one error is judged worse than the other.
  //
  // What counts as sensible: reach one false report a day, then stop buying more of it. The second dark
  // reading is not one of the costs: both dark phases are read in any case and both are in hand when the
  // cycle ends, so the mean is always taken. Readings beyond the confirmations plus
  // one spare buy nothing either: a shorter phase reads the ball more often than the ball can move,
  // and every one of those reads is core time the rest of the firmware does not get.
  function betterThan(a, b, k) {
    if (!b) return true;
    // The year is the requirement, not a preference. Anything short of it leaves a channel amber, which
    // means it works and would not survive the noise being guessed low, and every noise input here is a
    // guess. Surplus beyond the year is spent on the spare reading, not the other way round.
    const ag = a.fe >= GOOD_S, bg = b.fe >= GOOD_S;
    if (ag !== bg) return ag;
    if (!ag) return a.fe > b.fe;
    const need = k + 1;                         // the confirmations, plus one in hand
    const ae = a.reads >= need, be = b.reads >= need;
    if (ae !== be) return ae;                   // then catching the ball, which cannot be recovered
    if (!ae && a.reads !== b.reads) return a.reads > b.reads;
    if (Math.abs(a.duty - b.duty) > 1e-6) return a.duty < b.duty;   // then the cheaper phase
    if (a.kept !== b.kept) return a.kept;       // then the resistor already fitted, over a new one
    return a.fe > b.fe;
  }

  // Two requirements bracket the phase, and both are inequalities that solve directly.
  //
  //   floor   N (t_conv + t_ovh) + jitter        the block and its guard have to fit inside one phase
  //   ceiling D / (2 (k + 1))                    a value costs two phases, so this many still fit the dwell
  //
  // Inside the bracket every phase length delivers the same number of readings, so the readings step of
  // the ranking cannot separate two of them. What is left is the CPU load, N (t_conv + t_ovh) / T, and it
  // falls as the phase grows. The ceiling is therefore the answer whenever a resistor reaches the year
  // there, and no phase length below it can win.
  function phaseWindow(p, bounds) {
    const step = bounds.step || 1, tconv = p.clk / p.fspi;
    const fit = Math.ceil((Math.ceil(p.N * (tconv + p.tovh) + p.jit) + START) / step) * step;
    const cap = Math.floor(p.D * 1000 / (2 * (p.k + 1)) / step) * step;
    return { step, lo: Math.max(fit, bounds.min), hi: Math.min(cap, bounds.max) };
  }

  // The pull-down is not solved for. The margin is not monotone in it, because the amplitude grows
  // with R while the settling slows by the same factor, so the best value sits somewhere inside the
  // range rather than at an end. Three is the whole set on offer, so the set is evaluated.
  // Per sensor the resistor comes from the table and there is nothing to pick.
  function bestPullDown(p, T, rs, keepR) {
    let best = null;
    for (const R of rs) {
      const m = model({ ...p, R }, T);
      if (!usable(m)) continue;
      if (m.acqNeed > m.acqWin) continue;
      if (m.ambTot + m.peak >= 1024) continue;
      const cand = {
        T, R, kept: R === keepR, fe: Math.min(3.15e9, m.falseEvery), snr: m.snr,
        reads: m.reads, duty: m.duty, steps: m.steps, sw: m.worst.f, worst: m.worst.name
      };
      if (betterThan(cand, best, p.k)) best = cand;
    }
    return best;
  }

  function propose(p, bounds) {
    const keepR = p.R, rs = p.perMode === 'each' ? [keepR] : PULLDOWNS, w = phaseWindow(p, bounds);
    let best = null;
    // Walking down from the ceiling only happens where the ceiling itself falls short. The floor is not
    // taken as given either: the sign inversion moves with the pull-down, so the shortest phase that
    // still reads the right way round differs from one resistor to the next.
    for (let T = w.hi; T >= w.lo; T -= w.step) {
      const c = bestPullDown(p, T, rs, keepR);
      if (c && c.fe >= GOOD_S) return c;
      if (c && betterThan(c, best, p.k)) best = c;
    }
    // The year outranks the readings, so where the bracket holds nothing that reaches it the phase is
    // allowed past its ceiling and buys the year at the cost of a reading. Margin against phase length
    // has no closed form once the sign inversion is inside it, so this part enumerates.
    for (let T = Math.max(w.hi + w.step, w.lo); T <= bounds.max; T += w.step) {
      const c = bestPullDown(p, T, rs, keepR);
      if (c && betterThan(c, best, p.k)) best = c;
    }
    return best;
  }

  // The phase lengths the control can actually be set to, over the whole slider range, with the
  // usable ones marked. Sampling between the steps draws the quantised read start as a sawtooth.
  function sweep(p, bounds, T0, T1) {
    const st = bounds.step || 1, out = [];
    for (let T = Math.ceil(T0 / st) * st; T <= T1 + 1e-9; T += st) out.push(model(p, T));
    return out;
  }

  function usableRange(p, bounds) {
    const st = bounds.step || 1;
    let lo = null, hi = null;
    for (let T = bounds.min; T <= bounds.max; T += st) {
      if (usable(model(p, T))) { if (lo === null) lo = T; hi = T; }
    }
    return { lo, hi };
  }

  function exportDoc(p, m, sceneLabel) {
    return {
      generatedBy: 'IR channel model, ROKR EG01 modification',
      readThisFirst: [
        'Model output computed from ESTIMATES. No figure here was measured on hardware.',
        'The thresholds are placeholders. Real ones come from a build step that measures each channel over a clear track and again with a ball on it; the startup calibration rescales both for drift and the firmware computes threshold = (clear + ball) / 2 per channel.',
        'phaseMicros and readOrder are the design decisions this model settles. darkSample and confirmationsRequired are fixed by the design and are not open.',
        'Everything under unknowns has to be measured or wired before this becomes a working driver.'
      ],
      units: {
        time: 'microseconds', current: 'microamperes', resistance: 'kilohms',
        voltage: 'millivolts', reading: 'converter steps, full scale 1024'
      },
      fixedByParts: {
        converters: '2 x Microchip MCP3008, DS21295D, on one SPI bus with one chip select each',
        host: 'Teensy 4.1, 3.3 V logic, not 5 V tolerant',
        spiClockHz: p.fspi * 1e6,
        spiMode: 0,
        bitsPerTransfer: 8,
        framesPerConversion: 3,
        clocksPerConversion: p.clk,
        conversionMicros: r2(m.tconv),
        referenceVolts: p.vref,
        referenceSource: 'the board rail, so the reading is ratiometric',
        lsbMillivolts: r2(m.lsb),
        chipSelectFloorMicros: 0.37
      },
      phase: {
        phaseMicros: m.T, cycleMicros: m.cycle,
        channels: p.N,
        readBlockMicros: r1(m.tbud),
        firstReadAtMicros: r1(m.tfirst),
        firstReadFrom: `T minus the block minus the jitter guard, taken down to the next ${START} µs. Down, because an earlier start reads a less settled channel and only costs signal, while a later one runs the end of the block into the phase boundary`,
        firmwareBudgetPerChannelMicros: p.tovh,
        startJitterGuardMicros: m.guard,
        guardMeaning: 'a timer fires late, never early, and the driver stops at the phase boundary rather than reading across it. The guard is therefore what keeps a late start from costing the last channels in the block their reading for that cycle. Those are the ones read last, which under the strongest-first order are the weakest parts.',
        firmwareCeilingPerChannelMicros: r1(m.ovhCeil),
        signInvertsAtMicros: r1(m.worst.tz),
        dwellMillis: p.D, readingsPerPass: m.reads,
        coreHeldByBlock: r2(m.duty),
        phaseCeilingMicros: 1500,
        phaseCeilingReason: 'the stock machine pulses its emitter with a measured 3 ms period, and a cycle is two phases, so a phase above 1.5 ms would sample the ball more slowly than the machine being replaced'
      },
      driver: {
        confirmationsRequired: p.k,
        darkSample: 'the mean of the two dark phases either side of the lit one, always',
        readOrderPolicy: 'strongest first, fixed. The earliest slot in the block has settled least, so it goes to the part that can spare it and the weakest part is read last',
        readOrder: m.chans.map(c => c.name),
        correction: Math.round((1 + m.railRel) * 1e4) / 1e4,
        correctionApplyAs: 'value = lit - correction * dark, per reading, per channel',
        correctionFrom: 'the 1 subtracts the ambient, the rest is the rail step between the lit and the dark phase, from the MCP1700 load regulation at the LED bus current of this channel count, midpoint of typical and worst case',
        railStepMillivolts: p.railmv,
        railStepCouldBeOffByMillivolts: Math.round(m.residFrac * 3300)
      },
      channels: m.chans.map(c => ({
        readSlot: c.i, name: c.name,
        sampledAtMicros: r1(c.t), settledFraction: r2(c.f),
        pullDownKilohm: r2(c.R), tauMicros: r1(c.tau),
        ballMicroamps: r2(c.ball), clearTrackMicroamps: r2(c.clear),
        clearTrackSteps: r1(c.clrS), ballSteps: r1(c.ballS),
        thresholdSteps: r1(c.thr), gapSteps: r1(c.steps),
        marginOverNoise: r1(c.mg),
        microampsPerStep: r2(c.perStep)
      })),
      noise: {
        totalSteps: r2(m.noise),
        converterSteps: p.nconv,
        referenceResidueSteps: r2(m.refRes),
        mainsFlickerSteps: r2(m.flick),
        mainsFlickerFrom: 'amp*(1-cos(w*Ts)), the residue a sine leaves between the lit reading and the mean of the two darks either side. The modulation depth behind amp is assumed, not measured, and is an upper bound: the stock machine runs a 3 ms emitter period without trouble in lit rooms, which this depth would not permit',
        gapForOneFalseReportAYearSteps: Math.round(m.goodMargin * m.noise),
        marginForOneAYear: r1(m.goodMargin),
        marginForOneADay: r1(m.thinMargin),
        room: sceneLabel,
        darkChannelReadingSteps: m.ambTot
      },
      verdict: {
        worstChannel: m.worst.name,
        worstMarginOverNoise: r1(m.snr),
        falseReportMeanWait: every(m.falseEvery),
        criterion: 'the threshold splits the gap, so a reading stands half the gap clear of it. The margin that meets a given rate of false reports follows from that half-distance, the reading rate and the confirmation count; it is not a fixed constant.',
        blockFitsPhase: m.fitOK,
        everyReadPastSignInversion: m.signOK,
        passDeliversEnoughReadings: m.readOK,
        readPerformanceClearsTauFloor: m.perfOK,
        readPerformanceFloorPercent: PERF_MIN * 100,
        readPerformanceFloorFrom: 'below half the swing the read sits on the steep part of the settling curve and multiplies an error in tau instead of following it. tau is not measured, and its two candidates stand a factor of 3.5 apart. The floor comes out once tau is measured on the built board',
        converterAcquiresInWindow: m.acqNeed <= m.acqWin,
        channelStaysInRange: m.ambTot + m.peak < 1024
      },
      unknowns: [
        'which converter and which of its eight inputs each named sensor is wired to',
        't_ovh measured on the finished system with ARM_DWT_CYCCNT, not on a bare Teensy',
        'the worst-case latency from the phase timer firing to the first conversion starting, which is what startJitterGuardMicros has to cover',
        'tau measured per fitted sensor, read once settled and once at the read instant',
        'what a ball and a clear track actually return, per fitted sensor',
        'the rail step between the lit and dark phase, measured on the built board',
        'the dwell, from the envelope width on a stock sensor at plunger speed'
      ]
    };
  }

  return {
    GOOD_S, THIN_S, SELF, SCENES, START, BALL_MM, PERF_MIN, PULLDOWNS,
    fig6, qtail, marginFor, every, snrOf,
    tauOf, newChannels, chOrder, model, usable, betterThan,
    phaseWindow, bestPullDown, propose, sweep, usableRange, exportDoc
  };
})();

if (typeof module !== 'undefined' && module.exports) module.exports = IRTM;
