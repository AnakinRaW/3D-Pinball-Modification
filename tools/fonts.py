"""Converts the host screens' fonts into LVGL fonts for the firmware.

Archivo Black sets titles and buttons, JetBrains Mono Bold sets numbers, and Montserrat SemiBold
sets text with Montserrat Bold for its lines in bold, as docs/firmware/host-screens.md gives. The
fonts come from Fontsource's npm packages and lv_font_conv from npm, each at the version below,
installed into a temporary folder. Each font keeps the printable ASCII characters with the middle
dot, the times sign and the minus sign. The large title sizes keep the capitals of them. Every font
falls back to LVGL's Montserrat 14 for LVGL's symbols. The files go to
firmware/src/host/views/fonts/, with the fonts' licenses.

    python tools/fonts.py           writes the fonts and the licenses
    python tools/fonts.py --check   exits with 1 where a file differs from what the conversion
                                    gives, or where the folder holds a font the list leaves out

It needs Node.js and npm on the PATH.
"""
import argparse
import os
import subprocess
import sys
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "firmware", "src", "host", "views", "fonts")
PACKAGES = ["lv_font_conv@1.5.3", "@fontsource/archivo-black@5.2.6",
            "@fontsource/jetbrains-mono@5.2.6", "@fontsource/montserrat@5.2.6"]
# the printable ASCII characters with the middle dot, the times sign and the minus sign
CHARACTERS = "0x20-0x7E,0xB7,0xD7,0x2212"
CAPITALS = "0x20-0x5F"     # ASCII without the lower case, for the titles set in capitals
BITS_PER_PIXEL = "4"       # 16 levels of coverage, LVGL's smooth edges
FALLBACK = "lv_font_montserrat_14"

# the font's name in C, its package, its file in the package's files/ folder, its size in pixels and
# its characters
_ARCHIVO = ("archivo-black", "archivo-black-latin-400-normal.woff")
_MONO = ("jetbrains-mono", "jetbrains-mono-latin-700-normal.woff")
_MONTSERRAT = ("montserrat", "montserrat-latin-600-normal.woff")
_MONTSERRAT_BOLD = ("montserrat", "montserrat-latin-700-normal.woff")
FONTS = [
    ("archivo_black_9", *_ARCHIVO, 9, CHARACTERS),
    ("archivo_black_12", *_ARCHIVO, 12, CHARACTERS),
    ("archivo_black_13", *_ARCHIVO, 13, CHARACTERS),
    ("archivo_black_14", *_ARCHIVO, 14, CHARACTERS),
    ("archivo_black_17", *_ARCHIVO, 17, CHARACTERS),
    ("archivo_black_20", *_ARCHIVO, 20, CHARACTERS),
    ("archivo_black_30", *_ARCHIVO, 30, CAPITALS),
    ("archivo_black_40", *_ARCHIVO, 40, CAPITALS),
    ("jetbrains_mono_bold_12", *_MONO, 12, CHARACTERS),
    ("jetbrains_mono_bold_16", *_MONO, 16, CHARACTERS),
    ("jetbrains_mono_bold_20", *_MONO, 20, CHARACTERS),
    ("montserrat_semibold_12", *_MONTSERRAT, 12, CHARACTERS),
    ("montserrat_semibold_14", *_MONTSERRAT, 14, CHARACTERS),
    ("montserrat_bold_12", *_MONTSERRAT_BOLD, 12, CHARACTERS),
    ("montserrat_bold_14", *_MONTSERRAT_BOLD, 14, CHARACTERS),
    ("montserrat_bold_16", *_MONTSERRAT_BOLD, 16, CHARACTERS),
]
LICENSES = {"archivo-black": "Archivo Black", "jetbrains-mono": "JetBrains Mono",
            "montserrat": "Montserrat"}


def run(args, cwd):
    subprocess.run(args, cwd=cwd, check=True, shell=os.name == "nt",
                   stdout=subprocess.DEVNULL)


def text_of(path):
    """Reads a file with its line endings as the repository holds them."""
    return open(path, encoding="utf-8").read().replace("\r\n", "\n")


def convert():
    """Converts every font and copies the licenses. Gets each file's name and text."""
    files = {}
    with tempfile.TemporaryDirectory() as tmp:
        run(["npm", "init", "-y"], tmp)
        run(["npm", "install", "--no-audit", "--no-fund"] + PACKAGES, tmp)
        modules = os.path.join(tmp, "node_modules")
        converter = os.path.join(modules, "lv_font_conv", "lv_font_conv.js")
        for name, package, file, size, characters in FONTS:
            # Paths relative to the temporary folder, since the file's head repeats them.
            source = "/".join(["node_modules", "@fontsource", package, "files", file])
            generated = name + ".c"
            run(["node", converter, "--font", source, "-r", characters, "--size", str(size),
                 "--bpp", BITS_PER_PIXEL, "--format", "lvgl", "--no-compress",
                 "--lv-include", "lvgl.h", "--lv-font-name", name, "--lv-fallback", FALLBACK,
                 "-o", generated], tmp)
            # The generated tables follow lv_font_conv's style, not the firmware's.
            text = text_of(os.path.join(tmp, generated))
            files[generated] = "// NOLINTBEGIN\n" + text.rstrip("\n") + "\n// NOLINTEND\n"
        for package in LICENSES:
            source = os.path.join(modules, "@fontsource", package, "LICENSE")
            files["OFL-" + package + ".txt"] = text_of(source)
    return files


def main():
    parser = argparse.ArgumentParser(description="Converts the host screens' fonts for LVGL.")
    parser.add_argument("--check", action="store_true",
                        help="compare with the files in the repository instead of writing them")
    args = parser.parse_args()
    files = convert()
    if args.check:
        bad = []
        for name, text in files.items():
            path = os.path.join(OUT, name)
            if not os.path.exists(path):
                bad.append(name + " is missing")
            elif text_of(path) != text:
                bad.append(name + " differs from its conversion")
        for name in sorted(os.listdir(OUT)) if os.path.isdir(OUT) else []:
            if name not in files:
                bad.append(name + " is no font of the list")
        for line in bad:
            print(os.path.relpath(OUT, ROOT) + "/" + line)
        print("fonts: " + ("%d files to fix, run python tools/fonts.py" % len(bad) if bad
                           else "every file matches its conversion"))
        return 1 if bad else 0
    os.makedirs(OUT, exist_ok=True)
    for name, text in files.items():
        open(os.path.join(OUT, name), "w", encoding="utf-8", newline="\n").write(text)
        print(os.path.relpath(os.path.join(OUT, name), ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
