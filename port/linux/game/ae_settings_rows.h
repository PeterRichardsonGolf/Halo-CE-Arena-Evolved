/*
AE_SETTINGS_ROWS.H

The 0.3.7 menus' SETTINGS model (ae_settings_rows.c; the design: the mockups' SPEC.md and rows.md): the nine groups,
every row of the groups built so far (CONTROLS, VIEW, HUD: each player's own), what each shows and when, its values
(as shown and as the setting's text), the sections, and the texts of the KEEP / UNDO and RESTORE DEFAULTS dialogs.
The screens (ae_screen_settings.c) read and write the values through the glue. Pure: no engine includes (unit test:
port/linux/tests/ae_settings_rows_test.c).

A row's key is a config.toml setting ("display.fov") or one of the profile's own ten ("profile.button_preset"); its
values' texts are that setting's ("toggle", "0.25", "true"; a profile row's numbers as text).
*/

#ifndef __AE_SETTINGS_ROWS_H
#define __AE_SETTINGS_ROWS_H

#include <stddef.h>

enum
{
	AE_SETTINGS_CONTROLS, AE_SETTINGS_VIDEO, AE_SETTINGS_VIEW, AE_SETTINGS_AUDIO, AE_SETTINGS_HUD, AE_SETTINGS_CHAT,
	AE_SETTINGS_ONLINE, AE_SETTINGS_ACCESSIBILITY, AE_SETTINGS_SYSTEM, AE_SETTINGS_GROUPS
};
/* whose a group's settings are: each player's own, this PC's, or both (ACCESSIBILITY, CHAT AND STREAMING) */
enum { AE_SETTINGS_PLAYER, AE_SETTINGS_PC, AE_SETTINGS_MIXED };
struct ae_settings_group_info
{
	const char *name;
	const char *line;          /* the top level's line, the panel's text and the help */
	short whose;
	int built;                 /* its rows are here (phase A: CONTROLS, VIEW, HUD); the others are listed only */
};
struct ae_settings_group_info const *ae_settings_group(short group);

enum { AE_SETTINGS_SPINNER, AE_SETTINGS_TOGGLE, AE_SETTINGS_SLIDER, AE_SETTINGS_SUBSCREEN };
/* when a row shows (rows.md's "shown when") */
enum
{
	AE_SETTINGS_ALWAYS, AE_SETTINGS_WHEN_DUALSENSE, AE_SETTINGS_WHEN_KEYBOARD, AE_SETTINGS_WHEN_MOUSE_CLASSIC,
	AE_SETTINGS_WHEN_MOUSE_MCC, AE_SETTINGS_WHEN_ZOOM_BOTH, AE_SETTINGS_WHEN_MOUSE_ZOOM_BOTH, AE_SETTINGS_WHEN_CLOCK,
	AE_SETTINGS_WHEN_GLOW, AE_SETTINGS_WHEN_PANEL, AE_SETTINGS_WHEN_CALLOUTS
};
struct ae_settings_row
{
	short group;
	const char *section;
	const char *label;
	short kind;
	const char *key;
	const char *const *texts;  /* as shown */
	const char *const *values; /* as the setting has them */
	short count;               /* 0: the values are set at run time (CALLOUT VOICE: ae_settings_set_voices) */
	short when;
	const char *help;
	/* AE_SETTINGS_RESTART: needs a restart (its RESTART badge; leaving SETTINGS asks); AE_SETTINGS_THIS_PC: a PC row in
	a player's group (its THIS PC badge while 2+ players are joined). (Phase B's rows: none of CONTROLS, VIEW, HUD) */
	short flags;
};
enum { AE_SETTINGS_RESTART = 1, AE_SETTINGS_THIS_PC = 2 };
short ae_settings_row_count(void);
struct ae_settings_row const *ae_settings_row(short index);
/* the row of a key, -1 */
short ae_settings_row_of(const char *key);
/* a row's values (a run-time row's as set) */
short ae_settings_value_count(struct ae_settings_row const *row);
const char *ae_settings_value_text(struct ae_settings_row const *row, short index);
const char *ae_settings_value_of(struct ae_settings_row const *row, short index);
/* CALLOUT VOICE's values: the voices' folder names (shown upper-cased); copied, up to 32 */
void ae_settings_set_voices(const char *const *names, short count);

/* what the rows' "shown when" reads: the player's devices, and a setting's text as the player has it now */
struct ae_settings_facts
{
	int keyboard;              /* this player plays with the keyboard and mouse */
	int dualsense;             /* a DualSense is this player's pad */
	const char *(*value)(void *context, const char *key);
	void *context;
};
int ae_settings_row_shown(struct ae_settings_row const *row, struct ae_settings_facts const *facts);

/* a group's list: its shown rows (their indices) under their sections' headers (AE_SETTINGS_HEADER | the section's
first row); returns the entries */
enum { AE_SETTINGS_HEADER = 0x4000, AE_SETTINGS_ENTRIES = 128, AE_SETTINGS_SECTIONS = 8 };
short ae_settings_entries(short group, struct ae_settings_facts const *facts, short *entries, short maximum);
int ae_settings_entry_is_header(short entry);
/* the section name of an entry (a header's, or its row's) */
const char *ae_settings_entry_section(short entry);
/* the sections of a list: the header's place, the rows under it */
struct ae_settings_section { const char *name; short header, rows; };
short ae_settings_sections(short const *entries, short count, struct ae_settings_section *sections, short maximum);
/* the section of an entry's place, -1 */
short ae_settings_section_at(struct ae_settings_section const *sections, short count, short place);
/* past headers, from place in direction (1, -1): the next row's place, else place */
short ae_settings_row_step(short const *entries, short count, short place, short direction);

/* the value index of a setting's text: the exact text, else (a number) the nearest; -1 when none fits */
short ae_settings_value_index(struct ae_settings_row const *row, const char *text);
/* a slider's place, 0 to 1 */
float ae_settings_slider(struct ae_settings_row const *row, short index);

/* B on a screen with changes: "3 CONTROLS settings changed for Chief: LOOK SENSITIVITY, ZOOM and VIBRATION. They are
in use already." (who NULL: "on this PC"); more than 3: the first 3 and "N more" */
void ae_settings_keep_body(const char *group, const char *who, const char *const *labels, short count, char *text,
	size_t size);
/* Y: "Every CONTROLS setting for Chief goes back to its default: 4 settings change. Other players and this PC's
settings stay as they are. You can still undo it as you leave." (who NULL: this PC's text); 0 changes: "CONTROLS is
already at its defaults." Returns the changes */
short ae_settings_restore_body(const char *group, const char *who, short changes, char *text, size_t size);

#endif
