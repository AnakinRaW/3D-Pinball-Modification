"""Runs the Teensy tests of docs/firmware/testing.md on the test image in firmware/tests/teensy.

The image holds every Teensy test. This runner asks it for its tests over USB serial, sends one
"run <test>" line per test and reads the lines of the testing document's Results section back.
Every check is written into the test on the Teensy, so the runner judges nothing itself, except
the debug lines a CHECK line asks it to check: those it counts, with the losses the debug
output states, and answers with "checked". It
fails a test that ends in FAIL or ERROR, outlasts its limit, or loses the serial port, and it reads
PJRC's CrashReport, which the image prints as CRASH lines after a crash.

The Teensy restarts before each program, so each program starts from the state after power-up.
A test that restarts the Teensy itself prints RESTART first, and the runner then sends
"resume <test>".

    python firmware/tools/teensy-tests.py                 every Teensy test, quick
    python firmware/tools/teensy-tests.py "servo.*"       the servo program
    python firmware/tools/teensy-tests.py --soak          the durations testing.md states
    python firmware/tools/teensy-tests.py --bench         with the bench tests, at the bench
    python firmware/tools/teensy-tests.py --upload        builds and flashes the image first

Exit code 0 when every test passed or skipped, 1 when a test failed, hung or crashed, 2 when the
runner could not start a run.
"""

import argparse
import fnmatch
import os
import queue
import re
import subprocess
import sys
import threading
import time

import serial
import serial.tools.list_ports

IMAGE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests", "teensy")

TEENSY_VID = 0x16C0          # PJRC's USB vendor id
BAUD = 115200                # ignored by USB serial, needed by pyserial
LIST_TIMEOUT_S = 5           # an idle image answers list at once
BLANK_TIMEOUT_S = 3          # the display's begin() waits for the panel and sets it up in under 1 s

# The debug lines a CHECK line counts: "<ms> D <tag> <n> <payload>" from DEBUG_LOG() and
# "<ms> D trace <tag> <n> <b> at <us> us" from DEBUG_TRACE(), n counting up from 0.
DEBUG_LINE = re.compile(r"^\d+ D (\S+) (\d+) ([0-9a-f]{8})$")
TRACE_LINE = re.compile(r"^\d+ D trace (\S+) (-?\d+) (-?\d+) at \d+ us$")
# the notices of dropped debug lines and lost trace notes: "<ms> D debug <n> lines lost" and
# "<ms> D trace <n> notes lost"
LOST_LINE = re.compile(r"^\d+ D (?:debug \d+ lines|trace \d+ notes) lost$")
LOST_COUNT = re.compile(r" (\d+) (?:lines|notes) lost$")


def debug_payload(n):
    """The payload of debug line n, as the image's round-trip test computes it."""
    return f"{(n * 2654435761) & 0xFFFFFFFF:08x}"


def trace_payload(n):
    """The second number of trace note n, as the image's round-trip test computes it."""
    return n * 3 + 1


def check_lines(lines, tag, count):
    """Counts the lines of tag that came whole and in order, every line of tag, and the lines and
    notes the loss notices state."""
    good = total = lost = 0
    for line in lines:
        if LOST_LINE.match(line):
            lost += int(LOST_COUNT.search(line).group(1))
            continue
        debug = DEBUG_LINE.match(line)
        trace = TRACE_LINE.match(line)
        if debug and debug.group(1) == tag:
            total += 1
            n = int(debug.group(2))
            good += n == good and debug.group(3) == debug_payload(n)
        elif trace and trace.group(1) == tag:
            total += 1
            n = int(trace.group(2))
            good += n == good and int(trace.group(3)) == trace_payload(n)
    return min(good, count), total, lost
RECONNECT_TIMEOUT_S = 20     # a restart, or a crash report's wait of 8 s before it
LIMIT_MARGIN_S = 5           # added to a test's limit for the USB round trips
READ_TIMEOUT_S = 0.1


class PortLost(Exception):
    pass


class Hang(Exception):
    pass


def find_port():
    for port in serial.tools.list_ports.comports():
        if port.vid == TEENSY_VID:
            return port.device
    return None


