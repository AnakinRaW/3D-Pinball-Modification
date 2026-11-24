"""Renders the intro's circus march with ACE-Step 1.5's SFT model on the CPU, 12 s at 125 BPM.

  python march.py <ACE-Step checkout> <out dir>      (in WSL, with ~/.venvs/acestep)

32 diffusion steps with classifier-free guidance at seed 37 give the take the intro uses; the march itself
lasts 7.6 s of the 12. The result goes to <out dir>/acestep-circusmarch-sft.wav.
"""
import os, sys, time, shutil
os.environ.setdefault("ACESTEP_INIT_LLM", "false")
os.environ.setdefault("ACESTEP_GENERATION_TIMEOUT", "3600")     # the CPU can take longer than the default 600 s
from acestep.handler import AceStepHandler
from acestep.inference import GenerationParams, GenerationConfig, generate_music

CAPTION = ("classic circus calliope tune with rapid tootling organ runs, brass oom-pah, snare rolls, "
           "big top excitement, modernized with punchy electronic drums and cosmic synth pads")


def main():
    root, out = sys.argv[1], sys.argv[2]
    dit = AceStepHandler()
    msg, ok = dit.initialize_service(project_root=root, config_path="acestep-v15-sft", device="cpu")
    if not ok:
        sys.exit(msg)
    params = GenerationParams(caption=CAPTION, lyrics="[Instrumental]", instrumental=True, bpm=125, keyscale="A minor",
                              timesignature="4", duration=12, inference_steps=32, guidance_scale=7.0, seed=37,
                              thinking=False, use_cot_metas=False, use_cot_caption=False, use_cot_language=False)
    t0 = time.time()
    res = generate_music(dit, None, params, GenerationConfig(batch_size=1, audio_format="wav"), save_dir=out)
    if not res.success:
        sys.exit(res.error)
    shutil.move(res.audios[0]["path"], os.path.join(out, "acestep-circusmarch-sft.wav"))
    print("march %.0f s" % (time.time() - t0))


if __name__ == "__main__":
    main()
