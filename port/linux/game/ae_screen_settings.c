/*
AE_SCREEN_SETTINGS.C

The 0.3.7 menus' SETTINGS (ae_screen_settings.h). The top level is the flow screens' layout (CREATE GAME's: rows on
the left, the focused group's panel on the right, the help across the frame); a group's screen is GAME OPTIONS'
layout (the title and the crumb, the sections' rail, one list under its sections' headers, the help, the prompts).
Positions are fractions of the frame (ae_frame_compute), sizes u. Neither draw resets the view at its start: the
screen's slide and fade (ae_hooks' before_draw) stay theirs.

Values: CONTROLS, VIEW and HUD rows are the player's own: their profile's ten through ae_glue_profiles.h, the rest
through ae_glue_player_settings.h (their profile's ae.toml). A screen keeps each row's value as it was opened: a row
whose value differs is changed (its dot, its struck base, the rail's dot, the crumb's count), and UNDO puts the opened
values back.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "saved games/player_profile.h"
#include "../src/ae_draw.h"
#include "../src/ae_discs.h"
#include "../src/ae_ds5.h"
#include "../src/build_identity.h"
#include "../src/ae_platform.h"
#include "ae_glue_player_settings.h"
#include "ae_glue_profiles.h"
#include "ae_hooks.h"
#include "ae_screen_keys.h"
#include "ae_screen_test_run.h"
#include "ae_screen_settings.h"
#include "ae_seats.h"
#include "ae_settings_rows.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#ifdef HALO_GAME_BROWSER
#include "gametype_files.h"
#endif

void platform_log(char const *format, ...);
const char *platform_data_root(void);
int platform_folder_list(const char *path, void (*visit)(const char *name, void *context), void *context);
int platform_folder_exists(const char *path);
int platform_restart(void);
int config_write(const char *name, const char *value);
int config_text(const char *name, char *text, size_t size);
int config_default(const char *name, char *text, size_t size);
/* (sdl_platform.c's, as menu_tags.c declares them: VIDEO's sizes and AUDIO's devices; the display taken up) */
int platform_display_resolutions(long *widths, long *heights, int maximum);
int platform_window_sizes(long *widths, long *heights, int maximum);
int platform_audio_devices(int recording, char (*names)[128], int maximum);
void platform_display_apply(void);
int platform_video_hdr(float *sdr_white, float *headroom);

enum
{
	SETTINGS_HIT = 0x7E00,
	HIT_TOP_ROWS = SETTINGS_HIT + 1, HIT_TOP_PROMPTS, HIT_LIST, HIT_RAIL, HIT_PROMPTS,
	PLAYERS = 4,
	ROWS_MAXIMUM = 192,
	VALUE_TEXT = 64,
	PROMPTS = 7
};

/* the frame's positions (fractions of 1920 x 1080 at 100 %): GAME OPTIONS' */
#define TITLE_Y (70.0f / 1080.0f)
#define RULE_Y (156.0f / 1080.0f)
#define CONTENT_Y (186.0f / 1080.0f)
#define RAIL_WIDTH 0.17f
/* sizes, u */
#define TITLE_U 60.0f
#define CRUMB_U 20.0f
#define CRUMB_TRACKING 0.09f
#define CRUMB_GAP_U 14.0f
#define CRUMB_DOT_U 5.0f
#define EMBLEM_U 30.0f
#define EMBLEM_NUMBER_U 20.0f
#define EMBLEM_GAP_U 12.0f
#define RULE_U 1.0f
#define RAIL_PITCH_U 44.0f
#define RAIL_GAP_U 4.0f
#define RAIL_STRIPE_U 4.0f
#define RAIL_PAD_U 16.0f
#define DOT_U 8.0f
#define LIST_GAP_U 48.0f
#define HELP_LINES 2
#define HELP_GAP_U 28.0f
#define FOOTER_ABOVE_U 62.0f
#define FOOTER_ROOM_U 40.0f
/* the top level's panel */
#define PANEL_NAME_U 36.0f
#define PANEL_SUB_U 20.0f
#define PANEL_EMBLEM_U 24.0f
#define PANEL_LINE_GAP_U 18.0f
#define PANEL_SECTION_U 22.0f
#define PANEL_SECTION_PITCH_U 31.0f
#define PANEL_COUNT_X_U 160.0f
#define RIGHT_COLUMN_U 18.0f
#define PANEL_HEADER_INSET_U 10.0f

/* the player colours (the gallery's), by player */
static unsigned int const player_colors[PLAYERS] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };

/* the players on this PC: whose settings show, and who else may (X) */
static struct
{
	short player;
	short joined[PLAYERS], count;
	char name[PLAYERS][48];
	/* a RESTART row was changed: leaving SETTINGS asks */
	int restart_pending;
} who;

static struct
{
	struct ae_flow_screen flow;
	int dialog;
} top;

static struct
{
	short group;
	struct ae_density density;
	struct ae_frame frame;
	short entries[AE_SETTINGS_ENTRIES], count;
	struct ae_settings_section sections[AE_SETTINGS_SECTIONS];
	short section_count;
	struct ae_list_view list;
	struct ae_rect focus_rect;           /* the focused row as last drawn, layout units (the picker's place) */
	short focus_row, logged_focus;
	struct ae_motion tick;
	short tick_row, tick_side;
	char tick_old[VALUE_TEXT];
	/* each row's value as the screen opened (by row: ae_settings_row's index; "" none) */
	char opened[ROWS_MAXIMUM][VALUE_TEXT];
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	int dialog;                          /* 0 none, 1 KEEP / UNDO, 2 RESTORE DEFAULTS, 3 already at defaults,
	                                        4 a display row's KEEP / UNDO (timed) */
	short display_row;                   /* (4) the display row changed, and its value before */
	char display_old[VALUE_TEXT];
	short then_link;                     /* the KEEP / UNDO asked by a LINK row's A: its row + 1 (0 none) */
	short dragging_row;                  /* a slider held by the pointer, -1 */
	int then_switch;                     /* the KEEP / UNDO asked by X: the player switches after it */
	float logged_width, logged_scale;
	/* (0.3.9) a number row's value typed (the keyboard and mouse: Enter, or a click on the value): its row (-1 none),
	the text, its edit, the caret's blink */
	short typing_row;
	struct ae_text typed;
	struct ae_field_edit typed_edit;
	unsigned long caret_since;
	/* (0.7.0) TYPE A NUMBER... chosen in a whole-number row's ALL VALUES: its row, AE's keyboard opened from the next
	update (-1 none) */
	short keyboard_row;
} group;

static struct ae_screen_class const top_class, group_class;

/* ---------- the players */

static void name_of(short player, char *text, int size)
{
	struct player_profile profile;
	int at;

	memset(&profile, 0, sizeof(profile));
	player_ui_get_active_player_profile(player, &profile);
	for (at = 0; at < size - 1 && at < MAXIMUM_PLAYER_PROFILE_NAME_LENGTH && profile.player_name[at]; at++)
		text[at] = profile.player_name[at] >= 0x20 && profile.player_name[at] < 0x7F ? (char)profile.player_name[at] : '?';
	text[at] = 0;
	if (!text[0])
		snprintf(text, (size_t)size, "Player %d", player + 1);
}

static int player_joined(short player)
{
	return player == 0 || player_ui_get_active_player_profile_index(player) != NONE || ae_profile_is_guest(player);
}

static void players_read(void)
{
	short player;

	who.count = 0;
	for (player = 0; player < PLAYERS; player++)
	{
		if (!player_joined(player))
			continue;
		who.joined[who.count++] = player;
		name_of(player, who.name[player], sizeof(who.name[player]));
	}
	if (!player_joined(who.player))
		who.player = 0;
}

static short player_next(void)
{
	short index;

	for (index = 0; index < who.count; index++)
	{
		if (who.joined[index] == who.player)
			return who.joined[(index + 1) % who.count];
	}
	return who.player;
}

static void upper(const char *text, char *out, int size)
{
	int at;

	for (at = 0; at < size - 1 && text[at]; at++)
		out[at] = text[at] >= 'a' && text[at] <= 'z' ? (char)(text[at] - 'a' + 'A') : text[at];
	out[at] = 0;
}

/* ---------- values */

static int profile_read(short player, struct ae_profile_controls *controls)
{
	memset(controls, 0, sizeof(*controls));
	return ae_profile_get_controls(player, controls).ok;
}

/* (this PC's rows) a setting's text as the rows show and compare it, as the classic menus read it (menu_functions.c's
setting_text): DISPLAY MODE empty is display.fullscreen's (the default: borderless), WINDOW SIZE empty 640 x 480 times
display.window_scale; COMMUNITY SERVICES not yet answered ("ask") is OFF */
static void pc_text(const char *key, int default_value, char *text, int size)
{
	if (!strcmp(key, "display.mode") && !text[0])
	{
		char fullscreen[16] = "";

		if (!default_value)
			config_text("display.fullscreen", fullscreen, sizeof(fullscreen));
		snprintf(text, (size_t)size, "%s", default_value || !strcmp(fullscreen, "true") ? "borderless" : "windowed");
	}
	else if (!strcmp(key, "display.window_size") && !text[0])
	{
		char scale_text[32] = "";
		long scale;

		if (default_value)
			config_default("display.window_scale", scale_text, sizeof(scale_text));
		else
			config_text("display.window_scale", scale_text, sizeof(scale_text));
		scale = atol(scale_text);
		if (scale < 1)
			scale = 1;
		snprintf(text, (size_t)size, "%ldx%ld", 640 * scale, 480 * scale);
	}
	else if (!strcmp(key, "network.community_services") && !strcmp(text, "ask"))
		snprintf(text, (size_t)size, "no");
}

/* a row's value for the player now, as its setting's text */
static void row_value(short player, struct ae_settings_row const *row, char *text, int size)
{
	text[0] = 0;
	if (!strncmp(row->key, "profile.", 8))
	{
		struct ae_profile_controls controls;
		int value = 0;

		profile_read(player, &controls);
		if (!strcmp(row->key, "profile.button_preset"))
			value = controls.layout;
		else if (!strcmp(row->key, "profile.joystick_preset"))
			value = controls.stick;
		else if (!strcmp(row->key, "profile.look_sensitivity"))
			value = controls.look_sensitivity;
		else
		{
			int on = !strcmp(row->key, "profile.invert_look") ? controls.invert :
				!strcmp(row->key, "profile.flight_inversion") ? controls.invert_flight :
				!strcmp(row->key, "profile.autocenter") ? controls.autocenter :
				!strcmp(row->key, "profile.ingame_help") ? controls.help : controls.vibration;

			snprintf(text, (size_t)size, "%s", on ? "true" : "false");
			return;
		}
		snprintf(text, (size_t)size, "%d", value);
		return;
	}
	if (!strcmp(row->key, "controls.bindings") || !strncmp(row->key, "action.", 7))
		return;
	if (!ae_player_setting_get(player, row->key, text, size))
		text[0] = 0;
	pc_text(row->key, 0, text, size);
	/* (the mouse's zoom: "same" is the controller's) */
	if (!strcmp(row->key, "input.mouse_zoom_mode") && (!strcmp(text, "same") || !text[0]))
		ae_player_setting_get(player, "input.zoom_mode", text, size);
	else if (!strcmp(row->key, "input.mouse_zoom_reset") && (!strcmp(text, "same") || !text[0]))
		ae_player_setting_get(player, "input.zoom_reset", text, size);
	else if (!strcmp(row->key, "input.mouse_zoom_hold_time") && !(atof(text) > 0.0))
		ae_player_setting_get(player, "input.zoom_hold_time", text, size);
}