class Link:
    """One USB serial connection to the image, read line by line."""

    def __init__(self, port):
        self.port_name = port
        self.port = None
        self.buffer = b""

    def open(self, timeout_s):
        deadline = time.monotonic() + timeout_s
        while True:
            name = self.port_name or find_port()
            try:
                if name is not None:
                    self.port = serial.Serial(name, BAUD, timeout=READ_TIMEOUT_S)
                    self.buffer = b""
                    return
            except (serial.SerialException, OSError):
                pass
            if time.monotonic() > deadline:
                raise PortLost("no Teensy on USB serial")
            time.sleep(0.2)

    def close(self):
        if self.port is not None:
            try:
                self.port.close()
            except (serial.SerialException, OSError):
                pass
            self.port = None

    def send(self, line):
        try:
            self.port.write((line + "\n").encode())
        except (serial.SerialException, OSError) as e:
            raise PortLost(str(e))

    def read_line(self, deadline):
        """Gets the next line, or None once the deadline has passed."""
        while b"\n" not in self.buffer:
            if time.monotonic() > deadline:
                return None
            try:
                chunk = self.port.read(self.port.in_waiting or 1)
            except (serial.SerialException, OSError) as e:
                raise PortLost(str(e))
            self.buffer += chunk
        line, self.buffer = self.buffer.split(b"\n", 1)
        return line.decode(errors="replace").rstrip("\r")

    def wait_gone(self, timeout_s):
        """Waits until the port has gone, as it does when the Teensy restarts."""
        deadline = time.monotonic() + timeout_s
        try:
            while time.monotonic() < deadline:
                self.port.read(self.port.in_waiting or 1)
        except (serial.SerialException, OSError):
            pass
        self.close()
        # Windows may list the old port for a moment after it has gone.
        time.sleep(0.5)


class Image:
    """The test image on the Teensy: its tests and the commands it takes."""

    def __init__(self, link):
        self.link = link
        self.tests = []          # (name, runs on, quick limit s, soak limit s) in order
        self.tests_run = 0       # tests the image has run since its start
        self.crash = []          # the lines of the last crash report

    def connect(self, timeout_s=RECONNECT_TIMEOUT_S):
        self.link.open(timeout_s)
        self.list()

    def list(self):
        self.link.send("list")
        self.tests = []
        deadline = time.monotonic() + LIST_TIMEOUT_S
        while True:
            line = self.link.read_line(deadline)
            if line is None:
                raise Hang("no answer to list")
            word, _, rest = line.partition(" ")
            if word == "CRASH":
                self.crash.append(rest)
            elif word == "TEST":
                name, runs_on, quick, soak = rest.split()
                self.tests.append((name, runs_on, int(quick), int(soak)))
            elif word == "READY":
                self.tests_run = int(rest)
                return

    def take_crash(self):
        crash, self.crash = self.crash, []
        return crash

    def restart(self):
        self.link.send("reset")
        self.link.wait_gone(RECONNECT_TIMEOUT_S)
        self.connect()

    def blank(self):
        """Restarts the image and has it fill the display black.

        The panel stays awake through a restart, so the last restart of a run can leave a stripe
        of stray bytes on it. The display sets itself up within BLANK_TIMEOUT_S.
        """
        self.restart()
        self.link.send("blank")
        deadline = time.monotonic() + BLANK_TIMEOUT_S
        while True:
            line = self.link.read_line(deadline)
            if line is None:
                raise Hang("no answer to blank")
            if line == "BLANK" or line.startswith("ERROR"):
                return line


def answers(enabled):
    """A thread that hands on every line typed at the keyboard, for the ASK lines."""
    lines = queue.Queue()
    if enabled:
        def read():
            for line in sys.stdin:
                lines.put(line.strip())
        threading.Thread(target=read, daemon=True).start()
    return lines


