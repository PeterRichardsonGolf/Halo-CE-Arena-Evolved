# Steam Controller glyphs

The grips' pictures the BACK PADDLES screen draws for a Steam Controller (2026):

| File | Shows |
| --- | --- |
| `controller_button_l4`, `controller_button_l5` | the left upper and lower grips |
| `controller_button_r4`, `controller_button_r5` | the right upper and lower grips |

Kenney's Input Prompts 1.5a ("Steam Controller" set, Default, 64 by 64, white on transparent), by Kenney
(www.kenney.nl), CC0 1.0 (`KENNEY-CC0.txt`). The files are unchanged.

`tools/ae_steam_glyphs.py` writes their alpha into `port/linux/src/ae_steam_glyphs_data.c`, which
`port/linux/src/ae_steam_glyphs.c` draws tinted:

    python3 -I tools/ae_steam_glyphs.py
