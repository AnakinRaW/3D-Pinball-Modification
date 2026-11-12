# PCB

Custom circuit boards for the modification: schematics, layouts, and the fabrication outputs generated from them.

A board's netlist is declared in its subsystem's `schematic.py` under [`model/parts/`](../../model/parts/). `python tools/netlist.py model/parts/<subsystem>/figures.py <board> --kicad <out.net>` writes it out for KiCad. Each board that is made as a printed circuit board gets its own directory here, with a `fab/` directory holding the Gerber, drill, pick-and-place and BOM files sent to the manufacturer.
