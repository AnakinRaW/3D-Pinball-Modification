# Game files: plan

The work comes in two stages. The first stage builds the light shows from `.lights` files and the slides on the screen through LVGL. Complete games in `game.yml` follow in the second stage, later. [`game-files.md`](../firmware/game-files.md) gives the game files and the classes. Each piece comes with computer tests and its rows in [`testing.md`](../firmware/testing.md).

## Stage 1: shows and slides

The pieces of this stage need only code that exists: `Layer`, the lamps with [`since()`](../../firmware/src/components/lamps.h), the screen, the picture memory, the video player and the storage driver.

### Step 1: the YAML tree and the lamp names

`YamlTree` holds a parsed YAML file in memory its user gives. Every entry gives its name, its text, its line and its column. libyaml takes its memory from the heap during each parse, which `firmware/CLAUDE.md` allows as the one exception after the start. `YamlTree` and the config stand on the branch `claude/config-component`, which main does not hold yet.

The config names each lamp and the machine's groups. A show names its lamps, so the names come before the show player.

### Step 2: the show player

The show player reads a `.lights` file into a `YamlTree` and plays its tracks on one layer with `since()`. A higher track covers a lower one. When an effect of a higher track ends, the player starts the lower track's effect again and dates it back with `since()` to the moment it first began, so a pulse or a run goes on in step. A restored `fade()` starts from the color the lamp shows at that moment. A lamp name the config does not know is an error with the file, the line and the column. The Arcade's Studio plays the same shows through the firmware's WebAssembly module.

### Step 3: the slides

The slide class builds LVGL objects from a list of widgets. [`slide-test.md`](slide-test.md) plans it with a test in the Arcade.

## Stage 2: complete games

### Step 1: classes that need no game or host code

1. Expressions: a compiler from text such as `"lanes == 3"` and from action lines such as `score += jackpot` to bytecode, and an evaluator on whole numbers with a fixed stack. Names resolve to indices into the tables of variables, event fields and component states. The compiler's grammar for expressions and action lines is written down in EBNF beside its code.
2. The variable store: the variables in a fixed table with a value for each of up to four players, the scopes `player`, `ball`, `game` and `machine`, the multiplier under `times`, a generator with a seed, the `machine` variables read and written through the file system, and an LVGL subject for each bound variable. A player change swaps the values the subjects show.
3. The scheduler: a queue for the events the objects raise, the timers ordered by their due time, and both merged with the pinball events by time.
4. The rule classes: timer with tick and its changes, hurry-up, counter, accrual, sequence, shot, shot group and award list. Each takes events and the time and raises an event. Each keeps one state for each player.
5. A directory listing in the storage driver, so the host finds `/games/*/game.yml`.

### Step 2: the game abstraction in code

`Game`, `Mode`, `ModeStack`, `Machine` and the host move from [`firmware/gist/game-abstraction.cpp`](../../firmware/gist/game-abstraction.cpp) into `firmware/src/game/`. A game gives its cover, its number of players and each player's score, as [what the host expects of a game](../firmware/game-abstraction.md#what-the-host-expects-of-a-game) sets out. The event types get fixed numbers, which replays and the Arcade share.

The host takes the work every game shares:

- At power-up it shows the game choice, with the built-in game as the first card. A single game starts at once. Without a card, a warning shows and the built-in game starts.
- After every game it asks each player for a name. The host then compares every score with the game's list and writes `highscores.yml`. Without a card, the built-in game's list stays in RAM until power-off.
- A swipe from the right edge opens its menu.

The host's screens come from the storyboard of the host UI.

### Step 3: the game interpreter

`RuleGame` runs a game from the objects, and `RuleMode` runs each mode. This covers the phases, the modes with their handlers, `take` and the actions, and a builder that assembles a game in C++, such as `mode("jackpot").priority(30).on("ball_in_scoop", {...})`. Names stay text and are checked when the game starts, so a game built in C++ passes the same check as a file. The ball's classes follow once the drain component exists as code: the turns of the players, `ball.end` with the bonus, `extra_ball` and `player.add`.

### Step 4: the first game

The first game is built with the builder. It runs in the computer tests and in the Arcade, and replays record it.

### Step 5: the loader

The loader reads `game.yml`, the files in `modes/` and the files in `slides/` into a second `YamlTree`, creates the objects from it and checks all files before the game starts. A game with one error does not start, and every error names the file, the line and the column. The classes produce the JSON Schema for editors. The host lists the games on the card. The first game is written again as `game.yml`. Replays then have to reach the same score and the same modes with the C++ game and with the file.

### Step 6: the Arcade

The Arcade lists the games of the card, the firmware and the repository, and reads each file's title and top-level keys, in [`games.ts`](../../tools/arcade/src/model/games.ts). Showing the loader's errors and drawing a game's rules from the file remain. The loader also runs on the computer as a command in Node.js on the same WebAssembly module.

### Step 7: the bench

A test on the Teensy measures the time an event takes, the time a pass takes and the time a game takes to load.

## Open

1. Do games written in C++ stay beside `game.yml`? The `Game` interface takes both, and the first game exists in both forms after step 5.
2. Is the built-in game a `game.yml` with its pictures and sounds in the flash, which the same loader reads, or a game built in C++ with the builder? The file keeps one check for every game and leaves no game in C++. Each full-screen picture costs 153.6 KB of flash.
3. Does a rule the classes cannot express get a new class in the firmware, which serves every game after one build, or a script mode, which needs no build? A video minigame is such a rule. Its input is the touch screen alone, since the toggle switch sits too far from the flippers for quick play.

## Deferred

A Python step that translates `game.yml` into C++ pays off only if several games are built before the loader exists.