static int row_set(short player, struct ae_settings_row const *row, const char *text)
{
	if (!strncmp(row->key, "profile.", 8))
	{
		struct ae_profile_controls controls;
		int value = atoi(text), on = !strcmp(text, "true");
		struct ae_result result;

		if (!profile_read(player, &controls))
			return 0;
		if (!strcmp(row->key, "profile.button_preset"))
			controls.layout = (short)value;
		else if (!strcmp(row->key, "profile.joystick_preset"))
			controls.stick = (short)value;
		else if (!strcmp(row->key, "profile.look_sensitivity"))
			controls.look_sensitivity = (short)value;
		else if (!strcmp(row->key, "profile.invert_look"))
			controls.invert = on;
		else if (!strcmp(row->key, "profile.flight_inversion"))
			controls.invert_flight = on;
		else if (!strcmp(row->key, "profile.autocenter"))
			controls.autocenter = on;
		else if (!strcmp(row->key, "profile.ingame_help"))
			controls.help = on;
		else
			controls.vibration = on;
		result = ae_profile_set_controls(player, &controls);
		return result.ok;
	}
	if (row->kind == AE_SETTINGS_SUBSCREEN || row->kind == AE_SETTINGS_KEY)
		return 0;
	/* (a THIS PC row in a player's group: config.toml's, every player's) */
	if (!ae_player_setting_is_own(row->key))
	{
		if (!config_write(row->key, text))
			return 0;
		/* (taken up at once, as the classic menus' OK does: the window and v-sync; the AE menus' own size, motion and
		sounds, which they keep read) */
		if (!strcmp(row->key, "display.mode") || !strcmp(row->key, "display.resolution") ||
			!strcmp(row->key, "display.window_size") || !strcmp(row->key, "display.vsync"))
			platform_display_apply();
		else if (!strncmp(row->key, "display.arena_menus_", 20) || !strcmp(row->key, "audio.arena_menus_volume"))
			ae_settings_refresh();
		return 1;
	}
	return ae_player_setting_put(player, row->key, text);
}

/* a row's default, as its setting's text (the profile's ten: a new profile's, player_profile.c's: look 3, vibration
on, the rest off; AUTO-CENTER LOOK off there too, whatever rows.md's ON says) */
static void row_default(short player, struct ae_settings_row const *row, char *text, int size)
{
	if (!strncmp(row->key, "profile.", 8))
	{
		snprintf(text, (size_t)size, "%s", !strcmp(row->key, "profile.look_sensitivity") ? "3" :
			!strcmp(row->key, "profile.vibration") || !strcmp(row->key, "profile.ingame_help") ? "true" :
			!strcmp(row->key, "profile.invert_look") || !strcmp(row->key, "profile.flight_inversion") ||
			!strcmp(row->key, "profile.autocenter") ? "false" : "0");
		return;
	}
	/* (the setting's own default text, exactly: "same" for the mouse's zoom, 0.0 for MCC MOUSE SENSITIVITY (seed from
	the classic one), never snapped to a step) */
	(void)player;
	if (!ae_player_setting_default(row->key, text, size))
		text[0] = 0;
	pc_text(row->key, 1, text, size);
}

/* a value as the row shows and compares it: the mouse's zoom "same" (or no hold time) as the controller's */
static void row_resolved(short player, struct ae_settings_row const *row, const char *text, char *out, int size)
{
	snprintf(out, (size_t)size, "%s", text);
	if (!strcmp(row->key, "input.mouse_zoom_mode") && (!strcmp(text, "same") || !text[0]))
		ae_player_setting_get(player, "input.zoom_mode", out, size);
	else if (!strcmp(row->key, "input.mouse_zoom_reset") && (!strcmp(text, "same") || !text[0]))
		ae_player_setting_get(player, "input.zoom_reset", out, size);
	else if (!strcmp(row->key, "input.mouse_zoom_hold_time") && !(atof(text) > 0.0))
		ae_player_setting_get(player, "input.zoom_hold_time", out, size);
}

/* whether two values of a row are the same: the same text, the same number written another way, or (a word) the same
step. Off-step numbers (a seeded 1.3, the 0.0 seed) are their own values, never their nearest step's */
static int values_same(struct ae_settings_row const *row, const char *a, const char *b)
{
	char *end_a, *end_b;
	double x, y;

	if (!strcmp(a, b))
		return 1;
	x = strtod(a, &end_a);
	y = strtod(b, &end_b);
	if (end_a != a && !*end_a && end_b != b && !*end_b)
		return x == y;
	return ae_settings_value_index(row, a) >= 0 && ae_settings_value_index(row, a) == ae_settings_value_index(row, b);
}

static short row_index(struct ae_settings_row const *row, const char *text)
{
	short index = ae_settings_value_index(row, text);

	return index;
}

/* the value as shown (a sub-screen's summary; a value no step has: as its text) */
static void row_shown(short player, struct ae_settings_row const *row, char const *value, char *text, int size)
{
	short index;

	if (!strcmp(row->key, "input.back_left"))
	{
		char left[24], right[24], a[24], b[24];

		ae_player_setting_get(player, "input.back_left", left, sizeof(left));
		ae_player_setting_get(player, "input.back_right", right, sizeof(right));
		upper(left[0] ? left : "none", a, sizeof(a));
		upper(right[0] ? right : "none", b, sizeof(b));
		snprintf(text, (size_t)size, "%s, %s", a, b);
		return;
	}
	if (!strcmp(row->key, "action.test_run"))
	{
		snprintf(text, (size_t)size, "%s", ae_screen_test_run_summary());
		return;
	}
	if (row->kind == AE_SETTINGS_SUBSCREEN)
	{
		snprintf(text, (size_t)size, "%s", ae_settings_value_text(row, 0));
		return;
	}
	/* (0.7.0: a whole-number row's value outside its list as itself, 237) */
	if (ae_settings_row_whole(row, NULL))
	{
		ae_settings_whole_shown(row, value, text, (size_t)size);
		return;
	}
	/* (0.3.9: a number row's value between its steps as itself, 1.37) */
	if (ae_settings_row_number(row, NULL))
	{
		ae_settings_number_shown(row, value, text, (size_t)size);
		return;
	}
	index = row_index(row, value);
	if (index >= 0)
		snprintf(text, (size_t)size, "%s", ae_settings_value_text(row, index));
	else
		upper(value, text, size);
}

/* the facts the rows' "shown when" read, for the player */
static const char *fact_value(void *context, const char *key)
{
	static char text[VALUE_TEXT];
	struct ae_settings_row const *row = ae_settings_row(ae_settings_row_of(key));

	(void)context;
	if (row)
		row_value(who.player, row, text, sizeof(text));
	else if (!ae_player_setting_get(who.player, key, text, sizeof(text)))
		text[0] = 0;
	return text;
}

static void facts_of(short player, struct ae_settings_facts *facts)
{
	facts->keyboard = player == (short)ae_seats_keyboard_controller();
	facts->dualsense = ae_ds5_port_active(player) != 0;
	facts->value = fact_value;
	facts->context = NULL;
	facts->release_build = !strcmp(build_identity_channel(), "release");
#ifndef HALO_ANDROID
	{
		float white, headroom;

		facts->hdr_display = platform_video_hdr(&white, &headroom) != 0;
	}
#else
	facts->hdr_display = 0;
#endif
}

/* (phase B) the run-time rows' values: VIDEO's display sizes (NATIVE first) and window sizes, AUDIO's devices
(SYSTEM DEFAULT first, each by its name as SDL gives it); the value set now is always one of them */
enum { RUN_VALUES = 64 };
static void run_list_set(const char *key, const char *const *texts, const char *const *values, short count)
{
	static char extra[VALUE_TEXT], extra_text[VALUE_TEXT];
	static const char *all_texts[RUN_VALUES + 1], *all_values[RUN_VALUES + 1];
	struct ae_settings_row const *row = ae_settings_row(ae_settings_row_of(key));
	char now[VALUE_TEXT];
	short index;

	for (index = 0; index < count && index < RUN_VALUES; index++)
	{
		all_texts[index] = texts[index];
		all_values[index] = values[index];
	}
	if (row)
	{
		row_value(who.player, row, now, sizeof(now));
		for (index = 0; index < count && index < RUN_VALUES && strcmp(values[index], now); index++)
			;
		if (index == count && now[0] && count < RUN_VALUES)
		{
			long width, height;
			char tail;

			snprintf(extra, sizeof(extra), "%s", now);
			/* (a size as the list shows sizes: "1280 X 960") */
			if (sscanf(now, "%ldx%ld%c", &width, &height, &tail) == 2)
				snprintf(extra_text, sizeof(extra_text), "%ld X %ld", width, height);
			else
				upper(now, extra_text, sizeof(extra_text));
			all_texts[count] = extra_text;
			all_values[count] = extra;
			count++;
		}
	}
	ae_settings_set_values(key, all_texts, all_values, count < RUN_VALUES ? count : RUN_VALUES);
}

static void sizes_read(const char *key, int (*list)(long *, long *, int), int native)
{
	static char value[RUN_VALUES][24], text[RUN_VALUES][24];
	const char *values[RUN_VALUES], *texts[RUN_VALUES];
	long widths[RUN_VALUES], heights[RUN_VALUES];
	int added, index;
	short count = 0;

	if (native)
	{
		values[count] = "native";
		texts[count++] = "NATIVE";
	}
	added = list(widths, heights, RUN_VALUES - count);
	for (index = 0; index < added && count < RUN_VALUES; index++)
	{
		snprintf(value[count], sizeof(value[count]), "%ldx%ld", widths[index], heights[index]);
		snprintf(text[count], sizeof(text[count]), "%ld X %ld", widths[index], heights[index]);
		values[count] = value[count];
		texts[count] = text[count];
		count++;
	}
	run_list_set(key, texts, values, count);
}

static void devices_read(const char *key, int recording)
{
	static char names[2][RUN_VALUES][128];
	const char *values[RUN_VALUES], *texts[RUN_VALUES];
	int added, index;
	short count = 0;

	values[count] = "default";
	texts[count++] = "SYSTEM DEFAULT";
	added = platform_audio_devices(recording, names[recording ? 1 : 0], RUN_VALUES - 1);
	for (index = 0; index < added && count < RUN_VALUES; index++)
	{
		/* (a name as long as a value can be: longer ones can't be told apart, left out) */
		if (strlen(names[recording ? 1 : 0][index]) >= VALUE_TEXT)
			continue;
		values[count] = texts[count] = names[recording ? 1 : 0][index];
		count++;
	}
	run_list_set(key, texts, values, count);
}

/* the voices for CALLOUT VOICE: the data root's voices/ folders */
static const char *voice_names[32];
static char voice_name_text[32][48];
static short voice_name_count;

static void voice_seen(const char *name, void *context)
{
	char path[1200];

	if (voice_name_count >= 32 || name[0] == '.' || strlen(name) >= sizeof(voice_name_text[0]))
		return;
	snprintf(path, sizeof(path), "%s/%s", (const char *)context, name);
	if (!platform_folder_exists(path))
		return;
	snprintf(voice_name_text[voice_name_count], sizeof(voice_name_text[0]), "%s", name);
	voice_names[voice_name_count] = voice_name_text[voice_name_count];
	voice_name_count++;
}

static void voices_read(void)
{
	char folder[1100], current[VALUE_TEXT];
	short index;

	voice_name_count = 0;
	snprintf(folder, sizeof(folder), "%s/voices", platform_data_root());
	platform_folder_list(folder, voice_seen, folder);
	/* (the voice set now is always one of them) */
	ae_player_setting_get(who.player, "game.callout_voice", current, sizeof(current));
	for (index = 0; index < voice_name_count && strcmp(voice_names[index], current); index++)
		;
	if (index == voice_name_count && current[0] && voice_name_count < 32)
	{
		snprintf(voice_name_text[voice_name_count], sizeof(voice_name_text[0]), "%s", current);
		voice_names[voice_name_count] = voice_name_text[voice_name_count];
		voice_name_count++;
	}
	ae_settings_set_voices(voice_names, voice_name_count);
}

/* MODDED DISC's values: STOCK (the empty text), then each folder of discs/ (and of mods/, where earlier releases kept
them) with a maps/ in it, by name as the classic menus' picker has them; the disc now is always one of them */
static char disc_names[32][64];
static short disc_count;

