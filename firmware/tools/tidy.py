"""Runs clang-tidy over the firmware's code with the checks of firmware/.clang-tidy.

PlatformIO writes the release build's compile commands, and the run takes those of the files in
firmware/src. They are commands for the ARM GCC of the Teensy core, so the run
turns each into one clang understands: the target becomes arm-none-eabi, the version and the
include folders of that GCC are added, and the flags only GCC knows are dropped. freertos-teensy
binds std::mutex to FreeRTOS only for a GCC from 6.1 on, so clang reports that GCC's version. Headers in firmware/ are checked
where a checked file includes them.

    python firmware/tools/tidy.py              every file of firmware/src
    python firmware/tools/tidy.py servo        the files whose path holds "servo"
    python firmware/tools/tidy.py --fix        applies the fixes clang-tidy offers
    python firmware/tools/tidy.py --self-test  checks two planted files, one clean, one not

The self-test writes its files into firmware/.tidy-self-test/ and compiles them like the first
file of firmware/src. The clean file includes Arduino.h and has to come through
with no finding and no error in any header, which proves the commands clang reads. The other file
has to report the checks it plants.

Exit code 0 when no check reports anything, 1 when one does, 2 when the run could not start. With
--self-test, 0 when both planted files got their verdict, 1 otherwise.
"""

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OWN = ("firmware/src/",)
ENV = "release"

# flags of GCC that clang does not take, with the number of arguments that follow each
GCC_ONLY = {"-felide-constructors": 0, "-fno-threadsafe-statics": 0, "-Wno-psabi": 0,
            "-Wno-maybe-uninitialized": 0, "-fpermissive": 0, "-nostdlib": 0, "-o": 1, "-c": 0}
# the flags that pick the GCC's library variant, passed on when its include folders are asked
ARCH = ("-mthumb", "-mcpu=", "-mfloat-abi=", "-mfpu=")
# The ARM GCC types a 32-bit integer as long, and clang for arm-none-eabi as int. Newlib takes the
# types from these macros, so without them uint32_t and FastLED's u32 differ.
GCC_TYPES = ["__INT32_TYPE__=long int", "__UINT32_TYPE__=long unsigned int",
             "__INT_LEAST32_TYPE__=long int", "__UINT_LEAST32_TYPE__=long unsigned int"]
CLANG_EXTRA = ["--target=arm-none-eabi", "-Wno-unknown-warning-option",
               "-Wno-unused-command-line-argument", "-Wno-macro-redefined", "-Wvla"]
CLANG_EXTRA += [f"-D{definition}" for definition in GCC_TYPES]


def compile_commands():
    """Writes the release build's compile commands with PlatformIO and reads them back."""
    result = subprocess.run([sys.executable, "-m", "platformio", "run", "-e", ENV, "-t", "compiledb"],
                            cwd=ROOT, capture_output=True, text=True)
    path = os.path.join(ROOT, "compile_commands.json")
    if result.returncode != 0 or not os.path.exists(path):
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(2)
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def arguments(entry):
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry["command"], posix=os.name != "nt")


def unquote(argument):
    return argument[1:-1] if len(argument) > 1 and argument[0] == argument[-1] == '"' else argument


def gcc_includes(gcc, arch):
    """Gets the folders the GCC searches for <...> includes, for the library variant arch picks."""
    result = subprocess.run([gcc, "-xc++", "-E", "-v", *arch, "-"], input="", capture_output=True,
                            text=True)
    lines = result.stderr.splitlines()
    try:
        start = lines.index("#include <...> search starts here:") + 1
        end = lines.index("End of search list.")
    except ValueError:
        sys.stderr.write(result.stderr)
        raise SystemExit(2)
    return [os.path.normpath(line.strip()) for line in lines[start:end]]


def gcc_version(gcc):
    """Gets the GCC's version, such as 11.3.1."""
    result = subprocess.run([gcc, "-dumpfullversion"], capture_output=True, text=True)
    if result.returncode != 0 or not result.stdout.strip():
        sys.stderr.write(result.stderr)
        raise SystemExit(2)
    return result.stdout.strip()


def for_clang(entry):
    """Turns one GCC compile command into the arguments clang-tidy reads."""
    args = [unquote(a) for a in arguments(entry)]
    gcc = args[0]
    arch = [a for a in args if a.startswith(ARCH)]
    kept = ["clang++"]
    skip = 0
    for argument in args[1:]:
        if skip:
            skip -= 1
            continue
        if argument in GCC_ONLY:
            skip = GCC_ONLY[argument]
            continue
        if argument.startswith("-W") and argument not in ("-Wall", "-Wextra"):
            continue
        if argument.replace("\\", "/") == entry["file"].replace("\\", "/"):
            continue
        kept.append(argument)
    for folder in gcc_includes(gcc, arch):
        kept += ["-isystem", folder]
    kept.append(f"-fgnuc-version={gcc_version(gcc)}")
    return kept + CLANG_EXTRA + [entry["file"]]


