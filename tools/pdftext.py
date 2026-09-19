"""Pull the text-showing strings out of a PDF, enough to look a figure up.

This is not a PDF reader. It walks the objects, inflates the content streams,
collects the strings the text operators show, and maps two-byte codes through
every ToUnicode table in the file, merged into one. Where two subset fonts
assign the same code to different glyphs the merged table picks one, so **a
figure that is found is not proof that the sheet states it**. A figure that is
*not* found is worth re-reading by hand, and that is what the report says.

Several manufacturer sheets carry the standard security handler with an empty
user password, which encrypts every stream while leaving the file readable in
any viewer. Those are decrypted here the same way a viewer decrypts them, for
revisions 2 and 3, which use RC4. Revision 4 and up may use AES, which this
module does not implement: such a file comes back empty and the caller reports
the sheet as unread rather than treating its figures as checked.
"""
from __future__ import annotations

import hashlib
import re
import struct
import zlib

OBJ = re.compile(rb"(\d+)\s+(\d+)\s+obj\b")
STREAM_HEAD = re.compile(rb"stream\r?\n")
SHOW = re.compile(rb"\((?:\\.|[^()\\])*\)|<[0-9A-Fa-f\s]+>")
BFCHAR = re.compile(rb"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>")
BFRANGE = re.compile(rb"<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>")
ESC = {b"n": b"\n", b"r": b"\r", b"t": b"\t", b"b": b"\b", b"f": b"\f",
       b"(": b"(", b")": b")", b"\\": b"\\"}
OCTAL = b"01234567"

# the padding string algorithm 2 of the PDF specification prepends to a password
PAD = bytes([0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41, 0x64, 0x00, 0x4E, 0x56,
             0xFF, 0xFA, 0x01, 0x08, 0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80,
             0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A])


def _inflate(payload: bytes) -> bytes | None:
    for candidate in (payload, payload.rstrip(b"\r\n"), payload.rstrip()):
        try:
            return zlib.decompress(candidate)
        except zlib.error:
            continue
    try:
        return zlib.decompressobj().decompress(payload)
    except zlib.error:
        return None


def _unescape(raw: bytes) -> bytes:
    out, i = bytearray(), 0
    while i < len(raw):
        c = raw[i:i + 1]
        if c == b"\\" and i + 1 < len(raw):
            nxt = raw[i + 1:i + 2]
            if nxt in ESC:
                out += ESC[nxt]
                i += 2
                continue
            if nxt in OCTAL:
                j = i + 1
                while j < len(raw) and j < i + 4 and raw[j:j + 1] in OCTAL:
                    j += 1
                out.append(int(raw[i + 1:j], 8) & 0xFF)
                i = j
                continue
            i += 2
            continue
        out += c
        i += 1
    return bytes(out)


def _literal(data: bytes, name: bytes) -> bytes | None:
    """The value of `/Name(...)`, parentheses balanced and escapes undone."""
    i = data.find(name)
    if i < 0:
        return None
    i = data.find(b"(", i)
    if i < 0:
        return None
    depth, j = 0, i
    while j < len(data):
        c = data[j:j + 1]
        if c == b"\\":
            j += 2
            continue
        if c == b"(":
            depth += 1
        elif c == b")":
            depth -= 1
            if depth == 0:
                return _unescape(data[i + 1:j])
        j += 1
    return None


def _rc4(key: bytes, data: bytes) -> bytes:
    s = list(range(256))
    j = 0
    for i in range(256):
        j = (j + s[i] + key[i % len(key)]) & 0xFF
        s[i], s[j] = s[j], s[i]
    out, i, j = bytearray(), 0, 0
    for ch in data:
        i = (i + 1) & 0xFF
        j = (j + s[i]) & 0xFF
        s[i], s[j] = s[j], s[i]
        out.append(ch ^ s[(s[i] + s[j]) & 0xFF])
    return bytes(out)


