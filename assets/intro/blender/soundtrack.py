"""Mixes the intro's sound track for the film's 260 frames, one WAV in mono at 44.1 kHz.

  python soundtrack.py <paths.json> <out.wav>     (in WSL, with ~/.venvs/audio)

The sources lie in assets/sounds, which the trailer shares: the circus march from march.py, the announcer's call from
announcer.py, the effects of sfx.py, and recordings of the kit: a steel ball rolling on plywood, which roll.py
lays out, the flipper snapping up and falling back, and two steel balls knocked together. The paths file holds
every ball's position for every frame. The chimes, the whoosh, the drain's knock and the logo's kicks, neon and
sparks are synthesized here.

The track, by the film's frames at 25 a second:

  1..50     from the first black frame the march swells in from far away, muffled and wet, and opens to full
            clarity at the kick; a riser builds under it; the announcer calls the mode at 18
  38        the other ball hits bumper 2, far off, with a soft chime
  50        the kickout fires the camera ball out of the scoop, and it rolls from here
  71        the camera ball hits bumper 3, with a chime
  97        the balls collide with a clear metallic clink, the march and the rolls giving way
  108       the camera whips round to the drain
  128..158  the flippers snap up and fall back and miss
  152..164  the march winds down like a stopped tape as the ball drains; the drain knocks at 156
  157..167  in the black the tape starts again, running up to full speed
  167..260  the logo: the march plays on to its last chord at 239 over a pumping kick on every beat, with echo
            and a wide hall, and rings out as the picture fades; a neon tube buzzes, sparks fly where the
            writing light whips fast as it writes PINBALL and as the line draws out, and the flipper fires the
            ball along the line at 207
"""
import json, os, sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import lfilter
from pedalboard import Pedalboard, Reverb, Compressor, Limiter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import roll as rolling

SOUNDS = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "sounds")
SR = 44100
FPS = 25
FRAMES = 260
N = int(FRAMES * SR / FPS)
RNG = np.random.default_rng(5)

# the moments of the film, in frames
VOICE, BUMPER2, KICK, BUMPER3, CLACK, WHIP = 18, 38, 50, 71, 97, 108
FLIPS = ((128, 2), (130, 1), (134, 1), (136, 1), (152, 2), (155, 1))   # (up, frames held) from film.py's flippers()
# the hits in the recordings, the onsets in seconds: the kit's flipper snapping up and falling back, two steel balls
FLIP_UP, FLIP_DOWN, BALLS = (1.523, 2.359), (1.664, 2.514), 2.958
HIT_GAIN = 0.0          # dB on the balls' collision, set so its first 100 ms sit 7.6 dB over the 350 ms before it
STOP = (152, 164)                 # the march winds down
LEVEL_END = 146                   # keep the march's reference level before the tape stops
DRAIN = 156
LOGO, LOGO_FLIP = 167, 207
IMPACT = 228.2          # the last letter of ANAKINRAW lights, from film.py
MARCH = "acestep-circusmarch-sft.wav"
SPIN = 0.4              # s the stopped tape takes to run up to full speed before the logo
# the march's beat, measured from its onsets: 125.3 a minute, the first at 0.47 s; the logo starts on beat 8, a
# downbeat at 4.30 s, and the march's last chord falls on beat 14
BEAT0, BEAT = 0.47, 0.47875
LOGO_BEAT, LAST_BEAT = 8, 14

# the score chimes' notes, in A-flat major, which the march's bass leans on
HZ = {"Eb5": 622.25, "Ab5": 830.61}


def sample(f):
    return int(round((f - 1) / FPS * SR))


def db(v):
    return 10 ** (v / 20)


def load(path):
    """Mono, at SR, its peak at 1."""
    sr, x = wavfile.read(path)
    x = x.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    if sr != SR:
        x = np.interp(np.arange(int(len(x) * SR / sr)) * sr / SR, np.arange(len(x)), x)
    return x / max(1e-9, np.abs(x).max())


def place(track, x, at, gain):
    a, b = max(0, at), min(len(track), at + len(x))
    if b > a:
        track[a:b] += x[a - at:b - at] * gain