# the planted files of the self-test, and the checks the second one has to report
CLEAN = """#include <Arduino.h>

constexpr uint32_t PLANTED_LIMIT = 7;

uint32_t plantedClamp(uint32_t n) {
    return n > PLANTED_LIMIT ? PLANTED_LIMIT : n;
}
"""
FLAWED = """#include <Arduino.h>

int plantedDepth(int n) {
    return n > 1 ? plantedDepth(n - 1) * 3 : 7;
}
"""
PLANTED_CHECKS = ("readability-magic-numbers", "misc-no-recursion")
SELF_TEST = os.path.join("firmware", ".tidy-self-test")


def check(tidy, entries, fix=False, show=True):
    """Runs clang-tidy on the entries. Gets the findings in firmware/ and the foreign errors.

    A finding counts where it stands in firmware/, and a header that several files include reports
    it once. An error in the Teensy core or a library comes from a declaration GCC accepts and
    clang does not, so such errors are only counted.
    """
    with tempfile.TemporaryDirectory() as folder:
        database = [{"directory": e["directory"], "file": e["file"], "arguments": for_clang(e)}
                    for e in entries]
        with open(os.path.join(folder, "compile_commands.json"), "w", encoding="utf-8") as f:
            json.dump(database, f)

        def run(entry):
            command = [tidy, "-p", folder, "--quiet", entry["file"]]
            if fix:
                command.append("--fix")
            return subprocess.run(command, cwd=ROOT, capture_output=True, text=True)

        jobs = 1 if fix else os.cpu_count() or 1
        with ThreadPoolExecutor(jobs) as pool:
            results = list(pool.map(run, entries))

    seen = {}
    foreign = set()
    for result in results:
        for block in diagnostics(result.stdout):
            if in_firmware(block[0]):
                if block[0] not in seen:
                    seen[block[0]] = block
                    if show:
                        print("\n".join(block))
            else:
                foreign.add(block[0])
        if show and result.returncode and not result.stdout.strip():
            print(result.stderr.strip())
    return seen, foreign


def planted(template, name, source):
    """Writes a planted file and gets its compile entry, made from the template's command."""
    path = os.path.join(SELF_TEST, name)
    with open(os.path.join(ROOT, path), "w", encoding="utf-8") as f:
        f.write(source)
    args = [path if unquote(a).replace("\\", "/") == template["file"].replace("\\", "/") else a
            for a in arguments(template)]
    return {"directory": template["directory"], "file": path, "arguments": args}


def self_test(tidy, template):
    """Checks a clean planted file and a flawed one, and requires each verdict."""
    os.makedirs(os.path.join(ROOT, SELF_TEST), exist_ok=True)
    try:
        clean = planted(template, "clean.cpp", CLEAN)
        flawed = planted(template, "flawed.cpp", FLAWED)
        seen, foreign = check(tidy, [clean], show=False)
        ok_clean = not seen and not foreign
        print(f"{'ok  ' if ok_clean else 'FAIL'} the clean file: {len(seen)} findings, "
              f"{len(foreign)} errors in other headers, expected none")
        for line in list(seen) + sorted(foreign):
            print("    " + line)
        seen, _ = check(tidy, [flawed], show=False)
        reported = {name for name in PLANTED_CHECKS if any(name in line for line in seen)}
        ok_flawed = reported == set(PLANTED_CHECKS)
        print(f"{'ok  ' if ok_flawed else 'FAIL'} the flawed file: reports "
              f"{', '.join(sorted(reported)) or 'nothing'}, expected {', '.join(PLANTED_CHECKS)}")
    finally:
        shutil.rmtree(os.path.join(ROOT, SELF_TEST), ignore_errors=True)
    return 0 if ok_clean and ok_flawed else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("patterns", nargs="*", help="check only the files whose path holds one")
    parser.add_argument("--fix", action="store_true", help="apply the fixes clang-tidy offers")
    parser.add_argument("--self-test", action="store_true",
                        help="check a clean and a flawed planted file and require each verdict")
    args = parser.parse_args()

    tidy = shutil.which("clang-tidy")
    if tidy is None:
        sys.stderr.write("no clang-tidy on the path: python -m pip install clang-tidy==18.1.8\n")
        return 2

    entries = [e for e in compile_commands() if e["file"].replace("\\", "/").startswith(OWN)]
    if not entries:
        sys.stderr.write("the compile commands hold no file of firmware/src\n")
        return 2
    if args.self_test:
        return self_test(tidy, entries[0])
    if args.patterns:
        entries = [e for e in entries if any(p in e["file"].replace("\\", "/") for p in args.patterns)]
    if not entries:
        sys.stderr.write("no file of firmware/src matches\n")
        return 2

    seen, foreign = check(tidy, entries, fix=args.fix)
    print(f"\n{len(entries)} files checked, {len(seen)} findings in firmware/, "
          f"{len(foreign)} errors in other headers")
    return 1 if seen else 0


def diagnostics(output):
    """Splits clang-tidy's output into its diagnostics, each its first line and the lines below."""
    block = []
    for line in output.splitlines():
        if ": warning: " in line or ": error: " in line:
            if block:
                yield block
            block = [line]
        elif block:
            block.append(line)
    if block:
        yield block


def in_firmware(line):
    """Gets whether a diagnostic's first line names a file in firmware/."""
    path = line.split(": ", 1)[0].rsplit(":", 2)[0]
    path = os.path.normpath(os.path.join(ROOT, path))
    return path.startswith(os.path.join(ROOT, "firmware") + os.sep)


if __name__ == "__main__":
    sys.exit(main())
