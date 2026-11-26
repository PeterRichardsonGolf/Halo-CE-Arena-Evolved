# A weapon pack is done when it is right, not when its pieces exist

A weapon pack (docs/weapon-packs.md) is DONE when every point below holds. The static half is
`tools/weapon_pack/pack_done.py`: it reads a built pack and says PASS, FAIL or SKIPPED for each point it can see,
one line each. The in-game half is a short staged run of the game on the finished pack, and the owner's eyes decide the
rest. **A pack is not offered for play-testing until the static checks pass and the in-game half has run.** A check that
cannot be made is "skipped" and says why; it is never a pass.

The yardstick is the shipped weapons. A first-person placement, a reach on the back, a sound's tail or a spread is
judged against the range the shipped weapons of the same class measure in
(`tools/weapon_pack/class_reference.json`, written by `class_reference.py` from the shipped packs, so it follows them).
A tuning is judged against the design sheet: the numbers the weapon was meant to have.

## The design block

A recipe may carry a `design` block (the design sheet's numbers for the weapon):

    "design": {"class": "smg", "fire_mode": "auto", "ttk_seconds": [2.3, 2.9]}

- `class`: pistol, smg, rifle, shotgun, launcher or melee. It chooses the range points 15, 18, 20 and 21 hold the
  weapon to. A class no shipped weapon is in is held to the range of the class named in the reference file's
  "fallback", and the line says so.
- `fire_mode`: `auto` (a held trigger keeps firing), `semi` (one round a pull), `burst` (a burst a pull) or `charge`.
- `ttk_seconds`: the lowest and highest time to kill a full-shield player the design allows.

Without a design block the points that need it (15, 17, 18, 21) are skipped with "the recipe has no design block".
`pack_done.py --design file.json` reads a block from a file, for a pack with no recipe.

## The points

| # | The point | How it is checked |
|---|---|---|
| 1 | Third-person model, first-person model, animations, collision | Static: pack_done 1 (models present). In game: ready, fire, reload, melee and idle play. |
| 2 | Textures and shaders for both views; any counter or screen on the gun | Static: pack_done 2. Owner's eyes for the art. |
| 3 | HUD interface: counter, reticle (hip and zoomed), zoom overlay, message icon, crosshair colours | Static: pack_done 3 (HUD, icon, load line). Reticle while zoomed: pack_done 16. |
| 4 | Pickup texts (weapon and ammo) | Static: pack_done 4. |
| 5 | Sounds, muzzle flash and impact effects | Static: pack_done 5 (they exist); pack_done 21 (the firing sound's tail). |
| 6 | Projectile and damage tags; tuning | Static: pack_done 6 (they exist); pack_done 18 (time to kill, spread). |
| 7 | Credits, recipe pins, no tag corrections in the game | Static: pack_done 7. |
| 8 | Slot, aliases, pack rows, start-weapon offers, loadouts | Game tests; the in-game half (the weapon is offered and can be chosen). |
| 9 | Bots: rating, ranges, ammo, swaps | Game tests; the in-game half (a bot picks it up and fires it). |
| 10 | Kill feed name and icon, medals, timers, waypoint picture, DEV MODE page | Static: pack_done 10 (name, icon). In game: a kill shows its row. |
| 11 | Seeds, docs, changelog | Review. |
| 12 | Every download that plays AE gametypes carries it | The release kit check. |
| 13 | A revision changes the fingerprint; release notes say players need the new packs | The release kit check. |
| 14 | Haptics: rumble, a thump for a special event, an adaptive trigger profile | Static: pack_done 14 (a rumble on the damage effect). Owner on each controller family. |
| 15 | First-person placement: the model sits in the view as the shipped weapons of its class do (the rear of the model and its lowest point, idle pose, against the class range) | Static: pack_done 15. In game: the staged view (a screenshot of the weapon held, hip and zoomed, next to a shipped weapon of the class). |
| 16 | A zooming weapon's reticle draws while zoomed (an aim crosshair that is not "only on default zoom") | Static: pack_done 16. In game: zoom in and look. |
| 17 | The trigger does what the design says (a held trigger repeats for `auto`, a latched trigger for `semi`, a charge time for `charge`) | Static: pack_done 17. In game: hold the trigger and count the rounds. |
| 18 | The time to kill a full-shield player is in the design's window, and the spread is within the shipped class's range | Static: pack_done 18 (computed from the tags, formula below). In game: kill a dummy at point blank and time it. |
| 19 | A dropped weapon rests flat: with a `ground point` marker (the game turns a dropped weapon so that marker's up is the ground's normal), the model's long direction lies within 10 degrees of that ground; without one, the model's long axis runs along its barrel; or it has a collision model of its own | Static: pack_done 19. In game: drop it and look; drop it on a slope. |
| 20 | The weapon hangs on the back or hip where the game places it, outside the body (how far it reaches toward the body against every shipped weapon in the same place, with its fit from ae_backpack_rules.c applied: turned, moved out, leaned). One number along the body's forward axis: it catches a weapon through the chest, not a pommel in a shoulder pad or helmet, which only the in-game look shows | Static: pack_done 20. In game: carry it on the back and look from behind, from the side and while moving. |
| 21 | The firing sound's tail is within the class's limit (the time after which it has fallen 30 dB below its loudest 10 ms) | Static: pack_done 21. In game: fire it indoors. |

## How the numbers are measured

- First-person placement (15): the model's lowest x (the rear: how far behind the view's origin its back end goes) and
  lowest z, in a hundredth of a world unit, after the idle animation moved the model's root node. Only the root node's
  idle move is applied. A weapon whose animation graph cannot be read is measured at rest and says so.
- Time to kill (18): the player has 75 body and 75 shield (the stock multiplayer cyborg's collision model). A hit does the
  mean of the damage effect's lower and upper bounds, times the effect's modifier for the cyborg's shield on the shield; what
  a hit does beyond the shield goes into the body times the cyborg armor modifier. A shot does its pellets at once. Every
  hit lands, the first at second 0: time = (shots - 1) / rounds a second. A weapon the engine paces (an AE rifle
  behaviour) takes its rounds a pull, ticks between rounds and recovery from the rifle profiles.
- Back placement (20): the game's own rules (kind by name and size, then the place's offset and axes) put the model's
  bounds on the body; the number is how far the geometry reaches toward the chest (the back) or the middle (a thigh).
- Sound tail (21): the firing effects' own sounds are decoded and the last 10 ms window within 30 dB of the loudest is
  found. A sound that is not in the pack (a stock sound) is not measured.

## What no static check sees

Each of these needs the in-game half: what a reticle looks like, whether a weapon feels automatic, a held fuse's
length, whether a dropped item sinks into the floor on a given map, a hammer jump, how an animation reads, whether a
model suits a weapon. The in-game half is one staged run (every check opens on its exact scene) before any play-test.
