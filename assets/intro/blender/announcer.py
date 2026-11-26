"""Turns a spoken "Multiball!" into the machine's announcer call: "M-M-M-MULTIBAAALL!".

  python announcer.py <in.wav> <out.wav>          (in WSL, with ~/.venvs/audio)

The word's first consonant is stuttered three times, its last syllable stretched, and the voice pitched down,
driven, band-limited like a small speaker and set in a hall. The result is mono at 44.1 kHz.
"""
import sys
import numpy as np, soundfile as sf
from pedalboard import (Pedalboard, PitchShift, Distortion, HighpassFilter, LowpassFilter, Compressor, Reverb,
                        Delay, Gain, time_stretch)

SR = 44100


def trim(a, floor=0.02):
    idx = np.nonzero(np.abs(a) > floor * np.abs(a).max())[0]
    return a[idx[0]:idx[-1] + 1]


def main():
    src, dst = sys.argv[1], sys.argv[2]
    a, sr = sf.read(src, dtype="float32")
    a = trim(a)
    # to 44.1 kHz first, so every cut is in the output's samples
    a = np.interp(np.arange(int(len(a) * SR / sr)) * sr / SR, np.arange(len(a)), a).astype(np.float32) if sr != SR else a
    # the stutter: the word's first 95 ms three times, each faded out, with short gaps
    m = a[: int(0.095 * SR)].copy()
    m *= np.linspace(1.0, 0.0, len(m)) ** 0.5
    gap = np.zeros(int(0.045 * SR), np.float32)
    # the last 45 % of the word, its "ball", stretched to twice its length at the same pitch
    cut = int(len(a) * 0.55)
    tail = time_stretch(a[cut:][None, :], SR, stretch_factor=0.5)[0].astype(np.float32)
    word = np.concatenate([a[:cut], tail])
    voice = np.concatenate([m, gap, m, gap, m, gap, word])
    board = Pedalboard([PitchShift(semitones=-3), Distortion(drive_db=10), HighpassFilter(260), LowpassFilter(4800),
                        Compressor(threshold_db=-18, ratio=4), Delay(delay_seconds=0.16, feedback=0.25, mix=0.18),
                        Reverb(room_size=0.55, wet_level=0.28, dry_level=0.85), Gain(-1)])
    out = board(np.concatenate([voice, np.zeros(int(0.6 * SR), np.float32)]), SR)
    out = out / max(1e-6, np.abs(out).max()) * 0.9
    sf.write(dst, out, SR, subtype="PCM_16")
    print(dst, "%.2f s" % (len(out) / SR))


if __name__ == "__main__":
    main()
