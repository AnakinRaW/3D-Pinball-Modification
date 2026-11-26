# Machine config

The machine config holds the settings that change without building and flashing the firmware again. It is the YAML file `/config.yml` on the memory card. It is edited on a computer in [`firmware/src/resources/config.yml`](../../../firmware/src/resources/config.yml). Every value the hardware dictates stays in the code.

## Settings

The config is a hierarchical key-value store. A key is a path through the file, such as `servo.start_deg` or `ir.channels.0.order`. The part that uses a setting gives its key, its default and its range in that part's document.

Every key is defined in [`components/config-keys.h`](../../../firmware/src/components/config-keys.h).

A file that sets the larger adapter and keeps the other settings at their defaults reads:

```yaml
# The machine's settings. A key that is missing takes its default.
lighting:
  adapter_a: 6
audio:
  music_volume: 0.5
  effects_volume: 0.7
servo:
  start_deg: 0
  end_deg: 40
  speed_deg_s: 50
touch:
  left_edge: 327
  right_edge: 1
  top_edge: 3
  bottom_edge: 254
solenoid:
  pull_ms: [30, 30, 30, 30]
```

## The file

The file's top holds sections, such as `servo:`. A section holds named entries. A list holds entries counted from 0. Each entry is a value, a section or a list.

The file takes 8 KB at most. The config keeps 512 entries at most, where every section, every list and every value counts as one. Their names and values take 8 KB at most. A key holds 16 names at most, such as the two of `servo.start_deg`.

An entry the tree cannot hold is left out with everything under it. A line in the [log](../logging.md) names its line and column in the file.

`YamlTree` in [`components/yaml-tree.h`](../../../firmware/src/components/yaml-tree.h) holds the file's tree, with the line and the column of every entry. Its user hands it its memory, so a game's file uses the same tree in the PSRAM.

## Reading

The config component reads the file once, in its `begin()`, with [libyaml](https://github.com/yaml/libyaml). A file that is missing, unreadable, too large or broken leaves every setting at its default. libyaml takes 64 KB from the heap during `begin()` and gives them back before it returns.

A part reads a setting with `get(key, fallback, min, max)`. Some settings have no default. Those are read with `get(key, min, max)`, which returns no value until the file holds one. A number outside its range counts as the nearer end. Reading never changes the tree.

The component writes a line to the log when a key with a default is missing, a value is no number, or a number lies outside its range.

A mutex guards the tree, so any task may read and write it. An interrupt may not, because it cannot wait for a mutex. A value an interrupt needs is therefore read beforehand and kept by its part.

## Writing

`set()` changes a value in the tree and `remove()` takes a key out with everything under it. `save()` then writes the whole tree as the file. The [IR build step](../drivers/ir-sensing.md#build-step) writes its results this way.

The file `save()` writes holds every key of the tree, the ones no part reads included. A comment added to the file by hand does not survive a save.

`set()` changes a value at once for every later `get()`. A part that reads its settings only at start-up sees the change at its next start. `save()` keeps the change across a restart. A save that comes while the previous one is still being written is refused.

## Interface

[`components/config.h`](../../../firmware/src/components/config.h) holds `Config`.
