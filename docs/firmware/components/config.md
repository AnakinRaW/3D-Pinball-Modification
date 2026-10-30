# Machine config

The machine config holds the settings that change without building and flashing the firmware again. It is the YAML file `/config.yml` on the memory card. It is edited on a computer. Every value the hardware dictates stays in the code.

## Settings

The config is a hierarchical key-value store. A key is a path through the file, such as `servo.start_deg` or `ir.channels.0.order`. The part that uses a setting gives its key, its default and its range in its own document.

Every key is defined in `config-keys.h`.

## Reading

The config component reads the file once, in its `begin()`. It parses the file with [libyaml](https://github.com/yaml/libyaml) into a tree of sections, lists and values. The library reports a broken file through its return values, so the component can go on with an empty tree.

A part reads a setting with `get(key, fallback, min, max)`. Some settings have no default. Those are read with `get(key, min, max)`, which returns no value until the file holds one. 

Reading never changes the tree.

The component writes a line to the [log](../logging.md) in three cases: a key with a default is missing, a value is no number, or a number lies outside its range.

A mutex guards the tree, so any task may read and write it. An interrupt may not, because it cannot wait for a mutex. A value an interrupt needs is therefore read beforehand and kept by its part.

## Writing

`set()` changes a value in the tree and `remove()` takes a key out with everything under it. `save()` then writes the whole tree as the file. The [IR build step](../drivers/ir-sensing.md#build-step) writes its results this way.

The file `save()` writes holds every key of the tree, the ones no part reads included. A comment added to the file by hand does not survive a save.

`set()` changes a value at once for every later `get()`. A part that reads its settings only at start-up sees the change at its next start. `save()` keeps the change across a restart. A save that comes while the previous one is still being written is refused.

## Interface

```cpp
class Config {
public:
    explicit Config(FileSystem& files);

    // reads /config.yml into the tree; a file that is missing, broken or too large leaves the tree
    // empty, so every setting takes its default. The main task, right after the logger's begin()
    void begin();

    // gets the number under key, fitted to min to max, or fallback where the key is missing, holds ~ or
    // holds no number; a missing key, a value that is no number and a fitted value each write a line
    // to the log. Any task
    double get(const char* key, double fallback, double min, double max) const;

    // gets the number under key, fitted to min to max, or nothing where the key is missing, holds ~ or
    // holds no number; a value that is no number and a fitted value each write a line to the log. Any task
    std::optional<double> get(const char* key, double min, double max) const;

    uint8_t count(const char* key) const;        // gets how many entries the list under key holds, 0 where there is none
    bool    set(const char* key, double value);  // sets key, creating what lies above it; false once the tree is full
    void    remove(const char* key);             // removes key with everything under it

    // starts writing the tree as the whole file, so it survives a restart; false while the previous
    // save is still being written
    bool save();
};

extern Config config;   // the one config, defined in the main file
```
