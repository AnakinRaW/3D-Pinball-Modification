# Model

The Python files the checks in [`tools/`](../tools/) read, in folders that mirror those under [`docs/`](../docs/). Each one belongs to the documents and drawings of the folder it mirrors.

| File | Contents |
|---|---|
| [`research/teensy-4.1.py`](research/teensy-4.1.py) | The Teensy 4.1's figures and what each of its pins can carry, which the subsystems take their Teensy figures from |
| [`firmware/figures.py`](firmware/figures.py) | The firmware's periods, watchdog timeout, interrupt priorities and slots |
| `parts/<subsystem>/figures.py` | Every figure the subsystem derives, each input with its source |
