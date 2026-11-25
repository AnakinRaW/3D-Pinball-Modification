"""Converts the host screens' fonts into LVGL fonts for the firmware.

Archivo Black sets titles and buttons, JetBrains Mono Bold sets numbers, and Montserrat SemiBold
sets text, as docs/firmware/host-screens.md gives. The fonts come from Fontsource's npm packages and
lv_font_conv from npm, each at the version below, installed into a temporary folder. Each font
keeps the printable ASCII characters and falls back to LVGL's Montserrat 14 for LVGL's symbols.
The files go to firmware/src/host/views/fonts/, with the fonts' license.

    python tools/fonts.py

It needs Node.js and npm on the PATH.
"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "firmware", "src", "host", "views", "fonts")
PACKAGES = ["lv_font_conv@1.5.3", "@fontsource/archivo-black@5.2.6",
            "@fontsource/jetbrains-mono@5.2.6", "@fontsource/montserrat@5.2.6"]
CHARACTERS = "0x20-0x7E"   # the printable ASCII characters
BITS_PER_PIXEL = "4"       # 16 levels of coverage, LVGL's smooth edges
FALLBACK = "lv_font_montserrat_14"

# the font's name in C, its package, its file in the package's files/ folder, its size in pixels
FONTS = [
    ("archivo_black_13", "archivo-black", "archivo-black-latin-400-normal.woff", 13),
    ("archivo_black_14", "archivo-black", "archivo-black-latin-400-normal.woff", 14),
    ("archivo_black_17", "archivo-black", "archivo-black-latin-400-normal.woff", 17),
    ("archivo_black_20", "archivo-black", "archivo-black-latin-400-normal.woff", 20),
    ("jetbrains_mono_bold_12", "jetbrains-mono", "jetbrains-mono-latin-700-normal.woff", 12),
    ("jetbrains_mono_bold_16", "jetbrains-mono", "jetbrains-mono-latin-700-normal.woff", 16),
    ("montserrat_semibold_12", "montserrat", "montserrat-latin-600-normal.woff", 12),
    ("montserrat_semibold_14", "montserrat", "montserrat-latin-600-normal.woff", 14),
]
LICENSES = {"archivo-black": "Archivo Black", "jetbrains-mono": "JetBrains Mono",
            "montserrat": "Montserrat"}


def run(args, cwd):
    subprocess.run(args, cwd=cwd, check=True, shell=os.name == "nt")


def main():
    os.makedirs(OUT, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        run(["npm", "init", "-y"], tmp)
        run(["npm", "install", "--no-audit", "--no-fund"] + PACKAGES, tmp)
        modules = os.path.join(tmp, "node_modules")
        converter = os.path.join(modules, "lv_font_conv", "lv_font_conv.js")
        for name, package, file, size in FONTS:
            # Paths relative to the temporary folder, since the file's head repeats them.
            source = "/".join(["node_modules", "@fontsource", package, "files", file])
            generated = name + ".c"
            run(["node", converter, "--font", source, "-r", CHARACTERS, "--size", str(size),
                 "--bpp", BITS_PER_PIXEL, "--format", "lvgl", "--no-compress",
                 "--lv-include", "lvgl.h", "--lv-font-name", name, "--lv-fallback", FALLBACK,
                 "-o", generated], tmp)
            target = os.path.join(OUT, generated)
            # The generated tables follow lv_font_conv's style, not the firmware's.
            text = open(os.path.join(tmp, generated), encoding="utf-8").read()
            open(target, "w", encoding="utf-8", newline="\n").write(
                "// NOLINTBEGIN\n" + text.rstrip("\n") + "\n// NOLINTEND\n")
            print(os.path.relpath(target, ROOT))
        for package, family in LICENSES.items():
            source = os.path.join(modules, "@fontsource", package, "LICENSE")
            target = os.path.join(OUT, "OFL-" + package + ".txt")
            shutil.copyfile(source, target)
            print(os.path.relpath(target, ROOT), "for", family)
    return 0


if __name__ == "__main__":
    sys.exit(main())
