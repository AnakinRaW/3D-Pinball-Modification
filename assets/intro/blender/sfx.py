"""Renders the intro's sound effects with Stable Audio Open Small on the CPU, one WAV each at 44.1 kHz.

  python sfx.py <out dir> [name ...]         (in WSL, with ~/.venvs/sao)

Each effect is a text prompt and a length, rendered at seed 11; soundtrack.py takes the bumper's and the
kickout's first hit and the riser's first two seconds. The kit's own sounds come from recordings, and the rest
of the sound is synthesized in soundtrack.py.
"""
import os, sys, time
import torch, torchaudio
torch.set_num_threads(os.cpu_count() or 4)
from einops import rearrange
from stable_audio_tools import get_pretrained_model
from stable_audio_tools.inference.generation import generate_diffusion_cond

EFFECTS = {
    "bumper": ("pinball pop bumper hit, solenoid thump with a rubber bounce, short and punchy, close, dry", 1.0),
    "kickout": ("pinball kickout hole fires a steel ball out, solenoid kick thud and a metal rattle, close, dry", 1.2),
    "riser": ("cinematic tension riser, a building swell of noise and strings", 3.0),
}
SEEDS = (11,)                 # one take each; the decoder is slow on the CPU


def main():
    out = sys.argv[1]
    names = sys.argv[2:] or list(EFFECTS)
    os.makedirs(out, exist_ok=True)
    model, cfg = get_pretrained_model("stabilityai/stable-audio-open-small")
    model = model.to("cpu").eval()
    sr, size = cfg["sample_rate"], cfg["sample_size"]
    for name in names:
        prompt, seconds = EFFECTS[name]
        for seed in SEEDS:
            t0 = time.time()
            torch.manual_seed(seed)
            # only the length needed, in whole latent steps: the decoder's time grows with it
            n = 2048 * -(-int(seconds * sr) // 2048)
            audio = generate_diffusion_cond(model, steps=8, conditioning=[{"prompt": prompt, "seconds_total": seconds}],
                                            sample_size=n, sampler_type="pingpong", device="cpu", seed=seed)
            audio = rearrange(audio, "b d n -> d (b n)").to(torch.float32)[:, : int(seconds * sr)]
            audio = audio.mean(0, keepdim=True)                       # the machine plays mono
            audio = audio / audio.abs().max().clamp(min=1e-6) * 0.9
            torchaudio.save(os.path.join(out, f"sfx-{name}-{seed}.wav"), audio, sr, bits_per_sample=16)
            print(name, seed, "%.1f s" % (time.time() - t0), flush=True)


if __name__ == "__main__":
    main()
