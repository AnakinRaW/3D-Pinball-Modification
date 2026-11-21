# Model

The Python files the checks in [`tools/`](../tools/) read, in folders that mirror those under [`docs/`](../docs/).

| File | Contents |
|---|---|
| `parts/<subsystem>/figures.py` | Every figure the subsystem derives, each input with its source |
| `parts/<subsystem>/schematic.py` | The subsystem's boards, part by part and net by net, where they are declared |
| `parts/<subsystem>/*.layout.py` | A perfboard layout of one of those boards |
