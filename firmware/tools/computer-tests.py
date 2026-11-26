"""Runs the computer tests, several programs at the same time.

Each folder firmware/tests/test_<program> is one run of `pio test -e computer -f test_<program>`.
A run builds the firmware's code and the libraries once and keeps their object files, so each
further program in the same build folder only compiles its test and links, which takes a few
seconds. Several workers therefore take the programs one after another from a shared list, each
in a build folder below the workspace that PLATFORMIO_BUILD_DIR names. Each worker also keeps a
build cache, which PLATFORMIO_BUILD_CACHE_DIR names, since SCons writes the signatures of a build
into the cache folder and two builds writing that file at once break each other. The processor's
cores are split between the workers through PLATFORMIO_RUN_JOBS, and the program that took
longest in the last run starts first.

    python firmware/tools/computer-tests.py                    every program
    python firmware/tools/computer-tests.py screen "s*"        the programs whose name matches
    python firmware/tools/computer-tests.py --jobs 8           eight workers
    python firmware/tools/computer-tests.py --json-output-path report.json

The report written with --json-output-path holds the test suites of every run in the format of
`pio test --json-output-path`, so `test-coverage.py --computer-results` reads it.

Exit code 0 when every program passed, 1 when one failed, 2 when the run could not start.
"""

import argparse
import fnmatch
import glob
import json
import math
import os
import queue
import subprocess
import sys
import tempfile
import threading
import time

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
TESTS_DIR = os.path.join(ROOT, "firmware", "tests")
ENV = "computer"
# Every worker builds the firmware's code and the libraries once in an empty folder, so each further
# worker lengthens the first run. Four workers gave the shortest first run on 16 cores.
DEFAULT_WORKERS = 4
# the file below the workers' folders that keeps each program's duration in the last run
DURATIONS_FILE = "durations.json"

print_lock = threading.Lock()


def programs(patterns):
    """Gets the names of the test folders that match one of the patterns, or all of them."""
    names = sorted(os.path.basename(path) for path in glob.glob(os.path.join(TESTS_DIR, "test_*"))
                   if os.path.isdir(path))
    if not patterns:
        return names
    return [name for name in names
            if any(fnmatch.fnmatch(name[len("test_"):], pattern) for pattern in patterns)]


def workers_root():
    """Gets the folder that holds the workers' folders, inside the workspace of platformio.ini."""
    from platformio.project.config import ProjectConfig

    config = ProjectConfig(os.path.join(ROOT, "platformio.ini"))
    return os.path.join(config.get("platformio", "workspace_dir"), "computer-tests")


def read_durations(root):
    """Gets each program's duration in the last run, in seconds, as far as it is known."""
    try:
        with open(os.path.join(root, DURATIONS_FILE), encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def run(program, folder, compile_jobs, report_dir):
    """Runs one program in a worker's folder. Gets (program, passed, seconds, report path)."""
    report = os.path.join(report_dir, f"{program}.json")
    env = dict(os.environ, PLATFORMIO_BUILD_DIR=os.path.join(folder, "build"),
               PLATFORMIO_BUILD_CACHE_DIR=os.path.join(folder, "cache"),
               PLATFORMIO_RUN_JOBS=str(compile_jobs))
    command = [sys.executable, "-m", "platformio", "test", "-e", ENV, "-f", program,
               "--json-output-path", report]
    start = time.monotonic()
    result = subprocess.run(command, cwd=ROOT, env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, errors="replace")
    seconds = time.monotonic() - start
    passed = result.returncode == 0
    with print_lock:
        print(f"{'passed' if passed else 'FAILED':6}  {program:32} {seconds:6.1f} s", flush=True)
        if not passed:
            print(result.stdout, flush=True)
    return program, passed, seconds, report


def work(pending, folder, compile_jobs, report_dir, results):
    """Takes programs from the list until it is empty and runs each in one folder."""
    while True:
        try:
            program = pending.get_nowait()
        except queue.Empty:
            return
        results.append(run(program, folder, compile_jobs, report_dir))


def combine(results, seconds, path):
    """Writes one report holding the suites that the runs ran, as pio test writes it."""
    suites = []
    for program, _, _, report in results:
        if not os.path.exists(report):
            continue
        with open(report, encoding="utf-8") as f:
            run_report = json.load(f)
        suites += [suite for suite in run_report["test_suites"]
                   if suite["env_name"] == ENV and suite["test_name"] == program]
    combined = {"version": "1.0", "project_dir": ROOT, "duration": seconds}
    for key in ("testcase_nums", "error_nums", "failure_nums", "skipped_nums"):
        combined[key] = sum(suite.get(key, 0) for suite in suites)
    combined["test_suites"] = suites
    with open(path, "w", encoding="utf-8") as f:
        json.dump(combined, f, indent=1)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("patterns", nargs="*",
                        help="picks the programs whose name without test_ matches, such as screen")
    parser.add_argument("--jobs", "-j", type=int, default=DEFAULT_WORKERS,
                        help=f"the workers that run programs at the same time, {DEFAULT_WORKERS} "
                             "by default")
    parser.add_argument("--json-output-path", metavar="JSON",
                        help="writes the report of every run into one file")
    args = parser.parse_args()

    names = programs(args.patterns)
    if not names:
        print("no test program matches", file=sys.stderr)
        return 2
    if "LOCALAPPDATA" not in os.environ:
        print("LOCALAPPDATA has to be set, since platformio.ini builds its workspace from it",
              file=sys.stderr)
        return 2

    cores = os.cpu_count() or 1
    workers = max(1, min(args.jobs, len(names)))
    compile_jobs = max(1, math.ceil(cores / workers))
    root = workers_root()
    durations = read_durations(root)
    names.sort(key=lambda name: durations.get(name, 0.0), reverse=True)
    pending = queue.Queue()
    for name in names:
        pending.put(name)

    # The runs would otherwise install the libraries into the shared folder at the same time.
    start = time.monotonic()
    install = subprocess.run([sys.executable, "-m", "platformio", "pkg", "install", "-e", ENV],
                             cwd=ROOT, capture_output=True, text=True)
    if install.returncode != 0:
        sys.stderr.write(install.stdout + install.stderr)
        return 2

    print(f"{len(names)} programs on {workers} workers, {compile_jobs} compiler jobs each",
          flush=True)
    results = []
    with tempfile.TemporaryDirectory() as report_dir:
        threads = [threading.Thread(target=work, args=(pending, os.path.join(root, str(index)),
                                                       compile_jobs, report_dir, results))
                   for index in range(workers)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        seconds = time.monotonic() - start
        if args.json_output_path:
            combine(results, seconds, args.json_output_path)

    durations.update({program: round(took, 1) for program, _, took, _ in results})
    os.makedirs(root, exist_ok=True)
    with open(os.path.join(root, DURATIONS_FILE), "w", encoding="utf-8") as f:
        json.dump(durations, f, indent=1, sort_keys=True)

    failed = sorted(program for program, passed, _, _ in results if not passed)
    minutes, rest = divmod(round(seconds), 60)
    print(f"{len(results) - len(failed)} of {len(results)} programs passed in "
          f"{minutes} min {rest} s" + (f", failed: {', '.join(failed)}" if failed else ""))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
