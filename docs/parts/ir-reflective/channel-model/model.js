// IR channel model: the computation, with no reference to the document.
// Every entry point takes a parameter object p holding the inputs, so the same call can be made
// for a phase length or a pull-down the user has not selected.
//
// p = {N, R, rcol, sensor, clk, fspi, tovh, jit, vref, vmin, vmax, amb, ambself, mains, mod,
//      railmv, nconv, D, k, iph, iclr, taumodel, taufix, CH, perMode}
// bounds = {step, min, max} for the phase length, as the control offers it.

const IRTM = (function () {

  // The two reflective sensors this build has in hand. What separates them here is what their sheets
  // publish about speed. Sharp plots response time against load resistance, so a pull-down the part
  // was never characterised at still has a settling figure. Vishay gives the CNY70 no switching
  // figure of any kind, not a rise time and not a curve, so every tau for it comes off the bench and
  // the models on offer differ accordingly.
  //
  //   curve   t_r/t_f against the load resistance in kOhm, microseconds, or null where none is published
  //   icmax   absolute maximum collector current, mA
  //   vcesat  collector-emitter saturation voltage, V; it comes off the top of the node's range
  //   rcol    the collector load the sensor board carries between the rail and the collector
  //   models  the settling models the sheet supports, in the order the control offers them
  const SENSORS = {
    gp2s700: {
      name: 'GP2S700HCP', sheet: 'Sharp D3-A02201EN',
      // read off Figure 6, V_CE = 5 V, at 1, 1.5, 2, 3, 4, 4.7, 5, 6, 8 and 10 kOhm
      curve: [[1, 51.6], [1.5, 52.1], [2, 56.0], [3, 61.7], [4, 68.5], [4.7, 70.9],
              [5, 72.1], [6, 77.0], [8, 85.6], [10, 93.7]],
      icmax: 20,
      vcesat: 0,          // the sheet specifies none, so nothing is taken off the ceiling
      rcol: 1.58,         // the E96 value the rebuilt board carries beside the stock 1.585 kOhm
      models: ['linear', 'fig6max', 'fig6typ', 'fixed']
    },
    cny70: {
      name: 'CNY70', sheet: 'Vishay 83751 Rev. 1.8',
      curve: null,
      icmax: 50,
      vcesat: 0.3,        // max at I_F = 20 mA, I_C = 0.1 mA, d = 0.3 mm
      rcol: 0,            // nothing on a CNY70 dictates one; the collector goes to the rail
      models: ['measlin', 'measflat']
    }
  };
  const sensorOf = p => SENSORS[p.sensor] || SENSORS.gp2s700;

  // Log-log interpolation along a published curve, held flat outside the points that were read.
  function curveAt(cv, R) {
    const lr = Math.log10(Math.max(0.3, R));
    let a = cv[0], b = cv[cv.length - 1];
    for (let i = 0; i < cv.length - 1; i++) { if (R >= cv[i][0] && R <= cv[i + 1][0]) { a = cv[i]; b = cv[i + 1]; break; } }
    if (R < cv[0][0]) { a = cv[0]; b = cv[1]; } if (R > b[0]) { a = cv[cv.length - 2]; b = cv[cv.length - 1]; }
    const la = Math.log10(a[0]), lb = Math.log10(b[0]);
    const f = (lr - la) / (lb - la);
    return Math.exp(Math.log(a[1]) + f * (Math.log(b[1]) - Math.log(a[1])));
  }
  const fig6 = R => curveAt(SENSORS.gp2s700.curve, R);

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
  // The floor keeps a wrong tau from swallowing the signal. The target is where a wrong tau starts
  // giving back less than it takes: at 70 % of swing a 30 % error in tau costs 13 % of the signal,
  // against 30 % at half. Above the target the curve is flat enough that more phase buys little, and
  // phase is what the ball speed is paid in, so the target is a threshold and not a preference.
  const AIM_F = 0.70;

  // Three values, because a swap is a swap: 4.7 kΩ is what the board carries, 2.2 kΩ is the step
  // down a channel takes when ambient light already fills its range, and 1.5 kΩ is one more of the
  // same. Every other E12 value between them would be a resistor nobody stocks for this build.
  const PULLDOWNS = [1.5, 2.2, 4.7];

  // taufix is tau at the pull-down currently selected, so every model that rests on it carries the
  // measurement to another pull-down by a shape. Which shapes are honest depends on the part. With a
  // published curve the curve is the shape. Without one there are two, and they bracket the answer:
  // tau proportional to the load, which is the first-order relation a load resistance against a fixed
  // capacitance gives, and tau flat, which is what the measurement alone says. The proportional one is
  // the pessimistic side at a larger pull-down and the optimistic side at a smaller.
  function tauOf(p, R) {
    const m = p.taumodel, s = sensorOf(p);
    if (!s.curve) return m === 'measflat' ? p.taufix : p.taufix * R / p.R;
    if (m === 'linear') return 100 * R / 2.2;
    if (m === 'fig6max') return 100 * (curveAt(s.curve, R) / curveAt(s.curve, 1)) / 2.2;
    if (m === 'fig6typ') return curveAt(s.curve, R) / 2.2;
    return p.taufix * (curveAt(s.curve, R) / curveAt(s.curve, p.R));
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
      ball: b, clear: c, R, rcol: p.rcol, sensor: p.sensor, tau: +tau.toFixed(1)
    });
    return out;
  }

  function chOrder(p) {
    const src = p.perMode === 'all'
      ? Array.from({ length: p.N }, () => ({
        ball: p.iph, clear: Math.min(p.iclr, p.iph), R: p.R,
        rcol: p.rcol, sensor: p.sensor, tau: tauOf(p, p.R)
      }))
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
    const bus = p.N * 10.3;                      // mA, N emitters at worst case
    // The step is measured, because the module publishes no load regulation and the cable's drop belongs to
    // the installation. What can be predicted is the cable: the bus current against the 0.1 ohm the design
    // bounds it at. The correction is a ratio, so what it leaves is a fraction of the dark reading rather
    // than a share of the step. Two 1 mV readings of a 3300 mV rail put that ratio 0.06 % out, and the
    // design holds itself usable while the whole residual, the meter and whatever fails to repeat, stays
    // under one step of 1024 of rail difference. That bound is what enters the noise floor.
    const railRel = p.railmv / 3300;            // the step as entered, against the 3.3 V rail
    const railPred = bus * 0.1;                 // mV the supply cable alone drops at this bus current
    const residFrac = 3.2 / 3300;               // one step of 1024 of rail difference, the design's bound
    const railStep = railRel * 3300;            // mV between lit and dark
    const refRes = residFrac * ambTot;
    const noise = Math.hypot(p.nconv, refRes, flick);
    // Every channel sees the same noise; what differs is its part, its place and how far its own
    // reading has settled by the time the block reaches it. The design has to hold for the worst one.
    const step = tconv + p.tovh;
    // The collector load and the pull-down divide the rail between them, and the node can never leave
    // the transistor less than V_CE(sat). The converter's reference is that same rail, so the divider's
    // share of full scale is the ceiling in steps whatever the rail does, and the rail enters only
    // through V_CE(sat). A falling rail is the larger share, so the ceiling is taken at the minimum.
    // Ambient light sits under the ball's reading and eats the same range.
    // Every channel carries its own part and its own board, because three of the sensor boards come
    // out of the stock machine and the rest are built. Ceiling and current limit are therefore read
    // per channel, from that channel's collector load and that channel's sensor.
    const sens = sensorOf(p), rcol = Math.max(0, p.rcol);
    const senCh = c => SENSORS[c.sensor] || sens;
    const rcCh = c => Math.max(0, c.rcol === undefined ? rcol : c.rcol);
    const ceilOf = c => 1024 * (c.R / (c.R + rcCh(c))) * (1 - senCh(c).vcesat / p.vmin);
    const icSatOf = c => p.vmax / (rcCh(c) + c.R);
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
        ceilS: ceilOf(c), icSat: icSatOf(c), icMax: senCh(c).icmax, rcol: rcCh(c), sensor: senCh(c).name,
        sw: tt => 1 - 2 * Math.exp(-tt / c.tau) / (1 + xi)
      };
    });
    const blank = {
      steps: 0, thr: 0, mg: 0, full: 0, i: 0, src: 0, t: tfirst, f: 0, tau: tauOf(p, p.R), R: p.R,
      x: Math.exp(-T / tauOf(p, p.R)), tz: 0, rcol, sensor: sens.name, icMax: sens.icmax,
      ceilS: ceilOf({ R: p.R, rcol, sensor: p.sensor }), icSat: icSatOf({ R: p.R, rcol }), sw: () => 0
    };
    const worst = chans.reduce((a, b) => b.mg < a.mg ? b : a, chans[0] || { ...blank, ballS: 0, clrS: 0, perStep: lsb / p.R });
    const steps = worst.steps, thr = worst.thr, A = worst.steps * lsb;
    const peak = chans.reduce((a, b) => b.full > a.full ? b : a, chans[0] || blank).full;
    const tau = Math.max(...chans.map(c => c.tau), 0) || blank.tau;   // the slowest sensor sets the bounds
    const Rmax = Math.max(...chans.map(c => c.R), 0) || p.R;
    const x = worst.x, sw = worst.sw, tzero = worst.tz;
    // A value spans three phases, dark, lit and the dark of the cycle after, and two consecutive
    // values share the middle one, so m values span 2m + 1 phases. Counting D / cycle treats a value
    // as one cycle and overcounts: at a 750 us phase it reports two where the dwell carries one.
    const cycle = 2 * T, reads = Math.max(0, Math.floor((p.D * 1000 / T - 1) / 2));
    // The same relation read the other way round: how fast a ball may cross a sensor and still be
    // read often enough. It travels its own diameter over the detection window, and n values span
    // 2n + 1 phases, so the window is (2k + 1) T for the confirmations and (2k + 3) T with one in hand.
    const vHit = BALL_MM / ((2 * p.k + 1) * T) * 1000, vSpare = BALL_MM / ((2 * p.k + 3) * T) * 1000;
    const duty = tbud / T;                      // share of the core the read block holds
    // p is the Gaussian tail beyond half the gap, the threshold sitting midway between the clear
    // track and the ball. Nothing has measured that the noise really is Gaussian out at 4 sigma, and
    // the two readings are treated as independent, which flicker is not. The interval that follows is
    // the mean of a Poisson process, so it has no minimum, and it moves by decades on small changes
    // in the noise: at sixteen channels a 10 % worse noise floor takes 21 hours down to 50 minutes.
    const goodMargin = marginFor(GOOD_S, p.N, T, p.k), thinMargin = marginFor(THIN_S, p.N, T, p.k);
    // Release sits m_r sigma under the report threshold, m_r from the year the same way as the report
    // margin, which is a full gap of goodMargin sigma with the threshold halfway, so m_r = goodMargin / 2.
    // It has to keep 2 sigma above the clear track, or a ball that leaves is never released.
    const dHyst = goodMargin / 2 * noise;
    chans.forEach(c => { c.relS = c.thr - dHyst; c.relOK = c.relS - c.clrS >= 2 * noise; });
    const tail = qtail(Math.max(0, snrOf(steps, noise)) / 2);
    const rate = p.N * (1e6 / (2 * T)) * Math.pow(tail, p.k);
    const falseEvery = rate > 0 ? 1 / rate : Infinity;
    const ovhCeil = (T - guard - tau * Math.LN2) / p.N - tconv;
    // Head is what the tightest channel has left between its ball reading, the ambient sitting under
    // it and its own ceiling. Negative means that channel clips and reads the ceiling for both phases.
    // With a mixed board the tightest channel and the strongest need not be the same one, so the
    // report names the channel the headroom belongs to.
    chans.forEach(c => { c.head = c.ceilS - ambTot - c.full; });
    const tight = chans.reduce((a, b) => b.head < a.head ? b : a, chans[0] || blank);
    const head = chans.length ? tight.head : ceilOf({ R: p.R, rcol, sensor: p.sensor }) - ambTot;
    const icWorst = chans.reduce((a, b) => (b.icSat / b.icMax) > (a.icSat / a.icMax) ? b : a,
      chans[0] || blank);
    return {
      T, tconv, tbud, tfirst, tlast, tau, x, sw, tzero, A, lsb, steps, thr, chans, worst, peak,
      flick, noise, refRes, residFrac, ovhCeil, bus, railRel, railStep, railPred, ambTot,
      sens, rcol, head, tight, icWorst, icSat: icWorst.icSat, icMax: icWorst.icMax,
      ceilS: tight.ceilS, atCeil: ambTot + tight.full,
      snr: snrOf(steps, noise), falseEvery, goodMargin, thinMargin, cycle, reads, vHit, vSpare,
      signOK: chans.every(c => c.t > c.tz), fitOK: tfirst > 0, readOK: reads >= p.k,
      perfOK: worst.f >= PERF_MIN, dHyst, relOK: chans.every(c => c.relOK),
      rangeOK: head > 0, icOK: chans.every(c => c.icSat <= c.icMax),
      // C_PIN 7 pF sits at the pad and C_SAMPLE 20 pF behind the switch, so the source charges
      // both: Rmax x 27 pF plus the switch's 1 kOhm x 20 pF, and ten bits need ln(1024) of that.
      acqWin: 1.5 / p.fspi, acqNeed: (Rmax * 0.027 + 0.02) * 6.9, guard, duty,
      swFirst: worst.f, swLast: chans.length ? chans[chans.length - 1].f : 0, mV: worst.steps * lsb
    };
  }

  // A configuration is usable when every one of these holds. Anything that fails one is not ranked.
  const usable = m => m.fitOK && m.signOK && m.readOK && m.perfOK && m.relOK;

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
    // Then the settling target. It outranks the spare reading because the spare guards against a read
    // the firmware misses, which it counts and can report, while a tau read too early is wrong in
    // silence and every figure below rests on it.
    const af = a.sw >= AIM_F, bf = b.sw >= AIM_F;
    if (af !== bf) return af;
    if (!af && Math.abs(a.sw - b.sw) > 1e-9) return a.sw > b.sw;
    const need = k + 1;                         // the confirmations, plus one in hand
    const ae = a.reads >= need, be = b.reads >= need;
    if (ae !== be) return ae;                   // then catching the ball, which cannot be recovered
    if (!ae && a.reads !== b.reads) return a.reads > b.reads;
    // Then the fastest ball the phase still confirms. Settling rises with the phase and ball speed
    // falls with it, so once the target is met the shortest phase that meets it is the best one.
    if (Math.abs(a.vHit - b.vHit) > 1e-9) return a.vHit > b.vHit;
    if (Math.abs(a.duty - b.duty) > 1e-6) return a.duty < b.duty;   // then the cheaper phase
    if (a.kept !== b.kept) return a.kept;       // then the resistor already fitted, over a new one
    return a.fe > b.fe;
  }

  // Two requirements bracket the phase, and both are inequalities that solve directly.
  //
  //   floor   N (t_conv + t_ovh) + jitter        the block and its guard have to fit inside one phase
  //   ceiling D / (2 (k + 1) + 1)                k + 1 values span that many phases, so this many fit the dwell
  //
  // Inside the bracket every phase length delivers the same number of readings, so the readings step of
  // the ranking cannot separate two of them. The bracket is where a reading in hand is still free, and
  // the ranking leaves it where the settling target lies above the ceiling.
  function phaseWindow(p, bounds) {
    const step = bounds.step || 1, tconv = p.clk / p.fspi;
    const fit = Math.ceil((Math.ceil(p.N * (tconv + p.tovh) + p.jit) + START) / step) * step;
    const cap = Math.floor(p.D * 1000 / (2 * (p.k + 1) + 1) / step) * step;
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
      if (!m.rangeOK || !m.icOK) continue;
      const cand = {
        T, R, kept: R === keepR, fe: Math.min(3.15e9, m.falseEvery), snr: m.snr,
        reads: m.reads, duty: m.duty, steps: m.steps, sw: m.worst.f, worst: m.worst.name,
        vHit: m.vHit, vSpare: m.vSpare
      };
      if (betterThan(cand, best, p.k)) best = cand;
    }
    return best;
  }

  function propose(p, bounds) {
    const keepR = p.R, rs = p.perMode === 'each' ? [keepR] : PULLDOWNS, w = phaseWindow(p, bounds);
    let best = null;
    // The ceiling of the bracket no longer wins by construction. It is the longest phase that still
    // carries a reading in hand, and the settling target sits above it at most channel counts, so the
    // whole range the control offers is ranked and betterThan decides where the two collide. The floor
    // is where the read block stops fitting, and nothing below it is a configuration at all.
    for (let T = Math.max(w.lo, bounds.min); T <= bounds.max; T += w.step) {
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
        sensor: p.perMode === 'each'
          ? `per channel, see the channel list; ${[...new Set(m.chans.map(c => c.sensor))].join(' and ')}`
          : `${m.sens.name}, ${m.sens.sheet}`,
        collectorLoadKilohm: p.perMode === 'each' ? 'per channel, see the channel list' : m.rcol,
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
        phaseCeilingReason: 'a value spans three phases and two consecutive values share the middle one, so m values span 2m + 1 phases. The dwell has to carry the confirmations, which puts the ceiling at D / (2 (k + 1) + 1)'
      },
      driver: {
        confirmationsRequired: p.k,
        darkSample: 'the mean of the two dark phases either side of the lit one, always',
        readOrderPolicy: 'strongest first, fixed. The earliest slot in the block has settled least, so it goes to the part that can spare it and the weakest part is read last',
        readOrder: m.chans.map(c => c.name),
        correction: Math.round((1 + m.railRel) * 1e4) / 1e4,
        correctionApplyAs: 'value = lit - correction * dark, per reading, per channel',
        correctionFrom: 'the 1 subtracts the ambient, the rest is the measured rail step between the lit and the dark phase; of that step the supply cable accounts for the bus current of this channel count against the 0.1 ohm bound, and the module for the remainder',
        railStepMillivolts: p.railmv,
        railResidualBoundMillivolts: Math.round(m.residFrac * 3300 * 10) / 10
      },
      channels: m.chans.map(c => ({
        readSlot: c.i, name: c.name,
        sampledAtMicros: r1(c.t), settledFraction: r2(c.f),
        sensor: c.sensor, pullDownKilohm: r2(c.R), collectorLoadKilohm: r2(c.rcol),
        tauMicros: r1(c.tau),
        nodeCeilingSteps: r1(c.ceilS), headroomSteps: r1(c.head), saturationMilliamps: r2(c.icSat),
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
        releaseThresholdClearsClearTrack: m.relOK,
        readPerformanceFloorPercent: PERF_MIN * 100,
        readPerformanceFloorFrom: 'below half the swing the read sits on the steep part of the settling curve and multiplies an error in tau instead of following it. tau is not measured, and its two candidates stand a factor of 3.5 apart. The floor comes out once tau is measured on the built board',
        converterAcquiresInWindow: m.acqNeed <= m.acqWin,
        channelStaysInRange: m.rangeOK,
        tightestChannel: m.tight.name,
        channelCeilingSteps: r1(m.ceilS),
        headroomSteps: r1(m.head),
        channelCeilingFrom: 'the collector load and the pull-down divide the rail, and V_CE(sat) comes off the top. The converter reference is the same rail, so the ceiling in steps is that divider against full scale, taken at the rail minimum where V_CE(sat) is the largest share of it',
        collectorCurrentInsideMaximum: m.icOK,
        collectorSaturationMilliamps: r2(m.icSat),
        collectorMaximumMilliamps: m.icMax
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
    GOOD_S, THIN_S, SELF, SCENES, START, BALL_MM, PERF_MIN, AIM_F, PULLDOWNS, SENSORS,
    fig6, curveAt, sensorOf, qtail, marginFor, every, snrOf,
    tauOf, newChannels, chOrder, model, usable, betterThan,
    phaseWindow, bestPullDown, propose, sweep, usableRange, exportDoc
  };
})();

if (typeof module !== 'undefined' && module.exports) module.exports = IRTM;