def biquad(kind, f, q=0.707):
    w = 2 * np.pi * min(f, 0.45 * SR) / SR; al = np.sin(w) / (2 * q); c = np.cos(w)
    b = {"low": [(1 - c) / 2, 1 - c, (1 - c) / 2], "high": [(1 + c) / 2, -(1 + c), (1 + c) / 2],
         "band": [al, 0.0, -al]}[kind]
    a = [1 + al, -2 * c, 1 - al]
    return np.array(b) / a[0], np.array(a) / a[0]


def filt(x, kind, f, q=0.707):
    b, a = biquad(kind, f, q)
    return lfilter(b, a, x)


def swept(x, kind, freqs, q=0.707, block=256):
    """Filters x block by block, the corner following freqs, one value a sample."""
    out = np.zeros_like(x); zi = np.zeros(2)
    for a in range(0, len(x), block):
        b_, a_ = biquad(kind, freqs[min(a + block // 2, len(x) - 1)], q)
        out[a:a + block], zi = lfilter(b_, a_, x[a:a + block], zi=zi)
    return out


def first_hit(x, longest):
    """The first impact of a take, from its onset until the level falls 36 dB below its peak."""
    w = int(0.004 * SR)
    env = np.sqrt(np.convolve(x ** 2, np.ones(w) / w, mode="same"))
    pk = int(np.argmax(env[:int(0.15 * SR)]))
    on = np.flatnonzero(env[:pk + 1] < env[pk] * db(-30))
    start = max(0, (on[-1] if on.size else 0) - int(0.002 * SR))
    quiet = np.flatnonzero(env[pk:] < env[pk] * db(-36))
    end = min(pk + (quiet[0] if quiet.size else len(x)), start + int(longest * SR))
    y = x[start:end].copy()
    fade = min(len(y), int(0.012 * SR)); y[len(y) - fade:] *= np.linspace(1, 0, fade)
    return y / np.abs(y).max()


def glock(freq, dur=1.8):
    """A glockenspiel bar: the partials of a free bar, the upper ones dying fast."""
    t = np.arange(int(dur * SR)) / SR
    y = sum(a * np.sin(2 * np.pi * freq * r * t) * np.exp(-t / tau)
            for r, a, tau in ((1.0, 1.0, 0.65), (2.76, 0.4, 0.16), (5.40, 0.2, 0.06), (8.93, 0.1, 0.025)))
    att = int(0.0015 * SR); y[:att] *= np.linspace(0, 1, att)
    return y / np.abs(y).max()


def take(x, onset, dur):
    """One hit cut from a recording: from 3 ms before its onset, faded out over its last 15 ms."""
    a = max(0, int((onset - 0.003) * SR))
    y = x[a:a + int(dur * SR)].copy()
    fade = int(0.015 * SR); y[-fade:] *= np.linspace(1, 0, fade)
    return y / np.abs(y).max()


def whoosh(dur=0.55):
    """Air past the ear: noise through a band that sweeps up and back down."""
    n = int(dur * SR); u = np.linspace(0, 1, n)
    centre = 350 * (2200 / 350) ** np.sin(np.pi * u) ** 1.5
    y = swept(RNG.standard_normal(n), "band", centre, q=1.2) * np.sin(np.pi * u) ** 2
    return y / np.abs(y).max()


def knock():
    """The ball dropping into the wooden drain: a low thump that falls in pitch and a short wooden knock."""
    t = np.arange(int(0.4 * SR)) / SR
    f = 65 + 45 * np.exp(-t / 0.03)
    thump = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.09)
    wood = filt(RNG.standard_normal(len(t)), "band", 520.0, 2.5) * np.exp(-t / 0.02) * 0.8
    y = thump + wood
    return y / np.abs(y).max()


def neon(dur):
    """A neon tube's transformer: a buzz on twice the mains frequency, rich in harmonics, that flickers."""
    t = np.arange(int(dur * SR)) / SR
    buzz = sum(np.sin(2 * np.pi * 100.0 * k * t + k) / k ** 0.8 for k in range(1, 30))
    flicker = np.repeat(RNG.random(int(dur * 60) + 1) > 0.12, SR // 60 + 1)[:len(t)].astype(float)
    flicker = np.convolve(flicker, np.ones(64) / 64, mode="same")
    y = buzz * (0.6 + 0.4 * flicker)
    return y / np.abs(y).max()


def sparks(rate, strength):
    """Arcing: short hard snaps, rate a second and strength each, sample by sample, a few of them louder pops."""
    n = len(rate)
    hits = RNG.random(n) < rate / SR
    y = np.zeros(n); y[hits] = RNG.lognormal(0.0, 0.9, hits.sum()) * strength[hits]
    return filt(np.convolve(y, np.exp(-np.arange(48) / 6.0))[:n], "high", 1800.0)


LINE_MM = 209.0         # the gold line's length in film.py's logo
FAST = 26.0             # mm a frame of the writing light above which sparks fly from it


def swing_speed(d):
    """How fast the writing light moves, in mm a frame at every sample: its tip while it rises out of the drain
    and writes PINBALL, then the line's growing end as it draws out from the flipper."""
    last = max(int(k) for k in d["tip"])
    get = lambda key, f: d[key][str(min(f, last))]
    tip = np.array([get("tip", f) for f in range(1, FRAMES + 1)]) * 10
    shown = np.array([get("tip_scale", f) > 0.5 for f in range(1, FRAMES + 1)])
    line = np.array([get("line", f) for f in range(1, FRAMES + 1)])
    v = np.r_[0.0, np.linalg.norm(np.diff(tip, axis=0), axis=1)] * shown + np.r_[0.0, np.diff(line)] * LINE_MM
    v[:LOGO] = 0.0
    vs = np.interp(np.arange(N) / SR * FPS, np.arange(FRAMES), v)
    return lfilter([0.006], [1, -0.994], vs)          # smoothed over about 4 ms, so no frame steps are heard


def swing(speed):
    """Sparks fly from the writing light only where it whips fast, the more and the louder the faster."""
    whip = np.clip((speed - FAST) / FAST, 0.0, 1.0)
    return rms_scale(sparks(2500 * whip ** 1.2, 0.5 + whip), -18.0, whip > 0.05)


def kick(weight=1.0):
    """An electronic kick: a sine falling from 155 to 45 Hz with a click on top, saturated a little so that its
    harmonics carry on a small speaker."""
    t = np.arange(int(0.35 * SR)) / SR
    body = np.sin(2 * np.pi * np.cumsum(45 + 110 * np.exp(-t / 0.03)) / SR) * np.exp(-t / (0.18 * weight))
    click = filt(RNG.standard_normal(len(t)) * np.exp(-t / 0.002), "high", 2500.0) * 0.5
    y = np.tanh((1.2 + 0.4 * (weight - 1)) * (body + click))
    return y / np.abs(y).max()


def logo_music(path):
    """The logo's music, the march itself made modern and spacey: the stopped tape starts again in the black,
    its speed rising from nothing to full over SPIN seconds, so its pitch climbs and its sound opens; from the
    logo's first frame, a downbeat, the march plays on to its last chord over a kick on every beat, pumping with
    it, with a dotted-eighth echo and a wide hall; a heavier kick lands on the last chord and the tails ring
    out as the picture fades."""
    m = load(path)
    p0 = int((BEAT0 + LOGO_BEAT * BEAT) * SR)                                  # the march at the logo's first frame
    start = sample(LOGO)
    part = np.zeros(N)
    place(part, m[p0:int((BEAT0 + (LAST_BEAT + 2) * BEAT) * SR)], start, 1.0)
    t = np.arange(N) / SR
    beats = [start / SR + k * BEAT for k in range(LAST_BEAT - LOGO_BEAT + 1)]
    pump = np.ones(N)
    for b in beats:
        dt = t - b
        pump *= 1 - 0.55 * np.where(dt >= 0, np.exp(-dt / 0.11), np.exp(np.clip(dt, -1, 0) / 0.004))
    part *= pump
    delay = int(0.75 * BEAT * SR)
    echo = np.zeros(N)
    for k, g in enumerate((0.35, 0.18, 0.09), start=1):
        echo[k * delay:] += filt(part, "high", 300.0)[:N - k * delay] * g
    hall = Pedalboard([Reverb(room_size=0.85, damping=0.5, wet_level=1.0, dry_level=0.0, width=0.0)])(
        part[np.newaxis, :].astype(np.float32), SR)[0].astype(np.float64)
    track = part + echo + hall * 0.25
    for k, b in enumerate(beats):
        last = k == len(beats) - 1
        place(track, kick(1.6 if last else 1.0), int(b * SR), 0.9 if last else 0.6)
    n = int(SPIN * SR); tau = np.arange(n) / SR
    rate = (tau / SPIN) ** 2                                                   # the tape's speed, nothing to full
    pos = p0 - SPIN / 3 * SR + tau ** 3 / (3 * SPIN ** 2) * SR                 # the read point, reaching p0
    spin = swept(np.interp(pos, np.arange(len(m)), m), "low", 200 + 15800 * rate) * np.sqrt(rate)
    place(track, spin, start - n, 1.0)
    return track


def rms_scale(x, target_db, active=None):
    """Scales x so that its RMS over the active samples sits at target_db."""
    seg = x if active is None else x[active]
    return x * db(target_db) / max(1e-9, np.sqrt((seg ** 2).mean()))


def march(path):
    """The circus march: in from far away, muffled and wet, open at the kick, then stopped like a tape."""
    m = load(path)
    m = np.r_[np.zeros(int(0.06 * SR)), m]                     # a beat of the march falls on the kick
    t = np.arange(N) / SR
    # the stopped tape: the read speed falls to nothing, so the pitch sinks with it
    t0, t1 = (STOP[0] - 1) / FPS, (STOP[1] - 1) / FPS
    v = np.clip((t - t0) / (t1 - t0), 0, 1)
    pos = np.cumsum((1 - v) ** 2) - 1.0                        # in samples of the march
    x = np.interp(pos, np.arange(len(m)), m, right=0.0) * (1 - v) ** 0.6
    x = filt(x, "high", 30.0)                                   # a nearly stopped tape reads a constant
    # the fade-in: from the first black frame the march swells, muffled behind a low-pass whose corner opens
    # from 600 Hz to 16 kHz, slowly first and fast near the kick
    u = np.clip(t / ((KICK - 1) / FPS), 0, 1)
    corner = 600 * 27 ** (u ** 2)
    corner = np.where(t > t0, np.minimum(corner, 16000 * (1 - 0.97 * v)), corner)
    dry = swept(x, "low", corner)
    wet = Pedalboard([Reverb(room_size=0.9, damping=0.6, wet_level=1.0, dry_level=0.0, width=0.0)])(
        dry[np.newaxis, :].astype(np.float32), SR)[0].astype(np.float64)
    far = 1 - u
    swell = db(np.interp(t, [0.0, (VOICE - 1) / FPS, (KICK - 1) / FPS], [-24.0, -8.0, 0.0]))
    out = (dry * db(-6 * far) + wet * 0.6 * far) * swell + wet * 0.3 * v   # far off at first, a hall as it stops
    return rms_scale(out, -20.0, slice(sample(KICK), sample(LEVEL_END)))


def logo_sound(flip, speed, march_path):
    """The logo: the march runs up out of the black and plays on over a pumping kick, the neon buzzes and sparks
    fly where the writing light whips fast, and the flipper fires the ball."""
    track = np.zeros(N)
    hum = neon((FRAMES - LOGO + 1) / FPS)
    keys = [LOGO, 176, 199, 229, 248, FRAMES]
    hum *= np.interp(np.arange(len(hum)), [sample(f) - sample(LOGO) for f in keys], [0.0, 1.0, 1.0, 0.6, 0.3, 0.0])
    place(track, rms_scale(hum, -30.0), sample(LOGO), 1.0)
    track += swing(speed)
    place(track, flip, sample(LOGO_FLIP), db(-3))
    track += rms_scale(logo_music(march_path), -18.0, slice(sample(LOGO), sample(LOGO + 50)))     # 2 dB over the march
    return track


def main():
    paths, out = sys.argv[1:3]
    tests = SOUNDS
    sfx = lambda name: load(os.path.join(tests, "sfx", f"sfx-{name}-11.wav"))
    d = json.load(open(paths))
    last = max(int(k) for k in d["cam"])                       # the balls rest after the paths' last frame
    pos = {who: [d[who][str(min(f, last))] for f in range(1, FRAMES + 1)] for who in ("cam", "other", "logo")}

    music = march(os.path.join(tests, MARCH))

    fx = np.zeros(N)
    # the riser climbs to the kick
    riser = sfx("riser")[:sample(KICK)]
    riser *= np.linspace(0, 1, len(riser)) ** 2
    place(fx, rms_scale(riser, -30.0), 0, 1.0)
    # the announcer, in the hall behind the music
    place(fx, rms_scale(load(os.path.join(tests, "multiball-am_michael.wav")), -24.0), sample(VOICE), 1.0)
    # the impacts
    bumper, kick = first_hit(sfx("bumper"), 0.2), first_hit(sfx("kickout"), 0.3)
    place(fx, filt(bumper, "low", 2500.0), sample(BUMPER2), db(-16))
    place(fx, kick, sample(KICK), db(-4))
    place(fx, bumper, sample(BUMPER3), db(-2))
    # the balls collide: the recording's strike with its full steel ring, a clink of steel over it, the whole
    # hit a little dull above 5 kHz, and the march and the rolls give way so it rings clear
    clack = take(load(os.path.join(tests, "clack-recording.wav")), BALLS, 0.35)
    t = np.arange(len(clack)) / SR
    clink = sum(a * np.sin(2 * np.pi * f * t + p) * np.exp(-t / tau) for f, a, tau, p in
                ((2700, 1.0, 0.12, 0.2), (3650, 0.8, 0.09, 1.3), (4650, 0.6, 0.07, 2.2), (5840, 0.45, 0.05, 0.6),
                 (7140, 0.3, 0.04, 1.9)))
    clink[:int(0.0005 * SR)] *= np.linspace(0, 1, int(0.0005 * SR))
    hit = filt(clack / np.abs(clack).max() * db(10) + clink / np.abs(clink).max() * db(5), "low", 5000.0)
    place(fx, hit, sample(CLACK), db(HIT_GAIN))
    t = np.arange(N) / SR - (CLACK - 1) / FPS
    duck = 1 - 0.8 * np.exp(-np.abs(t) / np.where(t >= 0, 0.25, 0.004))
    music *= duck
    place(fx, whoosh(), sample(WHIP), db(-20))
    # each flip snaps up and falls back two frames after its hold, the takes alternating
    flipper = load(os.path.join(tests, "flipper-recording.wav"))
    ups = [take(flipper, t, 0.13) for t in FLIP_UP]
    downs = [take(flipper, t, 0.13) for t in FLIP_DOWN]
    for k, (f, hold) in enumerate(FLIPS):
        place(fx, ups[k % 2], sample(f), db(-7))
        place(fx, downs[k % 2], sample(f + hold + 2), db(-10))
    flip = ups[0]
    place(fx, knock(), sample(DRAIN), db(-6))
    # the score chimes at the bumpers, climbing
    for f, nm, g in ((BUMPER2, "Eb5", -24), (BUMPER3, "Ab5", -15)):
        place(fx, glock(HZ[nm]), sample(f), db(g))
    # the rolls: the camera ball close, the other ball by its distance, the logo's ball on the line
    rec = rolling.load(os.path.join(tests, "roll-recording.wav"))
    runs = rolling.pool(rec)
    ref = np.median([np.sqrt((rec[a:b] ** 2).mean()) for a, b in runs])
    level = db(-24.0) / ref
    cam = rolling.roll(rec, rolling.speed(pos["cam"]), FPS, seed=7, normalize=False)[:N]
    other = rolling.roll(rec, rolling.speed(pos["other"]), FPS, seed=8, normalize=False)[:N]
    dist = np.linalg.norm(np.subtract(pos["cam"], pos["other"]), axis=1) * 10
    near = np.interp(np.arange(N) / SR * FPS, np.arange(FRAMES), 1 / (1 + dist / 25))
    logo = rolling.roll(rec, rolling.speed(pos["logo"]), FPS, seed=9, normalize=False)[:N]
    rolls = (cam + other * near) * level * duck + logo * level * db(-4)

    # the bed of march and rolls is compressed; the hits go on top of it untouched, so they keep their punch
    process = lambda board, x: board(x[np.newaxis, :].astype(np.float32), SR)[0].astype(np.float64)
    bed = process(Pedalboard([Compressor(threshold_db=-16, ratio=2.5, attack_ms=5, release_ms=120)]), music + rolls)
    finale = logo_sound(flip, swing_speed(d), os.path.join(tests, MARCH))
    mix = process(Pedalboard([Limiter(threshold_db=-1.0)]), bed + fx + finale)
    mix = mix / max(1e-9, np.abs(mix).max()) * db(-1)
    fade = int(0.3 * SR); mix[-fade:] *= np.linspace(1, 0, fade) ** 2       # silent as the picture goes black
    wavfile.write(out, SR, (mix * 32767).astype(np.int16))
    print(out, "%.2f s" % (len(mix) / SR))


if __name__ == "__main__":
    main()
