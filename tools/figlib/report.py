"""What a run found, and the lines it prints."""
from __future__ import annotations


class Report:
    def __init__(self):
        self.errors: list[str] = []
        self.notes: list[str] = []
        # (kind, key) beside each error, so a caller counts or looks one up
        # without matching on the message text
        self.tags: list[tuple[str, str]] = []

    def error(self, msg, kind="", key=""):
        self.errors.append(msg)
        self.tags.append((kind, key))

    def count(self, kind):
        return sum(1 for k, _ in self.tags if k == kind)

    def has(self, kind, key):
        return (kind, key) in self.tags

    def note(self, msg):
        self.notes.append(msg)


def status(ok: bool, label: str, detail: str):
    print(f"  {'ok ' if ok else 'FAIL'}  {label:<30} {detail}")


def listing(title: str, lines):
    """A titled block of the report, one indented line each, after a blank line."""
    print()
    print(title)
    for line in lines:
        print("  " + line)
