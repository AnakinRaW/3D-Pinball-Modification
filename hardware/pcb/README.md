# PCB

Custom circuit boards for the modification: schematics, layouts, and the fabrication outputs generated from them.

Each board made as a printed circuit board gets a directory here, with a `fab/` directory holding the Gerber, drill, pick-and-place and BOM files sent to the manufacturer. A board's netlist is declared in its subsystem's `schematic.py` under [`model/parts/`](../../model/parts/), and [`tools/README.md`](../../tools/README.md) gives the KiCad export.
