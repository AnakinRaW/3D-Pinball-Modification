"""Copies the repository's files for the memory card onto the card in the Teensy, over USB.

The firmware's card upload takes each file in chunks with a checksum and answers each chunk once
it is on the card. Every file under firmware/src/resources/ but its README goes to the card's root
with its path below that folder, so config.yml becomes /config.yml. Every file under games/ goes to
/games/ with its path. A file on the card that the upload writes is replaced. Every other file on
the card stays. The Teensy restarts at the end, so the machine starts with the new files.

    python firmware/tools/card-upload.py                   firmware/src/resources/ and games/
    python firmware/tools/card-upload.py <file>            the named files of the repository
    python firmware/tools/card-upload.py --port COM3       a given port instead of the first Teensy
    python firmware/tools/card-upload.py --no-restart      leaves the Teensy running

python -m platformio run -t uploadcard runs it with no arguments.

Exit code 0 when every file is on the card, 1 when one is not, 2 when no Teensy answers.
Design: docs/firmware/components/card-upload.md.
"""

import argparse
import os
import sys
import time
import zlib

import serial
import serial.tools.list_ports

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
RESOURCES = os.path.join(ROOT, "firmware", "src", "resources")   # the card's root
GAMES = os.path.join(ROOT, "games")                               # the card's /games/
SKIPPED = {"README.md"}   # the folder's description, which stays in the repository
TEENSY_VID = 0x16C0        # PJRC's USB vendor id
BAUD = 115200              # ignored by USB serial, needed by pyserial
CHUNK_BYTES = 4096         # CardUpload::CHUNK_BYTES
ANSWER_TIMEOUT_S = 10      # longer than any write of a chunk takes on the card
OPEN_TIMEOUT_S = 10        # the Teensy's start after a flash
RETRIES = 3                # the times a chunk whose checksum failed is sent again
PREFIX = "CARD "


class Failure(Exception):
    pass


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
    """The serial port to the firmware, read as answers. Debug lines in between are passed over."""

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
    if answer != "READY":
        raise Failure(f"{card_path}: {answer}")
    at = 0
    while at < len(data):
        chunk = data[at:at + CHUNK_BYTES]
        for attempt in range(RETRIES):
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


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("files", nargs="*", help="files under firmware/src/resources/ or games/, "
                        "by default all of them")
    parser.add_argument("--port", help="the Teensy's serial port, by default the first one found")
    parser.add_argument("--no-restart", action="store_true", help="leaves the Teensy running")
    args = parser.parse_args()

    try:
        files = card_files(args.files)
    except Failure as e:
        print(e)
        return 1
    port = open_port(args.port)
    if port is None:
        print("no Teensy on USB serial")
        return 2
    link = Link(port)
    failed = 0
    with port:
        for path, card_path in files:
            with open(path, "rb") as f:
                data = f.read()
            try:
                put(link, card_path, data)
                print(f"{card_path}: {len(data)} bytes")
            except Failure as e:
                print(e)
                failed += 1
        if not args.no_restart:
            link.send(f"{PREFIX}RESTART\n".encode())
            print("the Teensy restarts")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