class Runner:
    def __init__(self, image, soak, typed):
        self.image = image
        self.soak = soak
        self.typed = typed
        self.results = []        # (test, result, detail, seconds)

    def command(self, verb, name):
        return f"{verb} {name}" + (" soak" if self.soak else "")

    def run_test(self, name, limit_s):
        """Runs one test and gets its result, detail and the seconds it took."""
        start = time.monotonic()
        link = self.image.link
        link.send(self.command("run", name))
        deadline = start + limit_s + LIMIT_MARGIN_S
        result, detail = None, ""
        seen = []                # the lines of this test that are no protocol line
        while True:
            try:
                line = link.read_line(deadline)
            except PortLost:
                return self.lost(name, start, result)
            if line is None:
                raise Hang(f"{name} gave no END within {limit_s + LIMIT_MARGIN_S} s")
            word, _, rest = line.partition(" ")
            if word in ("PASS", "FAIL", "SKIP"):
                result, detail = word, rest.partition(" ")[2]
                print(f"{word:5} {name}  {detail}".rstrip())
            elif word == "ERROR":
                result, detail = "FAIL", "ERROR " + rest
                print(f"ERROR {rest}")
            elif word == "END":
                if result is None:
                    return "FAIL", "END without a result", time.monotonic() - start
                return result, detail, time.monotonic() - start
            elif word == "RESTART":
                print(f"      {name} restarts the Teensy")
                link.wait_gone(RECONNECT_TIMEOUT_S)
                self.image.connect()
                crash = self.image.take_crash()
                for c in crash:
                    print(f"      | {c}")
                link.send(self.command("resume", name))
                deadline = time.monotonic() + limit_s + LIMIT_MARGIN_S
            elif word == "DO":
                print(f"DO    {rest}")
            elif word == "ASK":
                print(f"ASK   {rest}  [y/n]")
                answer = self.wait_answer(deadline)
                if answer:
                    link.send(answer)
            elif word == "RUN":
                continue
            elif word == "CHECK":
                _, tag, count = rest.split()
                good, total, lost = check_lines(seen, tag, int(count))
                link.send(f"checked {good} {total} {lost}")
            else:
                seen.append(line)
                print(f"      | {line}")

    def wait_answer(self, deadline):
        while time.monotonic() < deadline:
            try:
                typed = self.typed.get(timeout=0.2)
            except queue.Empty:
                continue
            if typed[:1] in ("y", "n"):
                return typed[:1]
        return None

    def lost(self, name, start, result):
        """The port went away during a test: a crash when a crash report follows."""
        self.image.link.close()
        try:
            self.image.connect()
        except (PortLost, Hang) as e:
            raise Hang(f"{name} lost the serial port, and the Teensy did not come back: {e}")
        crash = self.image.take_crash()
        seconds = time.monotonic() - start
        if crash:
            print(f"CRASH {name}")
            for c in crash:
                print(f"      | {c}")
            return "FAIL", "crashed, the crash report stands above", seconds
        print(f"FAIL  {name}  serial port lost")
        return "FAIL", "serial port lost", seconds

    def run(self, selected):
        programs = []
        for test in selected:
            program = test[0].split(".")[0]
            if not programs or programs[-1][0] != program:
                programs.append((program, []))
            programs[-1][1].append(test)
        for program, tests in programs:
            if self.image.tests_run > 0:
                self.image.restart()
            for name, _, quick, soak in tests:
                print(f"RUN   {name}")
                start = time.monotonic()
                try:
                    result, detail, seconds = self.run_test(name, soak if self.soak else quick)
                except (Hang, PortLost) as e:
                    self.results.append((name, "FAIL", f"hung: {e}", time.monotonic() - start))
                    raise
                self.results.append((name, result, detail, seconds))
                self.image.tests_run += 1


def upload():
    """Builds the image and flashes it through PJRC's Teensy Loader."""
    print("building and flashing the test image")
    done = subprocess.run([sys.executable, "-m", "platformio", "run", "-d", IMAGE_DIR,
                           "-t", "upload"])
    return done.returncode == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("patterns", nargs="*", default=["*"],
                        help="tests to run, such as servo.* or servo.pulses; every test without")
    parser.add_argument("--soak", action="store_true",
                        help="take the durations testing.md states rather than the quick ones")
    parser.add_argument("--bench", action="store_true",
                        help="run the bench tests as well, with a person at the bench")
    parser.add_argument("--port", help="the Teensy's serial port; the first Teensy without")
    parser.add_argument("--upload", action="store_true",
                        help="build the image and flash it before the run")
    parser.add_argument("--list", action="store_true", help="list the image's tests and stop")
    args = parser.parse_args()

    if args.upload and not upload():
        print("the upload failed")
        return 2

    image = Image(Link(args.port))
    try:
        image.connect()
    except (PortLost, Hang) as e:
        print(f"cannot reach the test image: {e}")
        return 2
    for c in image.take_crash():
        print(f"crash report from before this run: {c}")

    if args.list:
        for name, runs_on, quick, soak in image.tests:
            print(f"{name:32} {runs_on:7} {quick:5} s {soak:6} s")
        return 0

    matching = [t for t in image.tests
                if any(fnmatch.fnmatchcase(t[0], p) for p in args.patterns)]
    selected = [t for t in matching if args.bench or t[1] != "bench"]
    left_out = len(matching) - len(selected)
    if not selected:
        print("no test matches " + " ".join(args.patterns))
        return 2

    start = time.monotonic()
    runner = Runner(image, args.soak, answers(args.bench))
    hang = None
    try:
        runner.run(selected)
        answer = image.blank()
        if answer != "BLANK":
            print(f"the display stays as the run left it: {answer}")
    except (Hang, PortLost) as e:
        hang = str(e)
        print(f"HANG  {hang}, the run stops here")
    finally:
        image.link.close()
    seconds = time.monotonic() - start

    counts = {r: sum(1 for x in runner.results if x[1] == r) for r in ("PASS", "FAIL", "SKIP")}
    not_run = len(selected) - len(runner.results)
    print()
    for name, result, detail, took in runner.results:
        print(f"{result:5} {name:32} {took:6.1f} s  {detail}".rstrip())
    print(f"\n{counts['PASS']} passed, {counts['FAIL']} failed, {counts['SKIP']} skipped"
          + (f", {not_run} not run" if not_run else "")
          + (f", {left_out} bench tests left out" if left_out else "")
          + f", {'soak' if args.soak else 'quick'}, {seconds:.1f} s")
    return 1 if counts["FAIL"] or hang or not_run else 0


if __name__ == "__main__":
    sys.exit(main())
