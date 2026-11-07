# Grenades

> Not in 0.7.6: comes in 0.7.7. (The EMP grenade and the grenade types below are held.)

Arena Evolved 0.7.6 adds hand-grenade types beside Halo CE's frag and plasma grenades. The host's gametype decides
which ones a game plays. A gametype that names none, every gametype from before 0.7.6 included, plays frag and plasma
alone, exactly as before, and every client can join it.

The first new type is the **EMP grenade**. The first half of this page is for players and hosts. The second half,
[For builders](#for-builders), is the grenade catalogue, the grenade pack and how to add a type.

## The EMP grenade

The EMP does no damage and pops no shield. It shuts things down instead.

- **Vehicles stall.** A vehicle in the blast loses its throttle and brakes to a stop. Its
  driver's own gun is locked for the stall. Gunners and passengers in other seats keep firing.
- **A Banshee sinks.** A stalled Banshee keeps about a third of its lift, so it drifts down while the stall lasts.
- **Motion trackers scramble.** A player caught in the blast has their motion tracker scrambled: the real contacts are
  hidden and neutral grey-white ghost contacts move about in their place. For the first 0.75 s the HUD flickers.
  Shield and health always stay readable. Enemies of the thrower are scrambled; teammates only when the gametype has
  friendly fire on; the thrower never. A player in a vehicle is scrambled too.
- **Powerups on the ground short out.** An overshield, active camouflage or other powerup lying in the blast is
  destroyed and comes back at its spawn's next respawn, as if someone had picked it up. Halo CE has no powerups a
  player carries, so a player's own camouflage, overshield, shields and health are never touched.
- **Not affected:** players' shields and health, weapons, grenades on the ground, the bomb, flags and projectiles.

### Numbers

| | EMP grenade |
|---|---|
| Fuse | 1.2 s from the throw (armed after 0.2 s); it bounces, it does not stick |
| Blast | full strength within 2.0 world units, 40 % at 3.5, nothing past 3.5; walls block it |
| Damage | none, and no shield pop |
| Vehicle stall | 4.0 s at the centre, 1.6 s at the edge; another hit refreshes it but never stacks; then 2.0 s immune |
| Scramble | 5.0 s at the centre, 2.0 s at the edge; the HUD flickers for the first 0.75 s |
| Carried | 2 at most |
| At spawn | 0 |
| On the map | on the plasma grenade spots nearest each team's vehicles (a map without vehicles: the plasma spot nearest the centre); 30 s respawn |

A warthog is about 2 world units long, so a throw that lands near one stalls it fully. Bots do not throw the EMP.

## Picking a grenade

**SWITCH GRENADE** (RB on a controller, X on the keyboard) cycles through every grenade type you carry, skipping the
ones you have none of.

On the keyboard and mouse there are two more keys, in Settings > KEY BINDINGS (WEAPONS):

| Key | Default | What it does |
|---|---|---|
| NEXT GRENADE | N | the next grenade type you carry, as SWITCH GRENADE does |
| PREVIOUS GRENADE | unbound (B is MARK) | the type before |

**HUD STYLE AE** draws a pair (picture and count) for each grenade type the game plays: frag, plasma, then the EMP's
bolt. The type you have ready is bright, the others dimmed, and a type you have none of is drawn as an outline.
**HUD STYLE CE and CLASSIC** show the grenade you have ready, as Halo CE does, with the EMP's own grenade panel.

## Hosting a game with EMP grenades

In the gametype editor, **GAME OPTIONS > WEAPONS > EMP GRENADES**, right after GRENADES ON MAP: **ON** puts the EMP in
the gametype. Save the gametype and host it as usual. Players start with none and find them on the map.

A gametype can name at most 2 extra grenade types (today the EMP is the only one).

The EMP's parts come in the grenade pack, `packs/ae_grenades.aepack`, in the `packs` folder of the Windows, Linux and
macOS downloads. Keep it there.

### Who can join

While a gametype has EMP GRENADES on, **only Arena Evolved 0.7.6 or later can join**:

- An Arena Evolved build from before 0.7.6, OpenCE, ChupathingyCE and other clients are turned away. Their game
  objects and messages have no place for the new grenade, so they could not play the same game.
- A build that knows grenade types but not all of the game's (a later release's type) is told "This game uses Arena
  Evolved's grenade types, which this copy has not. Update Arena Evolved to play this game." Builds from before 0.7.6
  only say they could not join.
- A player already in the lobby when the host turns EMP GRENADES on is removed from it.
- With EMP GRENADES off, nothing changes and everyone joins as before.

### Older builds and gametype files

The EMP setting is stored in spare bits of the gametype's START weapon bytes, because a gametype has no free byte
left. Builds from before 0.7.6 do not know those bits:

- **A start weapon may read as stock.** A build from before 0.7.6 that opens a gametype file with EMP grenades on and
  a pack weapon as a START weapon on a pack slot past 8 starts that loadout slot with the stock weapon, and its menu
  shows stock there.
- **Editing the loadout drops the EMP.** A build from before 0.7.6 that edits that file's loadout or weapon pack rows
  and saves it loses the EMP setting. Editing any other row keeps it.
