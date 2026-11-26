"""Converts the host screens' fonts into LVGL fonts for the firmware.

Archivo Black sets titles and buttons, JetBrains Mono Bold sets numbers, and Montserrat SemiBold
sets text with Montserrat Bold for its lines in bold, as docs/firmware/host-screens.md gives. The
fonts lie in firmware/fonts/ with their licenses. lv_font_conv comes from npm at the version below,
installed into a temporary folder. Each font keeps the printable ASCII characters with the middle
dot, the times sign and the minus sign. The large title sizes keep the capitals of them. Every font
falls back to host_icons_14, the icons the screens need beyond LVGL's symbols, taken from Font
Awesome Free at 14 pixels, and that falls back to LVGL's Montserrat 14 for LVGL's symbols.

The converted fonts go to firmware/src/host/views/fonts/, which Git ignores, with a stamp that holds
a hash of the fonts and of this script. A conversion runs only when the stamp is missing or differs,
so every build that compiles the host's views runs this first at no cost.

    python tools/fonts.py           converts the fonts where the stamp differs
    python tools/fonts.py --force   converts them in any case

A conversion needs Node.js and npm on the PATH.
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SOURCES = os.path.join(ROOT, "firmware", "fonts")
OUT = os.path.join(ROOT, "firmware", "src", "host", "views", "fonts")
STAMP = os.path.join(OUT, "inputs.sha256")
CONVERTER = "lv_font_conv@1.5.3"
# the printable ASCII characters with the middle dot, the times sign and the minus sign
CHARACTERS = "0x20-0x7E,0xB7,0xD7,0x2212"
CAPITALS = "0x20-0x5F"     # ASCII without the lower case, for the titles set in capitals
BITS_PER_PIXEL = "4"       # 16 levels of coverage, LVGL's smooth edges
SYMBOLS = "lv_font_montserrat_14"   # LVGL's font with its symbols

# the icons beyond LVGL's symbols: the light bulb for the playfield's lamps, the sun for the
# ambient light and the crossed-out speaker for muted sound, at the size of LVGL's symbols
ICONS = "host_icons_14"
ICON_SOURCE = "fa-solid-900.ttf"
ICON_SIZE = 14
ICON_CHARACTERS = "0xF0EB,0xF185,0xF6A9"

# the font's name in C, its file in firmware/fonts/, its size in pixels and its characters
_ARCHIVO = "archivo-black-latin-400-normal.woff"
_MONO = "jetbrains-mono-latin-700-normal.woff"
_MONTSERRAT = "montserrat-latin-600-normal.woff"
_MONTSERRAT_BOLD = "montserrat-latin-700-normal.woff"
FONTS = [
    ("archivo_black_9", _ARCHIVO, 9, CHARACTERS),
    ("archivo_black_12", _ARCHIVO, 12, CHARACTERS),
    ("archivo_black_13", _ARCHIVO, 13, CHARACTERS),
    ("archivo_black_14", _ARCHIVO, 14, CHARACTERS),
    ("archivo_black_17", _ARCHIVO, 17, CHARACTERS),
    ("archivo_black_20", _ARCHIVO, 20, CHARACTERS),
    ("archivo_black_30", _ARCHIVO, 30, CAPITALS),
    ("archivo_black_40", _ARCHIVO, 40, CAPITALS),
    ("jetbrains_mono_bold_12", _MONO, 12, CHARACTERS),
    ("jetbrains_mono_bold_16", _MONO, 16, CHARACTERS),
    ("jetbrains_mono_bold_20", _MONO, 20, CHARACTERS),
    ("montserrat_semibold_12", _MONTSERRAT, 12, CHARACTERS),
    ("montserrat_semibold_14", _MONTSERRAT, 14, CHARACTERS),
    ("montserrat_bold_12", _MONTSERRAT_BOLD, 12, CHARACTERS),
    ("montserrat_bold_14", _MONTSERRAT_BOLD, 14, CHARACTERS),
    ("montserrat_bold_16", _MONTSERRAT_BOLD, 16, CHARACTERS),
]
# every converted font, whose file is <name>.c
NAMES = [name for name, _, _, _ in FONTS] + [ICONS]


class MissingTool(Exception):
    """A tool the conversion needs is not on the PATH."""


def inputs_hash():
    """Gets the hash of the font files and of this script, its line endings as Git stores them."""
    h = hashlib.sha256()
    for name in sorted(os.listdir(SOURCES)):
        h.update(name.encode("utf-8"))
        with open(os.path.join(SOURCES, name), "rb") as f:
            h.update(f.read())
    with open(os.path.abspath(__file__), "rb") as f:
        h.update(f.read().replace(b"\r\n", b"\n"))
    return h.hexdigest()


def current(digest):
    """Gets whether the converted fonts stand at the stamp of @p digest."""
    if not os.path.exists(STAMP):
        return False
    with open(STAMP, encoding="utf-8") as f:
        if f.read().strip() != digest:
            return False
    return all(os.path.exists(os.path.join(OUT, name + ".c")) for name in NAMES)


def run(args, cwd):
    subprocess.run(args, cwd=cwd, check=True, shell=os.name == "nt",
                   stdout=subprocess.DEVNULL)


def convert():
    """Converts every font into OUT, removes a file no font of the list writes, and writes the
    stamp last, so a conversion broken off runs again."""
    for tool in ("node", "npm"):
        if shutil.which(tool) is None:
            raise MissingTool(
                tool + " is not on the PATH. The host screens' fonts are converted with "
                "lv_font_conv from npm, which needs Node.js and npm: install Node.js 22 or newer "
                "from https://nodejs.org and build again.")
    os.makedirs(OUT, exist_ok=True)
    if os.path.exists(STAMP):
        os.remove(STAMP)
    written = set()
    with tempfile.TemporaryDirectory() as tmp:
        run(["npm", "init", "-y"], tmp)
        run(["npm", "install", "--no-audit", "--no-fund", CONVERTER], tmp)
        converter = os.path.join(tmp, "node_modules", "lv_font_conv", "lv_font_conv.js")

        def generate(name, source, size, characters, fallback):
            generated = name + ".c"
            path = os.path.join(OUT, generated)
            # Paths relative to the repository, since the file's head repeats them.
            run(["node", converter, "--font", "firmware/fonts/" + source, "-r", characters,
                 "--size", str(size), "--bpp", BITS_PER_PIXEL, "--format", "lvgl",
                 "--no-compress", "--lv-include", "lvgl.h", "--lv-font-name", name,
                 "--lv-fallback", fallback, "-o", "firmware/src/host/views/fonts/" + generated],
                ROOT)
            with open(path, encoding="utf-8") as f:
                text = f.read().replace("\r\n", "\n")
            # The generated tables follow lv_font_conv's style, not the firmware's.
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write("// NOLINTBEGIN\n" + text.rstrip("\n") + "\n// NOLINTEND\n")
            written.add(generated)

        for name, source, size, characters in FONTS:
            generate(name, source, size, characters, ICONS)
        generate(ICONS, ICON_SOURCE, ICON_SIZE, ICON_CHARACTERS, SYMBOLS)
    for name in os.listdir(OUT):
        if name not in written:
            os.remove(os.path.join(OUT, name))
    with open(STAMP, "w", encoding="utf-8", newline="\n") as f:
        f.write(inputs_hash() + "\n")


def ensure(force=False):
    """Converts the fonts where the stamp differs from the inputs. Gets whether it converted."""
    if not force and current(inputs_hash()):
        return False
    convert()
    return True


def main():
    parser = argparse.ArgumentParser(description="Converts the host screens' fonts for LVGL.")
    parser.add_argument("--force", action="store_true",
                        help="convert the fonts even where the stamp matches")
    args = parser.parse_args()
    try:
        converted = ensure(args.force)
    except MissingTool as e:
        print("tools/fonts.py: " + str(e), file=sys.stderr)
        return 1
    rel = os.path.relpath(OUT, ROOT).replace(os.sep, "/")
    print("fonts: " + ("converted into " + rel if converted else rel + " is current"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