static void disc_seen(const char *name, void *context)
{
	char maps[1200];
	short index;

	if (disc_count >= 32 || name[0] == '.' || strlen(name) >= sizeof(disc_names[0]) || strchr(name, '|') ||
		strchr(name, '\\') || strchr(name, '/'))
		return;
	snprintf(maps, sizeof(maps), "%s/%s/maps", (const char *)context, name);
	if (!platform_folder_exists(maps))
		return;
	for (index = 0; index < disc_count; index++)
	{
		if (!strcmp(disc_names[index], name))
			return;
	}
	snprintf(disc_names[disc_count++], sizeof(disc_names[0]), "%s", name);
}

static void discs_read(void)
{
	static char texts[33][64];
	const char *shown[33], *values[33];
	char folder[1100], current[VALUE_TEXT];
	short index, count = 0;

	disc_count = 0;
	snprintf(folder, sizeof(folder), "%s/discs", platform_data_root());
	platform_folder_list(folder, disc_seen, folder);
	snprintf(folder, sizeof(folder), "%s/mods", platform_data_root());
	platform_folder_list(folder, disc_seen, folder);
	ae_player_setting_get(who.player, "game.disc", current, sizeof(current));
	if (current[0])
	{
		for (index = 0; index < disc_count && strcmp(disc_names[index], current); index++)
			;
		if (index == disc_count && disc_count < 32)
			snprintf(disc_names[disc_count++], sizeof(disc_names[0]), "%s", current);
	}
	shown[count] = "STOCK";
	values[count++] = "";
	for (index = 0; index < disc_count; index++)
	{
		upper(disc_names[index], texts[index], sizeof(texts[index]));
		shown[count] = texts[index];
		values[count++] = disc_names[index];
	}
	ae_settings_set_values("game.disc", shown, values, count);
}

/* ---------- drawing helpers */

static float gu(float spec_units)
{
	return spec_units * group.frame.unit;
}

static float cap_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

static struct ae_prompt prompt_of(int button, const char *label, int action, int pair)
{
	struct ae_prompt prompt;

	prompt.button = button;
	prompt.label = label;
	prompt.key = NULL;
	prompt.action = action;
	prompt.pair = pair;
	return prompt;
}

/* the player's emblem: a square in their colour with their number (navy on it), its width */
static float emblem(float x, float center_y, float size, float number_size, short player)
{
	char number[4];

	ae_draw_rect(x, center_y - size * 0.5f, size, size, size * 0.07f, player_colors[player % PLAYERS]);
	snprintf(number, sizeof(number), "%d", player + 1);
	ae_draw_text(AE_FONT_ROW, number_size, x + size * 0.5f, cap_top(AE_FONT_ROW, number_size, center_y),
		AE_ALIGN_CENTER, ae_emblem_number_color(player_colors[player % PLAYERS]), number);
	return size;
}

/* the crumb, right-aligned at x on the title's capitals' middle: parts separated by dots, the first after the
player's emblem (PC groups: none); returns its left */
static float crumb(struct ae_density const *density, float unit, float x, float middle, const char *const *parts,
	short count, unsigned int last_color, short player)
{
	float size = CRUMB_U * unit;
	short index;

	for (index = (short)(count - 1); index >= 0; index--)
	{
		x -= ae_draw_text_tracked(AE_FONT_BODY, size, CRUMB_TRACKING, x, cap_top(AE_FONT_BODY, size, middle),
			AE_ALIGN_RIGHT, index == count - 1 ? last_color : AE_COLOR_TEXT, parts[index]);
		if (index > 0)
		{
			x -= CRUMB_GAP_U * unit;
			ae_widget_separator(density, x, middle, CRUMB_DOT_U, AE_COLOR_MUTED);
			x -= CRUMB_GAP_U * unit;
		}
	}
	if (player >= 0)
	{
		x -= EMBLEM_GAP_U * unit + EMBLEM_U * unit;
		emblem(x, middle, EMBLEM_U * unit, EMBLEM_NUMBER_U * unit, player);
	}
	return x;
}

/* ---------- the top level */

static const char *group_whose(short index, char *text, int size)
{
	struct ae_settings_group_info const *info = ae_settings_group(index);

	if (info->whose == AE_SETTINGS_PC)
		snprintf(text, (size_t)size, "THIS PC");
	else if (info->whose == AE_SETTINGS_MIXED)
		snprintf(text, (size_t)size, "%s + THIS PC", who.name[who.player]);
	else
		snprintf(text, (size_t)size, "%s", who.name[who.player]);
	return text;
}

static void top_rows(void)
{
	struct ae_flow_screen *flow = &top.flow;
	short index;

	for (index = 0; index < AE_SETTINGS_GROUPS; index++)
		flow->rows[index] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, ae_settings_group(index)->name, NULL, NULL,
			NULL };
	flow->count = AE_SETTINGS_GROUPS;
}

static void top_panel(struct ae_flow_screen *flow)
{
	struct ae_density const *d = &flow->density;
	struct ae_settings_group_info const *info;
	float x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, width = ae_flow_right_x(flow) - x;
	float y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), body = ae_flow_u(flow, d->metrics->body);
	float line = ae_flow_u(flow, d->metrics->body_line), name = ae_flow_u(flow, PANEL_NAME_U);
	float sub = ae_flow_u(flow, PANEL_SUB_U), minor = ae_size_minor(d);
	char lines[3][160], text[96], upper_name[48];
	int count, index;

	if (flow->focus < 0 || flow->focus >= AE_SETTINGS_GROUPS)
		return;
	info = ae_settings_group(flow->focus);
	ae_draw_text(AE_FONT_ROW, name, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, info->name);
	y += ae_draw_cap_height(AE_FONT_ROW, name) + ae_flow_u(flow, PANEL_LINE_GAP_U);
	/* the sub-line: the player's emblem, NAME . PLAYER n; this PC's: THIS PC */
	{
		float middle = y + ae_flow_u(flow, PANEL_EMBLEM_U) * 0.5f, pen = x;

		if (info->whose == AE_SETTINGS_PC)
			ae_draw_text_tracked(AE_FONT_BODY, sub, CRUMB_TRACKING, pen, cap_top(AE_FONT_BODY, sub, middle), AE_ALIGN_LEFT,
				AE_COLOR_TEXT, "THIS PC");
		else
		{
			pen += emblem(pen, middle, ae_flow_u(flow, PANEL_EMBLEM_U), ae_flow_u(flow, 16.0f), who.player) +
				ae_flow_u(flow, EMBLEM_GAP_U);
			upper(who.name[who.player], upper_name, sizeof(upper_name));
			snprintf(text, sizeof(text), "%s", upper_name);
			pen += ae_draw_text_tracked(AE_FONT_BODY, sub, CRUMB_TRACKING, pen, cap_top(AE_FONT_BODY, sub, middle),
				AE_ALIGN_LEFT, AE_COLOR_TEXT, text) + ae_flow_u(flow, CRUMB_GAP_U);
			ae_widget_separator(d, pen, middle, CRUMB_DOT_U, AE_COLOR_MUTED);
			pen += ae_flow_u(flow, CRUMB_GAP_U);
			snprintf(text, sizeof(text), "PLAYER %d", who.player + 1);
			ae_draw_text_tracked(AE_FONT_BODY, sub, CRUMB_TRACKING, pen, cap_top(AE_FONT_BODY, sub, middle), AE_ALIGN_LEFT,
				AE_COLOR_TEXT, text);
		}
		y += ae_flow_u(flow, PANEL_EMBLEM_U) + ae_flow_u(flow, PANEL_LINE_GAP_U);
	}
	count = ae_wrap_text(AE_FONT_BODY, body, info->line, width, lines, 3);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
	y += line * (float)count + ae_flow_u(flow, PANEL_LINE_GAP_U);
	if (info->built)
	{
		struct ae_settings_facts facts;
		struct ae_settings_section sections[AE_SETTINGS_SECTIONS];
		short entries[AE_SETTINGS_ENTRIES], entry_count, found, section;
		float size = ae_flow_u(flow, PANEL_SECTION_U), count_x;

		facts_of(who.player, &facts);
		entry_count = ae_settings_entries(flow->focus, &facts, entries, AE_SETTINGS_ENTRIES);
		found = ae_settings_sections(entries, entry_count, sections, AE_SETTINGS_SECTIONS);
		/* (SECTIONS as a list's section header: Overpass 750, tracked, muted, inset as the headers are) */
		ae_draw_text_tracked(AE_FONT_BODY, ae_flow_u(flow, d->metrics->group), CRUMB_TRACKING,
			x + ae_flow_u(flow, PANEL_HEADER_INSET_U), y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "SECTIONS");
		y += ae_draw_cap_height(AE_FONT_BODY, ae_flow_u(flow, d->metrics->group)) + ae_flow_u(flow, PANEL_LINE_GAP_U);
		/* (the counts' column: PANEL_COUNT_X_U, or past the widest name by 16 u) */
		{
			float column = ae_flow_u(flow, PANEL_COUNT_X_U);

			for (section = 0; section < found; section++)
			{
				float right = ae_draw_text_width(AE_FONT_ROW, size, sections[section].name) + ae_flow_u(flow, 16.0f);

				if (right > column)
					column = right;
			}
			count_x = x + column;
		}
		for (section = 0; section < found; section++)
		{
			ae_draw_text(AE_FONT_ROW, size, x, y, AE_ALIGN_LEFT, AE_COLOR_TEXT, sections[section].name);
			snprintf(text, sizeof(text), "%d SETTING%s", sections[section].rows, sections[section].rows == 1 ? "" : "S");
			ae_draw_text_tracked(AE_FONT_BODY, minor, CRUMB_TRACKING, count_x,
				y + (ae_draw_cap_height(AE_FONT_ROW, size) - ae_draw_cap_height(AE_FONT_BODY, minor)) * 0.5f,
				AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
			y += ae_flow_u(flow, PANEL_SECTION_PITCH_U);
		}
		y += ae_flow_u(flow, PANEL_LINE_GAP_U) * 0.5f;
		/* the notes: who else has their own, X; DUALSENSE while one is plugged in */
		if (info->whose != AE_SETTINGS_PC && who.count > 1)
		{
			short other = player_next();

			snprintf(text, sizeof(text), "%s, player %d, has their own. {X} switches player.", who.name[other], other + 1);
			ae_draw_text(AE_FONT_BODY, body, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
			y += line;
		}
		if (flow->focus == AE_SETTINGS_CONTROLS && !facts.dualsense)
			ae_draw_text(AE_FONT_BODY, body, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "DUALSENSE shows while one is plugged in.");
	}
	else
		ae_draw_text_tracked(AE_FONT_BODY, minor, CRUMB_TRACKING, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "COMING SOON");
}

static void top_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &top.flow;
	struct ae_density const *d = &flow->density;
	float width, minor, body, line, help_y, x, middle, title_size;
	char text[96], name[48];
	char lines[2][160];
	short index;
	int count;

	(void)instance;
	if (!ae_ui_drawing_ghost())
		players_read();
	ae_flow_full_density(flow);
	top_rows();
	ae_flow_title(flow, "SETTINGS", NULL);
	/* the crumb: the player's emblem, NAME . PLAYER n */
	title_size = ae_flow_u(flow, TITLE_U);
	middle = ae_frame_y(&flow->frame, AE_FLOW_TITLE_Y) + ae_draw_cap_height(AE_FONT_TITLE, title_size) * 0.5f;
	upper(who.name[who.player], name, sizeof(name));
	snprintf(text, sizeof(text), "PLAYER %d", who.player + 1);
	{
		const char *parts[2] = { name, text };

		crumb(d, flow->frame.unit, ae_flow_right_x(flow), middle, parts, 2, AE_COLOR_TEXT, who.player);
	}
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	ae_flow_draw_rows(flow, width);
	/* the right column: whose (the main menu's rows have no chevron) */
	minor = ae_flow_u(flow, RIGHT_COLUMN_U);
	for (index = 0; index < flow->count; index++)
	{
		struct ae_rect const *rect = &flow->rects[index];

		if (rect->width <= 0.0f)
			continue;
		ae_draw_text(AE_FONT_BODY, minor, rect->x + rect->width - ae_flow_u(flow, d->metrics->pad),
			cap_top(AE_FONT_BODY, minor, rect->y + rect->height * 0.5f), AE_ALIGN_RIGHT,
			index == flow->focus ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, group_whose(index, text, sizeof(text)));
	}
	top_panel(flow);
	/* the help across the frame: the group's line */
	body = ae_flow_u(flow, d->metrics->body);
	line = ae_flow_u(flow, d->metrics->body_line);
	help_y = ae_flow_content_bottom(flow) - line * 2.0f;
	x = ae_flow_left_x(flow);
	if (flow->focus >= 0 && flow->focus < AE_SETTINGS_GROUPS)
	{
		ae_draw_rect(x, help_y - ae_flow_u(flow, HELP_GAP_U) * 0.5f, ae_flow_right_x(flow) - x,
			ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
		count = ae_wrap_text(AE_FONT_BODY, body, ae_settings_group(flow->focus)->line, ae_flow_right_x(flow) - x, lines, 2);
		for (index = 0; index < count; index++)
			ae_draw_text(AE_FONT_BODY, body, x, help_y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
	}
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Open", AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	if (who.count > 1)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Player", AE_ACTION_X);
	ae_flow_draw_prompts(flow);
}

static void player_switch(void)
{
	short next = player_next();

	if (next == who.player)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	who.player = next;
	platform_log("ae settings: player %d", who.player + 1);
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void restart_picked(short choice, void *context)
{
	(void)context;
	if (choice == 0)
	{
		platform_log("ae settings: restart now");
		platform_restart();
		return;
	}
	platform_log("ae settings: restart later");
	who.restart_pending = 0;
	ae_ui_pop();
}

static int top_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_flow_screen *flow = &top.flow;

	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		if (who.restart_pending)
		{
			struct ae_dialog_spec spec;

			memset(&spec, 0, sizeof(spec));
			spec.kind = AE_DIALOG_CONFIRM;
			spec.title = "Restart now?";
			spec.body = "A change you made takes effect when the game restarts.";
			spec.choices[0] = "RESTART NOW";
			spec.choices[1] = "LATER";
			spec.choice_count = 2;
			spec.safe_choice = 1;
			spec.cancel_choice = 1;
			spec.has_cancel = 1;
			spec.density = flow->density;
			spec.bounds = flow->frame.rect;
			spec.picked = restart_picked;
			platform_log("ae settings: restart asked%s", ae_dialog_open(&spec, AE_OWNER_PARENT) ? "" : " (refused)");
			return 1;
		}
		return 0;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_X:
		if (who.count > 1)
			player_switch();
		return 1;
	case AE_ACTION_ACCEPT:
		if (flow->focus >= 0 && flow->focus < AE_SETTINGS_GROUPS)
		{
			platform_log("ae settings: %s", ae_settings_group(flow->focus)->name);
			if (ae_screen_settings_open_group(flow->focus, who.player))
				ae_sound_request(AE_SOUND_FORWARD, 0);
			else
				ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		return 1;
	default:
		return 1;
	}
}

static void top_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_flow_screen *flow = &top.flow;
	struct ae_event event;
	short row;

	if (ae_prompts_pointer(flow->prompts, flow->prompt_count, pointer, HIT_TOP_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && !who.restart_pending)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			top_handle(instance, &event);
		return;
	}
	row = ae_flow_rows_pointer(flow, pointer);
	if (row >= 0)
	{
		event.player = 0;
		event.action = AE_ACTION_ACCEPT;
		event.device = AE_DEVICE_KEYBOARD_MOUSE;
		event.repeat = 0;
		top_handle(instance, &event);
	}
}

