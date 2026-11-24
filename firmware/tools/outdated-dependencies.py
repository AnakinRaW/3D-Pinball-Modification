"""Lists the dependencies of the firmware whose locked version has a newer release.

PlatformIO never updates a locked version, so a release that fixes a bug or a vulnerability goes
unseen until someone looks. The run reads every platformio.ini of the firmware: the one in the root,
the test image's and each bench program's. It checks the two kinds of dependency they hold.

A registry dependency is a package of PlatformIO's registry with an exact version, such as
fastled/FastLED@3.10.10 in lib_deps or platformio/framework-arduinoteensy@1.162.0 in
platform_packages. The registry's newest release is the reference. A prerelease does not count.

A git dependency is a repository locked to a commit, such as
https://github.com/yaml/libyaml.git#<commit>. The reference is the repository's newest release tag,
found with git ls-remote. A repository without a release tag is compared with the head of its
default branch. The dependency is behind when the reference has commits that the locked commit
lacks. GitHub's compare endpoint answers that, and without an answer a locked commit that differs
from the reference counts as behind. A GITHUB_TOKEN in the environment raises the endpoint's rate
limit.

    python firmware/tools/outdated-dependencies.py

An entry that holds no exact version, such as a platform given by its name only, is listed as not
locked and does not change the exit code.

Exit code 0 when no dependency is behind, 1 when one is, 2 when a reference could not be looked up.
"""

import glob
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.request

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
INIS = ("platformio.ini", "firmware/tests/teensy/platformio.ini", "firmware/bench/*/platformio.ini")
REGISTRY = "https://api.registry.platformio.org/v3/packages"
GITHUB = "https://api.github.com/repos"

# the registry type of the packages each key of platformio.ini names, in the order to try them
TYPES = {"lib_deps": ("library",), "platform_packages": ("tool", "platform"), "platform": ("platform",)}

KEY = re.compile(r"^([A-Za-z_][\w.]*)\s*=\s*(.*)$")
REGISTRY_SPEC = re.compile(r"^(?:[\w.-]+=)?(?:([\w.-]+)/)?([\w.-]+?)(?:@(.+))?$")
GIT_SPEC = re.compile(r"https://github\.com/([\w.-]+)/([\w.-]+?)(?:\.git)?#([\w./-]+)$")
VERSION = re.compile(r"\d+(?:\.\d+)*")
PRERELEASE = re.compile(r"pre|rc|alpha|beta|dev", re.IGNORECASE)
SHA = re.compile(r"^[0-9a-f]{40}$")


def numbers(version):
    """The numbers of a version or tag name, which order two versions of the same package."""
    found = VERSION.search(version)
    return tuple(int(part) for part in found.group().split(".")) if found else ()


def entries(path):
    """The (key, value) pairs of one platformio.ini, with each value of a list as a separate pair."""
    key = None
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.lstrip().startswith((";", "#")) or not line.strip():
                continue
            if line[0] not in " \t":
                found = KEY.match(line.rstrip())
                key = found.group(1) if found else None
                value = found.group(2) if found else ""
            else:
                value = line.strip()
            if key in TYPES or key == "platform":
                if value.strip():
                    yield key, value.strip()


def collect():
    """Maps each distinct locked dependency to the files that hold it."""
    registry, git, unlocked = {}, {}, {}
    for pattern in INIS:
        for path in sorted(glob.glob(os.path.join(ROOT, pattern))):
            name = os.path.relpath(path, ROOT).replace(os.sep, "/")
            for key, value in entries(path):
                found = GIT_SPEC.search(value)
                if found:
                    git.setdefault(found.groups(), []).append(name)
                    continue
                found = REGISTRY_SPEC.match(value)
                if not found or "/" not in value and key == "lib_deps" and found.group(3) is None:
                    # a library of the Teensy core, such as SD, has no registry entry and no version
                    continue
                owner, package, version = found.groups()
                if version is None or not VERSION.fullmatch(version):
                    unlocked.setdefault((key, value), []).append(name)
                else:
                    registry.setdefault((key, owner, package, version), []).append(name)
    return registry, git, unlocked


