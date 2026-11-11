# Authors

## Art

- **Arena Evolved's logo and icon**, ORBIT (`halo.exe`'s icon, the desktop
  windows' icon, the macOS and Android app icons, and the README's banner):
  Arena Evolved's own drawing, with no third-party art
  (`port/assets/branding`, made by `tools/ae_logo.py`). Its wordmark is set
  in Overpass (`port/assets/fonts`, SIL Open Font License).
- **OpenCE's icon** (upstream's builds; `port/assets/icon/opence-icon*`): by
  ORION.
- **The controller's A, B, X and Y buttons** (the menus' button icons and
  the HUD messages'): redraws by saulob (`port/assets/buttons`).
- **The high-res HUD** (HUD STYLE CE: its meters, icons, scopes and
  messages, and the weapons' reticles): Aerocatia's and Jessie's HUD art from
  [Halo PC Restored](https://github.com/Aerocatia/halopc-restored), release
  r220 (`port/assets/hud/art`, cut to the Xbox sheets by
  `tools/hud_assets.py`), brought in with ChupathingyCE 0.8.0b. Used with
  Aerocatia's permission on the condition that Aerocatia and Jessie are
  credited; not covered by the code's licence. `port/assets/hud/NOTICE.md`
  lists which files are theirs. The few textures still drawn from the older
  SVG redraws (`port/assets/hud/svg`) are MrBruh's, based on reference photos
  by Aerocatia.
- **The controller's button icons for ChupathingyCE's prompts**
  (`port/assets/prompts`): by snapchipper, CC0.
- **The scoreboard's platform logos** (`port/linux/ui/fonts/platform_icons`):
  one-colour marks from [Simple Icons](https://simpleicons.org) (CC0) and
  Wikimedia Commons (public domain), plus ChupathingyCE's own Xbox 360 ring
  and Wii U GamePad; the names and logos are their owners' trademarks, used
  only to identify platforms.
- **Cyborg Interface Gothic** (ChupathingyCE's high-res text font,
  `port/assets/fonts`): by fizzy, SIL Open Font License.
- **The menus' mouse cursor** (the blue arrow the menus show for the
  mouse): by pugzysixtyfour (`port/assets/cursor`).
- **The callout wheel's NEED A RIDE icon** (`port/assets/hud/steering-wheel.svg`):
  steering wheel icon by Delapouite (game-icons.net), CC BY 3.0, recoloured
  (the licence text is `LICENSES/CC-BY-3.0.txt`). Not covered by the code's
  licence; `port/assets/hud/NOTICE.md` says so.
- **The medal pictures drawn for medals MCC has no picture for** (31 of them, `port/assets/medals`; `port/assets/medals/NOTICE.md`
  says what each part is): Arena Evolved's own shapes, CC0, with these parts, CC BY 3.0 where from game-icons.net (the licence text
  is `LICENSES/CC-BY-3.0.txt`), recoloured and shaded. Not covered by the code's licence.
  - Yard Sale: price tag icon by Delapouite, its symbol removed; the dollar sign is Overpass (SIL OFL).
  - Sneak King: cloak icon by Lucasms; plain dagger and crown icons by Lorc.
  - Achilles Spine: achilles heel icon by Delapouite.
  - Last Shot: silver bullet icon by Delapouite; the 1x is Overpass.
  - Cluster Luck: shamrock icon by Delapouite.
  - Autopilot Engaged: steering wheel icon by Delapouite; targeted icon by Sbed.
  - Quigley: death skull icon by Sbed.
  - Fumble: falling icon by Sbed.
  - Goal Line Stand: tower flag icon by Delapouite.
  - Stopped Short: flying flag icon by Lorc; the word STOP is Overpass.
  - Straight Balling: the skull is Aerocatia's and Jessie's HUD art (Halo PC Restored r220, credited under the high-res HUD above),
    traced to vector and recoloured.
  - Hill Guardian: shield icon by Sbed.
  - Gunslinger, Rifleman, Scattergunner and Bomber: their weapons are Aerocatia's and Jessie's HUD art, traced to vector and
    recoloured.
  - Grenadier: flash grenade icon by Lorc.
  - Boxer: mailed fist icon by Lorc.
  - Driver: steering wheel icon by Delapouite.
  - Pilot: starfighter icon by Delapouite.
  - Gunner: turret icon by Sbed.
  - Driveby and Quick Draw: the pistol is Aerocatia's and Jessie's HUD art, traced to vector and recoloured.
  - No Scope: targeted icon by Sbed.
  - Reversal: shield reflect icon by Lorc.
  - Nade Shot: death skull icon by Sbed; the grenade's body is Aerocatia's and Jessie's HUD art, traced to vector and recoloured.
  - 360: clockwise rotation icon by Delapouite; the digits are Overpass.
  - Ninja: ninja star icon by Faithtoken.
  - Off the Rack: hanger icon by Delapouite, cropped; the rifle is Aerocatia's and Jessie's HUD art, traced to vector and recoloured.
  - Perfect: cut diamond icon by Lorc.
  - Pineapple Express: Arena Evolved's own drawing, no third-party part.

## Third-party art

- **Voice chat's speaker icons** (who talks, and who is muted, in the lobby,
  on the scoreboard and in the game): `volume-2` and `volume-x` from
  [Lucide](https://lucide.dev) 1.53.0, copyright (c) 2026 Lucide Icons and
  Contributors, under the ISC licence
  (`port/assets/icons/lucide/LICENSE`).
- **The Mods menu's warning icons** (a mod out of its requested order, and
  a mod that cannot be played): `triangle-alert` and `circle-x` from
  [Lucide](https://lucide.dev) 1.53.0, under the same licence.

## Streamer mode's word list

OpenCE's streamer mode hides the words of a table (`port/linux/game/text_filter_terms.inc`,
`port/assets/text_filter`) made from Surge AI's profanity list, "The Obscenity List", which carries this notice:

```
MIT License

Copyright (c) Surge AI

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

The English words it checks against are [english-words](https://github.com/dwyl/english-words), in the public
domain (`port/third_party/english-words`).

- **Additional controller button styles**: supplied by the contributor
  (`port/assets/controller_buttons`); original artist attribution is unverified.
