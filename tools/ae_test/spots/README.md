# Staging spots

A TEST RUN test opens on a scene made by its `setup` lines. Picking `setup goto <x> <y> <z> facing <deg>` blind puts the
player in a fight, on a ledge, or in a pit. A staging spot is a named place that was measured first.

`setup spot <name>` in a list stands for the spot's `setup goto` line. `run_list.py`, `preflight.py` and
`list_index.py --sync` write the `goto` line before the game reads the list, so the game never sees `spot`.
`test_run_lint.py` accepts a name from a catalog and refuses any other name, or a spot made for another map than the
test's.

## Pick a spot

1. Read the catalog: `python3 tools/ae_test/spots/spots.py list b30` (one row a spot: rating, flat room ahead,
   nearest enemy and ally squad start, ground offset).
2. Put `setup spot <name>` in the test, then `setup actor <variant> <distance>` with a distance below the spot's
   `flat_room` (an actor 4 to 8 units ahead needs 9 clear).
3. A spot rated TIGHT or POOR stands in the lint as a warning. Pick another or fix the cause in the row's `why`.

## Rating

`spots.py check <map> <x> <y> <z> <facing>` rates any candidate:

- `flat_room`: units ahead where the floor stays within 0.6 of the spot's floor (steps of at most 0.35, 0.7 to each
  side) with no scenery footprint on it.
- `enemy_dist`, `ally_dist`: to the nearest enemy and ally AI squad start of the same BSP. Scripts place most
  campaign waves, so the starts are every place an AI may stand, not only the opening.
- GOOD: room at least the map's `need`, no enemy within `quiet`, no ally within `ally`. TIGHT: floor ahead below `need`.
  POOR: under 5 units of room, a floating spot, or a squad start too close.
- The geometry of a level is its first collision BSP. A spot on another BSP is rated NOT ASSESSED for flatness.

Example: the hillside ledge `-45.46 -5.37 16.8 facing 155` on b30 rates POOR (the floor drops 2 units at once, 17
from the beach fight).

## Data and pictures

Inputs and outputs stay out of the repo, in the required `HALO_SPOTS_OUT` folder:

- `<map>.scenario.json`: player starts, every AI squad start (team, actor variant), vehicles. Make it with a compatible
  tag extractor such as Invader:

      python3 tools/ae_test/spots/scenario_dump.py <maps folder>/<map>.map <out>.json --reader-dir <tag reader folder>

  When `--reader-dir` is omitted, `xreader.py` is found on `$PATH`.
- `<map>.json`: the level's ground, made by the game: a list that runs `setup nav dump` writes
  `bot_nav_geo/<file>.json` in the game's config folder.
- `python3 tools/ae_test/spots/spots.py render <map> [--box X0 Y0 X1 Y1]` writes `<map>_spots.png` (walkable ground by
  height, axes in world units, player starts blue, enemy squad starts red, ally green, vehicles orange, the catalog's
  spots as stars with their rating) and `<map>_spots.points.json`.

## Add a spot

1. `python3 tools/ae_test/spots/spots.py find <map> --box X0 Y0 X1 Y1` lists candidates (GOOD, best first); or read
   coordinates off the picture and `check` them.
2. Add a `[[spot]]` to `<map>.toml`: `name` (`<map>-<place>` with the map's `_` written `-`, lower case, digits, dashes), `x y z facing bsp notes`.
   `z` is the floor height: a player is placed standing on it.
3. `python3 tools/ae_test/spots/spots.py fill <map>` writes the measured numbers and the rating into the file.
4. Preflight a list that uses it and look at the frame (`tools/ae_test/preflight.py`).

A catalog's top lines tune the rating for a map (`quiet`, `need`, `ally`); b30 has `quiet = 18` because its scripts
place waves from starts spread over the whole island.
