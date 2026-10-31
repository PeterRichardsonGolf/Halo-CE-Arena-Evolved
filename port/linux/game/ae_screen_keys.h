/*
AE_SCREEN_KEYS.H

The 0.3.9 menus' SETTINGS > CONTROLS sub-screens (ae_screen_keys.c; the approved design): KEY BINDINGS and BACK PADDLES.

KEY BINDINGS: this PC's keyboard and mouse controls (config.toml's "controls.*", the classic menus' Controls Setup's),
the model ae_key_bindings.h: the sections MOVEMENT, WEAPONS, ACTIONS, CHAT on a rail (LB / RB), a row per
action with its PRIMARY and SECONDARY keys as caps. Left / right pick the cap, A (Enter, a click on a cap) waits for a
key: PRESS A KEY takes the next key, mouse button or wheel turn (platform_binding_capture_begin); Esc or B cancels,
Backspace, Delete or X leaves no key there; X (or its prompt, clicked) clears it at once. A key another action has
moves here (the classic menus' rule) and the help line says so. Each change is in use at once (config.toml's, re-read
by the bindings); B with changes asks KEEP / UNDO; Y RESTORE DEFAULTS for the section or every one.

BACK PADDLES: the player's own input.back_left / input.back_right (their ae.toml), a pad button each.

Its log: "ae keys: open (focus <LABEL>)", "focus <LABEL>", "section <NAME>", "press a key for <LABEL> <slot>", "set
<LABEL> <slot> <input> (moved from <LABEL>)", "cleared <LABEL> <slot>", "<key> = "<setting>"" (as written), "cancelled",
"keep asked (N changed)", "keep", "undo (N actions)", "defaults asked (<SECTION> N, all M)", "defaults <SECTION|ALL> (N
actions)", "closed (N changed)"; "ae paddles: ..." the same for BACK PADDLES.
*/

#ifndef __AE_SCREEN_KEYS_H
#define __AE_SCREEN_KEYS_H

/* KEY BINDINGS over the stack, the focus on the action of a config key ("controls.push_to_talk"; NULL or another:
the first); 0 when the stack is full */
int ae_screen_keys_open(const char *focus_key);
/* BACK PADDLES over the stack for this local player (0-3); 0 when the stack is full */
int ae_screen_paddles_open(short local_player);

#endif
