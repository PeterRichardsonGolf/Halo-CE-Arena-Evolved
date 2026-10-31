/*
AE_SETTINGS_ROWS.H

The 0.3.7 menus' SETTINGS model (ae_settings_rows.c; the design: the mockups' SPEC.md and rows.md): the nine groups,
every row of every group (CONTROLS, VIEW, HUD: each player's own; VIDEO, AUDIO, ONLINE, CHAT AND STREAMING, SYSTEM:
this PC's; ACCESSIBILITY: both), what each shows and when, its values
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
	int built;                 /* its rows are here (0: listed only, "COMING SOON") */
};
struct ae_settings_group_info const *ae_settings_group(short group);

/* (phase B) LINK: the same setting as another group's row (A goes there, left / right step it here too); KEY: a key
cap, read only for now (PUSH TO TALK: KEY BINDINGS sets it) */
enum { AE_SETTINGS_SPINNER, AE_SETTINGS_TOGGLE, AE_SETTINGS_SLIDER, AE_SETTINGS_SUBSCREEN, AE_SETTINGS_LINK,
	AE_SETTINGS_KEY };
/* when a row shows (rows.md's "shown when") */
enum
{
	AE_SETTINGS_ALWAYS, AE_SETTINGS_WHEN_DUALSENSE, AE_SETTINGS_WHEN_KEYBOARD, AE_SETTINGS_WHEN_MOUSE_CLASSIC,
	AE_SETTINGS_WHEN_MOUSE_MCC, AE_SETTINGS_WHEN_ZOOM_BOTH, AE_SETTINGS_WHEN_MOUSE_ZOOM_BOTH, AE_SETTINGS_WHEN_CLOCK,
	AE_SETTINGS_WHEN_GLOW, AE_SETTINGS_WHEN_PANEL, AE_SETTINGS_WHEN_CALLOUTS,
	/* (phase B) */
	AE_SETTINGS_WHEN_WINDOWED, AE_SETTINGS_WHEN_NOT_WINDOWED, AE_SETTINGS_WHEN_PERFORMANCE, AE_SETTINGS_WHEN_HDR,
	AE_SETTINGS_WHEN_HDR_DISPLAY, AE_SETTINGS_WHEN_VOICE, AE_SETTINGS_WHEN_PUSH_TO_TALK, AE_SETTINGS_WHEN_MAP_DOWNLOAD,
	AE_SETTINGS_WHEN_MAP_SEED, AE_SETTINGS_WHEN_DISCORD, AE_SETTINGS_WHEN_STREAMER,
	AE_SETTINGS_WHEN_GYRO,
	/* (TEST RUN) DEV MODE is on (system.dev_mode): the TEST RUN row */
	AE_SETTINGS_DEV_MODE,
	/* the DEV MODE switch: shown unless this is a release build, or DEV MODE is already on (so a release player who set it
	in config.toml can turn it off) */
	AE_SETTINGS_WHEN_DEV_SWITCH
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
	short count;               /* 0: the values are set at run time (ae_settings_set_values: CALLOUT VOICE, RESOLUTION,
	                              WINDOW SIZE, OUTPUT DEVICE, INPUT DEVICE) */
	short when;
	const char *help;
	/* AE_SETTINGS_RESTART: needs a restart (its RESTART badge; leaving SETTINGS asks); AE_SETTINGS_THIS_PC: a PC row in
	a player's group (its THIS PC badge while 2+ players are joined) */
	short flags;
};
/* AE_SETTINGS_PILL_ALWAYS: its THIS PC badge shows with one player too (DEV MODE's) */
enum { AE_SETTINGS_RESTART = 1, AE_SETTINGS_THIS_PC = 2, AE_SETTINGS_PILL_ALWAYS = 4 };
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
/* a run-time row's values (its key's): as shown and as the setting has them, copied (up to 64, 63 characters each);
texts NULL: the values upper-cased. 0 when the key is not a run-time row's */
int ae_settings_set_values(const char *key, const char *const *texts, const char *const *values, short count);
/* a row whose values include the empty text (LANGUAGE's English): "" is a value of its own there */
int ae_settings_row_takes_empty(struct ae_settings_row const *row);