- **An older host plays no EMP.** A build from before 0.7.6 hosting a gametype file with EMP grenades on runs no
  grenade layer at all. A 0.7.6 player can still join, but there are no EMP grenades in that game. Host grenade games
  on 0.7.6.

None of these affects a game hosted on 0.7.6 or later: it turns older builds away while the EMP is on.

## For builders

The engine and network half (the mask, the slot map, the join gate, the effects, AE's grenade message 108) is in
`port/linux/game/ae_grenades.h` and `ae_grenade_catalogue.h`; the network side is in `port/linux/NETCODE.md`. A
gametype names the types it plays in its grenade mask (bit n: catalogue id n); a mask of 0 is frag and plasma alone
and nothing here runs. The EMP grenade is id 0. The SMOKE (id 1) has a row, not available yet.

### The catalogue's rows

`port/linux/game/ae_grenade_catalogue_entries.inc`, one row per id, in id order. Ids are permanent. A row names its
tags in the grenade pack (without the pack's prefix), its HUD glyph, the most a player carries, the spawn count, its
map rule, its spot respawn, its effect modules and its numbers.

| | EMP (id 0) |
|---|---|
| Tags | `ae\grenades\emp\emp_grenade` (projectile, equipment, grenade HUD), `emp_grenades` (item collection), `emp_field` (the stall's arcs); the throw effect is the map's plasma grenade's |
| Glyph | the bolt (HUD STYLE AE); CE and CLASSIC styles draw its grenade HUD |
| Carried | 2 |
| At spawn | 0 (seeds may give their own, last before a release) |
| Map | the plasma grenade spots nearest each team's vehicles, 30 s respawn |
| Fuse | 1.2 s from the throw, armed after 0.2 s, bounces |
| Blast | full to 2.0 wu, 40 % at 3.5 wu, none past; line of sight; no damage, no shield pop |
| Stall | 4.0 s (1.6 s at the edge), re-hits refresh, 2.0 s immunity after |
| Scramble | 5.0 s (2.0 s at the edge), the HUD flickers the first 0.75 s |

### The grenade pack

`packs/ae_grenades.aepack`: every type's tags, added whole to a map whose game's mask is not 0 (so the tag indices are
the same for every mask). Built from `tools/weapon_pack/grenades/recipe.json`; see `docs/weapon-packs.md`.

### HUD STYLE AE's grenade box

The grenade box draws frag and plasma, then one pair per catalogue type the map's slot map holds, in slot order, at
most 2 until a compact layout is designed. A pair is 42 px at scale 1 (gap 5, a 17 x 26 glyph cell, 3, the count's
cell): the box is 99, 141 or 183 px. The chosen pair is white; the others' glyphs at 0.55 and counts at 0.70; a zero
count is an outline. The glyphs are drawn shapes (`ae_panel_draw_grenade_glyph`): the EMP's bolt and the smoke's
cloud. A map's own extra types (MCC maps) are not drawn in the box.

For pictures, `debug.ae_panel_state` takes `extra:<bolt|smoke>:<count>` (a pair after the plasma's), `armed:<pair>`
(0 frag, 1 plasma, 2 and 3 the extras) and `pairgap:<px>` (the pairs' gap; 5 when not given).

### Keys

SWITCH GRENADE cycles every type carried, skipping empty ones. NEXT GRENADE (`controls.grenade_next`, N) does the
same; PREVIOUS GRENADE (`controls.grenade_previous`, unbound: B is MARK) goes back. Both rows are in KEY BINDINGS'
WEAPONS section.

### Map spots

A row's map rule (`ae_grenade_spots_rules.h`) picks some of the map's plasma grenade spots, which then spawn the type
in place of plasma grenades (`ae_grenade_spots.c`, at the item spawn): `near_vehicles` takes, for each team with
vehicle spawns, the plasma spot nearest them (a map without vehicles: the plasma spot nearest the centre); `centre`
the plasma spot nearest the centre. A vehicle's team is the one the vehicle sets count it for. The log says
`grenades: map item <n> (plasma grenades at x y z) spawns the EMP GRENADE`.

### At spawn

Each type on the map gives its row's spawn count at each spawn, on the host (`ae_grenade_spawn.c`), up to the most
carried. `debug.grenade_spawn` (`"emp:2"`, a comma list of name or id and count) sets a type's count for tests.

### Testing

`game.grenade_types` (a mask: 1 the EMP) overrides every game's grenade types for a test; 0 uses the gametype's. In
DEV MODE, `give emp` hands you EMP grenades. The TEST RUN list `grenades-emp` checks the EMP on foot on Octagon 2 and
its vehicle checks and map spots on Blood Gulch.

### Adding a type

1. Its tags in the grenade pack's recipe under `ae_pack\<name>\` (roots: projectile, equipment, item collection,
   grenade HUD; an effect if a module needs one).
2. Its row in `ae_grenade_catalogue_entries.inc` at the next free id, its layer version raised (older builds are then
   refused from a game that names it).
3. Its glyph in `ae_panel_draw.c` if none fits, keylined and as tall as the 17 x 26 cell, and its kind in
   `ae_panel_model.h`.
4. Its effect modules: reuse one, or add one file `ae_grenade_fx_<name>.c`.
5. A TEST RUN list.
