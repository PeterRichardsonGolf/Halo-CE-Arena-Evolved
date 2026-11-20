# Arena Evolved's own medal pictures

These 31 pictures (80 by 80 pixels, 8-bit RGBA) are drawn for the medals that MCC's own picture pack has no picture for. The game
draws them, in the MCC medal style and in UNIVERSAL, only where MCC's install is read and has no picture for the medal; the 56
pixel size is these, filtered down. `tools/ae_medal_art.py` embeds them. A file's name is the medal's key in
`port/linux/game/ae_medal_ids.h` without `medal_` (`three_sixty.png` is `medal_360`).

They are not covered by the code's licence.

## Whose work is in them

- **Arena Evolved's own parts**: every mount (the disc, ring and blades), the chrome shading, the hill, the octagon, the tracers, the
  stars, the speed lines, the slash, the clock ticks, the hull and wheels of the vehicle, the pineapple, and the plain crown on the
  hill: drawn from plain geometry for Arena Evolved, CC0 (see `LICENSES/CC0-1.0.txt`).
- **[game-icons.net](https://game-icons.net) icons**, CC BY 3.0 (see `LICENSES/CC-BY-3.0.txt`), recoloured and shaded, a few of them
  cropped or with a part taken out:
  - Delapouite: price tag (Yard Sale, its symbol removed), achilles heel, silver bullet, shamrock, steering wheel (Autopilot Engaged
    and Driver), tower flag, starfighter, clockwise rotation, hanger.
  - Lorc: crown and plain dagger (Sneak King), flying flag, flash grenade, mailed fist, shield reflect, cut diamond.
  - Sbed: death skull (Quigley and Nade Shot), targeted (Autopilot Engaged and No Scope), falling, shield (Hill Guardian), turret.
  - Lucasms: cloak (Sneak King).
  - Faithtoken: ninja star.
- **[Overpass](https://fonts.google.com/specimen/Overpass)**, by The Overpass Project Authors, SIL Open Font License: the dollar
  sign, the 1x, the word STOP and the digits of 360.
- **Aerocatia's and Jessie's HUD art** from [Halo PC Restored](https://github.com/Aerocatia/halopc-restored), release r220, used with
  Aerocatia's permission on the condition that Aerocatia and Jessie are credited (see `port/assets/hud/NOTICE.md`), traced to vector
  and recoloured: the skull (Straight Balling), the pistol (Gunslinger, Driveby, Quick Draw), the assault rifle (Rifleman, Off the
  Rack), the shotgun (Scattergunner), the rocket launcher (Bomber) and the frag grenade's body (Nade Shot).

Each icon's parts, medal by medal, are the list in `AUTHORS.md`.
