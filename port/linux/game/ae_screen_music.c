/*
AE_SCREEN_MUSIC.C

AE's MENU MUSIC screens (ae_screen_music.h): MENU MUSIC, ONE SONG, MY PLAYLIST. The rules (modes, the playlist's text,
names, what a choice switches to) are ae_menu_music_rules.c's; the playing, the catalog of songs and the preview are
ae_mcc_music.c's. The settings are read each frame and written at once: the screens keep nothing the config does not.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_menu_music_rules.h"
#include "ae_mcc_music.h"
#include "ae_screen_music.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
char const *config_string(char const *name);
double config_real(char const *name);
int config_boolean(char const *name);
int config_write(char const *name, char const *value);
int config_write_boolean(char const *name, int value);
int ae_mcc_redetect(void);

enum
{
	MUSIC_HIT = 0x7B10,
	HIT_MENU_ROWS = MUSIC_HIT, HIT_MENU_CHIPS, HIT_MENU_PROMPTS,
	HIT_SONG_ITEMS, HIT_SONG_CHIPS, HIT_SONG_PLAY, HIT_SONG_PROMPTS,
	HIT_ORDER_ITEMS, HIT_ORDER_CELLS
};

/* sizes, u */
#define GROUP_NAMES 8
#define ITEM_U 42.0f
#define CARD_INSET_U 23.0f
#define CARD_TEXT_U 22.0f
#define ITEMS_MAXIMUM 1100
#define PLAY_CELL_U 28.6f
#define CHECK_BOX_U 15.0f

enum { ROW_MODE, ROW_SHUFFLE, ROW_SONG, ROW_PLAYLIST, ROW_VOLUME, ROW_AGAIN, ROWS };
enum { PANE_SONGS, PANE_ORDER };

/* ---------- what the settings say */

static int mode_get(void)
{
	struct ae_menu_music_legacy legacy;
	char const *value = config_string("audio.menu_music");
	int mode;

	if (ae_menu_music_legacy(value, &legacy))
		return legacy.mode;
	mode = ae_menu_music_mode_parse(value);
	return mode < 0 ? AE_MUSIC_MODE_GAME_MENUS : mode;
}

static int shuffle_get(void)
{
	struct ae_menu_music_legacy legacy;

	if (ae_menu_music_legacy(config_string("audio.menu_music"), &legacy))
		return legacy.shuffle;
	return config_boolean("audio.menu_music_shuffle");
}

static char const *song_get(void)
{
	char const *value = config_string("audio.menu_music_song");

	return ae_menu_music_id_valid(value) ? value : AE_MUSIC_TITLE_ID;
}

static void mode_set(int mode)
{
	if (mode_get() == mode)
		return;
	config_write("audio.menu_music", ae_menu_music_mode_value(mode));
	platform_log("ae menu music screen: mode %s", ae_menu_music_mode_label(mode));
}

static void shuffle_set(int on)
{
	config_write_boolean("audio.menu_music_shuffle", on);
	platform_log("ae menu music screen: shuffle %d", on);
}

static int volume_get(void)
{
	double volume = config_real("audio.music_volume");

	if (volume < 0.0)
		volume = 0.0;
	if (volume > 1.0)
		volume = 1.0;
	return (int)(volume * 10.0 + 0.5);
}

static void volume_set(int steps)
{
	char text[16];

	if (steps < 0)
		steps = 0;
	if (steps > 10)
		steps = 10;
	snprintf(text, sizeof(text), "%.1f", (double)steps / 10.0);
	config_write("audio.music_volume", text);
	platform_log("ae menu music screen: volume %d", steps);
}

static void playlist_get(struct ae_menu_music_playlist *list)
{
	ae_menu_music_playlist_parse(config_string("audio.menu_music_playlist"), list);
}

static void playlist_set(struct ae_menu_music_playlist const *list)
{
	static char text[8200];

	if (ae_menu_music_playlist_serialize(list, text, sizeof(text)))
		config_write("audio.menu_music_playlist", text);
}

/* whether MCC has no songs to give (found none, or no installation): the catalog says it once read */
static int no_mcc(void)
{
	return ae_mcc_music_catalog_state() == AE_MCC_MUSIC_CATALOG_NONE;
}

static int catalog_ready(void)
{
	return ae_mcc_music_catalog_state() == AE_MCC_MUSIC_CATALOG_READY;
}

/* a playlist's length in seconds, the songs found only */
static long playlist_seconds(struct ae_menu_music_playlist const *list)
{
	long seconds = 0;
	int index, entry;

	if (!catalog_ready())
		return 0;
	for (index = 0; index < list->count; index++)
	{
		entry = ae_mcc_music_catalog_find(list->ids[index]);
		if (entry > 0)
			seconds += ae_mcc_music_catalog_seconds(entry);
	}
	return seconds;
}

static void playlist_summary(struct ae_menu_music_playlist const *list, char *text, size_t size)
{
	if (list->count > 0 && !catalog_ready())
		snprintf(text, size, "%d %s", list->count, list->count == 1 ? "SONG" : "SONGS");
	else
		ae_menu_music_playlist_summary(list->count, playlist_seconds(list), text, size);
}

void ae_screen_music_summary(char *text, int size)
{
	ae_mcc_music_catalog_request();
	ae_menu_music_summary(mode_get(), shuffle_get(), song_get(), no_mcc(), text, (size_t)size);
}

/* ---------- drawing helpers */

static float pu(struct ae_flow_screen const *flow, float spec_units)
{
	return ae_flow_u(flow, spec_units);
}

static void text_at(struct ae_flow_screen const *flow, int font, float size_u, float x, float center_y, int align,
	unsigned int color, char const *text)
{
	float size = pu(flow, size_u);

	ae_draw_text(font, size, x, ae_flow_cap_top(font, size, center_y), align, color, text);
}

static void label_at(struct ae_flow_screen const *flow, float x, float top, float size_u, unsigned int color,
	char const *text)
{
	float size = pu(flow, size_u);

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x, ae_flow_cap_top(AE_FONT_BODY, size,
		top + ae_draw_cap_height(AE_FONT_BODY, size) * 0.5f), AE_ALIGN_LEFT, color, text);
}

static void card_box(struct ae_flow_screen const *flow, float x, float y, float width, float height)
{
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_outline(x, y, width, height, 0.0f, pu(flow, 1.0f), AE_COLOR_RULE);
}

/* three bars, 3 u wide, 2 u apart, 14 u tall: a song plays */
static void playing_mark(struct ae_flow_screen const *flow, float x, float center_y, unsigned int color)
{
	float bar = pu(flow, 3.0f), gap = pu(flow, 2.0f), tall = pu(flow, 14.0f);
	float heights[3] = { 0.62f, 1.0f, 0.78f };
	int index;

	for (index = 0; index < 3; index++)
		ae_draw_rect(x + (float)index * (bar + gap), center_y + tall * 0.5f - tall * heights[index], bar, tall * heights[index],
			0.0f, color);
}

static float playing_mark_width(struct ae_flow_screen const *flow)
{
	return pu(flow, 3.0f) * 3.0f + pu(flow, 2.0f) * 2.0f;
}