def get_json(url):
    request = urllib.request.Request(url, headers={"User-Agent": "outdated-dependencies"})
    token = os.environ.get("GITHUB_TOKEN")
    if token and url.startswith(GITHUB):
        request.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def registry_newest(key, owner, package):
    """The newest release of a package in PlatformIO's registry, None where it has no entry."""
    for kind in TYPES[key]:
        try:
            return get_json(f"{REGISTRY}/{owner or 'platformio'}/{kind}/{package}")["version"]["name"]
        except urllib.error.HTTPError as error:
            if error.code != 404:
                raise
    return None


def ls_remote(url, *options, patterns=()):
    result = subprocess.run(["git", "ls-remote", *options, url, *patterns], capture_output=True, text=True)
    if result.returncode != 0:
        raise OSError(result.stderr.strip() or f"git ls-remote {url} failed")
    return [line.split("\t") for line in result.stdout.splitlines()]


def git_reference(url):
    """The newest release tag of a repository with its commit, else its default branch's head."""
    tags = {}
    for sha, ref in ls_remote(url, "--tags"):
        name = ref.removeprefix("refs/tags/")
        peeled = name.endswith("^{}")
        name = name.removesuffix("^{}")
        # an annotated tag lists the tag object first and the commit it points at second
        if peeled or name not in tags:
            tags[name] = sha
    releases = {name: sha for name, sha in tags.items() if numbers(name) and not PRERELEASE.search(name)}
    if releases:
        newest = max(releases, key=lambda name: (numbers(name), name))
        return newest, releases[newest], tags
    head = ls_remote(url, "--symref", patterns=("HEAD",))
    branch = head[0][0].split()[1].removeprefix("refs/heads/") if head[0][0].startswith("ref:") else "HEAD"
    return f"{branch} branch", head[-1][0], tags


def behind(owner, repository, locked, reference):
    """Whether the reference has commits the locked commit lacks, None where GitHub gives no answer."""
    try:
        status = get_json(f"{GITHUB}/{owner}/{repository}/compare/{locked}...{reference}")["status"]
    except (urllib.error.URLError, KeyError, ValueError):
        return None
    return status in ("ahead", "diverged")


def check_registry(dependencies):
    outdated, failed = [], []
    for (key, owner, package, version), files in sorted(dependencies.items()):
        name = f"{owner}/{package}" if owner else package
        try:
            newest = registry_newest(key, owner, package)
        except (OSError, KeyError, ValueError) as error:
            failed.append((name, str(error)))
            continue
        if newest is None:
            failed.append((name, "no entry in PlatformIO's registry"))
        elif numbers(newest) > numbers(version):
            outdated.append((name, version, newest, files))
    return outdated, failed


def check_git(dependencies):
    outdated, failed = [], []
    for (owner, repository, locked), files in sorted(dependencies.items()):
        name = f"{owner}/{repository}"
        url = f"https://github.com/{owner}/{repository}.git"
        try:
            label, reference, tags = git_reference(url)
        except OSError as error:
            failed.append((name, str(error)))
            continue
        shown = next((tag for tag, sha in sorted(tags.items()) if sha == locked), locked[:12])
        if SHA.match(locked) and locked == reference:
            continue
        if not SHA.match(locked):
            # a dependency locked to a tag or a branch
            if locked in tags and numbers(locked) >= numbers(label):
                continue
            shown = locked
        result = behind(owner, repository, locked, reference)
        if result is None or result:
            outdated.append((name, shown, label + (f" ({reference[:12]})" if "branch" in label else ""), files))
    return outdated, failed


def main():
    registry, git, unlocked = collect()
    outdated, failed = [], []
    for found, problems in (check_registry(registry), check_git(git)):
        outdated += found
        failed += problems
    for name, locked, newest, files in outdated:
        print(f"{name}: locked {locked}, newest {newest}")
        print(f"    in {', '.join(files)}")
    for name, reason in failed:
        print(f"{name}: no reference found, {reason}", file=sys.stderr)
    for (key, value), files in sorted(unlocked.items()):
        print(f"not locked: {key} = {value} in {', '.join(files)}")
    total = len(registry) + len(git)
    print(f"{total} dependencies checked, {len(outdated)} behind")
    if outdated:
        return 1
    return 2 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
