#!/usr/bin/env python3
"""Writes the figures the simulator's current readout takes from the repository's models into
tools/simulator/config/figures.json.

    python tools/simulator/scripts/figures.py           write the file
    python tools/simulator/scripts/figures.py --check   exit with 1 where the file differs

Each figure is evaluated by the model that declares it and written in the unit the app reads it in,
so the app holds no number of its own.
"""
import argparse
import json
import pathlib
import sys

TOOLS = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(TOOLS))
sys.modules.setdefault("figcheck", __import__("figcheck"))

from figlib.repo import ROOT, model_named  # noqa: E402

OUT = TOOLS / "simulator" / "config" / "figures.json"

# key: (model, unit the app reads it in)
FIGURES = {
    # FastLED's power model: what one LED draws at full value of each color, and while dark
    "pm_red": ("lighting", "mW"),
    "pm_green": ("lighting", "mW"),
    "pm_blue": ("lighting", "mW"),
    "pm_dark": ("lighting", "mW"),
    "v_rail": ("lighting", "V"),
    # the lighting's cap with each adapter, and what the whole layout draws at full white under it
    "i_budget": ("lighting", "mA"),
    "i_budget_6a": ("lighting", "mA"),
    "i_white_3a": ("lighting", "mA"),
    "i_white_6a": ("lighting", "mA"),
    # the adapters
    "i_supply_3a": ("power", "mA"),
    "i_supply_6a": ("power", "mA"),
    # the loads that draw all the time, which sum to i_permanent
    "i_teensy_vin": ("power", "mA"),
    "i_amp": ("power", "mA"),
    "i_ir_in": ("power", "mA"),
    "i_disp": ("power", "mA"),
    "i_permanent": ("power", "mA"),
    # the servo at rest, stalled, and the longest one move lasts
    "i_rest": ("servo", "mA"),
    "i_stall": ("servo", "mA"),
    "t_move_max": ("servo", "ms"),
    # the coils: a stock bumper coil, the scoop's coil, the longest pull-in the driver allows,
    # and the most that pull at once
    "i_coil": ("solenoid", "mA"),
    "i_coil_scoop": ("solenoid", "mA"),
    "t_on_max": ("solenoid", "ms"),
    "n_bumpers_at_once": ("solenoid", ""),
    "i_coils_at_once": ("solenoid", "mA"),
    # the most that draws at once with each adapter, and how far the 3 A case passes its adapter
    "i_most_3a": ("power", "mA"),
    "i_most_6a": ("power", "mA"),
    "i_over_3a": ("power", "mA"),
}


def collect() -> dict:
    out = {}
    for key, (name, unit) in FIGURES.items():
        model = model_named(name)
        model.evaluate()
        f = model.figs[key]
        value = f.value.raw if unit == "" else f.value.to(unit)
        out[key] = {"value": round(value, 6), "unit": unit, "model": name,
                    "file": model.path.relative_to(ROOT).as_posix()}
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--check", action="store_true", help="exit with 1 where the file differs")
    args = ap.parse_args()
    text = json.dumps(collect(), indent=2, ensure_ascii=False) + "\n"
    if args.check:
        same = OUT.is_file() and OUT.read_text(encoding="utf-8") == text
        print(f"{OUT.relative_to(ROOT).as_posix()} {'matches the models' if same else 'differs from the models'}")
        return 0 if same else 1
    OUT.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {OUT.relative_to(ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
