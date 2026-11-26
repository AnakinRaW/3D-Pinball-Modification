# Game files

A game is either written in C++ against the [game abstraction](game-abstraction.md) or described in the file `game.yml` on the SD card. A game in `game.yml` needs no C++ and no firmware build. The firmware runs it with C++ classes for the rules that many games share, such as a timer, a counter or a ball save. Each entry of the file creates an object of one of these classes and gives it its values: what a lane is worth, how long the ball save lasts, which lamps blink and which video plays.

A game described in `game.yml` becomes a `Game` with its `Mode`s, so the host, the mode stack and the components run it as they run any game. Each class's interface mirrors its entry one to one, so a game assembled in C++ from the classes and a game read from the file are the same objects.

## A game on the card

A game lies in `/games/<id>`, with `game.yml` at its top. Each mode is a file in `modes/`, and the file's name is the mode's name, such as `modes/jackpot.yml`. Each slide is a file in `slides/` in the same way. Its sounds lie in `audio/`, its videos in `video/`, its light shows in `lights/` and its pictures in `pictures/`. The id is the directory's name.

`game.yml` opens with the format's version, `format: 1`, and with the game's title. `players` gives the most players the game allows, at most four. `balls` gives the balls each player plays. `game.yml` also holds the variables, the groups, the shots, the combos, the settings and the phases. A picture `thumbnail.png` beside `game.yml` is the cover that the game's card in the [game choice](host-screens.md#games) shows. The host shows the `thumbnail.img` that [`tools/picture.py`](../../tools/picture.py) writes beside it. Without a cover, the card shows the title alone.

## Phases

