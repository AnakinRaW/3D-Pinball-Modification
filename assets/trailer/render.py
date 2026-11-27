"""Builds and renders the trailer on this computer, and picks a stopped render up where it ended.

  python assets/trailer/render.py                      asks for every setting
  python assets/trailer/render.py --quality fast -y    takes the defaults for the rest

The steps it runs, each only when it is asked for or its output is missing:
  1  the machine's shapes and textures, extract.py and frame_body.py of cgi/, into cgi/blender/out/
  2  the trailer's scene, trailer.py in Blender, into trailer.blend and blender/out/
  3  the display's frames, display.py, into display/
  4  the frames, Blender in the background on trailer.blend, into renders/<quality>/; a frame whose PNG exists
     is skipped, so running the script again resumes a stopped render
  5  the video, ffmpeg with trailer.wav where both are there, into renders/trailer-<quality>.mp4

Blender is found on the PATH, in the usual install folders, or given with --blender.
"""
import argparse, glob, os, shutil, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
BLEND = os.path.join(HERE, "trailer.blend")
CGI = os.path.join(os.path.dirname(HERE), "cgi", "blender")

# engine, resolution percentage of 1920 x 1080, samples
QUALITY = {
    "fast": ("BLENDER_EEVEE", 25, 2),
    "preview": ("BLENDER_EEVEE", 50, 10),
    "hd": ("BLENDER_EEVEE", 100, 32),
    "final": ("CYCLES", 100, 64),
}

# what Blender runs to render: the display's frames from this checkout, the settings, then every frame not
# yet on disk, printing one line a frame
RENDER = r'''
import bpy, os, sys, time
args = sys.argv[sys.argv.index("--") + 1:]
here, out, engine, pct, samples, first, last = args[0], args[1], args[2], int(args[3]), int(args[4]), int(args[5]), int(args[6])
for im in bpy.data.images:
    if im.source == "SEQUENCE":
        im.filepath = os.path.join(here, "display", "d0001.png"); im.reload()
sc = bpy.context.scene
sc.render.engine = engine
sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = 1920, 1080, pct
sc.render.image_settings.file_format = "PNG"
if engine == "CYCLES":
    sc.cycles.samples = samples; sc.cycles.use_denoising = True
    prefs = bpy.context.preferences.addons["cycles"].preferences
    for kind in ("OPTIX", "CUDA", "HIP", "METAL", "ONEAPI"):
        try:
            prefs.compute_device_type = kind; prefs.get_devices()
            if any(d.type == kind for d in prefs.devices):
                for d in prefs.devices:
                    d.use = d.type == kind
                sc.cycles.device = "GPU"; print("GPU", kind, flush=True); break
        except Exception:
            pass
else:
    sc.eevee.taa_render_samples = samples
os.makedirs(out, exist_ok=True)
todo = [f for f in range(first, last + 1) if not os.path.exists(os.path.join(out, "f%04d.png" % f))]
print("TODO", len(todo), flush=True)
for k, f in enumerate(todo):
    t0 = time.time()
    sc.frame_set(f)
    sc.render.filepath = os.path.join(out, "f%04d.png" % f)
    bpy.ops.render.render(write_still=True)
    print("FRAME %d %d %d %.1f" % (f, k + 1, len(todo), time.time() - t0), flush=True)
'''


def find_blender(given):
    """Blender's executable: the one given, one on the PATH, or the newest in the usual install folders; asks
    until it gets a file that exists."""
    candidates = [given] if given else []
    candidates.append(shutil.which("blender"))
    patterns = {"win32": [r"C:\Program Files\Blender Foundation\Blender*\blender.exe",
                          r"C:\Program Files (x86)\Steam\steamapps\common\Blender\blender.exe",
                          os.path.expandvars(r"%LOCALAPPDATA%\Programs\Blender Foundation\Blender*\blender.exe")],
                "darwin": ["/Applications/Blender*.app/Contents/MacOS/Blender"]}.get(sys.platform, ["/opt/blender*/blender",
                                                                                                    "/usr/bin/blender"])
    for pattern in patterns:
        candidates += sorted(glob.glob(pattern), reverse=True)
    for c in candidates:
        if c and os.path.isfile(c):
            return c
    while True:
        path = input("Blender was not found. Path to blender.exe: ").strip().strip('"')
        if os.path.isfile(path):
            return path
        print("No file at", path)


def ask(prompt, default, yes):
    if yes:
        return default
    answer = input(f"{prompt} [{default}]: ").strip()
    return answer or default


