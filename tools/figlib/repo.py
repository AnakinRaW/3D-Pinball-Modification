"""The repository the checker runs in: its root, and the models found and loaded there."""
from __future__ import annotations

import functools
import importlib.util
import pathlib
import subprocess
import sys
import tempfile
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    # the model imports this module, so the names stand for the annotations alone
    from .model import Model, PinData

ROOT = pathlib.Path(__file__).resolve().parents[2]

_LOADED: dict = {}


def load_model(path: pathlib.Path) -> Model:
    """The model a file declares, loaded once, so a figure another model takes is one object."""
    path = pathlib.Path(path).resolve()
    if path in _LOADED:
        return _LOADED[path]
    name = f"figmodel_{len(_LOADED)}"
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    mod.MODEL.path = path
    _LOADED[path] = mod.MODEL
    return mod.MODEL


def model_named(name: str) -> Model:
    for path in find_models():
        model = load_model(path)
        if model.name == name:
            return model
    raise KeyError(f"no model is named {name!r}")


def rel(path) -> str:
    """A path as it reads from the repository root."""
    try:
        return pathlib.Path(path).resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def model_file(name) -> pathlib.Path:
    """A model the command line names, as a file under this checkout's docs/.

    A relative path reads from the repository root first. A file outside docs/, such as
    another checkout's model, is refused, since it would be taken against these models.
    """
    name = pathlib.Path(name)
    tried = [name] if name.is_absolute() else [ROOT / name, pathlib.Path.cwd() / name]
    found = [p for p in (t.resolve() for t in tried) if p.is_file()]
    docs = ROOT / "docs"
    for p in found:
        if docs in p.parents:
            return p
    if not found:
        raise ValueError(f"no model at {tried[0]}")
    home = next((d for d in found[0].parents
                 if d != ROOT and (d / "tools" / "figcheck.py").is_file()), None)
    raise ValueError(f"{found[0]} lies outside {docs}, whose models this figcheck checks"
                     + (f". Run {home / 'tools' / 'figcheck.py'} for the models of that "
                        f"checkout" if home else ""))


def find_models() -> list[pathlib.Path]:
    """The Teensy's model and the firmware's first, since the subsystems take figures from them."""
    return (sorted((ROOT / "docs" / "research").glob("*.py"))
            + sorted((ROOT / "docs" / "firmware").glob("figures.py"))
            + sorted((ROOT / "docs" / "parts").glob("*/figures.py")))


@functools.cache
def values_at(ref: str) -> dict[str, dict]:
    """Every figure of every model as it stood at the git revision `ref`, by model name and key.

    A model that git cannot read there, or that does not load or evaluate, has no entry.
    """
    global ROOT, _LOADED
    try:
        names = subprocess.run(["git", "ls-tree", "-r", "-z", "--name-only", ref, "--", "docs"],
                               cwd=ROOT, capture_output=True, text=True, check=True).stdout
        files = {n: subprocess.run(["git", "show", f"{ref}:{n}"], cwd=ROOT,
                                   capture_output=True, check=True).stdout
                 for n in names.split("\0") if n.endswith(".py")}
    except (subprocess.CalledProcessError, FileNotFoundError):
        return {}
    here = ROOT, _LOADED
    with tempfile.TemporaryDirectory() as tmp:
        ROOT, _LOADED = pathlib.Path(tmp).resolve(), {}
        try:
            for name, body in files.items():
                (ROOT / name).parent.mkdir(parents=True, exist_ok=True)
                (ROOT / name).write_bytes(body)
            values = {}
            for path in find_models():
                try:
                    model = load_model(path)
                    model.evaluate()
                except Exception:  # a model this version of the checker cannot read
                    continue
                values[model.name] = {k: f.value for k, f in model.figs.items()}
            return values
        finally:
            ROOT, _LOADED = here


def pin_data() -> PinData:
    """What the pins can carry, from the one model that declares it."""
    found = [m for m in (load_model(p) for p in find_models()) if m.pin_data]
    if len(found) != 1:
        raise ValueError(f"{len(found)} models declare what the pins can carry, and one has to")
    return found[0].pin_data