def _file_key(raw: bytes) -> bytes | None:
    """The RC4 file key for an empty user password, or None if there is none.

    None also covers a handler this module cannot undo, so the caller sees an
    unreadable sheet rather than plausible-looking noise.
    """
    if b"/Encrypt" not in raw:
        return None
    enc = re.search(rb"/Filter\s*/Standard(.{0,800}?)>>", raw, re.S)
    if not enc:
        return None
    d = enc.group(1)
    owner = _literal(d, b"/O")
    perm = re.search(rb"/P\s*(-?\d+)", d)
    rev = re.search(rb"/R\s*(\d+)", d)
    if owner is None or perm is None or rev is None:
        return None
    r = int(rev.group(1))
    if r > 3:
        return None
    ident = re.search(rb"/ID\s*\[\s*<([0-9A-Fa-f\s]+)>", raw)
    if ident:
        first = bytes.fromhex(re.sub(rb"\s", b"", ident.group(1)).decode())
    else:
        lit = re.search(rb"/ID\s*\[\s*\((.*?)\)", raw, re.S)
        first = _unescape(lit.group(1)) if lit else b""
    length = re.search(rb"/Length\s*(\d+)", d)
    n = 5 if r == 2 else max(5, int(length.group(1)) // 8 if length else 16)
    key = hashlib.md5(PAD + owner + struct.pack("<i", int(perm.group(1)))
                      + first).digest()[:n]
    if r >= 3:
        for _ in range(50):
            key = hashlib.md5(key[:n]).digest()[:n]
    return key


def _object_key(key: bytes, num: int, gen: int) -> bytes:
    salt = struct.pack("<I", num)[:3] + struct.pack("<I", gen)[:2]
    return hashlib.md5(key + salt).digest()[:min(len(key) + 5, 16)]


def _streams(raw: bytes) -> list[bytes]:
    """Every stream in the file, decrypted where it has to be, then inflated."""
    key = _file_key(raw)
    out = []
    for m in OBJ.finditer(raw):
        head = STREAM_HEAD.search(raw, m.end(), m.end() + 4000)
        if not head:
            continue
        end = raw.find(b"endstream", head.end())
        if end < 0:
            continue
        payload = raw[head.end():end]
        if key is not None:
            payload = _rc4(_object_key(key, int(m.group(1)), int(m.group(2))), payload)
        data = _inflate(payload)
        # A stream that will not inflate is skipped. Handing the compressed
        # bytes on instead produced 75 000 characters of binary noise that a
        # two-digit figure then matched by chance.
        if data is not None:
            out.append(data)
    return out


def _cmap(chunks: list[bytes]) -> dict[int, str]:
    """One code-to-character table, merged from every ToUnicode in the file."""
    table: dict[int, str] = {}
    for data in chunks:
        for block in re.findall(rb"beginbfchar(.*?)endbfchar", data, re.S):
            for src, dst in BFCHAR.findall(block):
                try:
                    table.setdefault(int(src, 16),
                                     bytes.fromhex(dst.decode()).decode("utf-16-be"))
                except Exception:
                    continue
        for block in re.findall(rb"beginbfrange(.*?)endbfrange", data, re.S):
            for lo, hi, dst in BFRANGE.findall(block):
                try:
                    start = bytes.fromhex(dst.decode()).decode("utf-16-be")
                    if len(start) != 1:
                        continue
                    base = ord(start)
                    for k, code in enumerate(range(int(lo, 16), int(hi, 16) + 1)):
                        table.setdefault(code, chr(base + k))
                except Exception:
                    continue
    return table


def extract(path) -> str:
    raw = open(path, "rb").read()
    streams = _streams(raw)
    table = _cmap([d for d in streams if b"beginbfchar" in d or b"beginbfrange" in d])
    pieces = []
    for data in streams:
        if b"Tj" not in data and b"TJ" not in data:
            continue
        for m in SHOW.finditer(data):
            tok = m.group(0)
            if tok.startswith(b"<"):
                hexs = re.sub(rb"\s", b"", tok[1:-1])
                if len(hexs) % 4 == 0 and table:
                    codes = [int(hexs[i:i + 4], 16) for i in range(0, len(hexs), 4)]
                    pieces.append("".join(table.get(c, "�") for c in codes))
                elif len(hexs) % 2 == 0:
                    pieces.append(bytes.fromhex(hexs.decode()).decode("latin-1"))
            else:
                pieces.append(_unescape(tok[1:-1]).decode("latin-1"))
    return " ".join(pieces)


def readable(text: str) -> float:
    """How much of the extracted text is plausible sheet content.

    A sheet whose fonts this reader cannot map comes out as noise, and a short
    figure then matches by chance. The caller uses this to call a sheet
    unreadable rather than blaming each reading in it.
    """
    if not text:
        return 0.0
    good = sum(c.isalnum() or c in " .,;:-+/()[]%=<>" for c in text)
    return good / len(text)


def squeeze(text: str) -> str:
    """The text with whitespace and unmapped glyphs squeezed out.

    Kerning splits a figure across several show operators, so `1.35` only
    survives once the gaps between them are gone.
    """
    return re.sub(r"[\s�]+", "", text)
