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

- HUD STYLE AE (ae_hud_messages.c's block): the white line-art keycap (#dde3e8, a black outline, no fill; the owner's pick of 11-01 over ice on navy), 26 px tall
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

#include <wchar.h>

#include "ae_pure_types.h"

union rectangle2d;

/* the next render_state_bitmap draws the button picture of icon_type (hud_messaging.c's hud_icon_type, the sticks
remapped), which CE resolved to the pad's button (0 to 17; else none), its chip at scale (a CE keycap takes it):
hud_messaging.c's message chip, or (_text_icon) ui_widget.c's text icon in a custom line */
void ae_key_prompts_note(short icon_type, short button, real scale);
void ae_key_prompts_note_text_icon(short icon_type, short button, real scale);
/* the local player the keyboard and mouse play (-1 for none): the keyboard's controller through the players'
controllers; the marks' button and wheel use it too */
int ae_key_prompts_keyboard_player(void);
/* (ae_buttons_tags.c, a map's tags loaded) the keys' sheet decoded while the keyboard is in use or seats are on */
void ae_key_prompts_tags_loaded(void);
/* (render_state_bitmap's start, hud_messaging.c and ui_widget.c) the noted picture drawn as its key, the cursor
moved past it: TRUE; FALSE (the pad's chip, as before): nothing noted (a weapon's picture), the prompt not the
keyboard player's (alone: a pad the last used), no key bound. color: the picture's as CE draws it (its alpha the
keycap's: a message's chip its own, a custom line's the text's fade) */
boolean ae_key_prompts_icon(union rectangle2d *cursor_bounds, pixel32 color);
/* (0.7.4) a pad's buttons in the lines, HUD STYLE AE only (CLASSIC keeps the map's pictures, BUTTON STYLE's as upstream's): when the player is not the keyboard's (ae_key_prompts_icon above), the noted
icon of a pad button is drawn as Kenney's glyph for the view's own pad (ae_key_prompts_rules.h's ae_key_prompts_pad_glyph), white,
as large as a keycap, and for a kind with no glyph for it as its word on a plain keycap; the map's picture only for what is no pad
button, and on CLASSIC. ae_key_prompts_pad_icon_wanted: whether that is so for the button icon (hud_messaging.c's button_icons index) now, for
hud_messaging.c's hook that keeps the d-pad's words (the stock tags flag them as text) from standing in for the glyph */
boolean ae_key_prompts_pad_icon_wanted(short icon_index);
/* ui_widget.c's in-line text icons (an AE hook): on an MCC map in a game, a button icon of the HUD globals
("Press X to enter driver seat of Warthog") drawn as AE's high-res button (display.hud_button_style), as on every
other map, before MCC's own faces (in AE's HUD messages, color and own_color as for ae_hud_message_icon); FALSE elsewhere or when AE has no picture for it (a PlayStation pad: cross circle square triangle only) */
struct icon_hud_element_definition;
boolean ae_mcc_prompt_button(struct icon_hud_element_definition const *icon, union rectangle2d *cursor_bounds, pixel32 color,
	boolean own_color);
/* hud_messaging.c's other in-line pictures (an AE hook): on an MCC map in a game with AE's HUD messages, a message's
picture ("Hold X to swap for <weapon>") flows in its line at text height, as a Custom Edition map's does, instead of
MCC's own placement; FALSE elsewhere (MCC's own drawing, or CE's) */
struct bitmap_data;
union real_rectangle2d;
boolean ae_mcc_message_picture(struct bitmap_data const *bitmap, union real_rectangle2d const *clip, pixel32 color,
	boolean own_color);
/* (game_engine.c, the first seconds' "Hold BACK for score": the Xbox maps' text names the button as a word, no
picture) the keyboard's player reads its "BACK" as %back-button, which the custom line draws as the scoreboard's key
("Hold [Tab] for score"); anyone else, or a text without the word (another language), keeps it as it is */
void ae_key_prompts_score_text(long player_index, wchar_t *buffer, long buffer_size);

/* the chips of the callout wheel's legend and the mark prompt: AE's own pictures only (the keys' sheet, Kenney's glyph of the
view's own pad on it, the pad kind's own word on a keycap where it has none; CLASSIC's, the map's pictures as before), the one
routine for both. action: AE_CHIP_MARK (the mark button: the white button, or the key bound to MARK, B by default) or
AE_CHIP_PICK (what picks: the look stick, or a keycap MOVE for the mouse). The body's left at x and its middle on
capitals_middle (the view's screen pixels, whole), height tall (a pad's picture by its own proportions to the A button's),
at alpha. *width is the body's, for the caller to add its gap to. FALSE (nothing drawn, *width 0) when there is no picture
to draw (no sheet, the key unbound) */
enum { AE_CHIP_MARK, AE_CHIP_PICK };
boolean ae_key_prompts_draw_chip(short local_player_index, int action, real x, real capitals_middle, real height,
	real alpha, real *width);
/* the same chip's body width without drawing it, to lay a row out before it is drawn */
boolean ae_key_prompts_chip_width(short local_player_index, int action, real height, real *width);

/* the item prompt's chip (ae_item_prompt.c): the button a hud_icon_type stands for (the action button, the Y button) for this view's
player, on HUD STYLE AE: the key bound to it on the keyboard's player, else Kenney's glyph of the pad's button for the view's own pad, white (the item prompt is HUD STYLE AE's only). The body's left at x
and its middle on middle (whole pixels), height tall (a pad's picture by its own proportions to the A button's), at alpha; draw FALSE only
measures. *width is the body's. AE_ICON_CHIP_WORD: a wide key (SHIFT, ENTER, SPACE, CTRL) at a chip under 20 px is not a chip, its name is
the plain word in *word for the caller to draw; AE_ICON_CHIP_NONE: no picture and no key (nothing drawn) */
enum { AE_ICON_CHIP_NONE, AE_ICON_CHIP_PICTURE, AE_ICON_CHIP_WORD };
int ae_key_prompts_icon_chip(short view, short icon_type, real x, real middle, real height, real alpha, boolean draw, real *width,
	wchar_t *word, int word_size);

#endif /* __AE_KEY_PROMPTS_H */