def run(cmd, **kw):
    print(">", " ".join(f'"{c}"' if " " in c else c for c in cmd), flush=True)
    for part in cmd[:3]:
        if part.endswith((".py", ".exe")) and not os.path.isfile(part):
            sys.exit(f"missing: {part}")
    subprocess.run(cmd, check=True, **kw)


def scene_frames(blender):
    """The trailer's first and last frame, read from trailer.blend."""
    expr = "import bpy; s=bpy.context.scene; print('RANGE', s.frame_start, s.frame_end)"
    out = subprocess.run([blender, "-b", BLEND, "--python-expr", expr], capture_output=True, text=True).stdout
    for line in out.splitlines():
        if line.startswith("RANGE"):
            _, a, b = line.split()
            return int(a), int(b)
    raise RuntimeError("could not read the frame range from trailer.blend")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--quality", choices=QUALITY)
    ap.add_argument("--first", type=int)
    ap.add_argument("--last", type=int)
    ap.add_argument("--build", choices=("auto", "yes", "no"), help="rebuild the scene first; auto builds what is missing")
    ap.add_argument("--blender")
    ap.add_argument("-y", "--yes", action="store_true", help="take the default for every setting not given")
    a = ap.parse_args()

    blender = find_blender(a.blender)
    print("Blender:", blender)
    if not os.path.isfile(os.path.join(HERE, "blender", "game.json")):
        sys.exit("missing: assets/trailer/blender/game.json, the game the trailer plays")
    quality = a.quality or ask("Quality (" + ", ".join(QUALITY) + ")", "fast", a.yes)
    if quality not in QUALITY:
        sys.exit(f"unknown quality {quality}")
    build = a.build or ask("Rebuild the scene first (auto, yes, no)", "auto", a.yes)

    # 1 to 3: the scene
    if build == "yes" or (build == "auto" and not os.path.isdir(os.path.join(CGI, "out"))):
        out = os.path.join(CGI, "out")
        run([sys.executable, os.path.join(CGI, "extract.py"), out])
        run([sys.executable, os.path.join(CGI, "frame_body.py"), out])
    if build == "yes" or (build == "auto" and not os.path.exists(BLEND)):
        run([blender, "-b", "--factory-startup", "--python", os.path.join(HERE, "blender", "trailer.py")])
    if build == "yes" or (build == "auto" and not os.path.isdir(os.path.join(HERE, "display"))):
        run([sys.executable, os.path.join(HERE, "blender", "display.py")])

    # 4: the frames
    first, last = scene_frames(blender)
    first = a.first or int(ask("First frame", first, a.yes))
    last = a.last or int(ask("Last frame", last, a.yes))
    frames = os.path.join(HERE, "renders", quality)
    engine, pct, samples = QUALITY[quality]
    print(f"Rendering frames {first} to {last} at {quality} into {frames}; frames already there are skipped.")
    script = os.path.join(HERE, "renders", "_render.py")
    os.makedirs(os.path.dirname(script), exist_ok=True)
    open(script, "w").write(RENDER)
    t0 = time.time()
    proc = subprocess.Popen([blender, "-b", BLEND, "--python", script, "--", HERE, frames, engine, str(pct), str(samples),
                             str(first), str(last)], stdout=subprocess.PIPE, text=True)
    try:
        for line in proc.stdout:
            if line.startswith("FRAME"):
                _, f, k, n, s = line.split()
                left = (time.time() - t0) / int(k) * (int(n) - int(k))
                print(f"\rframe {f}  {k}/{n}  {float(s):.1f} s  about {left / 60:.0f} min left   ", end="", flush=True)
            elif line.startswith(("TODO", "GPU")):
                print(line.strip())
        proc.wait()
    except KeyboardInterrupt:
        proc.terminate()
        sys.exit("\nstopped; run the script again to go on where it ended")
    print()
    if proc.returncode:
        sys.exit(f"Blender ended with {proc.returncode}")

    # 5: the video
    ffmpeg, wav = shutil.which("ffmpeg"), os.path.join(HERE, "trailer.wav")
    if ffmpeg:
        video = os.path.join(HERE, "renders", f"trailer-{quality}.mp4")
        cmd = [ffmpeg, "-y", "-loglevel", "error", "-framerate", "25", "-start_number", str(first),
               "-i", os.path.join(frames, "f%04d.png")]
        if os.path.exists(wav):
            cmd += ["-i", wav, "-c:a", "aac", "-b:a", "192k", "-shortest"]
        run(cmd + ["-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "18", video])
        print("Video:", video)
    else:
        print("ffmpeg is not on the PATH; the frames are in", frames)


if __name__ == "__main__":
    main()
