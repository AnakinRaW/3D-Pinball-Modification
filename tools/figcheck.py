#!/usr/bin/env python3
"""Recompute the figures a design document states, and check the document against them.

A model file declares the inputs with their sources and the formulas that derive the
figures. tools/README.md describes the checks, the flags and what a model declares.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
# Run as a script this module is __main__. A model imports it by name, which then
# finds it here instead of loading the file a second time.
sys.modules.setdefault("figcheck", sys.modules[__name__])

# The names a model file imports from here.
from figlib.model import TEENSY_RAIL, Model, contact, drives, pull, reads
from figlib.quantities import (Q, ceil_to, db, exp, floor_to, from_db, interp_log, ln,
                               log10, sqrt)
from figlib.cli import main

__all__ = ["Model", "Q", "TEENSY_RAIL", "ceil_to", "contact", "db", "drives", "exp",
           "floor_to", "from_db", "interp_log", "ln", "log10", "main", "pull", "reads",
           "sqrt"]

if __name__ == "__main__":
    sys.exit(main())