static void top_leave(struct ae_screen *instance)
{
	(void)instance;
	ae_player_settings_flush();
	platform_log("ae settings: closed SETTINGS");
}

static struct ae_screen_class const top_class =
{
	.name = "settings", .leave = top_leave, .handle = top_handle, .draw = top_draw, .pointer = top_pointer,
};

int ae_screen_settings_open(short local_player)
{
	memset(&top, 0, sizeof(top));
	top.flow.hit_rows = HIT_TOP_ROWS;
	top.flow.hit_prompts = HIT_TOP_PROMPTS;
	who.player = local_player >= 0 && local_player < PLAYERS ? local_player : 0;
	ae_player_settings_update();
	players_read();
	top_rows();
	if (!ae_ui_push(&top_class, AE_OWNER_PARENT, &top))
		return 0;
	platform_log("ae settings: open player %d (%d joined)", who.player + 1, who.count);
	(void)ae_settings_ui_scale();
	return 1;
}

/* ---------- a group's screen */

static void group_density(void)
{
	struct ae_layout layout;
	float scale = ae_settings_ui_scale();

	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, scale, &group.density);
	ae_frame_compute(layout.width, scale, &group.frame);
	if (layout.width != group.logged_width || layout.scale != group.logged_scale)
	{
		group.logged_width = layout.width;
		group.logged_scale = layout.scale;
		platform_log("ae settings: density FULL s=%.2f frame=%.0fx%.0f row=%.1fpx", group.density.s,
			group.frame.rect.width * layout.scale, group.frame.rect.height * layout.scale,
			ae_size_row(&group.density) / group.density.pixel);
	}
}

static struct ae_settings_row const *focused_row(void)
{
	short focus = group.list.list.focus;

	return focus >= 0 && focus < group.count && !ae_settings_entry_is_header(group.entries[focus]) ?
		ae_settings_row(group.entries[focus]) : NULL;
}

static short focused_row_index(void)
{
	short focus = group.list.list.focus;

	return focus >= 0 && focus < group.count && !ae_settings_entry_is_header(group.entries[focus]) ?
		group.entries[focus] : -1;
}

static short place_of_row(short row)
{
	short place;

	for (place = 0; place < group.count; place++)
	{
		if (group.entries[place] == row)
			return place;
	}
	return -1;
}

/* whether a row's value differs from the one it had as the screen opened */
static int row_changed(short row_number)
{
	struct ae_settings_row const *row = ae_settings_row(row_number);
	char now[VALUE_TEXT];

	if (!row || row->kind == AE_SETTINGS_SUBSCREEN || row->kind == AE_SETTINGS_KEY)
		return 0;
	row_value(who.player, row, now, sizeof(now));
	return !values_same(row, now, group.opened[row_number]);
}

/* the changed rows shown (their labels, in the list's order): the count */
static short changes(const char **labels, short maximum)
{
	short place, found = 0;

	for (place = 0; place < group.count; place++)
	{
		short entry = group.entries[place];

		if (ae_settings_entry_is_header(entry) || !row_changed(entry))
			continue;
		if (labels && found < maximum)
			labels[found] = ae_settings_row(entry)->label;
		found++;
	}
	return found;
}

/* the entries again (a value can show or hide rows); the focus stays on its row, else on the row now at its place */
static void refresh(void)
{
	struct ae_settings_facts facts;
	short row = focused_row_index(), count;

	facts_of(who.player, &facts);
	count = ae_settings_entries(group.group, &facts, group.entries, AE_SETTINGS_ENTRIES);
	if (count != group.count)
	{
		short place;

		group.count = count;
		ae_list_set_count(&group.list.list, count);
		place = row >= 0 ? place_of_row(row) : -1;
		if (place >= 0)
			ae_list_set_focus(&group.list.list, place);
	}
	group.section_count = ae_settings_sections(group.entries, group.count, group.sections, AE_SETTINGS_SECTIONS);
	if (group.list.list.focus >= 0 && group.list.list.focus < group.count &&
		ae_settings_entry_is_header(group.entries[group.list.list.focus]))
		ae_list_set_focus(&group.list.list, ae_settings_row_step(group.entries, group.count, group.list.list.focus, 1));
	group.focus_row = focused_row_index();
	if (group.focus_row != group.logged_focus)
	{
		group.logged_focus = group.focus_row;
		if (group.focus_row >= 0)
			platform_log("ae settings: focus %s", ae_settings_row(group.focus_row)->label);
	}
}

static void skip_headers(short direction)
{
	short focus = group.list.list.focus;

	if (focus < 0 || focus >= group.count || !ae_settings_entry_is_header(group.entries[focus]))
		return;
	ae_list_set_focus(&group.list.list, ae_settings_row_step(group.entries, group.count, focus, direction));
	if (ae_settings_entry_is_header(group.entries[group.list.list.focus]))
		ae_list_set_focus(&group.list.list, ae_settings_row_step(group.entries, group.count, group.list.list.focus,
			(short)-direction));
}

/* the values as the screen opens (or the player changes): what changes count from */
static void snapshot(void)
{
	short index;

	memset(group.opened, 0, sizeof(group.opened));
	for (index = 0; index < ae_settings_row_count() && index < ROWS_MAXIMUM; index++)
	{
		struct ae_settings_row const *row = ae_settings_row(index);

		if (row->group == group.group)
			row_value(who.player, row, group.opened[index], VALUE_TEXT);
	}
}

static void dialog_picked(short choice, void *context);
static void show_place(short place);

/* the rows whose change asks to keep the display (spec 5): the window's mode and size, the drawing's scale */
static int display_row(struct ae_settings_row const *row)
{
	return !strcmp(row->key, "display.mode") || !strcmp(row->key, "display.resolution") ||
		!strcmp(row->key, "display.window_size") || !strcmp(row->key, "display.resolution_scaling");
}

/* the display after a change: "The screen is now BORDERLESS, 3440 X 1440." (or WINDOWED and the window's size; the
scale's own row its own words); KEEP, or UNDO when the time runs out */
static void display_ask(struct ae_settings_row const *row, const char *before)
{
	static const char *const choices[] = { "KEEP", "UNDO" };
	static char body[200];
	struct ae_dialog_spec spec;
	char mode[VALUE_TEXT], mode_shown[VALUE_TEXT], size[VALUE_TEXT], size_shown[VALUE_TEXT];
	struct ae_settings_row const *mode_row = ae_settings_row(ae_settings_row_of("display.mode")), *size_row;

	if (group.dialog)
		return;
	if (!strcmp(row->key, "display.resolution_scaling"))
	{
		row_value(who.player, row, size, sizeof(size));
		row_shown(who.player, row, size, size_shown, sizeof(size_shown));
		snprintf(body, sizeof(body), "RESOLUTION SCALING is now %s.", size_shown);
	}
	else
	{
		row_value(who.player, mode_row, mode, sizeof(mode));
		row_shown(who.player, mode_row, mode, mode_shown, sizeof(mode_shown));
		size_row = ae_settings_row(ae_settings_row_of(strcmp(mode, "windowed") ? "display.resolution" :
			"display.window_size"));
		row_value(who.player, size_row, size, sizeof(size));
		row_shown(who.player, size_row, size, size_shown, sizeof(size_shown));
		snprintf(body, sizeof(body), "The screen is now %s, %s.", mode_shown, size_shown);
	}
	group.display_row = (short)(row - ae_settings_row(0));
	snprintf(group.display_old, sizeof(group.display_old), "%s", before);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_TIMED_REVERT;
	spec.title = "Keep this display?";
	spec.body = body;
	spec.choices[0] = choices[0];
	spec.choices[1] = choices[1];
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 1;
	spec.timeout_choice = 1;
	spec.has_cancel = 1;
	spec.density = group.density;
	spec.bounds = group.frame.rect;
	spec.picked = dialog_picked;
	if (ae_dialog_open(&spec, AE_OWNER_PARENT))
	{
		group.dialog = 4;
		platform_log("ae settings: display asked (%s)", body);
	}
}

