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
- **The high-res HUD** (its meters, icons, scopes and messages, and the
  weapons' reticles): redraws by MrBruh (`port/assets/hud`), based on
  reference photos by Aerocatia. The sniper rifle's zoom labels (2x, 10x)
  are set in Overpass (`port/assets/fonts`).

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

