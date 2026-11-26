"""Builds the steel ball's rolling on the wooden playfield from a recording, following the ball's speed in the film.

  python roll.py <recording.wav> <paths.json> <out.wav> [cam|other]     (in WSL, with ~/.venvs/audio)

The recording is a steel ball rolling on bare plywood, mono. The paths file holds the world position of the
camera ball and of the other ball for every frame, in Blender units of 10 mm, and the frame rate.

The rolling stretches of the recording form a pool, cut where the level holds steady, so the ball's stops and
turns stay out. The output is laid from grains of that pool, 200 ms long and overlapping by half, each from a
random place in it. Each grain is brought to the pool's median level, then scaled by the ball's speed in the film
and played a little faster or slower with it, so a faster ball sounds louder and brighter.
"""
import json, sys
import numpy as np
from scipy.io import wavfile

SR = 44100
V_REF = 4.0             # mm a frame, the ride's cruising speed, where the roll is at full level
GRAIN = 0.2             # s
STEADY = 6.0            # dB below the recording's loud rolling level that still counts as steady rolling


def load(path):
    sr, x = wavfile.read(path)
    x = x.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    x /= np.abs(x).max()
    if sr != SR:
        x = np.interp(np.arange(int(len(x) * SR / sr)) * sr / SR, np.arange(len(x)), x)
    return x


def pool(x):
    """The runs of steady rolling, as (start, end) in samples."""
    w = int(0.05 * SR)
    rms = np.array([np.sqrt((x[a:a + w] ** 2).mean()) for a in range(0, len(x) - w, w)])
    db = 20 * np.log10(rms + 1e-9)
    steady = db > np.percentile(db, 80) - STEADY
    runs, start = [], None
    for i, on in enumerate(np.r_[steady, False]):
        if on and start is None:
            start = i
        elif not on and start is not None:
            a, b = (start + 2) * w, (i - 2) * w           # 100 ms off each end, away from the stops
            if b - a > 0.3 * SR:
                runs.append((a, b))
            start = None
    return runs


def speed(pos):
    """mm a frame for every frame, smoothed over five frames against the camera's shake."""
    p = np.array(pos) * 10.0
    v = np.r_[0.0, np.linalg.norm(np.diff(p, axis=0), axis=1)]
    v[v > 3 * V_REF] = 0.0                       # a jump between shots is no roll
    return np.convolve(v, np.ones(5) / 5, mode="same")


def roll(rec, v_frames, fps, seed, normalize=True):
    """The roll for a ball whose speed in mm a frame is v_frames. Without normalize, a ball at cruising speed
    rolls at the recording's steady level, so rolls of several balls keep their levels to one another."""
    rng = np.random.default_rng(seed)
    runs = pool(rec)
    ref = np.median([np.sqrt((rec[a:b] ** 2).mean()) for a, b in runs])
    n = int(len(v_frames) * SR / fps)
    out = np.zeros(n + int(GRAIN * SR))
    g = int(GRAIN * SR); hop = g // 2
    win = np.sin(np.pi * np.arange(g) / g) ** 2                        # Hann, sums to one at half overlap
    for start in range(-hop, n, hop):
        mid = (start + g / 2) / SR * fps
        s = np.clip(np.interp(mid, np.arange(len(v_frames)), v_frames) / V_REF, 0.0, 1.6)
        if s < 0.02:
            continue
        rate = np.clip(s ** 0.25, 0.8, 1.12)
        need = int(g * rate) + 2
        a, b = runs[rng.integers(len(runs))]
        if b - a <= need:
            continue
        o = rng.integers(a, b - need)
        src = rec[o:o + need]
        grain = np.interp(np.arange(g) * rate, np.arange(need), src)
        grain *= s ** 1.2 / max(1e-9, np.sqrt((grain ** 2).mean())) * ref
        lo = max(0, start); hi = start + g
        out[lo:hi] += (win * grain)[lo - start:]
    out = out[:n]
    return out / max(1e-9, np.abs(out).max()) * 0.7 if normalize else out


def main():
    rec_path, paths, out = sys.argv[1:4]
    who = sys.argv[4] if len(sys.argv) > 4 else "cam"
    d = json.load(open(paths))
    frames = sorted(int(k) for k in d[who])
    audio = roll(load(rec_path), speed([d[who][str(f)] for f in frames]), d["fps"], seed=7 if who == "cam" else 8)
    wavfile.write(out, SR, (audio * 32767).astype(np.int16))
    print(out, "%.2f s" % (len(audio) / SR), "pool runs", len(pool(load(rec_path))))


if __name__ == "__main__":
    main()