static void progress_line(struct ae_flow_screen const *flow, float x, float y, float width, long position, long length)
{
	char left[16], right[16];
	float track = pu(flow, 4.0f);

	ae_draw_rect(x, y, width, track, track * 0.5f, (AE_COLOR_RULE & 0xFFFFFF00u) | 0x40u);
	if (length > 0)
	{
		float fraction = (float)position / (float)length;

		if (fraction > 1.0f)
			fraction = 1.0f;
		ae_draw_rect(x, y, width * fraction, track, track * 0.5f, AE_COLOR_ACCENT);
	}
	ae_menu_music_time_text(position, left, sizeof(left));
	ae_menu_music_time_text(length, right, sizeof(right));
	text_at(flow, AE_FONT_BODY, 16.0f, x, y + pu(flow, 14.0f) + pu(flow, 8.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, left);
	if (length > 0)
		text_at(flow, AE_FONT_BODY, 16.0f, x + width, y + pu(flow, 14.0f) + pu(flow, 8.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED,
			right);
}

/* a song's name and its game, from its entry */
static void entry_names(int entry, char *name, size_t name_size, char *game, size_t game_size)
{
	char const *id = ae_mcc_music_catalog_id(entry);

	ae_menu_music_name(id, name, name_size);
	snprintf(game, game_size, "%s", ae_menu_music_game_full(id));
}

static void button_prompts_clear(struct ae_flow_screen *flow)
{
	flow->prompt_count = 0;
}

static void prompt_add(struct ae_flow_screen *flow, int button, char const *label, int action)
{
	if (flow->prompt_count < AE_FLOW_PROMPTS)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(button, label, action);
}

/* ========== MENU MUSIC */

static struct
{
	struct ae_flow_screen flow;
	short focus;                       /* a row: ROW_* */
	short hover_chip;
	unsigned long opened_at;
	int restoring;
} menu;

static int row_enabled(int row)
{
	switch (row)
	{
	case ROW_SHUFFLE:
		return !ae_menu_music_shuffle_reason(mode_get(), no_mcc());
	case ROW_SONG:
	case ROW_PLAYLIST:
		return !no_mcc();
	case ROW_AGAIN:
		return no_mcc();
	default:
		return 1;
	}
}

static int row_step(int from, int direction)
{
	int row = from;

	do
		row += direction;
	while (row >= 0 && row < ROWS && !row_enabled(row));
	return row >= 0 && row < ROWS ? row : from;
}

static char const *row_help(int row, char *text, size_t size)
{
	struct ae_menu_music_playlist list;

	switch (row)
	{
	case ROW_MODE:
		return "GAME MENUS plays each game's own menu song. ONE SONG plays one song over and over. MY PLAYLIST plays "
			"your own list.";
	case ROW_SHUFFLE:
		return "Plays the songs in a new order each time. OFF plays them in their order.";
	case ROW_SONG:
		return "The song ONE SONG plays.";
	case ROW_PLAYLIST:
		playlist_get(&list);
		if (!list.count)
			snprintf(text, size, "Empty: {A} opens it to add songs.");
		else
			snprintf(text, size, "Your own list of songs, in your order.");
		return text;
	case ROW_VOLUME:
		return "The music in menus and the campaign.";
	default:
		return "Looks for MCC in every Steam library on this PC again. The menus keep the title theme meanwhile.";
	}
}

static void chip_set(struct ae_chip *chips, int mode_now)
{
	int mode;

	for (mode = 0; mode < AE_MUSIC_MODES; mode++)
	{
		chips[mode].label = ae_menu_music_mode_label(mode);
		chips[mode].count = -1;
		chips[mode].flags = (mode == mode_now ? AE_CHIP_ON : 0) | (menu.focus == ROW_MODE && mode == mode_now ?
			AE_CHIP_FOCUSED : 0) | (menu.hover_chip == mode ? AE_CHIP_HOVER : 0);
		if (no_mcc() && (mode == AE_MUSIC_MODE_ONE_SONG || mode == AE_MUSIC_MODE_MY_PLAYLIST))
			chips[mode].flags = AE_CHIP_DISABLED;
	}
}

static void now_card(struct ae_flow_screen const *flow, float x, float y, float width, float height, int mode,
	struct ae_mcc_music_now const *now, int up_next)
{
	float inset = pu(flow, CARD_INSET_U) - pu(flow, 1.0f), inner = width - 2.0f * inset;
	char name[64], game[48];
	int index;

	card_box(flow, x, y, width, height);
	label_at(flow, x + inset, y + pu(flow, 22.0f), 18.0f, AE_COLOR_MUTED, "NOW PLAYING");
	if (mode != AE_MUSIC_MODE_OFF)
		playing_mark(flow, x + inset + ae_draw_text_tracked_width(AE_FONT_BODY, pu(flow, 18.0f), 0.09f, "NOW PLAYING") +
			pu(flow, 12.0f), y + pu(flow, 22.0f) + pu(flow, 8.0f), AE_COLOR_ACCENT);
	if (mode == AE_MUSIC_MODE_OFF)
	{
		text_at(flow, AE_FONT_ROW, 30.0f, x + inset, y + pu(flow, 56.0f) + pu(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE,
			"MUSIC IS OFF");
		return;
	}
	if (now->entry >= 0)
		entry_names(now->entry, name, sizeof(name), game, sizeof(game));
	else
	{
		/* (the game's own title music alone: nothing of this unit's plays) */
		ae_menu_music_name(AE_MUSIC_TITLE_ID, name, sizeof(name));
		snprintf(game, sizeof(game), "%s", ae_menu_music_game_full(AE_MUSIC_TITLE_ID));
	}
	text_at(flow, AE_FONT_ROW, 30.0f, x + inset, y + pu(flow, 56.0f) + pu(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
	text_at(flow, AE_FONT_BODY, 21.0f, x + inset, y + pu(flow, 91.0f) + pu(flow, 8.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT, game);
	if (now->entry > 0)
		progress_line(flow, x + inset, y + pu(flow, 136.0f), inner, now->position, now->length);
	else if (now->entry == 0 || now->entry < 0)
		text_at(flow, AE_FONT_BODY, 16.0f, x + inset, y + pu(flow, 142.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "LOOP");
	if (!up_next)
		return;
	ae_draw_rect(x + inset, y + pu(flow, 186.0f), inner, pu(flow, 1.0f), 0.0f, AE_COLOR_RULE);
	label_at(flow, x + inset, y + pu(flow, 206.0f), 18.0f, AE_COLOR_MUTED, "UP NEXT");
	for (index = 0; index < now->next_count && index < AE_MCC_MUSIC_NEXT; index++)
	{
		float line_y = y + pu(flow, 240.0f) + (float)index * pu(flow, 32.0f) + pu(flow, 10.0f), after;

		entry_names(now->next[index], name, sizeof(name), game, sizeof(game));
		after = ae_draw_text_width(AE_FONT_ROW, pu(flow, 21.0f), name);
		text_at(flow, AE_FONT_ROW, 21.0f, x + inset, line_y, AE_ALIGN_LEFT, AE_COLOR_TEXT, name);
		text_at(flow, AE_FONT_BODY, 18.0f, x + inset + after + pu(flow, 12.0f), line_y, AE_ALIGN_LEFT, AE_COLOR_MUTED,
			ae_menu_music_game_short(ae_mcc_music_catalog_id(now->next[index])));
	}
}

static void missing_card(struct ae_flow_screen const *flow, float x, float y, float width, float height)
{
	float inset = pu(flow, CARD_INSET_U), inner = width - 2.0f * inset;
	char lines[4][160];
	int count, index;

	card_box(flow, x, y, width, height);
	text_at(flow, AE_FONT_ROW, 24.0f, x + inset, y + pu(flow, 36.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, "MCC NOT FOUND");
	count = ae_wrap_text(AE_FONT_BODY, pu(flow, 21.0f), "The songs of Halo 3, Halo 3: ODST and Halo: Reach play from your "
		"own MCC installation on Steam. AE found none on this PC.", inner, lines, 4);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, pu(flow, 21.0f), x + inset, ae_flow_cap_top(AE_FONT_BODY, pu(flow, 21.0f),
			y + pu(flow, 78.0f) + (float)index * pu(flow, 31.0f)), AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
	text_at(flow, AE_FONT_BODY, 18.0f, x + inset, y + height - pu(flow, 30.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"Install MCC, then LOOK AGAIN.");
}

static void menu_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &menu.flow;
	struct ae_density const *d;
	float left, width, right_x, right_width, y, gap;
	struct ae_row row;
	struct ae_chip chips[AE_MUSIC_MODES];
	struct ae_menu_music_playlist list;
	struct ae_mcc_music_now now;
	char summary[96], value[32], help[160], expanded[320], lines_text[2][160];
	int mode, lines, index, none;

	(void)instance;
	ae_flow_full_density(flow);
	d = &flow->density;
	ae_mcc_music_catalog_request();
	none = no_mcc();
	mode = mode_get();
	if (!row_enabled(menu.focus))
		menu.focus = ROW_MODE;
	ae_flow_title(flow, "MENU MUSIC", "AUDIO");
	left = ae_flow_left_x(flow);
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	gap = pu(flow, d->metrics->gap);
	y += ae_widget_group(d, left, y, width, "MUSIC");

	/* MODE: the chips right-aligned in the row, the row lit by its stripe alone */
	memset(&row, 0, sizeof(row));
	row.label = "MODE";
	row.flags = 0;
	chip_set(chips, mode);
	{
		struct ae_rect rects[AE_MUSIC_MODES];
		float chips_width, chips_height, row_height = pu(flow, d->metrics->row);

		chips_height = ae_chips_layout(d, 100000.0f, chips, AE_MUSIC_MODES, rects);
		chips_width = rects[AE_MUSIC_MODES - 1].x + rects[AE_MUSIC_MODES - 1].width;
		ae_widget_row(d, left, y, width, &row, HIT_MENU_ROWS, ROW_MODE);
		if (menu.focus == ROW_MODE)
			ae_draw_rect(left, y, pu(flow, 5.0f), row_height, 0.0f, AE_COLOR_ACCENT);
		ae_widget_chips(d, left + width - pu(flow, 20.0f) - chips_width, y + (row_height - chips_height) * 0.5f, chips_width +
			1.0f, chips, AE_MUSIC_MODES, HIT_MENU_CHIPS);
		y += row_height + gap;
	}
	/* SHUFFLE */
	{
		char const *reason = ae_menu_music_shuffle_reason(mode, none);

		memset(&row, 0, sizeof(row));
		row.label = "SHUFFLE";
		row.value = shuffle_get() ? "ON" : "OFF";
		row.flags = (menu.focus == ROW_SHUFFLE ? AE_ROW_FOCUSED | AE_ROW_ON_BAR : 0) | (reason ? AE_ROW_DISABLED : 0);
		row.reason = reason;
		y += ae_widget_row(d, left, y, width, &row, HIT_MENU_ROWS, ROW_SHUFFLE) + gap;
	}
	/* SONG and MY PLAYLIST */
	playlist_get(&list);
	{
		char name_game[96];

		ae_menu_music_summary(AE_MUSIC_MODE_ONE_SONG, 1, song_get(), 0, name_game, sizeof(name_game));
		memset(&row, 0, sizeof(row));
		row.label = "SONG";
		row.value = name_game;
		row.flags = AE_ROW_SUBSCREEN | (menu.focus == ROW_SONG ? AE_ROW_FOCUSED | AE_ROW_ON_BAR : 0) |
			(none ? AE_ROW_DISABLED : 0);
		row.reason = none ? "NEEDS MCC" : NULL;
		y += ae_widget_row(d, left, y, width, &row, HIT_MENU_ROWS, ROW_SONG) + gap;
		playlist_summary(&list, summary, sizeof(summary));
		memset(&row, 0, sizeof(row));
		row.label = "MY PLAYLIST";
		row.value = summary;
		row.flags = AE_ROW_SUBSCREEN | (menu.focus == ROW_PLAYLIST ? AE_ROW_FOCUSED | AE_ROW_ON_BAR : 0) |
			(none ? AE_ROW_DISABLED : 0);
		row.reason = none ? "NEEDS MCC" : NULL;
		y += ae_widget_row(d, left, y, width, &row, HIT_MENU_ROWS, ROW_PLAYLIST) + gap;
	}
	y += ae_widget_group(d, left, y, width, "VOLUME");
	snprintf(value, sizeof(value), "%d", volume_get());
	memset(&row, 0, sizeof(row));
	row.label = "MUSIC VOLUME";
	row.value = value;
	row.slider = (float)volume_get() / 10.0f;
	row.flags = AE_ROW_SLIDER | (menu.focus == ROW_VOLUME ? AE_ROW_FOCUSED | AE_ROW_ON_BAR : 0);
	y += ae_widget_row(d, left, y, width, &row, HIT_MENU_ROWS, ROW_VOLUME) + gap;
	if (none)
	{
		y += ae_widget_group(d, left, y, width, "MCC");
		memset(&row, 0, sizeof(row));
		row.label = "LOOK AGAIN";
		row.value = "NOT FOUND";
		row.flags = AE_ROW_STATUS | (menu.focus == ROW_AGAIN ? AE_ROW_FOCUSED | AE_ROW_ON_BAR : 0);
		y += ae_widget_row(d, left, y, width, &row, HIT_MENU_ROWS, ROW_AGAIN) + gap;
	}

	/* the help line: under the rows, its rule at y 902 */
	{
		float rule_y = ae_frame_y(&flow->frame, 902.0f / 1080.0f), body = pu(flow, d->metrics->body);
		float line_height = pu(flow, d->metrics->body_line);

		ae_draw_rect(left, rule_y, width, pu(flow, 1.0f), 0.0f, AE_COLOR_RULE);
		snprintf(help, sizeof(help), "%s", row_help(menu.focus, help, sizeof(help)));
		lines = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(help, expanded, sizeof(expanded)), width, lines_text, 2);
		for (index = 0; index < lines; index++)
			ae_draw_text(AE_FONT_BODY, body, left, rule_y + pu(flow, 14.0f) + line_height * (float)index, AE_ALIGN_LEFT,
				AE_COLOR_TEXT, lines_text[index]);
	}

	/* the right column: NOW PLAYING and UP NEXT, or the title theme and what is missing */
	right_x = ae_frame_x(&flow->frame, AE_FLOW_PANEL_X);
	right_width = ae_flow_right_x(flow) - right_x;
	y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	ae_mcc_music_now(&now);
	if (none)
	{
		now_card(flow, right_x, y, right_width, pu(flow, 130.0f), AE_MUSIC_MODE_GAME_MENUS, &now, 0);
		missing_card(flow, right_x, y + pu(flow, 130.0f + 24.0f), right_width, pu(flow, 210.0f));
	}
	else
		now_card(flow, right_x, y, right_width, pu(flow, 342.0f), mode, &now, mode != AE_MUSIC_MODE_ONE_SONG &&
			mode != AE_MUSIC_MODE_OFF);

	button_prompts_clear(flow);
	prompt_add(flow, AE_BUTTON_A, menu.focus == ROW_PLAYLIST || menu.focus == ROW_SONG || menu.focus == ROW_AGAIN ?
		"Open" : "Change", AE_ACTION_ACCEPT);
	prompt_add(flow, AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	if (mode != AE_MUSIC_MODE_ONE_SONG && mode != AE_MUSIC_MODE_OFF)
		prompt_add(flow, AE_BUTTON_X, "Next song", AE_ACTION_X);
	prompt_add(flow, AE_BUTTON_Y, "Restore defaults", AE_ACTION_Y);
	ae_flow_draw_prompts(flow);
}

static void defaults_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1)
		return;
	config_write("audio.menu_music", ae_menu_music_mode_value(AE_MUSIC_MODE_GAME_MENUS));
	config_write_boolean("audio.menu_music_shuffle", 1);
	config_write("audio.menu_music_song", AE_MUSIC_TITLE_ID);
	config_write("audio.menu_music_playlist", "");
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae menu music screen: defaults restored");
}

static void defaults_ask(void)
{
	struct ae_dialog_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Restore MENU MUSIC defaults?";
	spec.body = "GAME MENUS with SHUFFLE on, and an empty playlist.";
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "RESTORE DEFAULTS";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = menu.flow.density;
	spec.bounds = menu.flow.frame.rect;
	spec.picked = defaults_picked;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

static int songs_open(int playlist);

static void menu_accept(void)
{
	switch (menu.focus)
	{
	case ROW_MODE:
		mode_set(ae_menu_music_mode_step(mode_get(), 1));
		if (no_mcc())
			while (mode_get() == AE_MUSIC_MODE_ONE_SONG || mode_get() == AE_MUSIC_MODE_MY_PLAYLIST)
				mode_set(ae_menu_music_mode_step(mode_get(), 1));
		break;
	case ROW_SHUFFLE:
		shuffle_set(!shuffle_get());
		break;
	case ROW_SONG:
		songs_open(0);
		break;
	case ROW_PLAYLIST:
		songs_open(1);
		break;
	case ROW_VOLUME:
		volume_set(volume_get() >= 10 ? 0 : volume_get() + 1);
		break;
	case ROW_AGAIN:
		ae_mcc_redetect();
		ae_mcc_music_catalog_retry();
		platform_log("ae menu music screen: look again: %s", no_mcc() ? "not found" : "found");
		break;
	default:
		break;
	}
}

/* left or right on the focused row */
static void menu_change(int direction)
{
	int mode = mode_get();

	switch (menu.focus)
	{
	case ROW_MODE:
		do
			mode = ae_menu_music_mode_step(mode, direction);
		while (no_mcc() && (mode == AE_MUSIC_MODE_ONE_SONG || mode == AE_MUSIC_MODE_MY_PLAYLIST));
		mode_set(mode);
		/* (an empty playlist opens its editor) */
		if (mode == AE_MUSIC_MODE_MY_PLAYLIST)
		{
			struct ae_menu_music_playlist list;

			playlist_get(&list);
			if (!list.count)
				songs_open(1);
		}
		break;
	case ROW_SHUFFLE:
		shuffle_set(!shuffle_get());
		break;
	case ROW_VOLUME:
		volume_set(volume_get() + direction);
		break;
	default:
		break;
	}
}

static int menu_handle(struct ae_screen *instance, struct ae_event const *event)
{
	int next;

	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		next = row_step(menu.focus, event->action == AE_ACTION_UP ? -1 : 1);
		if (next != menu.focus)
		{
			menu.focus = (short)next;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		}
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (menu.focus == ROW_MODE || menu.focus == ROW_SHUFFLE || menu.focus == ROW_VOLUME)
		{
			if (menu.focus == ROW_SHUFFLE && ae_menu_music_shuffle_reason(mode_get(), no_mcc()))
				ae_sound_request(AE_SOUND_FAILURE, 0);
			else
				menu_change(event->action == AE_ACTION_LEFT ? -1 : 1);
		}
		return 1;
	case AE_ACTION_ACCEPT:
		menu_accept();
		return 1;
	case AE_ACTION_X:
		if (mode_get() != AE_MUSIC_MODE_ONE_SONG && mode_get() != AE_MUSIC_MODE_OFF)
			ae_mcc_music_next();
		return 1;
	case AE_ACTION_Y:
		defaults_ask();
		return 1;
	default:
		return 1;
	}
}

static void menu_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (ae_prompts_pointer(menu.flow.prompts, menu.flow.prompt_count, pointer, HIT_MENU_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			menu_handle(instance, &event);
		return;
	}
	menu.hover_chip = -1;
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit))
		return;
	if (hit.id == HIT_MENU_CHIPS && hit.index >= 0 && hit.index < AE_MUSIC_MODES)
	{
		menu.hover_chip = hit.index;
		if (pointer->left_clicks && !(no_mcc() && (hit.index == AE_MUSIC_MODE_ONE_SONG ||
			hit.index == AE_MUSIC_MODE_MY_PLAYLIST)))
		{
			menu.focus = ROW_MODE;
			mode_set(hit.index);
			if (hit.index == AE_MUSIC_MODE_MY_PLAYLIST)
			{
				struct ae_menu_music_playlist list;

				playlist_get(&list);
				if (!list.count)
					songs_open(1);
			}
		}
		return;
	}
	if (hit.id == HIT_MENU_ROWS && hit.index >= 0 && hit.index < ROWS && row_enabled(hit.index))
	{
		if (pointer->moved)
			menu.focus = hit.index;
		if (pointer->left_clicks)
		{
			menu.focus = hit.index;
			if (hit.index == ROW_VOLUME)
			{
				/* (the track: the click sets the volume where it lands) */
				struct ae_row probe;
				struct ae_row_layout layout;
				float width = menu.flow.frame.rect.width * AE_FLOW_ROWS_WIDTH, fraction;

				memset(&probe, 0, sizeof(probe));
				probe.label = "MUSIC VOLUME";
				probe.value = "10";
				probe.flags = AE_ROW_SLIDER;
				ae_row_layout(&menu.flow.density, width, &probe, &layout);
				fraction = (pointer->x - (ae_flow_left_x(&menu.flow) + layout.track_x)) / layout.track_width;
				if (hit.part == AE_PART_TRACK || (fraction >= 0.0f && fraction <= 1.0f))
					volume_set((int)(fraction * 10.0f + 0.5f));
			}
			else
				menu_accept();
		}
	}
}

static void menu_leave(struct ae_screen *instance)
{
	(void)instance;
	ae_mcc_music_preview(-1);
	platform_log("ae menu music screen: closed");
}

static struct ae_screen_class const menu_class =
{
	.name = "menu music", .leave = menu_leave, .handle = menu_handle, .draw = menu_draw, .pointer = menu_pointer,
};

/* ========== ONE SONG and MY PLAYLIST */

struct item { short header; short entry; short mood; };

static struct
{
	struct ae_flow_screen flow;
	int playlist;                      /* MY PLAYLIST; else ONE SONG */
	struct ae_list_view songs;
	struct ae_list_view order;
	struct item items[ITEMS_MAXIMUM];
	short item_count;
	short filter;                      /* -1: ALL, else a game of games[] */
	char games[GROUP_NAMES][24];
	short game_counts[GROUP_NAMES], game_count, total;
	short pane;
	short moving, moving_from;         /* the PLAY ORDER row held (-1 none) */
	short hover_chip;
	struct ae_menu_music_playlist list;
	int mode_before;
	short list_count;                  /* the playlist's length when the screen opened */
} pick;

static int entry_game(int entry)
{
	char const *short_name = ae_menu_music_game_short(ae_mcc_music_catalog_id(entry));
	int index;

	for (index = 0; index < pick.game_count; index++)
		if (!strcmp(pick.games[index], short_name))
			return index;
	return -1;
}

static void items_build(void)
{
	int count = ae_mcc_music_catalog_state() == AE_MCC_MUSIC_CATALOG_READY ? ae_mcc_music_catalog_count() : 1, entry;
	int game;
	short focus_entry = -1;

	pick.game_count = 0;
	pick.total = 0;
	memset(pick.game_counts, 0, sizeof(pick.game_counts));
	for (entry = 0; entry < count; entry++)
	{
		char const *short_name = ae_menu_music_game_short(ae_mcc_music_catalog_id(entry));
		int index;

		for (index = 0; index < pick.game_count; index++)
			if (!strcmp(pick.games[index], short_name))
				break;
		if (index == pick.game_count && pick.game_count < GROUP_NAMES)
			snprintf(pick.games[pick.game_count++], sizeof(pick.games[0]), "%s", short_name);
		if (index < pick.game_count)
			pick.game_counts[index]++;
		pick.total++;
	}
	if (pick.filter >= pick.game_count)
		pick.filter = -1;
	if (pick.list_count < 0)
		focus_entry = -1;
	pick.item_count = 0;
	/* (a game's songs by section: MENU, CUTSCENES, HEROIC, ACTION, TENSION, POIGNANT, AMBIENT, OTHER; a header names
	the game and the section; the entries are in the catalog's order within a section) */
	for (game = 0; game < pick.game_count; game++)
	{
		int mood;

		if (pick.filter >= 0 && game != pick.filter)
			continue;
		for (mood = 0; mood < AE_MUSIC_MOODS; mood++)
		{
			int header_at = -1;

			for (entry = 0; entry < count && pick.item_count + 2 < ITEMS_MAXIMUM; entry++)
			{
				if (entry_game(entry) != game || ae_menu_music_mood(ae_mcc_music_catalog_id(entry)) != mood)
					continue;
				if (header_at < 0)
				{
					header_at = pick.item_count;
					pick.items[pick.item_count].header = 1;
					pick.items[pick.item_count].entry = (short)entry;
					pick.items[pick.item_count].mood = (short)mood;
					pick.item_count++;
				}
				pick.items[pick.item_count].header = 0;
				pick.items[pick.item_count].entry = (short)entry;
				pick.items[pick.item_count].mood = (short)mood;
				pick.item_count++;
			}
		}
	}
	(void)focus_entry;
	ae_list_set_count(&pick.songs.list, pick.item_count);
}

static short focused_entry(void)
{
	short focus = pick.songs.list.focus;

	if (focus < 0 || focus >= pick.item_count || pick.items[focus].header)
		return -1;
	return pick.items[focus].entry;
}

static void songs_skip_headers(int direction)
{
	short focus = pick.songs.list.focus;

	if (focus < 0 || focus >= pick.item_count)
		return;
	while (focus >= 0 && focus < pick.item_count && pick.items[focus].header)
		focus = (short)(focus + (direction < 0 ? -1 : 1));
	if (focus < 0 || focus >= pick.item_count)
	{
		focus = pick.songs.list.focus;
		while (focus >= 0 && focus < pick.item_count && pick.items[focus].header)
			focus = (short)(focus + (direction < 0 ? 1 : -1));
	}
	if (focus >= 0 && focus < pick.item_count)
		ae_list_set_focus(&pick.songs.list, focus);
}

static short item_of_entry(int entry)
{
	short index;

	for (index = 0; index < pick.item_count; index++)
		if (!pick.items[index].header && pick.items[index].entry == entry)
			return index;
	return -1;
}

static void preview_toggle(int entry)
{
	struct ae_mcc_music_now now;

	if (entry <= 0)
		return;
	ae_mcc_music_now(&now);
	if (now.preview == entry)
	{
		ae_mcc_music_preview(-1);
		platform_log("ae menu music screen: preview stopped");
	}
	else
	{
		ae_mcc_music_preview(entry);
		platform_log("ae menu music screen: preview %s", ae_mcc_music_catalog_id(entry));
	}
}

/* a song's row: its check, name, badge, the play cell and the length */
static void song_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_density const *d = (struct ae_density const *)context;
	struct ae_flow_screen *flow = &pick.flow;
	struct ae_row row;
	struct ae_mcc_music_now now;
	char name[64], length[16];
	char const *id;
	unsigned int ink, muted;
	float center = y + height * 0.5f, text_x, right = x + width - pu(flow, 16.0f);
	int on_bar, entry, checked = 0, playing, active = !pick.playlist || pick.pane == PANE_SONGS;

	if (item < 0 || item >= pick.item_count)
		return;
	entry = pick.items[item].entry;
	id = ae_mcc_music_catalog_id(entry);
	if (pick.items[item].header)
	{
		struct ae_density head = *d;
		float h = ae_widget_group_pair(&head, x, y, width, ae_menu_music_game_full(id),
			ae_menu_music_mood_label(pick.items[item].mood));

		(void)h;
		return;
	}
	/* (the other pane has the focus: this one's focus is left behind, a wash and the stripe) */
	if (!active)
		flags &= ~(AE_ROW_FOCUSED | AE_ROW_ON_BAR | AE_ROW_UNDER_BAR | AE_ROW_BAR_BY_LIST);
	on_bar = (flags & AE_ROW_ON_BAR) != 0;
	memset(&row, 0, sizeof(row));
	row.label = "";
	row.flags = flags;
	ae_widget_row(d, x, y, width, &row, HIT_SONG_ITEMS, item);
	if (!active && item == pick.songs.list.focus)
	{
		ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_HOVER);
		ae_draw_rect(x, y, pu(flow, 5.0f), height, 0.0f, AE_COLOR_ACCENT);
	}
	ink = on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE;
	muted = on_bar ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED;
	if (pick.playlist)
	{
		float box = pu(flow, CHECK_BOX_U), bx = x + pu(flow, 20.0f), by = center - box * 0.5f;

		checked = ae_menu_music_playlist_find(&pick.list, id) >= 0;
		if (checked)
		{
			ae_draw_rect(bx, by, box, box, pu(flow, 2.0f), on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
			ae_draw_line(bx + box * 0.22f, by + box * 0.55f, bx + box * 0.42f, by + box * 0.75f, pu(flow, 2.0f),
				on_bar ? AE_COLOR_SELECTION : AE_COLOR_SELECTION_TEXT);
			ae_draw_line(bx + box * 0.42f, by + box * 0.75f, bx + box * 0.80f, by + box * 0.25f, pu(flow, 2.0f),
				on_bar ? AE_COLOR_SELECTION : AE_COLOR_SELECTION_TEXT);
		}
		else
			ae_draw_outline(bx, by, box, box, pu(flow, 2.0f), pu(flow, 2.0f), on_bar ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED);
	}
	else if (!strcmp(id, song_get()))
	{
		float cx = x + pu(flow, 24.0f);

		checked = 1;
		ae_draw_line(cx - pu(flow, 6.0f), center, cx - pu(flow, 2.0f), center + pu(flow, 5.0f), pu(flow, 3.0f),
			on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
		ae_draw_line(cx - pu(flow, 2.0f), center + pu(flow, 5.0f), cx + pu(flow, 6.0f), center - pu(flow, 5.0f),
			pu(flow, 3.0f), on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
	}
	(void)checked;
	ae_menu_music_name(id, name, sizeof(name));
	text_x = x + pu(flow, 48.0f);
	text_at(flow, AE_FONT_ROW, 22.0f, text_x, center, AE_ALIGN_LEFT, ink, name);
	if (ae_menu_music_is_menu_song(id))
	{
		float badge_size = pu(flow, 13.0f), badge_x = text_x + ae_draw_text_width(AE_FONT_ROW, pu(flow, 22.0f), name) +
			pu(flow, 12.0f), badge_width = ae_draw_text_width(AE_FONT_ROW, badge_size, "MENU") + pu(flow, 14.0f);

		ae_draw_outline(badge_x, center - pu(flow, 10.0f), badge_width, pu(flow, 20.0f), pu(flow, 2.0f), pu(flow, 1.0f), muted);
		ae_draw_text(AE_FONT_ROW, badge_size, badge_x + badge_width * 0.5f, ae_flow_cap_top(AE_FONT_ROW, badge_size, center),
			AE_ALIGN_CENTER, muted, "MENU");
	}
	if (entry == 0)
		snprintf(length, sizeof(length), "LOOP");
	else
		ae_menu_music_time_text(ae_mcc_music_catalog_seconds(entry), length, sizeof(length));
	text_at(flow, AE_FONT_BODY, 18.0f, right - pu(flow, 0.0f), center, AE_ALIGN_RIGHT, muted, length);
	/* (the play cell: on the focused or hovered row, a song the preview can play) */
	ae_mcc_music_now(&now);
	playing = now.preview == entry && entry > 0;
	if (entry > 0 && ((flags & (AE_ROW_FOCUSED | AE_ROW_HOVER)) || playing))
	{
		float cell = pu(flow, PLAY_CELL_U), cx = right - ae_draw_text_width(AE_FONT_BODY, pu(flow, 18.0f), length) -
			pu(flow, 12.0f) - cell;

		if (playing)
			playing_mark(flow, cx + (cell - playing_mark_width(flow)) * 0.5f, center, on_bar ? AE_COLOR_SELECTION_TEXT :
				AE_COLOR_ACCENT);
		else
		{
			float tri = pu(flow, 6.0f), mid = cx + cell * 0.5f;

			ae_draw_line(mid - tri * 0.7f, center - tri, mid + tri, center, pu(flow, 2.0f), on_bar ? AE_COLOR_SELECTION_TEXT :
				AE_COLOR_TEXT);
			ae_draw_line(mid + tri, center, mid - tri * 0.7f, center + tri, pu(flow, 2.0f), on_bar ? AE_COLOR_SELECTION_TEXT :
				AE_COLOR_TEXT);
			ae_draw_line(mid - tri * 0.7f, center - tri, mid - tri * 0.7f, center + tri, pu(flow, 2.0f),
				on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT);
		}
		ae_hit_add(cx, y, cell, height, HIT_SONG_PLAY, AE_PART_BASE, item);
	}
}

/* PLAY ORDER's row */
static void order_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_density const *d = (struct ae_density const *)context;
	struct ae_flow_screen *flow = &pick.flow;
	struct ae_row row;
	struct ae_mcc_music_now now;
	char name[64], number[12], length[16];
	char const *id, *game;
	unsigned int ink, muted;
	float center = y + height * 0.5f, right = x + width - pu(flow, 16.0f), cell = pu(flow, PLAY_CELL_U);
	int on_bar, entry, active = pick.pane == PANE_ORDER, found;

	if (item < 0 || item >= pick.list.count)
		return;
	id = pick.list.ids[item];
	entry = ae_mcc_music_catalog_find(id);
	found = entry >= 0;
	if (!active)
		flags &= ~(AE_ROW_FOCUSED | AE_ROW_ON_BAR | AE_ROW_UNDER_BAR | AE_ROW_BAR_BY_LIST);
	if (!found)
		flags |= AE_ROW_DISABLED;
	on_bar = (flags & AE_ROW_ON_BAR) != 0;
	memset(&row, 0, sizeof(row));
	row.label = "";
	row.flags = flags;
	ae_widget_row(d, x, y, width, &row, HIT_ORDER_ITEMS, item);
	if (!active && item == pick.order.list.focus)
	{
		ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_HOVER);
		ae_draw_rect(x, y, pu(flow, 5.0f), height, 0.0f, AE_COLOR_ACCENT);
	}
	ink = on_bar ? AE_COLOR_SELECTION_TEXT : (found ? AE_COLOR_TITLE : AE_COLOR_DISABLED_TEXT);
	muted = on_bar ? AE_COLOR_SELECTION_MUTED : (found ? AE_COLOR_MUTED : AE_COLOR_DISABLED_TEXT);
	ae_mcc_music_now(&now);
	if (found && entry > 0 && now.preview == entry)
		playing_mark(flow, x + pu(flow, 40.0f) - playing_mark_width(flow), center, on_bar ? AE_COLOR_SELECTION_TEXT :
			AE_COLOR_ACCENT);
	else
	{
		snprintf(number, sizeof(number), "%d", item + 1);
		text_at(flow, AE_FONT_ROW, 18.0f, x + pu(flow, 40.0f), center, AE_ALIGN_RIGHT, muted, number);
	}
	ae_menu_music_name(id, name, sizeof(name));
	text_at(flow, AE_FONT_ROW, 22.0f, x + pu(flow, 56.0f), center, AE_ALIGN_LEFT, ink, name);
	if (found && active && (flags & AE_ROW_FOCUSED))
		right -= 2.0f * cell + pu(flow, 12.0f);
	if (found)
		ae_menu_music_time_text(entry == 0 ? 0 : ae_mcc_music_catalog_seconds(entry), length, sizeof(length));
	else
		snprintf(length, sizeof(length), "NOT FOUND");
	if (found && entry == 0)
		snprintf(length, sizeof(length), "LOOP");
	{
		float length_width = ae_draw_text_width(AE_FONT_BODY, pu(flow, 18.0f), length);

		text_at(flow, AE_FONT_BODY, 18.0f, right, center, AE_ALIGN_RIGHT, found ? muted : AE_COLOR_WARNING, length);
		game = found ? ae_menu_music_game_short(id) : "";
		if (game[0])
			ae_draw_text_tracked(AE_FONT_BODY, pu(flow, 16.0f), 0.09f, right - length_width - pu(flow, 20.0f),
				ae_flow_cap_top(AE_FONT_BODY, pu(flow, 16.0f), center), AE_ALIGN_RIGHT, muted, game);
	}
	if (active && (flags & AE_ROW_FOCUSED) && found)
	{
		unsigned int cell_color = pick.moving >= 0 ? AE_COLOR_ACCENT : 0;
		float ax = x + width - pu(flow, 16.0f) - 2.0f * cell, ay = center - cell * 0.5f, ink_size = pu(flow, 11.0f);
		int which;

		for (which = 0; which < 2; which++)
		{
			float cx = ax + (float)which * cell + cell * 0.5f;

			if (cell_color)
				ae_draw_rect(ax + (float)which * cell, ay, cell, cell, pu(flow, 2.0f), cell_color);
			/* (a triangle: up, then down) */
			ae_draw_line(cx - ink_size * 0.5f, center + (which ? -1.0f : 1.0f) * ink_size * 0.3f, cx,
				center + (which ? 1.0f : -1.0f) * ink_size * 0.4f, pu(flow, 2.0f), AE_COLOR_SELECTION_TEXT);
			ae_draw_line(cx, center + (which ? 1.0f : -1.0f) * ink_size * 0.4f, cx + ink_size * 0.5f,
				center + (which ? -1.0f : 1.0f) * ink_size * 0.3f, pu(flow, 2.0f), AE_COLOR_SELECTION_TEXT);
			ae_hit_add(ax + (float)which * cell, ay, cell, cell, HIT_ORDER_CELLS, AE_PART_BASE, (short)(item * 2 + which));
		}
	}
}

static void density_rows(struct ae_density const *base, struct ae_metrics *metrics, struct ae_density *out)
{
	*metrics = *base->metrics;
	metrics->row = ITEM_U;
	*out = *base;
	out->metrics = metrics;
}

static void songs_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &pick.flow;
	struct ae_metrics metrics;
	struct ae_density rows;
	struct ae_chip chips[GROUP_NAMES + 1];
	struct ae_mcc_music_now now;
	float left, width, y, clip_bottom, right_x, right_width, chips_height;
	char help[200], expanded[320], lines_text[2][160], name[64], game[48];
	short focus_entry;
	int index, lines;

	(void)instance;
	ae_flow_full_density(flow);
	ae_mcc_music_catalog_request();
	/* (a catalog that was loading: the list built again once it is read) */
	{
		static int built_total = -1;

		if (built_total != (catalog_ready() ? ae_mcc_music_catalog_count() : 0) || pick.item_count == 0)
		{
			built_total = catalog_ready() ? ae_mcc_music_catalog_count() : 0;
			items_build();
			if (pick.songs.list.focus < 0 || pick.songs.list.focus >= pick.item_count)
				ae_list_set_focus(&pick.songs.list, 0);
			songs_skip_headers(1);
			if (!pick.playlist)
			{
				short chosen = item_of_entry(ae_mcc_music_catalog_find(song_get()));

				if (chosen >= 0)
					ae_list_set_focus(&pick.songs.list, chosen);
			}
		}
	}
	density_rows(&flow->density, &metrics, &rows);
	ae_flow_title(flow, pick.playlist ? "MY PLAYLIST" : "ONE SONG", "MENU MUSIC");
	left = ae_flow_left_x(flow);
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	/* the game chips with their counts */
	snprintf(game, sizeof(game), "ALL");
	chips[0].label = "ALL";
	chips[0].count = pick.total;
	chips[0].flags = (pick.filter < 0 ? AE_CHIP_ON : 0) | (pick.hover_chip == 0 ? AE_CHIP_HOVER : 0);
	for (index = 0; index < pick.game_count; index++)
	{
		chips[index + 1].label = pick.games[index];
		chips[index + 1].count = pick.game_counts[index];
		chips[index + 1].flags = (pick.filter == index ? AE_CHIP_ON : 0) | (pick.hover_chip == index + 1 ? AE_CHIP_HOVER : 0);
	}
	{
		struct ae_rect rects[GROUP_NAMES + 1];

		chips_height = ae_chips_layout(&flow->density, width, chips, (short)(pick.game_count + 1), rects);
		ae_widget_chips(&flow->density, left, y, width, chips, (short)(pick.game_count + 1), HIT_SONG_CHIPS);
	}
	y += (chips_height > 0.0f ? chips_height : pu(flow, 34.0f)) + pu(flow, 16.0f);
	clip_bottom = ae_frame_y(&flow->frame, 852.0f / 1080.0f);
	if (catalog_ready() || pick.item_count > 0)
		ae_widget_list(&rows, &pick.songs, left, y, width, clip_bottom - y, song_item, &rows, HIT_SONG_ITEMS);
	else
		text_at(flow, AE_FONT_BODY, 21.0f, left + pu(flow, 20.0f), y + pu(flow, 25.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			ae_mcc_music_catalog_state() == AE_MCC_MUSIC_CATALOG_NONE ? "No MCC songs found." :
			"Reading the songs of MCC...");

	/* the help line */
	{
		float rule_y = ae_frame_y(&flow->frame, 902.0f / 1080.0f), body = pu(flow, flow->density.metrics->body);
		float line_height = pu(flow, flow->density.metrics->body_line);

		focus_entry = focused_entry();
		ae_draw_rect(left, rule_y, width, pu(flow, 1.0f), 0.0f, AE_COLOR_RULE);
		if (focus_entry >= 0)
			ae_menu_music_name(ae_mcc_music_catalog_id(focus_entry), name, sizeof(name));
		else
			name[0] = 0;
		if (!pick.playlist)
			snprintf(help, sizeof(help), "{A} makes %s the menus' one song, over and over. {Y} plays its preview.", name[0] ?
				name : "it");
		else if (pick.pane == PANE_ORDER && pick.moving >= 0)
			snprintf(help, sizeof(help), "Up and down move the song. {A} puts it down, {B} puts it back.");
		else if (pick.pane == PANE_ORDER)
			snprintf(help, sizeof(help), "{A} picks the song up: up and down move it, {A} puts it down. {X} takes it off the "
				"list. With SHUFFLE on, the order is new each time.");
		else
			snprintf(help, sizeof(help), "{A} adds %s to the end of your list, or takes it off. {Y} plays a preview.",
				name[0] ? name : "it");
		lines = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(help, expanded, sizeof(expanded)), width, lines_text, 2);
		for (index = 0; index < lines; index++)
			ae_draw_text(AE_FONT_BODY, body, left, rule_y + pu(flow, 14.0f) + line_height * (float)index, AE_ALIGN_LEFT,
				AE_COLOR_TEXT, lines_text[index]);
	}

	/* the right column */
	right_x = ae_frame_x(&flow->frame, AE_FLOW_PANEL_X);
	right_width = ae_flow_right_x(flow) - right_x;
	y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	ae_mcc_music_now(&now);
	if (!pick.playlist)
	{
		float inset = pu(flow, CARD_INSET_U), inner = right_width - 2.0f * inset;
		int shown = now.preview >= 1 ? now.preview : focus_entry;

		card_box(flow, right_x, y, right_width, pu(flow, 184.0f));
		label_at(flow, right_x + inset, y + pu(flow, 22.0f), 18.0f, AE_COLOR_MUTED, "PREVIEW");
		if (now.preview >= 1)
			playing_mark(flow, right_x + inset + ae_draw_text_tracked_width(AE_FONT_BODY, pu(flow, 18.0f), 0.09f, "PREVIEW") +
				pu(flow, 12.0f), y + pu(flow, 30.0f), AE_COLOR_ACCENT);
		if (shown >= 0)
		{
			entry_names(shown, name, sizeof(name), game, sizeof(game));
			text_at(flow, AE_FONT_ROW, 30.0f, right_x + inset, y + pu(flow, 56.0f) + pu(flow, 12.0f), AE_ALIGN_LEFT,
				AE_COLOR_TITLE, name);
			text_at(flow, AE_FONT_BODY, 21.0f, right_x + inset, y + pu(flow, 91.0f) + pu(flow, 8.0f), AE_ALIGN_LEFT,
				AE_COLOR_TEXT, game);
			progress_line(flow, right_x + inset, y + pu(flow, 136.0f), inner, now.preview >= 1 ? now.preview_position : 0,
				ae_mcc_music_catalog_seconds(shown));
		}
		y += pu(flow, 184.0f + 24.0f);
		card_box(flow, right_x, y, right_width, pu(flow, 118.0f));
		label_at(flow, right_x + inset, y + pu(flow, 22.0f), 18.0f, AE_COLOR_MUTED, "IN THE MENUS NOW");
		entry_names(ae_mcc_music_catalog_find(song_get()) >= 0 ? ae_mcc_music_catalog_find(song_get()) : 0, name,
			sizeof(name), game, sizeof(game));
		text_at(flow, AE_FONT_ROW, 24.0f, right_x + inset, y + pu(flow, 56.0f) + pu(flow, 9.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE,
			name);
		text_at(flow, AE_FONT_BODY, 18.0f, right_x + inset + ae_draw_text_width(AE_FONT_ROW, pu(flow, 24.0f), name) +
			pu(flow, 14.0f), y + pu(flow, 56.0f) + pu(flow, 9.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, game);
		text_at(flow, AE_FONT_BODY, 18.0f, right_x + inset, y + pu(flow, 82.0f) + pu(flow, 8.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"Esc keeps this one.");
	}
	else
	{
		float head_h = pu(flow, 40.0f);
		char count_text[64];
		long seconds = playlist_seconds(&pick.list);

		/* (PLAY ORDER's head) */
		ae_draw_rect(right_x, y, right_width, head_h, 0.0f, AE_COLOR_PANEL);
		text_at(flow, AE_FONT_ROW, 18.0f, right_x + pu(flow, 16.0f), y + head_h * 0.5f, AE_ALIGN_LEFT, AE_COLOR_TITLE,
			"PLAY ORDER");
		playlist_summary(&pick.list, count_text, sizeof(count_text));
		(void)seconds;
		ae_draw_text_tracked(AE_FONT_BODY, pu(flow, 16.0f), 0.09f, right_x + right_width - pu(flow, 16.0f),
			ae_flow_cap_top(AE_FONT_BODY, pu(flow, 16.0f), y + head_h * 0.5f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, count_text);
		ae_list_set_count(&pick.order.list, pick.list.count);
		if (pick.list.count)
			ae_widget_list(&rows, &pick.order, right_x, y + head_h + pu(flow, 10.0f), right_width,
				pu(flow, 862.0f - 240.0f) - pu(flow, 6.0f), order_item, &rows, HIT_ORDER_ITEMS);
		else
			text_at(flow, AE_FONT_BODY, 21.0f, right_x + pu(flow, 20.0f), y + head_h + pu(flow, 40.0f), AE_ALIGN_LEFT,
				AE_COLOR_MUTED, "Empty. Add songs with A on the left.");
		/* (the PREVIEW card, LAST GAME's place) */
		{
			float card_y = ae_frame_y(&flow->frame, 862.0f / 1080.0f), inset = pu(flow, CARD_INSET_U);
			int shown = now.preview >= 1 ? now.preview : -1;
			char times[40], right_time[16], left_time[16];

			card_box(flow, right_x, card_y, right_width, pu(flow, 116.0f));
			label_at(flow, right_x + inset, card_y + pu(flow, 22.0f), 18.0f, AE_COLOR_MUTED, "PREVIEW");
			if (shown >= 0)
			{
				playing_mark(flow, right_x + inset + ae_draw_text_tracked_width(AE_FONT_BODY, pu(flow, 18.0f), 0.09f,
					"PREVIEW") + pu(flow, 12.0f), card_y + pu(flow, 30.0f), AE_COLOR_ACCENT);
				ae_menu_music_time_text(now.preview_position, left_time, sizeof(left_time));
				ae_menu_music_time_text(ae_mcc_music_catalog_seconds(shown), right_time, sizeof(right_time));
				snprintf(times, sizeof(times), "%s / %s", left_time, right_time);
				text_at(flow, AE_FONT_BODY, 18.0f, right_x + right_width - inset, card_y + pu(flow, 30.0f), AE_ALIGN_RIGHT,
					AE_COLOR_MUTED, times);
				entry_names(shown, name, sizeof(name), game, sizeof(game));
				text_at(flow, AE_FONT_ROW, 24.0f, right_x + inset, card_y + pu(flow, 56.0f) + pu(flow, 9.0f), AE_ALIGN_LEFT,
					AE_COLOR_TITLE, name);
				text_at(flow, AE_FONT_BODY, 18.0f, right_x + inset + ae_draw_text_width(AE_FONT_ROW, pu(flow, 24.0f), name) +
					pu(flow, 14.0f), card_y + pu(flow, 56.0f) + pu(flow, 9.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
					ae_menu_music_game_short(ae_mcc_music_catalog_id(shown)));
				{
					float track = pu(flow, 4.0f), track_width = right_width - 2.0f * inset;
					long length = ae_mcc_music_catalog_seconds(shown);
					float fraction = length > 0 ? (float)now.preview_position / (float)length : 0.0f;

					ae_draw_rect(right_x + inset, card_y + pu(flow, 90.0f), track_width, track, track * 0.5f,
						(AE_COLOR_RULE & 0xFFFFFF00u) | 0x40u);
					ae_draw_rect(right_x + inset, card_y + pu(flow, 90.0f), track_width * (fraction > 1.0f ? 1.0f : fraction),
						track, track * 0.5f, AE_COLOR_ACCENT);
				}
			}
			else
				text_at(flow, AE_FONT_BODY, 18.0f, right_x + inset, card_y + pu(flow, 70.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
					"Y plays a preview of the focused song.");
		}
	}

	/* the prompts */
	button_prompts_clear(flow);
	if (!pick.playlist)
	{
		prompt_add(flow, AE_BUTTON_A, "Choose", AE_ACTION_ACCEPT);
		prompt_add(flow, AE_BUTTON_Y, now.preview >= 1 ? "Stop preview" : "Preview", AE_ACTION_Y);
		flow->prompts[flow->prompt_count++] = (struct ae_prompt){ AE_BUTTON_LEFT_SHOULDER, "Game", NULL, AE_ACTION_TAB_NEXT, 1 };
		prompt_add(flow, AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	}
	else if (pick.moving >= 0)
	{
		prompt_add(flow, AE_BUTTON_A, "Put down", AE_ACTION_ACCEPT);
		prompt_add(flow, AE_BUTTON_B, "Cancel", AE_ACTION_BACK);
	}
	else if (pick.pane == PANE_ORDER)
	{
		prompt_add(flow, AE_BUTTON_A, "Move", AE_ACTION_ACCEPT);
		prompt_add(flow, AE_BUTTON_X, "Remove", AE_ACTION_X);
		prompt_add(flow, AE_BUTTON_Y, now.preview >= 1 ? "Stop preview" : "Preview", AE_ACTION_Y);
		flow->prompts[flow->prompt_count++] = (struct ae_prompt){ AE_BUTTON_DPAD_LEFT, "All songs", NULL, AE_ACTION_LEFT, 0 };
		prompt_add(flow, AE_BUTTON_B, "Done", AE_ACTION_BACK);
	}
	else
	{
		int in_list = focus_entry >= 0 && ae_menu_music_playlist_find(&pick.list, ae_mcc_music_catalog_id(focus_entry)) >= 0;

		prompt_add(flow, AE_BUTTON_A, in_list ? "Remove" : "Add", AE_ACTION_ACCEPT);
		prompt_add(flow, AE_BUTTON_Y, now.preview >= 1 ? "Stop preview" : "Preview", AE_ACTION_Y);
		flow->prompts[flow->prompt_count++] = (struct ae_prompt){ AE_BUTTON_LEFT_SHOULDER, "Game", NULL, AE_ACTION_TAB_NEXT, 1 };
		flow->prompts[flow->prompt_count++] = (struct ae_prompt){ AE_BUTTON_DPAD_RIGHT, "Play order", NULL, AE_ACTION_RIGHT, 0 };
		prompt_add(flow, AE_BUTTON_B, "Done", AE_ACTION_BACK);
	}
	ae_flow_draw_prompts(flow);
}

static void order_clamp(void)
{
	ae_list_set_count(&pick.order.list, pick.list.count);
}

static void song_toggle(int entry)
{
	char const *id = ae_mcc_music_catalog_id(entry);
	int at = ae_menu_music_playlist_find(&pick.list, id);

	if (at >= 0)
	{
		ae_menu_music_playlist_remove(&pick.list, at);
		platform_log("ae menu music screen: playlist remove %s", id);
	}
	else
	{
		int before = pick.list.count;

		if (ae_menu_music_playlist_add(&pick.list, id) < 0)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		platform_log("ae menu music screen: playlist add %s", id);
		/* (the first song of an empty list makes it the mode; no later one switches anything) */
		if (ae_menu_music_mode_after_add(mode_get(), before) != mode_get())
			mode_set(ae_menu_music_mode_after_add(mode_get(), before));
	}
	playlist_set(&pick.list);
	order_clamp();
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void order_remove(void)
{
	short focus = pick.order.list.focus;

	if (focus < 0 || focus >= pick.list.count)
		return;
	platform_log("ae menu music screen: playlist remove %s", pick.list.ids[focus]);
	ae_menu_music_playlist_remove(&pick.list, focus);
	playlist_set(&pick.list);
	order_clamp();
	if (pick.list.count == 0)
		pick.pane = PANE_SONGS;
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void order_move(int direction)
{
	short focus = pick.order.list.focus, moved;

	if (focus < 0 || focus >= pick.list.count)
		return;
	moved = (short)ae_menu_music_playlist_move(&pick.list, focus, direction);
	if (moved != focus)
	{
		ae_list_set_focus(&pick.order.list, moved);
		playlist_set(&pick.list);
		ae_sound_request(AE_SOUND_CURSOR, 0);
	}
}

/* the held song put back where it was, or as near as the list now goes: a song taken out while it was held leaves
the list shorter, and a move at its end moves nothing */
static void order_put_back(void)
{
	while (pick.order.list.focus != pick.moving_from)
	{
		short before = pick.order.list.focus;

		order_move(pick.order.list.focus > pick.moving_from ? -1 : 1);
		if (pick.order.list.focus == before)
			break;
	}
}

static void filter_step(int direction)
{
	int next = pick.filter + direction, total = pick.game_count + 1;

	/* (round the chips: ALL, then each game) */
	next = (pick.filter + 1 + direction + total) % total - 1;
	pick.filter = (short)next;
	items_build();
	ae_list_set_focus(&pick.songs.list, 0);
	songs_skip_headers(1);
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void songs_accept(void)
{
	short entry = focused_entry();

	if (pick.playlist && pick.pane == PANE_ORDER)
	{
		if (pick.moving >= 0)
		{
			platform_log("ae menu music screen: playlist move %s to %d", pick.list.ids[pick.order.list.focus],
				pick.order.list.focus + 1);
			pick.moving = -1;
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else if (pick.list.count > 0)
		{
			pick.moving = pick.order.list.focus;
			pick.moving_from = pick.order.list.focus;
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		return;
	}
	if (entry < 0)
		return;
	if (pick.playlist)
	{
		song_toggle(entry);
		return;
	}
	/* (ONE SONG: the song is chosen, the mode ONE SONG, and back) */
	config_write("audio.menu_music_song", ae_mcc_music_catalog_id(entry));
	mode_set(AE_MUSIC_MODE_ONE_SONG);
	platform_log("ae menu music screen: song %s", ae_mcc_music_catalog_id(entry));
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_pop();
}

static int songs_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	if (pick.playlist && pick.pane == PANE_ORDER)
	{
		switch (event->action)
		{
		case AE_ACTION_UP:
		case AE_ACTION_DOWN:
			if (pick.moving >= 0)
				order_move(event->action == AE_ACTION_UP ? -1 : 1);
			else if (ae_list_view_event(&pick.order, event))
				return 1;
			return 1;
		case AE_ACTION_BACK:
			if (pick.moving >= 0)
			{
				order_put_back();
				pick.moving = -1;
				return 1;
			}
			return 0;
		case AE_ACTION_ACCEPT:
			songs_accept();
			return 1;
		case AE_ACTION_X:
		case AE_ACTION_DELETE:
			if (pick.moving < 0)
				order_remove();
			return 1;
		case AE_ACTION_Y:
			if (pick.order.list.focus >= 0 && pick.order.list.focus < pick.list.count)
				preview_toggle(ae_mcc_music_catalog_find(pick.list.ids[pick.order.list.focus]));
			return 1;
		case AE_ACTION_LEFT:
			if (pick.moving < 0)
			{
				pick.pane = PANE_SONGS;
				ae_sound_request(AE_SOUND_CURSOR, 0);
			}
			return 1;
		default:
			if (pick.moving < 0)
				ae_list_view_event(&pick.order, event);
			return 1;
		}
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_ACCEPT:
		songs_accept();
		return 1;
	case AE_ACTION_Y:
		preview_toggle(focused_entry());
		return 1;
	case AE_ACTION_TAB_NEXT:
		filter_step(1);
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
		filter_step(-1);
		return 1;
	case AE_ACTION_RIGHT:
		if (pick.playlist && pick.list.count > 0)
		{
			/* (to PLAY ORDER, at the same song when it is in the list) */
			short entry = focused_entry();
			int at = entry >= 0 ? ae_menu_music_playlist_find(&pick.list, ae_mcc_music_catalog_id(entry)) : -1;

			ae_list_set_focus(&pick.order.list, (short)(at >= 0 ? at : pick.order.list.focus));
			pick.pane = PANE_ORDER;
			ae_sound_request(AE_SOUND_CURSOR, 0);
		}
		return 1;
	default:
		break;
	}
	if (ae_list_view_event(&pick.songs, event))
		songs_skip_headers(event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP ? -1 : 1);
	return 1;
}

static void songs_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short item;

	if (ae_prompts_pointer(pick.flow.prompts, pick.flow.prompt_count, pointer, HIT_SONG_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			songs_handle(instance, &event);
		return;
	}
	pick.hover_chip = -1;
	if (ae_hit_at(pointer->x, pointer->y, 0, &hit))
	{
		if (hit.id == HIT_SONG_CHIPS && hit.index >= 0)
		{
			pick.hover_chip = hit.index;
			if (pointer->left_clicks)
			{
				pick.filter = (short)(hit.index - 1);
				items_build();
				ae_list_set_focus(&pick.songs.list, 0);
				songs_skip_headers(1);
			}
			return;
		}
		if (hit.id == HIT_SONG_PLAY && pointer->left_clicks && hit.index >= 0 && hit.index < pick.item_count)
		{
			preview_toggle(pick.items[hit.index].entry);
			return;
		}
		if (hit.id == HIT_ORDER_CELLS && pointer->left_clicks)
		{
			ae_list_set_focus(&pick.order.list, (short)(hit.index / 2));
			pick.pane = PANE_ORDER;
			order_move(hit.index % 2 ? 1 : -1);
			return;
		}
		if (hit.id == HIT_ORDER_ITEMS && hit.index >= 0 && hit.index < pick.list.count && pointer->left_clicks)
		{
			pick.pane = PANE_ORDER;
			ae_list_set_focus(&pick.order.list, hit.index);
			songs_accept();
			return;
		}
	}
	if (pick.playlist)
	{
		item = ae_list_view_pointer(&pick.order, pointer, HIT_ORDER_ITEMS);
		(void)item;
	}
	item = ae_list_view_pointer(&pick.songs, pointer, HIT_SONG_ITEMS);
	if (item >= 0 && item < pick.item_count && !pick.items[item].header && pointer->left_clicks)
	{
		/* (a click in the songs lets go of a held song: it may be the one the click takes out) */
		pick.moving = -1;
		pick.pane = PANE_SONGS;
		ae_list_set_focus(&pick.songs.list, item);
		songs_accept();
		return;
	}
	if (pick.songs.list.focus >= 0 && pick.songs.list.focus < pick.item_count && pick.items[pick.songs.list.focus].header)
		songs_skip_headers(1);
}

static void songs_leave(struct ae_screen *instance)
{
	(void)instance;
	ae_mcc_music_preview(-1);
	if (pick.playlist)
	{
		int mode = ae_menu_music_mode_after_leave(mode_get(), pick.mode_before, pick.list.count);

		if (mode != mode_get())
			mode_set(mode);
	}
	platform_log("ae menu music screen: left %s", pick.playlist ? "MY PLAYLIST" : "ONE SONG");
}

static struct ae_screen_class const songs_class =
{
	.name = "menu music songs", .leave = songs_leave, .handle = songs_handle, .draw = songs_draw,
	.pointer = songs_pointer,
};

static int songs_open(int playlist)
{
	memset(&pick, 0, sizeof(pick));
	pick.playlist = playlist;
	pick.filter = -1;
	pick.moving = -1;
	pick.hover_chip = -1;
	pick.mode_before = mode_get();
	pick.flow.hit_rows = HIT_SONG_ITEMS;
	pick.flow.hit_prompts = HIT_SONG_PROMPTS;
	playlist_get(&pick.list);
	ae_mcc_music_catalog_request();
	ae_list_view_init(&pick.songs, 0, 12);
	ae_list_view_init(&pick.order, pick.list.count, 12);
	if (!ae_ui_push(&songs_class, AE_OWNER_PARENT, &pick))
		return 0;
	if (playlist)
		platform_log("ae menu music screen: opened MY PLAYLIST (%d songs)", pick.list.count);
	else
		platform_log("ae menu music screen: opened ONE SONG");
	return 1;
}

/* ========== opening */

int ae_screen_music_open(void)
{
	memset(&menu, 0, sizeof(menu));
	menu.hover_chip = -1;
	menu.flow.hit_rows = HIT_MENU_ROWS;
	menu.flow.hit_prompts = HIT_MENU_PROMPTS;
	ae_mcc_music_catalog_request();
	if (!ae_ui_push(&menu_class, AE_OWNER_PARENT, &menu))
		return 0;
	platform_log("ae menu music screen: open (mode %s, shuffle %d)", ae_menu_music_mode_label(mode_get()), shuffle_get());
	return 1;
}

int ae_screen_music_open_at(int which)
{
	if (!ae_screen_music_open())
		return 0;
	if (which == 1)
		return songs_open(0);
	if (which == 2)
		return songs_open(1);
	return 1;
}

void ae_screen_music_close(void)
{
	ae_ui_remove(&pick);
	ae_ui_remove(&menu);
}
