"""Speaks "Multiball!" with Kokoro's US male voice am_michael, slowed to 0.8, for announcer.py to turn into the
machine's call.

  python voice.py <out.wav>                       (in WSL, with ~/.venvs/audio)

The result is mono at Kokoro's 24 kHz.
"""
import sys
import numpy as np, soundfile as sf
from kokoro import KPipeline


def main():
    pipe = KPipeline(lang_code="a")
    chunks = [a for _, _, a in pipe("Multiball!", voice="am_michael", speed=0.8)]
    sf.write(sys.argv[1], np.concatenate([np.asarray(c) for c in chunks]), 24000)


if __name__ == "__main__":
    main()
