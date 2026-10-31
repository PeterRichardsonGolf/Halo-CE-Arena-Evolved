/*
AE_KEY_BINDINGS.H

The 0.3.9 menus' KEY BINDINGS model (ae_key_bindings.c; the screen: ae_screen_keys.c; the approved design): the keyboard and mouse's actions as the classic menus' Controls Setup has
them (menu_functions.c's controls[]: the same config.toml keys, "controls.*", two bindings each, "Key A, Key B"), in
four sections, with their help; a setting's two bindings read and written; a binding put on an action, which takes it
off any other action that has it (the classic menus' rule: one input, one action), and the line that says so; the
KEEP / UNDO and RESTORE DEFAULTS texts. Pure: no engine includes (unit test: port/linux/tests/ae_key_bindings_test.c).

A binding is an input's name as config.toml has it ("W", "Left Ctrl", "Mouse 4", "Wheel Up"); names compare without
regard to case. "Wheel" (either way the wheel turns) overlaps "Wheel Up" and "Wheel Down".
*/

#ifndef __AE_KEY_BINDINGS_H
#define __AE_KEY_BINDINGS_H

#include <stddef.h>

enum { AE_KEYS_SLOTS = 2, AE_KEYS_NAME = 32, AE_KEYS_ACTIONS = 32, AE_KEYS_SECTIONS = 4 };

struct ae_key_action
{
	const char *key;           /* "controls.jump" */
	const char *label;         /* "JUMP" (the classic menus' label) */
	short section;             /* 0 MOVEMENT, 1 WEAPONS, 2 ACTIONS, 3 CHAT */
	const char *help;
	int only_with;             /* shown only with these on (AE_KEYS_WITH_*; 0 always) */
};
short ae_keys_action_count(void);
struct ae_key_action const *ae_keys_action(short action);
/* the action of a config key, -1 */
short ae_keys_action_of(const char *key);
const char *ae_keys_section_name(short section);

/* the list: each shown section's header (AE_KEYS_HEADER | the section) then its actions; returns the entries */
enum { AE_KEYS_HEADER = 0x4000, AE_KEYS_ENTRIES = 40 };
/* (an action's only_with, and the list's with: ChupathingyCE's text chat on (chat.enabled), DEV MODE on) */
enum { AE_KEYS_WITH_CHUPA_CHAT = 1, AE_KEYS_WITH_DEV_MODE = 2 };
short ae_keys_entries(int with, short *entries, short maximum);
int ae_keys_entry_is_header(short entry);
/* the section of a place in the list (its header's or its action's), -1 */
short ae_keys_section_at(short const *entries, short count, short place);
/* the place of a section's header, -1 */
short ae_keys_section_place(short const *entries, short count, short section);
/* the actions of a section shown in the list */
short ae_keys_section_rows(short const *entries, short count, short section);
/* past headers, from place in direction (1, -1): the next action's place, else place */
short ae_keys_row_step(short const *entries, short count, short place, short direction);

/* every action's two bindings ("" for none) */
struct ae_keys { char names[AE_KEYS_ACTIONS][AE_KEYS_SLOTS][AE_KEYS_NAME]; };
/* a setting's text ("Left Ctrl, C", " , C", "") into two bindings (a third and more left out) */
void ae_keys_parse(const char *text, char names[AE_KEYS_SLOTS][AE_KEYS_NAME]);
/* two bindings as the setting's text: "A, B", the one there alone ("C"), "" for none (as the classic menus write
them: an empty first slot leaves the second first) */
void ae_keys_join(char const names[AE_KEYS_SLOTS][AE_KEYS_NAME], char *text, size_t size);
/* whether two bindings name inputs that overlap: the same name (any case), or "Wheel" and "Wheel Up" / "Wheel Down" */
int ae_keys_overlap(const char *a, const char *b);
/* name put on action's slot: every other place holding an overlapping input loses it (the action's own other slot
too: a key it had there moves). Returns how many places lost it; *from (may be NULL) the first other ACTION that lost
it (-1 none: nothing lost, or only the action's own other slot). An empty name clears the slot */
short ae_keys_bind(struct ae_keys *keys, short action, short slot, const char *name, short *from);
/* whether an action has a binding left */
int ae_keys_has_any(struct ae_keys const *keys, short action);
/* the line after a binding moved: "G moved here from FLASHLIGHT, which has no key now." / "G moved here from
FLASHLIGHT." (shown: the input as the menus show it) */
void ae_keys_moved_text(const char *shown, const char *from_label, int from_has_none, char *text, size_t size);
/* B with changes: "3 actions have new keys on this PC: JUMP, FLASHLIGHT and MELEE. They are in use already." (more
than 3: the first 3 and "N more") */
void ae_keys_keep_body(const char *const *labels, short count, char *text, size_t size);
/* Y: "MOVEMENT: 2 actions go back to the game's keys. Every section: 5 actions. You can still undo it as you leave."
(section_changes / all_changes; the section's 0: "MOVEMENT has the game's keys already. Every section: ..."); all 0:
"Every key is already the game's." */
void ae_keys_restore_body(const char *section, short section_changes, short all_changes, char *text, size_t size);

#endif
