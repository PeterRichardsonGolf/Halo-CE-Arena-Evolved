/*
AE_KEY_PROMPTS.H

The HUD's prompts on mouse + keyboard (0.3.5 item 1; the owner's approved
design of 2026-10-18, its look A). While the keyboard
and mouse are the last used (ae_platform_keyboard_last_used: their keys, a
mouse's buttons and motion) a prompt of the keyboard's player ("Hold [X] to swap for", "Press [X] to enter driver seat
of Scorpion", a custom line's %x-button) shows the key bound to the button's
action in place of the pad's chip: the FIRST key of its controls.* setting,
as Kenney's keycap glyph (tools/ae_key_pictures.py's sheets, embedded as AE's
own pictures: ae_buttons.h), a mouse button as Kenney's mouse with that
button lit, any other key as a plain keycap with its name ("Num 7"). In split
screen (or with the seats on) the keyboard's seat always gets keys, whatever
was last used, and pad players never: no flicker as a pad's input comes in.

- HUD STYLE AE (ae_hud_messages.c's block): the ice-on-navy keycap, 26 px tall
  at the lines' 24 px em (scaling with them), its body 12 px from the words
  each side, centred on the capitals;
- CE's lines (HUD STYLE CE, AE's "ce" comparison): the blue keycap where the
  blue chip was.

The decisions are ae_key_prompts_rules.c's (pure, unit tested). Two AE hooks
in each of hud_messaging.c and ui_widget.c (draw_string_and_hack_in_icons, the
HUD's custom lines only): the icon a button picture stands for is noted just
before its render_state_bitmap, which asks here first.

Include after cseries.h (pixel32).
*/

#ifndef __AE_KEY_PROMPTS_H
#define __AE_KEY_PROMPTS_H

#include "ae_pure_types.h"

union rectangle2d;

/* the next render_state_bitmap draws the button picture of icon_type (hud_messaging.c's hud_icon_type, the sticks
remapped), which CE resolved to the pad's button (0 to 17; else none), its chip at scale (a CE keycap takes it):
hud_messaging.c's message chip, or (_text_icon) ui_widget.c's text icon in a custom line */
void ae_key_prompts_note(short icon_type, short button, real scale);
void ae_key_prompts_note_text_icon(short icon_type, short button, real scale);
/* (ae_buttons_tags.c, a map's tags loaded) the keys' sheet decoded while the keyboard is in use or seats are on */
void ae_key_prompts_tags_loaded(void);
/* (render_state_bitmap's start, hud_messaging.c and ui_widget.c) the noted picture drawn as its key, the cursor
moved past it: TRUE; FALSE (the pad's chip, as before): nothing noted (a weapon's picture), the prompt not the
keyboard player's (alone: a pad the last used), no key bound. color: the picture's as CE draws it (its alpha the
keycap's: a message's chip its own, a custom line's the text's fade) */
boolean ae_key_prompts_icon(union rectangle2d *cursor_bounds, pixel32 color);
/* (game_engine.c, the first seconds' "Hold BACK for score": the Xbox maps' text names the button as a word, no
picture) the keyboard's player reads its "BACK" as %back-button, which the custom line draws as the scoreboard's key
("Hold [Tab] for score"); anyone else, or a text without the word (another language), keeps it as it is */
void ae_key_prompts_score_text(long player_index, wchar_t *buffer, long buffer_size);

#endif /* __AE_KEY_PROMPTS_H */
