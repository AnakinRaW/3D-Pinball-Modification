"""Copies the repository's files for the memory card onto the card in the Teensy, over USB.

The firmware takes no files over USB, so the script flashes the program in card-upload/ for the
time of the copy, sends it the files and flashes the firmware back. The program takes each file in
chunks with a checksum and answers each chunk once it is on the card. Every file under
firmware/src/resources/ but its README goes to the card's root with its path below that folder, so
config.yml becomes /config.yml. Every file under games/ goes to /games/ with its path. A file on
the card that the copy writes is replaced. Every other file on the card stays.

    python firmware/tools/card-upload.py                   firmware/src/resources/ and games/
    python firmware/tools/card-upload.py <file>            the named files of the repository
    python firmware/tools/card-upload.py --port COM3       a given port instead of the first Teensy
    python firmware/tools/card-upload.py --env release     flashes the release firmware back
    python firmware/tools/card-upload.py --no-firmware     leaves the upload program on the Teensy

python -m platformio run -t uploadcard runs it with no arguments.

Exit code 0 when every file is on the card, 1 when one is not, 2 when a flash fails or no Teensy
answers. card-upload/src/main.cpp gives the commands and the answers.
"""

import argparse
import os
import subprocess
import sys
import time
import zlib

import serial
import serial.tools.list_ports

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
PROGRAM = os.path.join(ROOT, "firmware", "tools", "card-upload")   # the upload program
RESOURCES = os.path.join(ROOT, "firmware", "src", "resources")    # the card's root
GAMES = os.path.join(ROOT, "games")                               # the card's /games/
SKIPPED = {"README.md"}    # the folder's description, which stays in the repository
TEENSY_VID = 0x16C0        # PJRC's USB vendor id
BAUD = 115200              # ignored by USB serial, needed by pyserial
CHUNK_BYTES = 4096         # CHUNK_BYTES of the upload program
ANSWER_TIMEOUT_S = 10      # longer than any write of a chunk takes on the card
OPEN_TIMEOUT_S = 20        # the Teensy's start after a flash
RETRIES = 3                # the times a chunk whose checksum failed is sent again
PREFIX = "CARD "


class Failure(Exception):
    pass


def flash(project, env=None):
    """Builds and flashes a PlatformIO project onto the Teensy."""
    command = [sys.executable, "-m", "platformio", "run", "-d", project, "-t", "upload"]
    if env:
        command += ["-e", env]
    return subprocess.run(command).returncode == 0


def find_port():
    for port in serial.tools.list_ports.comports():
        if port.vid == TEENSY_VID:
            return port.device
    return None


def open_port(name):
    deadline = time.monotonic() + OPEN_TIMEOUT_S
    while time.monotonic() < deadline:
        found = name or find_port()
        if found is not None:
            try:
                return serial.Serial(found, BAUD, timeout=0.2)
            except (serial.SerialException, OSError):
                pass
        time.sleep(0.2)
    return None


class Link:
    """The serial port to the upload program, read as answers."""

    def __init__(self, port):
        self.port = port
        self.buffer = b""

    def send(self, data):
        self.port.write(data)

    def answer(self):
        """Gets the next line that starts with CARD, without the prefix."""
        deadline = time.monotonic() + ANSWER_TIMEOUT_S
        while True:
            while b"\n" not in self.buffer:
                if time.monotonic() > deadline:
                    raise Failure("the Teensy did not answer in time")
                self.buffer += self.port.read(self.port.in_waiting or 1)
            line, self.buffer = self.buffer.split(b"\n", 1)
            text = line.decode(errors="replace").rstrip("\r")
            if text.startswith(PREFIX):
                return text[len(PREFIX):]


def put(link, card_path, data):
    """Writes one file onto the card."""
    link.send(f"{PREFIX}PUT {card_path} {len(data)}\n".encode())
    answer = link.answer()
    if len(data) == 0:
        if answer != "DONE 0":
            raise Failure(f"{card_path}: {answer}")
        return
    if answer != "OPEN":
        raise Failure(f"{card_path}: {answer}")
    at = 0
    while at < len(data):
        chunk = data[at:at + CHUNK_BYTES]
        for _ in range(RETRIES):
            link.send(f"{PREFIX}DATA {len(chunk)} {zlib.crc32(chunk):08x}\n".encode() + chunk)
            answer = link.answer()
            if answer != "FAIL checksum":
                break
        at += len(chunk)
        wanted = f"DONE {at}" if at == len(data) else f"OK {at}"
        if answer != wanted:
            raise Failure(f"{card_path}: {answer}")


def card_path(path):
    """Gets a repository file's path on the card."""
    for folder, prefix in ((RESOURCES, "/"), (GAMES, "/games/")):
        relative = os.path.relpath(path, folder).replace("\\", "/")
        if not relative.startswith("..") and os.path.basename(path) not in SKIPPED:
            return prefix + relative
    raise Failure(f"{path} lies outside firmware/src/resources/ and games/")


def card_files(named):
    """Gets each repository file with its path on the card."""
    if named:
        paths = [os.path.abspath(p) for p in named]
    else:
        paths = []
        for top in (RESOURCES, GAMES):
            for folder, _, names in os.walk(top):
                paths += [os.path.join(folder, n) for n in sorted(names) if n not in SKIPPED]
    return [(path, card_path(path)) for path in paths]


def copy(port_name, files):
    """Sends the files to the upload program and gets how many failed, None where none went."""
    port = open_port(port_name)
    if port is None:
        print("no Teensy on USB serial")
        return None
    link = Link(port)
    failed = 0
    with port:
        try:
            answer = link.answer()
        except Failure as e:
            print(e)
            return None
        if answer != "READY":
            print(answer)
            return None
        for path, card in files:
            with open(path, "rb") as f:
                data = f.read()
            try:
                put(link, card, data)
                print(f"{card}: {len(data)} bytes")
            except Failure as e:
                print(e)
                failed += 1
    return failed


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("files", nargs="*", help="files under firmware/src/resources/ or games/, "
                        "by default all of them")
    parser.add_argument("--port", help="the Teensy's serial port, by default the first one found")
    parser.add_argument("--env", help="the firmware's environment flashed back, by default debug")
    parser.add_argument("--no-firmware", action="store_true",
                        help="leaves the upload program on the Teensy")
    args = parser.parse_args()

    try:
        files = card_files(args.files)
    except Failure as e:
        print(e)
        return 1
    if not flash(PROGRAM):
        print("the upload program did not flash")
        return 2
    failed = copy(args.port, files)
    if not args.no_firmware and not flash(ROOT, args.env):
        print("the firmware did not flash back")
        return 2
    if failed is None:
        return 2
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