/* what the rows' "shown when" reads: the player's devices, and a setting's text as the player has it now */
struct ae_settings_facts
{
	int keyboard;              /* this player plays with the keyboard and mouse */
	int dualsense;             /* a DualSense is this player's pad */
	const char *(*value)(void *context, const char *key);
	void *context;
	int hdr_display;           /* (phase B) the display reports HDR */
	int release_build;         /* this is a release build (build_identity_channel() is "release") */
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

/* (0.3.9) a number row: a slider whose value may be any number of its range to 0.01, stepped 0.01 at a time (Shift
with the arrows, Ctrl with the wheel) or typed, besides its steps. 0.3.9: the MOUSE sensitivities; 0.4.0's PC pass
adds the other sliders to the table (ae_settings_rows.c numbers[]). special: a value of its own outside the range
(VERTICAL SENSITIVITY's 0, SAME) */
struct ae_settings_number { double minimum, maximum, special; int has_special; };
/* 0 when the row is not a number row */
int ae_settings_row_number(struct ae_settings_row const *row, struct ae_settings_number *number);
/* a number as its setting's text: rounded to 0.01, one decimal unless the hundredths are not 0 ("1.37", "1.4", "10.0") */
void ae_settings_number_text(double value, char *text, size_t size);
/* a number row's value as shown: its step's text when it is a step's value ("1.0", "SAME"), else the number */
void ae_settings_number_shown(struct ae_settings_row const *row, const char *value, char *text, size_t size);
/* whether a text may stand in a number row's field as it is typed: digits and one '.', at most two decimals, at most
AE_SETTINGS_NUMBER_TYPED characters (empty too) */
enum { AE_SETTINGS_NUMBER_TYPED = 5 };
int ae_settings_number_typing_ok(const char *typed);
/* a typed text's value, as the setting's text: clamped to the range (the special exactly as typed: "0" is SAME) and
rounded to 0.01. 0 when it has no number (empty, ".") */
int ae_settings_number_typed(struct ae_settings_row const *row, const char *typed, char *text, size_t size);
/* a step from the value now (direction 1, -1). fine: 0.01, held at the range's ends (0 there; from the special, the
range's end on that side); else the next step's value past it (an off-step 1.37 goes to 1.5 and 1.25), round past the
ends as every row. text: the setting's text */
int ae_settings_number_step(struct ae_settings_row const *row, const char *now, short direction, int fine, char *text,
	size_t size);
/* (0.7.0) a whole-number row: a spinner whose list (AUTO, 30 ... 360, NONE) holds some values, any whole number of its
range typed besides (FRAME RATE LIMIT: 30 to 1000). 0 when the row is not one */
struct ae_settings_whole { long minimum, maximum; };
int ae_settings_row_whole(struct ae_settings_row const *row, struct ae_settings_whole *whole);
/* the value as shown: a list value's text, else the number itself ("237") */
void ae_settings_whole_shown(struct ae_settings_row const *row, const char *value, char *text, size_t size);
/* whether a text may stand in the field as it is typed: digits, at most AE_SETTINGS_WHOLE_TYPED of them (empty too) */
enum { AE_SETTINGS_WHOLE_TYPED = 4 };
int ae_settings_whole_typing_ok(const char *typed);
/* a typed text's value as the setting's text: held to the range ("20" is 30, "5000" is 1000); 0 when empty */
int ae_settings_whole_typed(struct ae_settings_row const *row, const char *typed, char *text, size_t size);
/* a step from the value now (direction 1, -1) to the list's neighbour in that direction (237: 200 and 240; AUTO is
before the numbers and NONE after them); 0 at the ends. text: the setting's value */
int ae_settings_whole_step(struct ae_settings_row const *row, const char *now, short direction, char *text, size_t size);
/* ALL VALUES' entries: the list's, the value now in its place when no list entry has it, and last "TYPE A NUMBER..."
(typed 2, no value); returns the entries (at most maximum) */
struct ae_settings_whole_value { char text[24], value[24]; int typed; };
short ae_settings_whole_values(struct ae_settings_row const *row, const char *now, struct ae_settings_whole_value *values,
	short maximum);
/* a slider's place, 0 to 1, for any value of its row: between its steps' places for a value between them */
float ae_settings_slider_at(struct ae_settings_row const *row, const char *text);

/* B on a screen with changes: "3 CONTROLS settings changed for Chief: LOOK SENSITIVITY, ZOOM and VIBRATION. They are
in use already." (who NULL: "on this PC"); more than 3: the first 3 and "N more" */
void ae_settings_keep_body(const char *group, const char *who, const char *const *labels, short count, char *text,
	size_t size);
/* Y: "Every CONTROLS setting for Chief goes back to its default: 4 settings change. Other players and this PC's
settings stay as they are. You can still undo it as you leave." (who NULL: this PC's text); 0 changes: "CONTROLS is
already at its defaults." Returns the changes */
short ae_settings_restore_body(const char *group, const char *who, short changes, char *text, size_t size);

#endif
