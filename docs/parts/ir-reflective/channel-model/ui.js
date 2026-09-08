// IR channel model: the controls, the figures and the readouts.
// Everything that touches the document lives here. The computation is in model.js, reached through
// IRTM, and receives a plain parameter object rather than reading a control itself.

const $ = id => document.getElementById(id);
const v = id => parseFloat($(id).value);
const esc = t => String(t).replace(/[&<>"]/g, ch => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[ch]));
const f1 = n => n.toFixed(1), f2 = n => n.toFixed(2), f0 = n => Math.round(n);

const IN = ['N', 'tovh', 'jit', 'T', 'D', 'k', 'R', 'iph', 'iclr', 'taumodel', 'taufix', 'scene', 'railmv'];
const DEF = {
  N: 16, tovh: 2, jit: 10, T: 600, D: 3, k: 2, R: 4.7, iph: 50, iclr: 10, vref: 3.383,
  taumodel: 'linear', taufix: 62, nconv: 0.32, railmv: 41, scene: 'led', amb: 60, ambself: 20,
  mod: 60, mains: 100
};
const COLS = [['ball', 'ball µA', 0.1, 1], ['clear', 'clear µA', 0.1, 1], ['R', 'R kΩ', 0.1, 0], ['tau', 'τ µs', 1, 0]];

let CH = [], perMode = 'all', lastOpt = null;

// Everything the model needs, read off the controls in one place.
const params = () => ({
  N: v('N'), R: v('R'), clk: v('clk'), fspi: v('fspi'), tovh: v('tovh'), jit: v('jit'),
  vref: v('vref'), amb: v('amb'), ambself: v('ambself'), mains: v('mains'), mod: v('mod'),
  railmv: v('railmv'), nconv: v('nconv'), D: v('D'), k: v('k'), T: v('T'),
  iph: v('iph'), iclr: v('iclr'), taumodel: $('taumodel').value, taufix: v('taufix'),
  CH, perMode
});
const bounds = () => ({ step: +$('T').step || 1, min: +$('T').min, max: +$('T').max });

function fillCH(keepNames) { CH = IRTM.newChannels(params(), keepNames ? CH : null); }
function syncR() {
  [...$('Rseg').children].forEach(c => c.setAttribute('aria-pressed', String(c.dataset.v === $('R').value)));
}

function metrics(p, m) {
  const rows = [
    ['Read starts at', f0(m.tfirst) + ' <small>µs</small>', true],
    ['Worst channel', (m.worst ? esc(m.worst.name || '?') + ' <small>in slot ' + m.worst.i + '</small>' : '—')],
    ['Worst channel settles', f0(m.swFirst * 100) + ' <small>%</small>'],
    ['Gap, worst channel', f1(m.steps) + ' <small>steps</small>'],
    ['Gap for one a year', f0(m.goodMargin * m.noise) + ' <small>steps</small>'],
    ['Threshold, worst channel', f1(m.thr) + ' <small>steps</small>'],
    ['Release threshold, worst channel', f1(m.worst.relS) + ' <small>steps</small>'],
    ['Current for one step', f2(m.worst.perStep) + ' <small>µA</small>'],
    ['Gap over noise', f1(m.snr) + '&thinsp;<small>×</small>', true],
    ['τ, slowest sensor', f0(m.tau) + ' <small>µs</small>'],
    ['Read block', f0(m.tbud) + ' <small>µs</small>'],
    ['Cycle', f1(m.cycle / 1000) + ' <small>ms</small>'],
    ['Readings per pass', String(m.reads)],
    ['Last channel settles', f0(m.swLast * 100) + ' <small>%</small>'],
    ['Sign inverts at', f0(m.tzero) + ' <small>µs</small>'],
    ['Firmware ceiling', (m.ovhCeil > 0 ? f1(m.ovhCeil) : '0') + ' <small>µs/ch</small>'],
    ['Core held by the block', f0(m.duty * 100) + ' <small>%</small>'],
    ['False report, mean wait', IRTM.every(m.falseEvery), true]
  ];
  $('metrics').innerHTML = rows.map(r =>
    `<div class="m${r[2] ? ' hero' : ''}"><dt>${r[0]}</dt><dd>${r[1]}</dd></div>`).join('');
  const chip = (ok, label, detail) =>
    `<span class="chip ${ok === true ? 'ok' : ok === false ? 'no' : 'warn'}"><b>${ok === true ? '✓' : ok === false ? '✕' : '!'}</b>${label} <span style="color:var(--muted)">${detail}</span></span>`;
  $('checks').innerHTML = [
    chip(m.fitOK, 'Block fits the phase, jitter included', f0(m.tbud + m.guard) + ' of ' + f0(p.T) + ' µs'),
    chip(m.signOK, 'Every read is past its sign inversion', f0(m.worst.t) + ' vs ' + f0(m.worst.tz) + ' µs'),
    chip(m.readOK, 'Pass delivers enough readings', m.reads + ' of ' + p.k),
    chip(m.perfOK, 'Read performance carries a wrong τ', f0(m.swFirst * 100) + ' of ' + f0(IRTM.PERF_MIN * 100) + ' %'),
    chip(m.acqNeed <= m.acqWin, 'Converter acquires in its window', f1(m.acqNeed) + ' of ' + f1(m.acqWin) + ' µs'),
    chip(m.relOK, 'Release threshold clears the clear track', f1(m.chans.length ? Math.min(...m.chans.map(c => c.relS - c.clrS)) : 0) + ' of ' + f1(2 * m.noise) + ' steps'),
    // the lit reading sits on top of the ambient one, so bright rooms run the channel into its ceiling
    chip(m.ambTot + m.peak < 1024, 'Channel stays inside the range', f0(m.ambTot + m.peak) + ' of 1024 steps'),
    chip(m.falseEvery >= IRTM.GOOD_S ? true : m.falseEvery >= IRTM.THIN_S ? null : false, 'False reports', IRTM.every(m.falseEvery) + ' apart, gap ' + f1(m.snr) + '× the noise')
  ].join('');
}

function traceChart(m) {
  const T = m.T, W = 760, H = 320, L = 54, Rp = 16, Tp = 32, B = 44, pw = W - L - Rp, ph = H - Tp - B;
  const X = t => L + t / T * pw, Y = u => Tp + (1 - u) * ph;
  const v0 = m.x / (1 + m.x), v1 = 1 / (1 + m.x);
  let lit = '', drk = '';
  for (let i = 0; i <= 200; i++) {
    const t = T * i / 200;
    lit += (i ? 'L' : 'M') + X(t).toFixed(1) + ' ' + Y(1 - (1 - v0) * Math.exp(-t / m.tau)).toFixed(1);
    drk += (i ? 'L' : 'M') + X(t).toFixed(1) + ' ' + Y(v1 * Math.exp(-t / m.tau)).toFixed(1);
  }
  let g = '';
  for (let u = 0; u <= 1.001; u += 0.25) {
    g += `<line x1="${L}" y1="${Y(u).toFixed(1)}" x2="${L + pw}" y2="${Y(u).toFixed(1)}" stroke="var(--line)" stroke-width="1"/>` +
      `<text x="${L - 9}" y="${(Y(u) + 4).toFixed(1)}" text-anchor="end" fill="var(--muted)" font-family="var(--mono)" font-size="11">${Math.round(u * 100)}%</text>`;
  }
  const marks = [0, T * .25, T * .5, T * .75, T].map(t =>
    `<text x="${X(t).toFixed(1)}" y="${H - B + 20}" text-anchor="middle" fill="var(--muted)" font-family="var(--mono)" font-size="11">${Math.round(t)}</text>`).join('');
  const tf = Math.max(0, m.worst ? m.worst.t : m.tfirst);   // the channel the traces belong to
  const gapTop = Y(1 - (1 - v0) * Math.exp(-tf / m.tau)), gapBot = Y(v1 * Math.exp(-tf / m.tau));
  const zx = X(Math.min(T, m.tzero));
  // The block is armed at tfirst and runs tbud. A timer fires late, never early, so the guard sits at
  // the tail of the block, and what is left between that and T is the room the phase keeps in hand.
  const b0 = Math.max(0, m.tfirst), b1 = Math.min(T, b0 + m.tbud);
  const bs = X(b0), be = X(b1), je = X(Math.min(T, b1 + m.guard));
  const jLeft = be + 44 > L + pw;   // no room for the label to the right of the band
  // the label flips to the left of the mark near the right edge, where it would otherwise run off
  const rx = X(tf), rEnd = rx > L + pw - 170;
  const rlab = `<line x1="${rx.toFixed(1)}" y1="${Tp - 7}" x2="${rx.toFixed(1)}" y2="${Tp}" stroke="var(--accent)" stroke-width="1.5"/>`
    + `<text x="${(rEnd ? rx - 5 : rx + 5).toFixed(1)}" y="${Tp - 9}" text-anchor="${rEnd ? 'end' : 'start'}" fill="var(--accent)" `
    + `font-family="var(--mono)" font-size="11" font-weight="600">reading starts at ${Math.round(tf)} µs</text>`;
  $('trace').innerHTML = `
    <rect x="${L}" y="${Tp}" width="${pw}" height="${ph}" fill="none" stroke="var(--line)"/>
    ${g}
    <rect x="${L}" y="${Tp}" width="${(zx - L).toFixed(1)}" height="${ph}" fill="var(--fail)" opacity="0.09"/>
    <rect x="${bs.toFixed(1)}" y="${Tp}" width="${(be - bs).toFixed(1)}" height="${ph}" fill="var(--shade)"/>
    ${je > be ? `<rect x="${be.toFixed(1)}" y="${Tp}" width="${(je - be).toFixed(1)}" height="${ph}" fill="var(--warn)" opacity="0.20"/>
    <text x="${(jLeft ? be - 4 : be + 4).toFixed(1)}" y="${Tp + ph - 7}" text-anchor="${jLeft ? 'end' : 'start'}" fill="var(--warn)" font-family="var(--mono)" font-size="11">jitter</text>` : ''}
    <line x1="${zx.toFixed(1)}" y1="${Tp}" x2="${zx.toFixed(1)}" y2="${Tp + ph}" stroke="var(--fail)" stroke-width="1.5" stroke-dasharray="4 4"/>
    <text x="${(zx + 5).toFixed(1)}" y="${Tp + 14}" fill="var(--fail)" font-family="var(--mono)" font-size="11">sign flips</text>
    <path d="${lit}" fill="none" stroke="var(--lit)" stroke-width="2.5"/>
    <path d="${drk}" fill="none" stroke="var(--node)" stroke-width="2.5"/>
    <line x1="${X(tf).toFixed(1)}" y1="${gapTop.toFixed(1)}" x2="${X(tf).toFixed(1)}" y2="${gapBot.toFixed(1)}" stroke="var(--accent)" stroke-width="3"/>
    <circle cx="${X(tf).toFixed(1)}" cy="${gapTop.toFixed(1)}" r="3.5" fill="var(--accent)"/>
    <circle cx="${X(tf).toFixed(1)}" cy="${gapBot.toFixed(1)}" r="3.5" fill="var(--accent)"/>
    <text x="${(X(tf) + 8).toFixed(1)}" y="${((gapTop + gapBot) / 2 + 4).toFixed(1)}" fill="var(--accent)" font-family="var(--mono)" font-size="12" font-weight="600">${Math.round(m.swFirst * 100)}%</text>
    ${rlab}
    ${marks}
    <text x="${L + pw / 2}" y="${H - 6}" text-anchor="middle" fill="var(--muted)" font-family="var(--sans)" font-size="11.5">microseconds into the phase</text>`;
}

// Each channel is read one conversion later than the one before it, so within a single block the
// same ball produces a different reading depending on position. The threshold itself cannot be
// computed here: it sits halfway between two measured numbers, per channel. What this shows is the
// spread the calibration has to absorb, and which position falls under the line first as N grows.
function chanChart(p, m) {
  const N = p.N, ch = m.chans;
  if (!ch.length) { $('chan').innerHTML = ''; return; }
  const rot = ch.some(c => c.name.length > 3);
  const W = 760, H = rot ? 322 : 288, L = rot ? 74 : 58, Rp = 22, Tp = 16, B = rot ? 84 : 50, pw = W - L - Rp, ph = H - Tp - B;
  $('chan').setAttribute('viewBox', '0 0 ' + W + ' ' + H);
  const nice = x => {
    const e = Math.pow(10, Math.floor(Math.log10(Math.max(x, 1e-6))));
    const q = x / e; return (q <= 1 ? 1 : q <= 2 ? 2 : q <= 2.5 ? 2.5 : q <= 5 ? 5 : 10) * e;
  };
  // The bar is the gap the decision lives on. What it has to reach is the clear-track reading plus
  // the margin the day target asks for, so the requirement is a level in steps rather than a count
  // is the midpoint of the bar and is not drawn, it carries nothing. The release threshold sits m.dHyst
  // under it and has to clear the clear-track reading by 2 sigma, or a ball that leaves is never
  // released; that is the one mark drawn, and it turns red where it fails.
  const need = c => c.clrS + m.goodMargin * m.noise, thin = c => c.clrS + m.thinMargin * m.noise;
  const yMax = nice(Math.max(...ch.map(c => Math.max(c.ballS, need(c))), 1));
  const X = i => L + (N > 1 ? (i / (N - 1)) * pw : pw / 2), Y = st => Tp + (1 - Math.min(st, yMax) / yMax) * ph;
  let g = '';
  [0, .2, .4, .6, .8, 1].forEach(f => {
    const st = yMax * f;
    g += `<line x1="${L}" y1="${Y(st).toFixed(1)}" x2="${L + pw}" y2="${Y(st).toFixed(1)}" stroke="var(--line)"/>` +
      `<text x="${L - 9}" y="${(Y(st) + 4).toFixed(1)}" text-anchor="end" fill="var(--ink-2)" font-family="var(--mono)" font-size="11">${+st.toFixed(1)}</text>`;
  });
  const xs = N > 1 ? ch.map(c => X(c.i)) : [L, L + pw];
  const lv = f => N > 1 ? ch.map(c => Y(f(c))) : [Y(f(ch[0])), Y(f(ch[0]))];
  const pts = (x, y) => x.map((val, i) => `${val.toFixed(1)} ${y[i].toFixed(1)}`);
  const yThin = lv(thin), yNeed = lv(need), top = Tp, bot = Tp + ph;
  const zones =
    `<path d="M${pts(xs, yThin).join(' L')} L${(L + pw).toFixed(1)} ${bot} L${L} ${bot} Z" fill="var(--fail)" opacity="0.13"/>`
    + `<path d="M${pts(xs, yNeed).join(' L')} L${pts(xs, yThin).reverse().join(' L')} Z" fill="var(--warn)" opacity="0.15"/>`
    + `<path d="M${pts(xs, yNeed).join(' L')} L${(L + pw).toFixed(1)} ${top} L${L} ${top} Z" fill="var(--pass)" opacity="0.11"/>`;
  let ballL = '', clrL = '', needL = '', thinL = '', marks = '', dots = '';
  const hw = N > 12 ? 5 : 7;                     // half width of the release marks
  ch.forEach((c, i) => {
    const x = X(c.i).toFixed(1);
    ballL += (i ? 'L' : 'M') + x + ' ' + Y(c.ballS).toFixed(1);
    clrL += (i ? 'L' : 'M') + x + ' ' + Y(c.clrS).toFixed(1);
    needL += (i ? 'L' : 'M') + x + ' ' + Y(need(c)).toFixed(1);
    thinL += (i ? 'L' : 'M') + x + ' ' + Y(thin(c)).toFixed(1);
    const xn = X(c.i), yR = Y(Math.max(0, c.relS)).toFixed(1);
    marks += `<line x1="${(xn - hw).toFixed(1)}" y1="${yR}" x2="${(xn + hw).toFixed(1)}" y2="${yR}" stroke="${c.relOK ? 'var(--node)' : 'var(--fail)'}" stroke-width="3"/>`;
    const col = c.mg >= m.goodMargin ? 'var(--pass)' : c.mg >= m.thinMargin ? 'var(--warn)' : 'var(--fail)';
    dots += `<circle cx="${x}" cy="${Y(c.ballS).toFixed(1)}" r="${N > 12 ? 3 : 4}" fill="${col}"/>`;
  });
  let ticks = '';
  ch.forEach(c => {
    const tx = X(c.i).toFixed(1), ty = H - B + 18;
    ticks += `<text x="${tx}" y="${ty}" text-anchor="${rot ? 'end' : 'middle'}" fill="var(--ink-2)" font-family="var(--mono)" font-size="11"`
      + `${rot ? ` transform="rotate(-38 ${tx} ${ty})"` : ''}>${esc(c.name)}</text>`;
  });
  $('chan').innerHTML = `
    <rect x="${L}" y="${Tp}" width="${pw}" height="${ph}" fill="none" stroke="var(--line)"/>
    ${zones}${g}
    <path d="${thinL}" fill="none" stroke="var(--warn)" stroke-width="1" stroke-dasharray="3 4"/>
    <path d="${needL}" fill="none" stroke="var(--pass)" stroke-width="1.5"/>
    <path d="${clrL}" fill="none" stroke="var(--muted)" stroke-width="1.5"/>
    ${marks}
    <path d="${ballL}" fill="none" stroke="var(--accent)" stroke-width="2"/>
    ${dots}${ticks}
    <text x="${L + pw / 2}" y="${H - 6}" text-anchor="middle" fill="var(--muted)" font-family="var(--sans)" font-size="11.5">the sensors, in the order they are read</text>
    <text x="13" y="${Tp + ph / 2}" text-anchor="middle" transform="rotate(-90 13 ${Tp + ph / 2})" fill="var(--ink-2)" font-family="var(--sans)" font-size="11.5">converter steps</text>`;
}

function sweepChart(p, opt) {
  const b = bounds();
  const W = 760, H = 300, L = 78, Rp = 52, Tp = 16, B = 42, pw = W - L - Rp, ph = H - Tp - B;
  const st = b.step, tLo = b.min, tHi = b.max;
  const fit = IRTM.usableRange(p, b);
  let T0, T1;
  if (fit.lo === null) { T0 = tLo; T1 = tHi; }
  else {
    const pad = Math.max(st * 3, (fit.hi - fit.lo) * 0.15);
    T0 = Math.max(tLo, fit.lo - pad); T1 = Math.min(tHi, fit.hi + pad);
  }
  T0 = Math.min(T0, p.T - st); T1 = Math.max(T1, p.T + st);   // the phase in use stays on the chart
  if (opt) { T0 = Math.min(T0, opt.T - st); T1 = Math.max(T1, opt.T + st); }   // and the one marked best
  const CAP = 3.15e9;                          // a hundred years; past it the axis says nothing more
  // Only phase lengths the slider can reach are plotted. Sampling between them draws the quantised
  // read start as a sawtooth: inside one step the phase keeps growing and flickering more while the
  // read instant stays put, so the curve sags and snaps back at the next step. Not one of those
  // intermediate points is a phase this design can be set to.
  let rmax = 0, rmin = Infinity;
  const pts = IRTM.sweep(p, b, T0, T1).map(mm => {
    const ok = IRTM.usable(mm);
    if (ok) { rmax = Math.max(rmax, mm.reads); rmin = Math.min(rmin, mm.reads); }
    return { T: mm.T, fe: Math.min(CAP, Math.max(0.01, mm.falseEvery)), reads: mm.reads, ok };
  });
  if (!isFinite(rmin)) { rmin = 0; rmax = p.k; }
  const R0 = Math.max(0, Math.min(rmin, p.k) - 1), R1 = Math.max(rmax, p.k) + 1;
  const TIME = [[1, '1 s'], [60, '1 min'], [3600, '1 h'], [86400, '1 day'], [2.6e6, '1 mo'], [3.15e7, '1 yr'], [3.15e9, '100 yr']];
  const fes = pts.map(q => q.fe);
  const F0 = Math.max(0.01, Math.min(Math.min.apply(null, fes), IRTM.THIN_S) / 4);
  const F1 = Math.min(CAP, Math.max(Math.max.apply(null, fes), IRTM.GOOD_S) * 4);
  const lgf = Math.log10(F1 / F0);
  const Y = sec => Tp + (1 - Math.log10(Math.min(F1, Math.max(F0, sec)) / F0) / lgf) * ph;
  const X = t => L + (t - T0) / (T1 - T0) * pw,
    Y2 = r => Tp + (1 - (Math.min(R1, Math.max(R0, r)) - R0) / (R1 - R0)) * ph;
  let g = '';
  TIME.filter(t => t[0] >= F0 && t[0] <= F1).forEach(t => {
    g += `<line x1="${L}" y1="${Y(t[0]).toFixed(1)}" x2="${L + pw}" y2="${Y(t[0]).toFixed(1)}" stroke="var(--line)"/>` +
      `<text x="${L - 9}" y="${(Y(t[0]) + 4).toFixed(1)}" text-anchor="end" fill="var(--accent)" font-family="var(--mono)" font-size="11">${t[1]}</text>`;
  });
  const rstep = Math.max(1, Math.ceil((R1 - R0) / 5));
  for (let r = R0; r <= R1; r += rstep) {
    g += `<text x="${L + pw + 9}" y="${(Y2(r) + 4).toFixed(1)}" fill="var(--node)" font-family="var(--mono)" font-size="11">${r}</text>`;
  }
  const gy = Y(IRTM.GOOD_S), ty = Y(IRTM.THIN_S);
  let bands = `<rect x="${L}" y="${Tp}" width="${pw}" height="${(gy - Tp).toFixed(1)}" fill="var(--pass)" opacity="0.07"/>`
    + `<line x1="${L}" y1="${gy.toFixed(1)}" x2="${L + pw}" y2="${gy.toFixed(1)}" stroke="var(--pass)" stroke-width="1.5"/>`
    + `<text x="${L + pw - 4}" y="${(gy - 5).toFixed(1)}" text-anchor="end" fill="var(--pass)" font-family="var(--mono)" font-size="11">good from here up</text>`
    + `<line x1="${L}" y1="${ty.toFixed(1)}" x2="${L + pw}" y2="${ty.toFixed(1)}" stroke="var(--warn)" stroke-width="1" stroke-dasharray="3 4"/>`
    + `<text x="${L + pw - 4}" y="${(ty - 5).toFixed(1)}" text-anchor="end" fill="var(--warn)" font-family="var(--mono)" font-size="11">thin from here up</text>`;
  let bad = '';
  pts.forEach((q, i) => {
    if (!q.ok && i < pts.length - 1)
      bad += `<rect x="${X(q.T).toFixed(1)}" y="${Tp}" width="${Math.max(1, (X(pts[i + 1].T) - X(q.T))).toFixed(2)}" height="${ph}" fill="var(--fail)" opacity="0.10"/>`;
  });
  let ln = '', rd = '';
  pts.forEach((q, i) => {
    ln += (i ? 'L' : 'M') + X(q.T).toFixed(1) + ' ' + Y(q.fe).toFixed(1);
    rd += (i ? 'L' : 'M') + X(q.T).toFixed(1) + ' ' + Y2(q.reads).toFixed(1);
  });
  const cur = X(p.T);
  const ox = opt ? X(Math.min(T1, Math.max(T0, opt.T))) : null;
  // the curve is drawn at the pull-down in use, so a proposal that moves it is marked off that curve
  const olab = opt && opt.R !== p.R ? `best at ${opt.R} kΩ` : 'best';
  // round tick values whatever the range works out to
  const tstep = [25, 50, 100, 200, 250, 500, 1000].find(q => (T1 - T0) / q <= 7) || 1000;
  let ticks = '';
  for (let t = Math.ceil(T0 / tstep) * tstep; t <= T1; t += tstep)
    ticks += `<text x="${X(t).toFixed(1)}" y="${H - B + 20}" text-anchor="middle" fill="var(--muted)" font-family="var(--mono)" font-size="11">${t}</text>`;
  $('sweep').innerHTML = `
    <rect x="${L}" y="${Tp}" width="${pw}" height="${ph}" fill="none" stroke="var(--line)"/>
    ${bad}${g}${bands}
    <path d="${rd}" fill="none" stroke="var(--node)" stroke-width="2" stroke-dasharray="5 4"/>
    <path d="${ln}" fill="none" stroke="var(--accent)" stroke-width="2.5"/>
    ${ox !== null ? `<line x1="${ox.toFixed(1)}" y1="${Tp}" x2="${ox.toFixed(1)}" y2="${Tp + ph}" stroke="var(--pass)" stroke-width="1.5"/>
      <text x="${(ox + 6).toFixed(1)}" y="${Tp + 13}" fill="var(--pass)" font-family="var(--mono)" font-size="11">${olab}</text>` : ''}
    <line x1="${cur.toFixed(1)}" y1="${Tp}" x2="${cur.toFixed(1)}" y2="${Tp + ph}" stroke="var(--ink)" stroke-width="1.5" stroke-dasharray="3 3"/>
    <text x="${(cur + 6).toFixed(1)}" y="${Tp + ph - 6}" fill="var(--ink)" font-family="var(--mono)" font-size="11">now</text>
    ${ticks}
    <text x="${L + pw / 2}" y="${H - 6}" text-anchor="middle" fill="var(--muted)" font-family="var(--sans)" font-size="11.5">phase length, microseconds</text>
    <text x="13" y="${Tp + ph / 2}" text-anchor="middle" transform="rotate(-90 13 ${Tp + ph / 2})" fill="var(--accent)" font-family="var(--sans)" font-size="11.5">mean wait to a false report</text>
    <text x="${W - 14}" y="${Tp + ph / 2}" text-anchor="middle" transform="rotate(90 ${W - 14} ${Tp + ph / 2})" fill="var(--node)" font-family="var(--sans)" font-size="11.5">readings per pass</text>`;
}

function exportBlock(p, m) {
  const scene = $('scene').selectedOptions[0].textContent;
  $('exp').textContent = JSON.stringify(IRTM.exportDoc(p, m, scene), null, 2);
}

function tableRows(p, m) {
  const each = perMode === 'each', t = $('chtbl');
  $('uniform').hidden = each;
  t.classList.toggle('hide', !each);
  if (!each) { t.dataset.n = ''; return; }
  t.style.gridTemplateColumns = 'minmax(46px,.85fr) repeat(4,minmax(0,1fr))';
  const N = p.N, worstSrc = m.worst ? m.worst.src : -1;
  let h = '<b>name</b>' + COLS.map(c => `<b>${c[1]}</b>`).join('');
  for (let i = 0; i < N; i++) {
    const c = CH[i], bad = i === worstSrc;
    h += `<input type="text" data-i="${i}" data-f="name" value="${esc(c.name)}" maxlength="12"${bad ? ' class="weak"' : ''}>`
      + COLS.map(col => `<input type="number" data-i="${i}" data-f="${col[0]}" value="${c[col[0]]}"`
        + ` min="0.1" max="4000" step="${col[2]}" class="${bad ? 'weak' : ''}${col[3] ? ' ships' : ''}">`).join('');
  }
  if (t.dataset.n !== String(N) || !t.contains(document.activeElement)) { t.innerHTML = h; t.dataset.n = String(N); }
}

function render() {
  syncR();
  // the scene drives the two ambient inputs, so it is applied before the parameters are read
  const sc = IRTM.SCENES[$('scene').value] || IRTM.SCENES.led;
  $('amb').value = sc.room; $('mod').value = sc.mod;
  const p = params();
  $('amb_o').textContent = (sc.room + IRTM.SELF) + ' steps';
  $('ambself_o').textContent = IRTM.SELF + ' steps';
  $('mod_o').textContent = sc.mod ? sc.mod + ' % at 100 Hz' : 'none';
  $('N_o').textContent = p.N;
  $('fspi_o').textContent = p.fspi.toFixed(2) + ' MHz';
  $('clk_o').textContent = p.clk;
  $('vref_o').textContent = p.vref.toFixed(3) + ' V';
  $('nconv_o').textContent = p.nconv.toFixed(2) + ' steps';
  $('tovh_o').textContent = p.tovh.toFixed(2) + ' µs';
  $('jit_o').textContent = p.jit + ' µs';
  $('T_o').textContent = p.T + ' µs';
  $('D_o').textContent = p.D.toFixed(1) + ' ms · ' + (IRTM.BALL_MM / p.D).toFixed(1) + ' m/s';
  $('k_o').textContent = p.k;
  $('iph_o').textContent = p.iph + ' µA';
  $('iclr_o').textContent = p.iclr + ' µA';
  $('taufix_o').textContent = p.taufix + ' µs';
  const tconvNow = p.clk / p.fspi;
  const ceil = (p.T - p.jit - IRTM.tauOf(p, p.R) * Math.LN2) / p.N - tconvNow;
  const over = p.tovh > ceil;
  $('tovh_o').style.color = over ? 'var(--fail)' : 'var(--ink)';
  // floor 0.37 us is t_CSH 270 ns + t_SUCS 100 ns, the chip select cycling once per channel;
  // the ceiling is where the block would have to start ahead of the sign inversion, so the
  // difference comes out negative. ARM_DWT_CYCCNT has to measure under it.
  $('ovhNote').textContent = ceil <= 0 ? 'none' : '0.37 to ' + ceil.toFixed(1) + ' µs';
  $('ovhNote').style.color = over ? 'var(--fail)' : '';
  const m = IRTM.model(p, p.T);
  $('railmv_o').textContent = p.railmv + ' mV';
  $('railpred_o').textContent = m.railPred.toFixed(0) + ' mV';
  $('railerr_o').textContent = (m.residFrac * 3300).toFixed(1) + ' mV';
  $('corr_o').textContent = (1 + m.railRel).toFixed(4);
  $('n1_o').textContent = p.nconv.toFixed(2) + ' steps';
  $('refNote').textContent = m.refRes.toFixed(2) + ' steps';
  $('n3_o').textContent = m.flick.toFixed(2) + ' steps';
  $('n4_o').textContent = m.noise.toFixed(2) + ' steps';
  $('w1').textContent = m.worst ? '· the worst channel, ' + m.worst.name : '';
  $('w3').textContent = $('w1').textContent;
  metrics(p, m); traceChart(m); chanChart(p, m); tableRows(p, m); exportBlock(p, m);
  lastOpt = IRTM.propose(p, bounds()); sweepChart(p, lastOpt);
  try {
    localStorage.setItem('irtm', JSON.stringify(
      Object.fromEntries(IN.map(k => [k, $(k).value]).concat([['ch', CH], ['mode', perMode]]))));
  } catch (e) { }
}

$('chtbl').addEventListener('input', e => {
  const t = e.target; if (t.tagName !== 'INPUT') return;
  if (t.dataset.f === 'name') { CH[+t.dataset.i].name = t.value; render(); return; }
  const val = parseFloat(t.value); if (!isFinite(val) || val <= 0) return;
  CH[+t.dataset.i][t.dataset.f] = val; render();
});

// Published on claude.ai the viewer's frame cannot start a download itself; the downloads
// capability mediates it. Opened as a local file there is no window.claude, and the blob link works.
const DLP = (window.claude && claude.use) ? claude.use('downloads').catch(() => null) : Promise.resolve(null);
$('dl').addEventListener('click', async () => {
  const b = $('dl'), txt = $('exp').textContent, name = 'ir-channel-model.json';
  const say = (t, ms) => { b.textContent = t; setTimeout(() => { b.textContent = 'Download JSON'; }, ms || 2500); };
  const dl = await DLP;
  if (dl) {
    try { await dl.save({ filename: name, data: txt }); say('Saved'); }
    catch (e) {
      const c = e && e.code;
      say(c === 'declined' ? 'Cancelled' : c === 'rate_limited' ? 'One at a time' : 'Use Copy instead', 3000);
    }
    return;
  }
  try {
    const url = URL.createObjectURL(new Blob([txt], { type: 'application/json' }));
    const a = document.createElement('a'); a.href = url; a.download = name;
    document.body.appendChild(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
  } catch (e) { say('Use Copy instead', 3000); }
});

// The sweep marks this same proposal, so the line and the button cannot part company.
$('propose').addEventListener('click', () => {
  const q = lastOpt;
  if (!q) {
    $('optText').textContent = 'Nothing in range satisfies every constraint. Drop a channel, widen the dwell, or accept one reading to confirm.';
    return;
  }
  $('T').value = q.T;
  if (perMode !== 'each') { $('R').value = q.R; fillCH(true); $('chtbl').dataset.n = ''; }
  render();
  $('optText').innerHTML = `Pull-down <b>${q.R} kΩ</b>, phase <b>${q.T} µs</b>, ` +
    `threshold halfway. ` +
    `Worst channel is ${esc(q.worst)} at <b>${q.steps.toFixed(1)}</b> steps of gap, ` +
    `a false report every <b>${IRTM.every(q.fe)}</b>, ${q.reads} readings per pass, ` +
    `<b>${Math.round(q.duty * 100)} %</b> of the core. ` +
    (q.fe < IRTM.GOOD_S
      ? `Nothing in range reaches one a year, so this is the quietest there is and the worst channel stays amber.`
      : `The difference always takes the mean of the two dark readings either side of the lit one.`);
});

$('copy').addEventListener('click', async () => {
  const b = $('copy');
  try { await navigator.clipboard.writeText($('exp').textContent); b.textContent = 'Copied'; }
  catch (e) { b.textContent = 'Clipboard refused'; }
  setTimeout(() => { b.textContent = 'Copy'; }, 2500);
});

$('taufix').addEventListener('input', () => {
  if ($('taumodel').value !== 'fixed') $('taumodel').value = 'fixed';
});

$('modeseg').addEventListener('click', e => {
  const b = e.target.closest('button'); if (!b) return;
  perMode = b.dataset.v;
  [...$('modeseg').children].forEach(c => c.setAttribute('aria-pressed', String(c === b)));
  if (perMode === 'each') fillCH(true);
  $('chtbl').dataset.n = ''; render();
});

['iph', 'iclr', 'taumodel', 'taufix'].forEach(id => $(id).addEventListener('input', () => {
  if (perMode === 'all') { fillCH(true); $('chtbl').dataset.n = ''; }
}));

$('Rseg').addEventListener('click', e => {
  const b = e.target.closest('button'); if (!b) return;
  $('R').value = b.dataset.v;
  if (perMode === 'all') { fillCH(true); $('chtbl').dataset.n = ''; }
  render();
});

IN.forEach(id => $(id).addEventListener('input', render));

$('reset').addEventListener('click', () => {
  Object.entries(DEF).forEach(([k, val]) => { $(k).value = val; });
  fillCH(); $('chtbl').dataset.n = '';
  perMode = 'all';
  [...$('modeseg').children].forEach(c => c.setAttribute('aria-pressed', String(c.dataset.v === 'all')));
  try { localStorage.removeItem('irtm'); } catch (e) { }
  render();
});

try {
  const s = JSON.parse(localStorage.getItem('irtm') || 'null');
  if (s) {
    IN.forEach(k => { if (s[k] !== undefined) $(k).value = s[k]; });
    if (Array.isArray(s.ch) && s.ch.length === 16 && s.ch.every(r => r && typeof r.ball === 'number')) CH = s.ch;
    if (s.mode === 'each' || s.mode === 'all') {
      perMode = s.mode;
      [...$('modeseg').children].forEach(c => c.setAttribute('aria-pressed', String(c.dataset.v === perMode)));
    }
  }
} catch (e) { }

if (!CH.length) fillCH();
render();