/* a value set, as its setting's text; logged as shown (NULL: as the row shows it) */
static void value_put(struct ae_settings_row const *row, const char *value, const char *logged)
{
	char before[VALUE_TEXT], shown[VALUE_TEXT];

	row_value(who.player, row, before, sizeof(before));
	if (!row_set(who.player, row, value))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		platform_log("ae settings: %s %s not set", row->label, value);
		return;
	}
	if (row->flags & AE_SETTINGS_RESTART)
		who.restart_pending = 1;
	if (!logged)
	{
		row_shown(who.player, row, value, shown, sizeof(shown));
		logged = shown;
	}
	platform_log("ae settings: set %s %s", row->label, logged);
	if (display_row(row) && !values_same(row, before, value))
		display_ask(row, before);
}

static void value_set(struct ae_settings_row const *row, short index)
{
	value_put(row, ae_settings_value_of(row, index), ae_settings_value_text(row, index));
}

/* (0.3.9) whether a step from this event is a fine one: Shift held on the keyboard (a number row's 0.01) */
static int fine_held(void)
{
	return (ae_platform_modifiers() & AE_MODIFIER_SHIFT) != 0;
}

/* a step left or right; fine: a number row's 0.01 (held at its range's ends), else its next step */
static void value_step(struct ae_settings_row const *row, short direction, int fine)
{
	char now[VALUE_TEXT], shown[VALUE_TEXT], next_value[VALUE_TEXT];
	short current, next = -1;
	int number = ae_settings_row_number(row, NULL), whole = ae_settings_row_whole(row, NULL);

	if (row->kind == AE_SETTINGS_SUBSCREEN || row->kind == AE_SETTINGS_KEY)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	row_value(who.player, row, now, sizeof(now));
	if (whole)
	{
		/* (a whole-number row: from the value itself, 237's neighbours in the list, 200 and 240) */
		if (!ae_settings_whole_step(row, now, direction, next_value, sizeof(next_value)) ||
			values_same(row, now, next_value))
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
	}
	else if (number)
	{
		/* (a number row: from the value itself, 1.37's next step 1.5, or 0.01 on) */
		if (!ae_settings_number_step(row, now, direction, fine, next_value, sizeof(next_value)) ||
			values_same(row, now, next_value))
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
	}
	else
	{
		current = row_index(row, now);
		if (current < 0)
			current = 0;
		next = ae_value_step(ae_settings_value_count(row), current, direction);
		if (next == current)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
	}
	row_shown(who.player, row, now, shown, sizeof(shown));
	snprintf(group.tick_old, sizeof(group.tick_old), "%s", shown);
	group.tick_row = (short)(row - ae_settings_row(0));
	group.tick_side = direction;
	ae_motion_start(&group.tick, 0.0f, 1.0f, AE_MOTION_VALUE_TICK_MS);
	if (number || whole)
		value_put(row, next_value, NULL);
	else
		value_set(row, next);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	refresh();
}

/* ---------- (0.3.9) a number row's value typed: Enter (the keyboard) or a click on the value opens a field in the
value's place; digits and one '.', Backspace, the arrows move the caret; Enter keeps it (to 0.01, held to the row's
range), Esc drops it. A pad's A keeps its ALL VALUES picker */

static void typing_finish(int keep)
{
	struct ae_settings_row const *row = ae_settings_row(group.typing_row);
	char value[VALUE_TEXT];

	if (group.typing_row < 0)
		return;
	group.typing_row = -1;
	if (!keep || !row || !(ae_settings_row_whole(row, NULL) ? ae_settings_whole_typed(row, group.typed.text, value,
		sizeof(value)) : ae_settings_number_typed(row, group.typed.text, value, sizeof(value))))
	{
		platform_log("ae settings: typing dropped%s", keep ? " (no number)" : "");
		return;
	}
	platform_log("ae settings: typed %s '%s'", row->label, group.typed.text);
	group.tick_row = -1;
	value_put(row, value, NULL);
	refresh();
}

/* (an end the screen didn't make: another field took the keys (kept), or the screen went away (dropped)) */
static void typing_done(int keep, void *context)
{
	(void)context;
	typing_finish(keep);
}

