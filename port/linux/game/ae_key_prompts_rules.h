/*
AE_KEY_PROMPTS_RULES.H

The HUD's key prompts on mouse + keyboard (ae_key_prompts.h), the decisions:
pure (no engine includes), so the unit test builds them alone
(port/linux/tests/ae_key_prompts_rules_test.c).

A prompt's button picture names a game control: an action icon its own
(hud_messaging.c's button_mappings, the same control the pad's chip shows), a
physical button the control the player's controller layout puts on it. The
control's keyboard setting (port_config.c's controls.*; Start's is
controls.pause, Back's controls.scoreboard) lists up to two keys; the FIRST
that names a key is shown ("Mouse Right, G": the right mouse button), as
Kenney's glyph when the sheets have one (tools/ae_key_pictures.py,
ae_key_sheet.inc), else as a plain keycap with the key's name on it ("Keypad
7" as "Num 7"). A button with no control (a stick), or a setting naming no
key, keeps the pad's chip.
*/

#ifndef __AE_KEY_PROMPTS_RULES_H
#define __AE_KEY_PROMPTS_RULES_H

#include <stddef.h>

enum
{
	/* hud_messaging.c's hud_icon_type: the physical buttons, then the actions */
	AE_KEY_ICON_RIGHT_STICK = 17,
	AE_KEY_ICON_ACTION = 18,
	AE_KEY_ICON_ZOOM = 27,
	AE_KEY_ICON_LOOK = 31,
	/* input_abstraction.c's game controls */
	AE_KEY_CONTROL_COUNT = 12,
	/* the gamepad's buttons (input.h): A to the right thumb */
	AE_KEY_BUTTON_COUNT = 16,
};

/* the sheets' styles: HUD STYLE AE's look A, the CE lines' blue keycap */
enum ae_key_style { AE_KEY_STYLE_AE, AE_KEY_STYLE_CE, AE_KEY_STYLE_COUNT };

struct ae_key_rect
{
	unsigned short x, y, w, h;
};

struct ae_key_cell
{
	const char *glyph;     /* Kenney's glyph (its filled name; "frame": the plain keycap) */
	unsigned int code;     /* its outline glyph's code point in input_keyboard.ttf (0: the plain keycap) */
	unsigned char mouse;   /* a mouse's (drawn taller) */
	struct ae_key_rect cells[AE_KEY_STYLE_COUNT]; /* in each style's sheet, texels, the outline's room included */
};

struct ae_key_name
{
	const char *name;      /* as config.toml or the keyboard's layout names the key; matched without case */
	short cell;
};

/* the game control a prompt's icon names: icon_type the message's (hud_messaging.c's enum: an action, or a
physical button, after the sticks' remap), button the pad's button CE resolved it to (0 to 15, else none),
controls_to_buttons the player's layout (game_input_preferences.game_control_to_xbox_buttons); -1: none */
int ae_key_prompts_control_of_icon(int icon_type, int button, const unsigned char *controls_to_buttons);
/* the control's keyboard setting ("controls.action"), or NULL (none) */
const char *ae_key_prompts_control_setting(int control);
/* a setting's first key as xinput_sdl.c's bindings_read takes it: of its first two pieces (leading spaces and commas
skipped, trailing spaces trimmed), the first that valid names a key or button ("Mouse Right, G" -> "Mouse Right",
", G" and "Foo, G" -> "G"); 0 for none */
int ae_key_prompts_first_binding(const char *list, int (*valid)(const char *name), char *key, size_t size);
/* the key's cell in the sheets (Kenney's glyph), or -1 (a plain keycap) */
int ae_key_prompts_cell_of(const char *key);
const struct ae_key_cell *ae_key_prompts_cell(int cell);
/* the plain keycap's cell */
int ae_key_prompts_frame_cell(void);
/* the name written on a plain keycap: "Keypad 7" -> "Num 7", at most 11 characters, else the name */
void ae_key_prompts_fallback_text(const char *key, char *text, size_t size);
/* whether a prompt of local_player shows keys: never a forced pad's (the debug lines' "pad") nor a player not the
keyboard's (keyboard_player, a local player index); with players sharing the machine (shared: more than one local
player, or the seats on) the keyboard's player always (a pad's input never flips it); alone, while the keyboard and
mouse are the last used (keyboard_last) */
int ae_key_prompts_wanted(int keyboard_last, int local_player, int keyboard_player, int shared, int force_pad);
/* the local player a controller plays (the keyboard's controller, ae_seats_keyboard_controller), as player_ui.c's
player_ui_autolevel_enabled finds it: in a network game the controller's own index; else the single player
bindings' (single_player_controllers[local player] = its controller, player_ui.c), or the controller's own index when
none is bound (ae_input.c's player_of); -1: none */
int ae_key_prompts_player_of_controller(int controller, int network_game, const short *single_player_controllers,
	int count);

/* ---------- sizes (the approved design's, at 1080p) */

/* a cell's picture for a keycap height (body, px, the keycap without its outline; a mouse's is its own taller
height for that keycap height): the picture's width and height and the room round the body on each side, px */
void ae_key_prompts_picture_size(int cell, enum ae_key_style style, float key_height, float *width, float *height,
	float *margin);
/* the sheet's size in texels */
void ae_key_prompts_sheet_size(enum ae_key_style style, float *width, float *height);
/* the plain keycap's width for its name's width at a keycap height: the name + 0.62 of the height, at least the
height (look C) */
float ae_key_prompts_frame_width(float text_width, float key_height);

#define AE_KEY_PROMPT_HEIGHT_EM (26.0f / 24.0f)  /* HUD STYLE AE: the keycap 26 px at the lines' 24 px em */
#define AE_KEY_PROMPT_GAP_EM 0.5f                /* 12 px from a word's ink to the keycap's body, each side */
#define AE_KEY_PROMPT_FRAME_TEXT 0.56f           /* a plain keycap's name: its capitals of the keycap's height */

#endif /* __AE_KEY_PROMPTS_RULES_H */