A game runs through the phases of the [game lifecycle](game-abstraction.md#game-lifecycle). `intro` and `post_game` each hold actions under `do` and a condition under `done_when`. The actions run when the phase begins. The phase ends once the condition holds. Play ends after the last ball of the last player.

## Variables

A variable is a number the game keeps, such as the score. Its scope sets who owns the number and when it returns to its start value. A `player` variable gives each player a number for the whole game, such as the score. A `ball` variable gives each player a number that returns to its start value at every ball, such as the lanes of this ball. A `game` variable is one number that all players share. A `machine` variable keeps its number after power-off, such as the count of games played.

A variable can hold a list of numbers of a fixed `size`, such as one entry for each hero of a game. `heroes[i]` reads or changes one entry. `sum(heroes)` adds the entries up. `count(heroes)` counts the entries that are not zero. The size is fixed, so every expression still ends after a fixed number of steps. An index outside the list is an error of the loader where it is a constant. Otherwise it is held to the ends of the list.

A variable can name a second variable under `times` as its multiplier. Every addition to the first variable is then multiplied by the second one's value. With a multiplier at 3, a lane worth 100 points adds 300 to the score.

## Modes

A mode names its priority. It starts on the events under `start_on` and stops on those under `stop_on`, after `timeout`, or with the action `stop`. A mode with `runs: play` runs for the whole of Play. Every other mode stops when a ball ends. The mode with priority 0 is the base mode.

What a mode shows lies on its layer and ends with it. A mode holds the objects of its rules, such as its timers and counters. They start afresh with the mode and keep one state for each player.

A mode ends as completed or as failed. It completes once the condition under `done_when` holds. It fails when its timeout runs out, when `stop` ends it or when its ball ends. On its end it raises `mode.<name>.completed` or `mode.<name>.failed`. For each player the game counts how often a mode was played and completed, as `mode.<name>.played` and `mode.<name>.completed`. With `once: game`, a mode starts at most once for each player and game. A mode can name its `group`, such as `missions`. `completed(missions)` then counts the modes of the group a player has completed, so a wizard mode starts on a condition over it.

A mode can let a variable fall over a time, as a hurry-up does. `hurry: {jackpot: {from: 50000, to: 10000, over: 20s}}` sets `jackpot` to 50 000 when the mode starts and lowers it evenly to 10 000 over 20 s.

## Players and balls

Up to four players take turns. The action `player.add` adds a player while the first player's first ball runs. The action `ball.end` ends a ball. Every mode without `runs: play` then stops. A mode of kind `bonus` counts the ball's bonus. The next player's ball comes after it, or the same player's after `extra_ball`.

The object `balls` counts the balls in play from the [drain](components/drain.md) and the [scoop](components/scoop.md). A ball released from the store comes into play. A drained ball leaves play. A ball resting in the scoop counts as locked. A player's ball ends once no ball is left in play. While a ball save runs, a drained ball comes back from the store into the plunger lane instead. `multiball.started` is raised when a second ball comes into play. `multiball.ended` is raised when only one is left. The machine holds two balls, so a multiball has two.

## Events

An event in `game.yml` is a pinball event by its type's name in lowercase, such as `lane_passed`. The objects raise further events named after them, such as `timer.<name>.done`. A mode's handlers run actions for an event. With `take: true`, the event goes no further down the mode stack.

## Conditions and values

A condition or a computed value is an expression in a short language of whole numbers, variables, arithmetic and comparisons. The language has no loops, so every expression ends after a fixed number of steps. An expression reads the variables, the event and the states of the components, such as `scoop.holding`. `random(1, 6)` draws a whole number from a generator whose seed the game draws at its start. A game assembled in C++ passes its conditions as the same expression text.

## Actions

A handler lists actions, one to a line. An action changes a variable with `=`, `+=` or `-=`, such as `score += jackpot`, or names a command with its arguments, such as `start jackpot` or `sound jackpot.wav`. The loader compiles each line together with the expressions. The compiler's grammar gives every word and operator a line may hold. Actions under `after 1500ms:` run later. They belong to the mode and end with it.

## Time

A handler takes its time from the event. A timer runs out at its due time. The outcome of a game therefore depends on the events and their times alone. With the generator's seed recorded, a replay reaches the same score.

## Lamps

A lamp or a group is named as the [machine config](components/config.md) names it, such as `drain` or `stars`. A game adds groups for its rules under `groups` in `game.yml`. A mode lights lamps with `lamps` for as long as it runs, with a show from `lights/`, or with the action `lamp`.

## Shots

A shot is a place on the playfield the player aims at, such as a bell, a lane, a track or the scoop. `shots` names each shot with the event that hits it and the lamp that marks it. A shot is off, lit or lit in a color. Its lamp follows that state on the layer of the mode that set it. A shot a mode lit goes dark when the mode ends. Each player has a separate state of every shot. The actions `light`, `unlight` and `rove` change it. `rove` moves the lit state within a group of shots to the next shot, on an event or after a time. Every hit raises `shot.<name>.hit`. A hit on a lit shot also raises `shot.<name>.collected`. An expression reads `shot.<name>.lit` and `shot.<name>.ago`, the time since the shot's last hit.

## Combos

A combo is a run of shots, each hit within a set time of the one before. `combos` names each combo with its shots and that time. The shots either follow an order or are any lit shot. The combo counts its run in `combo.<name>.count` and raises `combo.<name>.advanced` at every shot of the run. When the time runs out, it raises `combo.<name>.broken` and its count starts over.

## Screen

A slide lists widgets such as labels, pictures and videos, each with its place on the screen. A label shows variables in braces, such as `"{score}"`, and follows their changes. A mode names its slide by the file, such as `slide: jackpot.yml`, and the loader takes it from `slides/`. The overlay `slides/overlay.yml` stays over every slide, and the background `slides/background.yml` lies behind every slide. A slide is transparent where it has no widget, so the background shows through there. A game takes touches as `screen_tapped` and `screen_held`. A swipe from the right edge belongs to the [host](game-abstraction.md#what-the-host-expects-of-a-game).

The touch screen is the player's only input. A slide can hold a choice, a row of options the player picks from by touch, such as the next mode. An option shows a text or a picture. A condition can hold an option back. The choice raises `choice.<name>.picked` with the option the player tapped. Once its time has run out, it picks its default option. A game that lets the player choose holds the ball in the scoop until the choice is made.

## Settings

A game can offer settings the player changes on the touch screen, such as the balls for each player or a competition mode. `settings` names each setting with its label, its default and its range or its options. The host shows them on the game's page of its settings and keeps them in `/games/<id>/settings.yml`. A rule reads a setting as `setting.<name>`. `balls` in `game.yml` can name one as well.

## Loading and checking

The loader reads all files of a game and checks them before the game starts: every key, every lamp against the config, every name, every file the game names and every expression. A game with one error does not start. Every error carries the file, the line and the column, such as `modes/lanes.yml:7:42: lamp "top-lane-4" unknown`. The host loads every game at start-up, so a broken game shows its error on its card before anyone picks it. The classes produce a JSON Schema that lists every entry with its keys and their types. The schema covers the keys of the files, and the grammar covers the text of a line.

## Example

`game.yml`:

```yaml
format: 1
title: Star Hunt
players: 4
balls: 3

variables:
  score: 0
  jackpot: {value: 0, scope: ball}

intro:
  do: [scoop.eject, music intro.wav]
  done_when: music.ended and not scoop.holding

post_game:
  do: [music outro.wav]
  done_when: music.ended
```

`modes/jackpot.yml`:

```yaml
priority: 30
start_on: [ball.launched]
timeout: 20s
show: jackpot.lights
hurry:
  jackpot: {from: 50000, to: 10000, over: 20s}
slide: jackpot.yml
on:
  ball_in_scoop:
    - score += jackpot
    - sound jackpot.wav
    - stop jackpot
```