static void typing_end(int keep)
{
	if (group.typing_row < 0)
		return;
	ae_field_edit_end(&group.typed_edit, keep);
	typing_finish(keep);
	ae_sound_request(keep ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
}

static void typing_begin(struct ae_settings_row const *row)
{
	char now[VALUE_TEXT], shown[VALUE_TEXT];

	if (group.typing_row >= 0)
		return;
	/* (the value as it shows, all of it selected: what is typed replaces it; SAME starts empty) */
	row_value(who.player, row, now, sizeof(now));
	if (ae_settings_row_whole(row, NULL))
	{
		ae_settings_whole_shown(row, now, shown, sizeof(shown));
		if (!ae_settings_whole_typing_ok(shown))
			shown[0] = 0;
	}
	else
	{
		ae_settings_number_shown(row, now, shown, sizeof(shown));
		if (!ae_settings_number_typing_ok(shown))
			shown[0] = 0;
	}
	ae_text_init(&group.typed, shown, ae_settings_row_whole(row, NULL) ? AE_SETTINGS_WHOLE_TYPED :
		AE_SETTINGS_NUMBER_TYPED, 0);
	group.typed_edit.done = typing_done;
	group.typed_edit.context = NULL;
	group.typed_edit.holder = &group;
	group.typing_row = (short)(row - ae_settings_row(0));
	ae_field_edit_begin(&group.typed_edit, &group.typed);
	ae_text_select_all(&group.typed);
	group.caret_since = ae_motion_now();
	group.tick_row = -1;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae settings: typing %s from '%s'", row->label, shown);
}

/* (0.7.0) a row whose value is typed besides its steps: a number row (the mouse sensitivities), a whole-number row
(FRAME RATE LIMIT) */
static int typable(struct ae_settings_row const *row)
{
	return ae_settings_row_number(row, NULL) || ae_settings_row_whole(row, NULL);
}

/* what was typed, with only its digits kept (a character AE's keyboard put in that a whole number has no use for) */
static void whole_digits_only(void)
{
	char digits[16];
	int length = 0, index;

	for (index = 0; group.typed.text[index] && length < AE_SETTINGS_WHOLE_TYPED; index++)
		if (group.typed.text[index] >= '0' && group.typed.text[index] <= '9')
			digits[length++] = group.typed.text[index];
	digits[length] = 0;
	ae_text_init(&group.typed, digits, AE_SETTINGS_WHOLE_TYPED, 0);
	group.caret_since = ae_motion_now();
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* AE's keyboard closed: kept or dropped (typing_finish), unless a physical key took the field over (it goes on) */
static void whole_keyboard_done(int keep, void *context)
{
	(void)context;
	if (group.typing_row < 0)
		return;
	if (group.typed_edit.active && group.typed_edit.text == &group.typed)
	{
		group.caret_since = ae_motion_now();
		return;
	}
	if (!ae_settings_whole_typing_ok(group.typed.text))
		whole_digits_only();
	typing_finish(keep);
}

/* TYPE A NUMBER...: the row's field in place, its value empty, AE's keyboard over it */
static void whole_keyboard_open(struct ae_settings_row const *row)
{
	struct ae_keyboard_spec spec;

	if (!row || group.typing_row >= 0)
		return;
	ae_text_init(&group.typed, "", AE_SETTINGS_WHOLE_TYPED, 0);
	group.typed_edit.done = typing_done;
	group.typed_edit.context = NULL;
	group.typed_edit.holder = &group;
	group.typing_row = (short)(row - ae_settings_row(0));
	group.caret_since = ae_motion_now();
	group.tick_row = -1;
	memset(&spec, 0, sizeof(spec));
	spec.text = &group.typed;
	spec.field = group.focus_rect;
	spec.bounds = group.frame.rect;
	spec.density = group.density;
	spec.done = whole_keyboard_done;
	spec.edit = &group.typed_edit;
	spec.bottom_on_field = 0;
	if (!ae_keyboard_open(&spec, AE_OWNER_PARENT))
	{
		group.typing_row = -1;
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae settings: typing %s with the keyboard", row->label);
}

/* (0.7.0) a whole-number row's ALL VALUES: the list's values, a value it lacks in its place, TYPE A NUMBER... last;
the entries as the picker's index names them */
enum { WHOLE_PICKER_VALUES = 40 };
static struct ae_settings_whole_value whole_picker_values[WHOLE_PICKER_VALUES];
static short whole_picker_count;

static void picked(short index, void *context)
{
	struct ae_settings_row const *row = ae_settings_row((short)(intptr_t)context);

	if (row && ae_settings_row_whole(row, NULL))
	{
		if (index >= 0 && index < whole_picker_count && whole_picker_values[index].typed == 2)
			group.keyboard_row = (short)(row - ae_settings_row(0));
		else if (index >= 0 && index < whole_picker_count)
			value_put(row, whole_picker_values[index].value, NULL);
	}
	else if (row)
		value_set(row, index);
	group.tick_row = -1;
	refresh();
}

static int keep_ask(void);

/* (a LINK row's A) the linked row's own screen, the focus on it: this screen closed first (KEEP / UNDO asked when it
has changes) */
static void link_open(short target, short player)
{
	struct ae_settings_row const *to = ae_settings_row(target);
	short place;

	ae_ui_pop();
	if (!to || !ae_screen_settings_open_group(to->group, player))
		return;
	place = place_of_row(target);
	if (place >= 0)
		show_place(place);
	refresh();
	platform_log("ae settings: link to %s %s", ae_settings_group(to->group)->name, to->label);
}

static void link_go(struct ae_settings_row const *row)
{
	short target = ae_settings_row_of(row->key);

	if (target < 0 || ae_settings_row(target) == row)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	if (keep_ask())
	{
		group.then_link = (short)(target + 1);
		return;
	}
	link_open(target, who.player);
}

static void picker_open(struct ae_settings_row const *row)
{
	struct ae_picker_spec spec;
	char now[VALUE_TEXT];

	if (row->kind == AE_SETTINGS_LINK)
	{
		link_go(row);
		return;
	}
	/* (OPEN GAMETYPES FOLDER: the system's file manager) */
	if (!strcmp(row->key, "action.gametypes_folder"))
	{
#ifdef HALO_GAME_BROWSER
		if (gametype_files_open_folder())
		{
			ae_sound_request(AE_SOUND_FORWARD, 0);
			return;
		}
#endif
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (row->kind == AE_SETTINGS_SUBSCREEN || row->kind == AE_SETTINGS_KEY || !ae_settings_value_count(row))
	{
		/* (0.3.9: KEY BINDINGS and BACK PADDLES open their own screens (ae_screen_keys.h); a KEY row, PUSH TO TALK,
		opens KEY BINDINGS on its action) */
		int opened = 0;

		if (row->kind == AE_SETTINGS_KEY || !strcmp(row->key, "controls.bindings"))
			opened = ae_screen_keys_open(row->key);
		else if (!strcmp(row->key, "input.back_left"))
			opened = ae_screen_paddles_open(who.player);
		else if (!strcmp(row->key, "action.test_run"))
			opened = ae_screen_test_run_open();
		if (opened)
		{
			ae_sound_request(AE_SOUND_FORWARD, 0);
			return;
		}
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	row_value(who.player, row, now, sizeof(now));
	memset(&spec, 0, sizeof(spec));
	if (ae_settings_row_whole(row, NULL))
	{
		static const char *whole_texts[WHOLE_PICKER_VALUES];
		short index, current = -1;

		whole_picker_count = ae_settings_whole_values(row, now, whole_picker_values, WHOLE_PICKER_VALUES);
		for (index = 0; index < whole_picker_count; index++)
		{
			whole_texts[index] = whole_picker_values[index].text;
			if (whole_picker_values[index].typed != 2 && !strcmp(whole_picker_values[index].value, now))
				current = index;
		}
		spec.values = whole_texts;
		spec.count = whole_picker_count;
		spec.current = current < 0 ? 0 : current;
	}
	else
	{
	/* (the model's static texts: they outlive the picker and its ghost) */
	spec.values = row->count ? row->texts : NULL;
	if (!spec.values)
	{
		static const char *voices[RUN_VALUES];
		short index;

		for (index = 0; index < ae_settings_value_count(row) && index < RUN_VALUES; index++)
			voices[index] = ae_settings_value_text(row, index);
		spec.values = voices;
	}
	spec.count = ae_settings_value_count(row);
	spec.current = row_index(row, now);
	}
	spec.row = group.focus_rect;
	spec.bounds = group.frame.rect;
	spec.density = group.density;
	spec.title = row->label;
	spec.picked = picked;
	spec.context = (void *)(intptr_t)(row - ae_settings_row(0));
	if (ae_picker_open(&spec, AE_OWNER_PARENT))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* the focus on a section's first row, its header at the list's top (as far as the list goes) */
static void show_place(short place)
{
	struct ae_list *list = &group.list.list;
	short last_first = list->count > list->rows ? (short)(list->count - list->rows) : 0;

	ae_list_set_focus(list, place);
	list->first = (short)(place - 1 < last_first ? place - 1 : last_first);
	if (list->first < 0)
		list->first = 0;
}

static void section_go(short section)
{
	if (section < 0 || section >= group.section_count)
		return;
	show_place((short)(group.sections[section].header + 1));
	platform_log("ae settings: section %s", group.sections[section].name);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	refresh();
}

static void section_jump(short direction)
{
	short at = ae_settings_section_at(group.sections, group.section_count, group.list.list.focus);
	short next = (short)(at + direction);

	if (next < 0 || next >= group.section_count)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	section_go(next);
}

/* every shown row back to its value as opened (UNDO), or to its default (RESTORE DEFAULTS) */
static short rows_put_back(int defaults)
{
	short place, put = 0;

	for (place = 0; place < group.count; place++)
	{
		short entry = group.entries[place];
		struct ae_settings_row const *row;
		char now[VALUE_TEXT], wanted[VALUE_TEXT], resolved[VALUE_TEXT];

		if (ae_settings_entry_is_header(entry))
			continue;
		row = ae_settings_row(entry);
		if (row->kind == AE_SETTINGS_SUBSCREEN || row->kind == AE_SETTINGS_KEY)
			continue;
		/* (RESTORE DEFAULTS: the player's own rows only, this PC's stay) */
		if (defaults && (row->flags & AE_SETTINGS_THIS_PC))
			continue;
		row_value(who.player, row, now, sizeof(now));
		if (defaults)
			row_default(who.player, row, wanted, sizeof(wanted));
		else
			snprintf(wanted, sizeof(wanted), "%s", group.opened[entry]);
		row_resolved(who.player, row, wanted, resolved, sizeof(resolved));
		if ((!wanted[0] && !ae_settings_row_takes_empty(row)) || values_same(row, now, resolved))
			continue;
		/* (the exact text, as opened or as the default: never snapped to a step) */
		if (row_set(who.player, row, wanted))
			put++;
	}
	return put;
}

/* the rows a RESTORE DEFAULTS would change */
static short defaults_changes(void)
{
	short place, found = 0;

	for (place = 0; place < group.count; place++)
	{
		short entry = group.entries[place];
		struct ae_settings_row const *row;
		char now[VALUE_TEXT], wanted[VALUE_TEXT], resolved[VALUE_TEXT];

		if (ae_settings_entry_is_header(entry))
			continue;
		row = ae_settings_row(entry);
		if (row->kind == AE_SETTINGS_SUBSCREEN || row->kind == AE_SETTINGS_KEY)
			continue;
		if (row->flags & AE_SETTINGS_THIS_PC)
			continue;
		row_value(who.player, row, now, sizeof(now));
		row_default(who.player, row, wanted, sizeof(wanted));
		row_resolved(who.player, row, wanted, resolved, sizeof(resolved));
		if ((wanted[0] || ae_settings_row_takes_empty(row)) && !values_same(row, now, resolved))
			found++;
	}
	return found;
}

static const char *who_text(void)
{
	return ae_settings_group(group.group)->whose == AE_SETTINGS_PC ? NULL : who.name[who.player];
}

static void group_player_switch(void);

static void dialog_picked(short choice, void *context)
{
	int dialog = group.dialog, then_switch = group.then_switch;
	short then_link = group.then_link;

	(void)context;
	group.dialog = 0;
	group.then_switch = 0;
	group.then_link = 0;
	if (dialog == 4)
	{
		/* KEEP (0), UNDO (1: also the time running out) */
		struct ae_settings_row const *row = ae_settings_row(group.display_row);

		if (choice == 1 && row && row_set(who.player, row, group.display_old))
			platform_log("ae settings: display undo (%s %s)", row->label, group.display_old);
		else
			platform_log("ae settings: display keep");
		group.display_row = -1;
		group.tick_row = -1;
		refresh();
		return;
	}
	if (dialog == 1)
	{
		/* KEEP CHANGES (0), UNDO CHANGES (1), KEEP EDITING (2); asked by X: then the next player, else back */
		if (choice == 0)
		{
			/* (KEEP: in the player's file now) */
			ae_player_settings_flush();
			platform_log("ae settings: keep");
		}
		else if (choice == 1)
		{
			short put = rows_put_back(0);

			platform_log("ae settings: undo (%d rows)", put);
		}
		if (choice == 0 || choice == 1)
		{
			if (then_switch)
				group_player_switch();
			else if (then_link)
				link_open((short)(then_link - 1), who.player);
			else
				ae_ui_pop();
		}
	}
	else if (dialog == 2 && choice == 1)
	{
		short put = rows_put_back(1);

		platform_log("ae settings: defaults (%d rows)", put);
		group.tick_row = -1;
		refresh();
	}
}

static void dialog_open(const char *title, const char *body, const char *const *choices, short count, short safe,
	short cancel)
{
	struct ae_dialog_spec spec;
	short index;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	for (index = 0; index < count && index < AE_DIALOG_CHOICES; index++)
		spec.choices[index] = choices[index];
	spec.choice_count = count;
	spec.safe_choice = safe;
	spec.cancel_choice = cancel;
	spec.has_cancel = 1;
	spec.density = group.density;
	spec.bounds = group.frame.rect;
	spec.picked = dialog_picked;
	ae_dialog_open(&spec, AE_OWNER_PARENT);
}

static int keep_ask(void)
{
	static const char *const choices[] = { "KEEP CHANGES", "UNDO CHANGES", "KEEP EDITING" };
	const char *labels[AE_SETTINGS_ENTRIES];
	short count = changes(labels, AE_SETTINGS_ENTRIES);
	char body[400];

	if (!count)
		return 0;
	ae_settings_keep_body(ae_settings_group(group.group)->name, who_text(), labels, count, body, sizeof(body));
	group.dialog = 1;
	dialog_open("Keep your changes?", body, choices, 3, 0, 2);
	return 1;
}

static void defaults_ask(void)
{
	static const char *const choices[] = { "CANCEL", "RESTORE DEFAULTS" };
	static const char *const ok[] = { "OK" };
	char body[400], title[80];
	struct ae_settings_group_info const *info = ae_settings_group(group.group);
	short count = defaults_changes();

	ae_settings_restore_body(info->name, who_text(), count, body, sizeof(body));
	snprintf(title, sizeof(title), "Restore %s defaults?", info->name);
	if (!count)
	{
		group.dialog = 3;
		dialog_open(title, body, ok, 1, 0, 0);
		return;
	}
	group.dialog = 2;
	dialog_open(title, body, choices, 2, 0, 0);
}

/* ---------- drawing a group */

/* the width of a text's first characters */
static float prefix_width(int font, float size, const char *text, short count)
{
	char part[AE_TEXT_MAXIMUM];

	snprintf(part, sizeof(part), "%.*s", (int)count, text);
	return part[0] ? ae_draw_text_width(font, size, part) : 0.0f;
}

/* (0.3.9) the row being typed (always the focused one, on the bar): its value's place is a field, right where the
values end: the well (the text fields' own, AE_COLOR_WELL), its 2 u accent ring, the text, its selection and the caret;
the row's range left of it, quiet on the bar. No arrows, no track: left and right move the caret. The well is "10.00"
wide plus 12 u each side, 8 u inside the row's top and bottom (the clearance rule's 8 px at 1080p) */
#define TYPED_PAD_U 12.0f
#define TYPED_INSET_U 8.0f
#define TYPED_RING_U 2.0f
#define TYPED_CARET_U 2.0f
#define TYPED_HINT_GAP_U 16.0f
static void typed_row(struct ae_density const *density, struct ae_settings_row const *row, float x, float y,
	float width, struct ae_row *widget, short item)
{
	struct ae_settings_number range;
	struct ae_settings_whole whole;
	char low[16], high[16], hint[48];
	float unit = density->unit, text = ae_size_text(density), minor = ae_size_minor(density), height, well_width;
	float well_height, well_x, well_y, center, text_x, cap = ae_draw_cap_height(AE_FONT_ROW, text);
	short from, to;

	/* (the row as an action row: its label, dot and badges; the base value would sit where the field is) */
	widget->value = NULL;
	widget->base_value = NULL;
	widget->flags &= ~(unsigned int)AE_ROW_SLIDER;
	widget->tick = NULL;
	height = ae_widget_row(density, x, y, width, widget, HIT_LIST, item);
	well_width = ae_draw_text_width(AE_FONT_ROW, text, "10.00") + 2.0f * TYPED_PAD_U * unit + TYPED_CARET_U * unit;
	well_height = height - 2.0f * TYPED_INSET_U * unit;
	well_x = x + width - density->metrics->pad * unit - well_width;
	well_y = y + TYPED_INSET_U * unit;
	center = y + height * 0.5f;
	ae_draw_rect(well_x, well_y, well_width, well_height, 0.0f, AE_COLOR_WELL);
	ae_draw_outline(well_x, well_y, well_width, well_height, 0.0f, TYPED_RING_U * unit, AE_COLOR_ACCENT);
	text_x = well_x + TYPED_PAD_U * unit;
	if (ae_text_selection(&group.typed, &from, &to))
	{
		float left = prefix_width(AE_FONT_ROW, text, group.typed.text, from);
		float right = prefix_width(AE_FONT_ROW, text, group.typed.text, to);

		ae_draw_rect(text_x + left, center - well_height * 0.3f, right - left, well_height * 0.6f, 0.0f,
			AE_COLOR_TEXT_SELECT);
	}
	if (group.typed.text[0])
		ae_draw_text(AE_FONT_ROW, text, text_x, center - cap * 0.5f, AE_ALIGN_LEFT, AE_COLOR_TITLE, group.typed.text);
	if (ae_caret_visible(group.caret_since))
		ae_draw_rect(text_x + prefix_width(AE_FONT_ROW, text, group.typed.text, group.typed.caret),
			center - well_height * 0.3f, TYPED_CARET_U * unit, well_height * 0.6f, 0.0f, AE_COLOR_ACCENT);
	/* the range: "0.1 TO 4.0", "30 TO 1000" */
	if (ae_settings_row_whole(row, &whole))
	{
		snprintf(hint, sizeof(hint), "%ld TO %ld", whole.minimum, whole.maximum);
		ae_draw_text(AE_FONT_BODY, minor, well_x - TYPED_HINT_GAP_U * unit, center - ae_draw_cap_height(AE_FONT_BODY,
			minor) * 0.5f, AE_ALIGN_RIGHT, AE_COLOR_SELECTION_MUTED, hint);
	}
	else if (ae_settings_row_number(row, &range))
	{
		ae_settings_number_text(range.minimum, low, sizeof(low));
		ae_settings_number_text(range.maximum, high, sizeof(high));
		snprintf(hint, sizeof(hint), "%s TO %s", low, high);
		ae_draw_text(AE_FONT_BODY, minor, well_x - TYPED_HINT_GAP_U * unit, center - ae_draw_cap_height(AE_FONT_BODY,
			minor) * 0.5f, AE_ALIGN_RIGHT, AE_COLOR_SELECTION_MUTED, hint);
	}
	ae_hit_add(well_x, y, well_width, height, HIT_LIST, AE_PART_FIELD, item);
}

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_density const *density = context;
	short entry = group.entries[item];
	struct ae_settings_row const *row;
	struct ae_row widget;
	char now[VALUE_TEXT], shown[VALUE_TEXT], base[VALUE_TEXT];
	short index;

	if (ae_settings_entry_is_header(entry))
	{
		float head = density->metrics->group_height * density->unit;

		ae_widget_group(density, x, y + height - head, width, ae_settings_entry_section(entry));
		return;
	}
	row = ae_settings_row(entry);
	row_value(who.player, row, now, sizeof(now));
	row_shown(who.player, row, now, shown, sizeof(shown));
	memset(&widget, 0, sizeof(widget));
	widget.label = row->label;
	widget.value = shown;
	widget.flags = flags;
	if (row->kind == AE_SETTINGS_SLIDER)
	{
		index = row_index(row, now);
		widget.flags |= AE_ROW_SLIDER;
		/* (a number row's value between its steps: its place between theirs) */
		widget.slider = ae_settings_row_number(row, NULL) ? ae_settings_slider_at(row, now) :
			ae_settings_slider(row, index < 0 ? 0 : index);
	}
	else if (row->kind == AE_SETTINGS_SUBSCREEN)
		widget.flags |= AE_ROW_SUBSCREEN;
	else if (row->kind == AE_SETTINGS_LINK)
		widget.flags |= AE_ROW_LINK;
	else if (row->kind == AE_SETTINGS_KEY)
		widget.flags |= AE_ROW_KEY;
	if (row->flags & AE_SETTINGS_RESTART)
		widget.badges[0] = "RESTART";
	if (((row->flags & AE_SETTINGS_THIS_PC) && who.count > 1) || (row->flags & AE_SETTINGS_PILL_ALWAYS))
		widget.badges[1] = "THIS PC";
	if (row_changed(entry))
	{
		widget.flags |= AE_ROW_CHANGED;
		row_shown(who.player, row, group.opened[entry], base, sizeof(base));
		widget.base_value = base;
	}
	if (entry == group.tick_row)
	{
		widget.tick = &group.tick;
		widget.tick_side = group.tick_side;
		widget.old_value = group.tick_old;
	}
	if (flags & AE_ROW_FOCUSED)
	{
		group.focus_rect.x = x;
		group.focus_rect.y = y;
		group.focus_rect.width = width;
		group.focus_rect.height = height;
	}
	if (entry == group.typing_row)
	{
		typed_row(density, row, x, y, width, &widget, item);
		return;
	}
	ae_widget_row(density, x, y, width, &widget, HIT_LIST, item);
}

static void group_header(void)
{
	struct ae_settings_group_info const *info = ae_settings_group(group.group);
	float size = gu(TITLE_U), top_y = ae_frame_y(&group.frame, TITLE_Y), middle;
	float left = group.frame.rect.x + group.frame.margin, right = group.frame.rect.x + group.frame.rect.width -
		group.frame.margin;
	short count = changes(NULL, 0);
	char changed[32], name[48], player[16];

	middle = top_y + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;
	ae_draw_text(AE_FONT_TITLE, size, left, top_y, AE_ALIGN_LEFT, AE_COLOR_TITLE, info->name);
	if (count)
		snprintf(changed, sizeof(changed), "%d CHANGED", count);
	else
		snprintf(changed, sizeof(changed), "NO CHANGES");
	if (info->whose == AE_SETTINGS_PC)
	{
		const char *parts[2] = { "THIS PC", changed };

		crumb(&group.density, group.frame.unit, right, middle, parts, 2, count ? AE_COLOR_ACCENT : AE_COLOR_MUTED, -1);
	}
	else
	{
		const char *parts[3] = { name, player, changed };

		upper(who.name[who.player], name, sizeof(name));
		snprintf(player, sizeof(player), "PLAYER %d", who.player + 1);
		crumb(&group.density, group.frame.unit, right, middle, parts, 3, count ? AE_COLOR_ACCENT : AE_COLOR_MUTED,
			who.player);
	}
	ae_draw_rect(left, ae_frame_y(&group.frame, RULE_Y), right - left, gu(RULE_U), 0.0f, AE_COLOR_RULE);
}

/* the rail: the sections, the focus's lit, a dot where a change is */
static void group_rail(float x, float y, float width)
{
	float text = ae_size_text(&group.density), minor = ae_size_minor(&group.density), pitch = gu(RAIL_PITCH_U);
	short current = ae_settings_section_at(group.sections, group.section_count, group.list.list.focus), section;

	for (section = 0; section < group.section_count; section++)
	{
		float middle = y + (pitch - gu(RAIL_GAP_U)) * 0.5f, count_x = x + width - gu(RAIL_PAD_U);
		int lit = section == current, changed = 0;
		short place;
		char rows[8];

		for (place = (short)(group.sections[section].header + 1); place < group.count &&
			!ae_settings_entry_is_header(group.entries[place]); place++)
			changed |= row_changed(group.entries[place]);
		if (lit)
		{
			ae_draw_rect(x, y, width, pitch - gu(RAIL_GAP_U), 0.0f, AE_COLOR_ROW);
			ae_draw_rect(x, y, gu(RAIL_STRIPE_U), pitch - gu(RAIL_GAP_U), 0.0f, AE_COLOR_ACCENT);
		}
		ae_draw_text(AE_FONT_ROW, text, x + gu(RAIL_PAD_U), cap_top(AE_FONT_ROW, text, middle), AE_ALIGN_LEFT,
			lit ? AE_COLOR_TITLE : AE_COLOR_TEXT, group.sections[section].name);
		snprintf(rows, sizeof(rows), "%d", group.sections[section].rows);
		count_x -= ae_draw_text(AE_FONT_BODY, minor, count_x, cap_top(AE_FONT_BODY, minor, middle), AE_ALIGN_RIGHT,
			AE_COLOR_MUTED, rows);
		if (changed)
			ae_widget_separator(&group.density, count_x - gu(14.0f), middle, DOT_U, AE_COLOR_ACCENT);
		ae_hit_add(x, y, width, pitch - gu(RAIL_GAP_U), HIT_RAIL, AE_PART_ROW, section);
		y += pitch;
	}
}

static void group_help(float x, float y, float width)
{
	struct ae_settings_row const *row = focused_row();
	char expanded[320], lines[HELP_LINES][160];
	float body = gu(group.density.metrics->body), line = gu(group.density.metrics->body_line);
	int count, index;

	const char *help;
	char named[240], now[VALUE_TEXT];

	if (!row)
		return;
	help = row->help;
	/* (a device's whole name, which its row may cut: "Where the game's sound plays: <name>. A lists every output.") */
	if (!strcmp(row->key, "audio.output_device") || !strcmp(row->key, "audio.input_device"))
	{
		row_value(who.player, row, now, sizeof(now));
		if (now[0] && strcmp(now, "default"))
		{
			snprintf(named, sizeof(named), !strcmp(row->key, "audio.output_device") ?
				"Where the game's sound plays: %s. {A} lists every output." : "The microphone voice chat uses: %s.", now);
			help = named;
		}
	}
	/* (0.3.9: a number row's help says how to type it and step it finely, to the keyboard and mouse) */
	{
		struct ae_settings_number range;

		struct ae_settings_whole whole;

		if (ae_settings_row_whole(row, &whole))
		{
			if (group.typing_row >= 0)
				snprintf(named, sizeof(named), "Type %ld to %ld. Enter keeps it, Esc goes back.", whole.minimum,
					whole.maximum);
			else if (ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE)
				snprintf(named, sizeof(named), "%s Enter types a number.", row->help);
			else
				snprintf(named, sizeof(named), "%s ALL VALUES ends with TYPE A NUMBER...", row->help);
			help = named;
		}
		else if (ae_settings_row_number(row, &range) && group.typing_row >= 0)
		{
			char low[16], high[16];

			ae_settings_number_text(range.minimum, low, sizeof(low));
			ae_settings_number_text(range.maximum, high, sizeof(high));
			snprintf(named, sizeof(named), "Type %s to %s%s. Enter keeps it, Esc goes back.", low, high,
				range.has_special ? ", or 0 for SAME" : "");
			help = named;
		}
		else if (ae_settings_row_number(row, NULL) && ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE)
		{
			snprintf(named, sizeof(named), "%s Enter types a value. Shift with the arrows, or Ctrl with the wheel, "
				"steps 0.01.", row->help);
			help = named;
		}
	}
	count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(help, expanded, sizeof(expanded)), width, lines, HELP_LINES);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
}

static void group_footer(void)
{
	short count = 0;
	float left = group.frame.rect.x + group.frame.margin, right = group.frame.rect.x + group.frame.rect.width -
		group.frame.margin;
	struct ae_settings_row const *row = focused_row();

	/* (0.3.9: typing a number row's value: Enter keeps it, Esc drops it, nothing else) */
	if (group.typing_row >= 0)
	{
		group.prompts[count++] = prompt_of(AE_BUTTON_A, "Keep", AE_ACTION_ACCEPT, 0);
		group.prompts[count++] = prompt_of(AE_BUTTON_B, "Cancel", AE_ACTION_BACK, 0);
		group.prompt_count = count;
		ae_widget_prompts(&group.density, left, group.frame.rect.height - gu(FOOTER_ABOVE_U), group.prompts, count,
			NULL, right, -1, HIT_PROMPTS);
		return;
	}
	/* (a number row's Enter types its value; a pad's A lists ALL VALUES as before) */
	group.prompts[count++] = prompt_of(AE_BUTTON_A, row && typable(row) &&
		ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE ? "Type a value" : ae_string(AE_STR_ALL_VALUES),
		AE_ACTION_ACCEPT, 0);
	group.prompts[count++] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK, 0);
	group.prompts[count++] = prompt_of(AE_BUTTON_LEFT_SHOULDER, "Section", AE_ACTION_TAB_PREVIOUS, 1);
	group.prompts[count++] = prompt_of(AE_BUTTON_Y, "Restore defaults", AE_ACTION_Y, 0);
	if (who.count > 1 && ae_settings_group(group.group)->whose != AE_SETTINGS_PC)
		group.prompts[count++] = prompt_of(AE_BUTTON_X, "Player", AE_ACTION_X, 0);
	group.prompt_count = count;
	ae_widget_prompts(&group.density, left, group.frame.rect.height - gu(FOOTER_ABOVE_U), group.prompts, count, NULL,
		right, -1, HIT_PROMPTS);
}

static void group_draw(struct ae_screen *instance)
{
	float top_y, bottom, rail_width, list_x, list_width, help_y, line, left;

	(void)instance;
	group_density();
	if (!ae_ui_drawing_ghost())
	{
		players_read();
		refresh();
	}
	group_header();
	left = group.frame.rect.x + group.frame.margin;
	top_y = ae_frame_y(&group.frame, CONTENT_Y);
	bottom = group.frame.rect.height - gu(FOOTER_ABOVE_U) - gu(FOOTER_ROOM_U);
	line = gu(group.density.metrics->body_line);
	help_y = bottom - line * (float)HELP_LINES;
	rail_width = group.frame.rect.width * RAIL_WIDTH;
	list_x = left + rail_width + gu(LIST_GAP_U);
	list_width = group.frame.rect.x + group.frame.rect.width - group.frame.margin - list_x;
	group_rail(left, top_y, rail_width);
	ae_widget_list(&group.density, &group.list, list_x, top_y, list_width, help_y - gu(HELP_GAP_U) - top_y, list_item,
		&group.density, HIT_LIST);
	ae_draw_rect(list_x, help_y - gu(HELP_GAP_U) * 0.5f, list_width, gu(RULE_U), 0.0f, AE_COLOR_RULE);
	group_help(list_x, help_y, list_width);
	group_footer();
}

/* ---------- a group's input */

static void group_player_switch(void)
{
	short next = player_next();

	if (next == who.player)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	who.player = next;
	/* (the new player's changes count from their values now) */
	snapshot();
	group.tick_row = -1;
	platform_log("ae settings: player %d", who.player + 1);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	refresh();
}

static int group_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_settings_row const *row;

	(void)instance;
	refresh();
	row = focused_row();
	/* (0.3.9: typing: Enter keeps it (START, as the typing keyboard's Enter is), Esc drops it, the arrows move the
	caret (Shift selects); nothing else) */
	if (group.typing_row >= 0)
	{
		switch (event->action)
		{
		case AE_ACTION_ACCEPT:
		case AE_ACTION_START:
			typing_end(1);
			break;
		case AE_ACTION_BACK:
			typing_end(0);
			break;
		case AE_ACTION_LEFT:
		case AE_ACTION_RIGHT:
			ae_text_move(&group.typed, event->action == AE_ACTION_RIGHT ? 1 : -1, fine_held());
			group.caret_since = ae_motion_now();
			break;
		default:
			break;
		}
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		/* (with changes: KEEP / UNDO first) */
		if (keep_ask())
			return 1;
		return 0;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		section_jump(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		/* (Shift on the keyboard: a number row's 0.01) */
		if (row)
			value_step(row, event->action == AE_ACTION_RIGHT ? 1 : -1, event->device == AE_DEVICE_KEYBOARD_MOUSE &&
				fine_held());
		return 1;
	case AE_ACTION_ACCEPT:
		/* (a number row: the keyboard's Enter types its value; a pad's A lists its values as before) */
		if (row && typable(row) && event->device == AE_DEVICE_KEYBOARD_MOUSE)
			typing_begin(row);
		else if (row)
			picker_open(row);
		return 1;
	case AE_ACTION_Y:
		defaults_ask();
		return 1;
	case AE_ACTION_X:
		/* (with changes: KEEP / UNDO for this player first, then the next) */
		if (who.count > 1 && ae_settings_group(group.group)->whose != AE_SETTINGS_PC)
		{
			group.then_switch = 1;
			if (!keep_ask())
			{
				group.then_switch = 0;
				group_player_switch();
			}
		}
		return 1;
	default:
		break;
	}
	{
		short direction = event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP ? -1 : 1;

		if (ae_list_view_event(&group.list, event))
		{
			skip_headers(direction);
			refresh();
		}
	}
	return 1;
}

/* a slider's track under the pointer: the step at that point */
static void slider_at(short place, struct ae_hit const *hit, float pointer_x)
{
	struct ae_settings_row const *row;
	short count, index;
	char now[VALUE_TEXT];
	float fraction;

	if (place < 0 || place >= group.count || ae_settings_entry_is_header(group.entries[place]))
		return;
	row = ae_settings_row(group.entries[place]);
	count = ae_settings_value_count(row);
	if (count < 2 || hit->rect.width <= 0.0f)
		return;
	fraction = (pointer_x - hit->rect.x) / hit->rect.width;
	fraction = fraction < 0.0f ? 0.0f : fraction > 1.0f ? 1.0f : fraction;
	index = (short)(fraction * (float)(count - 1) + 0.5f);
	row_value(who.player, row, now, sizeof(now));
	if (index != row_index(row, now))
	{
		value_set(row, index);
		group.tick_row = -1;
		refresh();
	}
}

static void group_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short item;

	/* (0.3.9: typing: the prompts (KEEP, CANCEL); a click anywhere but the field keeps it, as a field losing the
	focus; the wheel does nothing) */
	if (group.typing_row >= 0)
	{
		if (ae_prompts_pointer(group.prompts, group.prompt_count, pointer, HIT_PROMPTS, &event))
			group_handle(instance, &event);
		else if (pointer->left_clicks && !(ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_LIST &&
			hit.part == AE_PART_FIELD))
			typing_end(1);
		return;
	}
	/* (0.3.9: Ctrl with the wheel over a number row: 0.01 a notch, away from you up) */
	if (pointer->wheel_steps && (ae_platform_modifiers() & AE_MODIFIER_CONTROL) &&
		ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_LIST && hit.index >= 0 && hit.index < group.count &&
		!ae_settings_entry_is_header(group.entries[hit.index]) &&
		ae_settings_row_number(ae_settings_row(group.entries[hit.index]), NULL))
	{
		int notches = pointer->wheel_steps > 0 ? pointer->wheel_steps : -pointer->wheel_steps;

		ae_list_set_focus(&group.list.list, hit.index);
		refresh();
		while (notches-- > 0)
			value_step(ae_settings_row(group.entries[hit.index]), pointer->wheel_steps > 0 ? 1 : -1, 1);
		return;
	}
	if (ae_prompts_pointer(group.prompts, group.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			if (!keep_ask())
			{
				ae_sound_request(AE_SOUND_BACK, 0);
				ae_ui_pop();
			}
		}
		else
			group_handle(instance, &event);
		return;
	}
	/* (a slider held: it follows the pointer until the button is let go of) */
	if (group.dragging_row >= 0)
	{
		if (!pointer->left_held)
			group.dragging_row = -1;
		else if (ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_LIST && hit.part == AE_PART_TRACK &&
			hit.index == group.dragging_row)
			slider_at(hit.index, &hit, pointer->x);
		return;
	}
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit))
	{
		if (hit.id == HIT_RAIL)
		{
			section_go(hit.index);
			return;
		}
		if (hit.id == HIT_LIST && hit.index >= 0 && hit.index < group.count &&
			!ae_settings_entry_is_header(group.entries[hit.index]))
		{
			if (hit.part == AE_PART_TRACK)
			{
				ae_list_set_focus(&group.list.list, hit.index);
				refresh();
				slider_at(hit.index, &hit, pointer->x);
				group.dragging_row = hit.index;
				return;
			}
			/* (Shift held: a number row's 0.01) */
			if (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT)
			{
				ae_list_set_focus(&group.list.list, hit.index);
				refresh();
				value_step(ae_settings_row(group.entries[hit.index]), hit.part == AE_PART_ARROW_RIGHT ? 1 : -1,
					fine_held());
				return;
			}
			/* (0.3.9: a number row's value clicked: typed there; a touch keeps the values' list) */
			if (hit.part == AE_PART_VALUE && !pointer->touch &&
				typable(ae_settings_row(group.entries[hit.index])))
			{
				ae_list_set_focus(&group.list.list, hit.index);
				refresh();
				typing_begin(ae_settings_row(group.entries[hit.index]));
				return;
			}
		}
	}
	item = ae_list_view_pointer(&group.list, pointer, HIT_LIST);
	skip_headers(1);
	refresh();
	if (item >= 0 && item < group.count && !ae_settings_entry_is_header(group.entries[item]))
		picker_open(ae_settings_row(group.entries[item]));
}

/* (0.3.9) the typed keys, once a frame: digits and one '.', two decimals, five characters; anything else stays out (a
failure sound) */
static void group_update(struct ae_screen *instance)
{
	struct ae_text before;
	int ok;

	(void)instance;
	/* (TYPE A NUMBER... chosen from ALL VALUES: AE's keyboard opens once the list is gone) */
	if (group.keyboard_row >= 0)
	{
		short row_number = group.keyboard_row;

		group.keyboard_row = -1;
		whole_keyboard_open(ae_settings_row(row_number));
		return;
	}
	if (group.typing_row >= 0 && ae_settings_row_whole(ae_settings_row(group.typing_row), NULL) &&
		!ae_settings_whole_typing_ok(group.typed.text))
		whole_digits_only();
	if (group.typing_row < 0 || !group.typed_edit.active)
		return;
	before = group.typed;
	ok = ae_field_edit_keys(&group.typed_edit);
	if (ae_settings_row_whole(ae_settings_row(group.typing_row), NULL))
	{
		if (!ae_settings_whole_typing_ok(group.typed.text))
		{
			group.typed = before;
			ok = 0;
		}
	}
	else if (!ae_settings_number_typing_ok(group.typed.text))
	{
		group.typed = before;
		ok = 0;
	}
	if (memcmp(&before, &group.typed, sizeof(before)))
		group.caret_since = ae_motion_now();
	if (!ok)
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static void group_leave(struct ae_screen *instance)
{
	(void)instance;
	/* (typing when the screen goes: dropped) */
	if (group.typing_row >= 0)
	{
		ae_field_edit_end(&group.typed_edit, 0);
		group.typing_row = -1;
	}
	ae_player_settings_flush();
	platform_log("ae settings: closed %s (%d changed)", ae_settings_group(group.group)->name, changes(NULL, 0));
}

static struct ae_screen_class const group_class =
{
	.name = "settings group", .leave = group_leave, .handle = group_handle, .draw = group_draw,
	.pointer = group_pointer, .update = group_update,
};

int ae_screen_settings_open_group(short group_number, short local_player)
{
	struct ae_settings_group_info const *info = ae_settings_group(group_number);

	if (!info || !info->built)
		return 0;
	memset(&group, 0, sizeof(group));
	group.group = group_number;
	group.tick_row = -1;
	group.logged_focus = -1;
	group.dragging_row = -1;
	group.typing_row = -1;
	group.keyboard_row = -1;
	group.logged_width = -1.0f;
	who.player = local_player >= 0 && local_player < PLAYERS ? local_player : 0;
	ae_player_settings_update();
	players_read();
	if (group_number == AE_SETTINGS_SYSTEM)
		discs_read();
	else if (group_number == AE_SETTINGS_VIDEO)
	{
		sizes_read("display.resolution", platform_display_resolutions, 1);
		sizes_read("display.window_size", platform_window_sizes, 0);
	}
	else if (group_number == AE_SETTINGS_AUDIO)
	{
		/* (CALLOUT VOICE moved here from HUD in 0.3.9: its voices are read here now) */
		voices_read();
		devices_read("audio.output_device", 0);
		devices_read("audio.input_device", 1);
	}
	group.display_row = -1;
	snapshot();
	{
		struct ae_settings_facts facts;

		facts_of(who.player, &facts);
		group.count = ae_settings_entries(group.group, &facts, group.entries, AE_SETTINGS_ENTRIES);
	}
	ae_list_view_init(&group.list, group.count, 12);
	/* (the first row, under the first section's header) */
	ae_list_set_focus(&group.list.list, 1);
	group.list.previous_focus = 1;
	if (!ae_ui_push(&group_class, AE_OWNER_PARENT, &group))
		return 0;
	platform_log("ae settings: group %s player %d rows %d", info->name, who.player + 1, group.count);
	(void)ae_settings_ui_scale();
	return 1;
}

/* ---------- the debug screens (debug.ae_test_screen 34 / 35) */

/* a local player on a profile of their own: the profile named so, made when this PC has none of that name */
/* the test's profile, made if this PC lacks it, and played with (use: else left on the PC unchosen, as after a
launch to AE's menus) */
static void test_profile(short player, const char *name, int use)
{
	static struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	short count = 0, index;
	int found = -1;

	if (player_ui_get_active_player_profile_index(player) != NONE)
		return;
	if (ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &count).ok)
	{
		for (index = 0; index < count; index++)
		{
			if (!strcmp(profiles[index].name, name))
				found = profiles[index].index;
		}
	}
	if (found == NONE && !ae_profile_new(name, &found).ok)
		return;
	if (use)
		ae_profile_switch(player, found);
}

int ae_screen_settings_test_open(int group_screen, int group_number, int players)
{
	short player;

	/* ($HALO_AE_TEST_SETTINGS_NO_ACTIVE: player 1 has none chosen, as AE's menus right after a launch) */
	test_profile(0, "Tester", !getenv("HALO_AE_TEST_SETTINGS_NO_ACTIVE"));
	for (player = 1; player < players && player < PLAYERS; player++)
	{
		char name[16];

		snprintf(name, sizeof(name), "Tester %d", player + 1);
		test_profile(player, name, 1);
	}
	if (!(group_screen ? ae_screen_settings_open_group((short)group_number, 0) : ae_screen_settings_open(0)))
		return 0;
	ae_ui_replace_menus();
	return 1;
}
