/*
AE_SCREEN_THEATER.C

See ae_screen_theater.h. Rows on the AE flow (ae_pve_menus_ui.h's rows screen): one row for each recording (its map and day as the label, its
length as the value), the newest AE_FLOW_ROWS less two; on the right the focused recording's facts in the campaign screens' panel. The rows
are read when the screen opens and after a delete: the folder does not change while the menus are up.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/port_config.h"
#include "ae_pve_menus_ui.h"
#include "ae_replay_rules.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_theater.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_theater_rules.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "network_replay.h"

void platform_log(char const *format, ...);

enum { THEATER_HIT = 0x7AD0, ROWS_MAX = AE_FLOW_ROWS - 2 };

static struct ae_pvem_rows theater_screen;
/* the recordings listed and what each is (its header, read when listed) */
static char names[ROWS_MAX][AE_REPLAY_NAME_SIZE];
static struct ae_replay_header headers[ROWS_MAX];
static unsigned long long sizes[ROWS_MAX];
static int readable[ROWS_MAX];
static char refusals[ROWS_MAX][96];
static int listed, in_folder, staged_empty;
static unsigned long long folder_bytes;
static char labels[AE_FLOW_ROWS][64], values[AE_FLOW_ROWS][24], helps[AE_FLOW_ROWS][240];
static char record_word[24];
static char delete_title[96], delete_body[300];
static int delete_target = -1;

static void list_read(void)
{
	static char all[64][AE_REPLAY_NAME_SIZE];
	long count = network_replay_list(all, 64), each;

	if (staged_empty)
		count = 0;
	in_folder = (int)count;
	listed = count > ROWS_MAX ? ROWS_MAX : (int)count;
	folder_bytes = 0;
	for (each = 0; each < count; each++)
	{
		unsigned long long bytes = 0;
		char why[96];
		int read = network_replay_describe(all[each], each < listed ? &headers[each] : NULL, &bytes, why, sizeof(why));

		folder_bytes += bytes;
		if (each >= listed)
			continue;
		snprintf(names[each], sizeof(names[each]), "%s", all[each]);
		sizes[each] = bytes;
		readable[each] = read == AE_REPLAY_HEADER_OK;
		snprintf(refusals[each], sizeof(refusals[each]), "%s", read == AE_REPLAY_HEADER_OK ? "" : why);
	}
}

static int recording_on(void)
{
	return config_boolean("network.record_replays") ? 1 : 0;
}

static void build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int each;
	short n = 0;

	snprintf(record_word, sizeof(record_word), "Record: %s", recording_on() ? "ON" : "OFF");
	r->y_word = record_word;
	r->x_hidden = listed == 0;
	if (listed == 0)
	{
		/* (how a recording comes: RECORD's state, and Y to turn it on) */
		flow->rows[n++] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, "NO RECORDINGS YET", NULL,
			recording_on() ? "RECORD is ON. Play a network game: it shows here afterwards." :
			"RECORD is OFF. Press {Y} to turn it on, then play a network game: it shows here afterwards.", NULL, 0, 0, 1 };
		flow->count = n;
		return;
	}
	for (each = 0; each < listed; each++)
	{
		char length[16];

		ae_theater_title_text(names[each], labels[n], sizeof(labels[0]));
		if (readable[each] && headers[each].length_ticks)
		{
			ae_theater_time_text((long)(headers[each].length_ticks / AE_REPLAY_TICKS_PER_SECOND), length, sizeof(length));
			snprintf(values[n], sizeof(values[0]), "%s", length);
		}
		else
			snprintf(values[n], sizeof(values[0]), "%s", readable[each] ? "-" : "CANNOT PLAY");
		snprintf(helps[n], sizeof(helps[0]), "{A} watches this recording from a player's eyes or a free camera. {X} deletes it. {Y} turns recording on or off.");
		flow->rows[n] = (struct ae_flow_row){ .kind = AE_FLOW_ROW_OWN, .option = -1, .label = labels[n], .value = values[n],
			.help = helps[n], .status = 1 };
		n++;
	}
	flow->count = n;
}

static int focused_recording(struct ae_pvem_rows *r)
{
	int each = r->flow.focus;

	return listed > 0 && each >= 0 && each < listed ? each : -1;
}

/* A: the recording plays, its game loads and the menus close; one that will not play says why */
static void step(struct ae_pvem_rows *r, int row, int direction)
{
	int each = focused_recording(r);
	char why[96];

	(void)row;
	(void)direction;
	if (each < 0)
		return;
	if (!readable[each])
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		ae_flow_error(&r->flow, "THIS RECORDING DID NOT PLAY", refusals[each]);
		return;
	}
	if (!network_replay_play(names[each], why, sizeof(why)))
	{
		platform_log("ae theater: %s did not play: %s", names[each], why);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		ae_flow_error(&r->flow, "THIS RECORDING DID NOT PLAY", why);
		return;
	}
	platform_log("ae theater: playing %s", names[each]);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	/* (the menus go with it: its game loads at the next frame) */
	ae_pve_menus_rows_close(r);
	ae_screen_main_menu_close();
}

static const char *a_word(struct ae_pvem_rows *r, int row)
{
	(void)r;
	(void)row;
	return "Watch";
}

/* Y: recording on or off, from the next game */
static void record_toggle(struct ae_pvem_rows *r)
{
	(void)r;
	if (config_write_boolean("network.record_replays", !recording_on()))
	{
		platform_log("ae theater: recording %s", recording_on() ? "on" : "off");
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
	else
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* ---------- delete */

static void delete_picked(short choice, void *context)
{
	struct ae_pvem_rows *r = context;

	if (choice != 1 || delete_target < 0 || delete_target >= listed)
		return;
	if (network_replay_delete(names[delete_target]))
	{
		platform_log("ae theater: %s deleted", names[delete_target]);
		ae_sound_request(AE_SOUND_BACK, 0);
	}
	else
		ae_sound_request(AE_SOUND_FAILURE, 0);
	list_read();
	if (r->flow.focus >= r->flow.count)
		r->flow.focus = (short)(r->flow.count > 0 ? r->flow.count - 1 : 0);
	delete_target = -1;
}

static void delete_open(struct ae_pvem_rows *r)
{
	struct ae_dialog_spec spec;
	char title[64];
	int each = focused_recording(r);

	if (each < 0)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	delete_target = each;
	ae_theater_title_text(names[each], title, sizeof(title));
	snprintf(delete_title, sizeof(delete_title), "DELETE THIS RECORDING?");
	snprintf(delete_body, sizeof(delete_body), "%s is deleted from this PC, with its KEPT mark. This cannot be undone.", title);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = delete_title;
	spec.body = delete_body;
	spec.choices[0] = "CANCEL";
	spec.choices[1] = "DELETE";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.destructive_choices = 1u << 1;
	spec.density = r->flow.density;
	spec.bounds = r->flow.frame.rect;
	spec.picked = delete_picked;
	spec.context = r;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

static void delete_asked(struct ae_pvem_rows *r)
{
	r->dialog_pending = 1;
}

/* ---------- the right-hand panel */

static void fact(struct ae_flow_screen const *flow, float x, float width, float y, const char *label, const char *value)
{
	float u = ae_flow_u(flow, 1.0f);

	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, flow->frame.rect.y + y * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width, flow->frame.rect.y + y * u, AE_ALIGN_RIGHT, AE_COLOR_TEXT, value);
}

static void upper(char *text)
{
	for (; *text; text++)
		if (*text >= 'a' && *text <= 'z')
			*text = (char)(*text - 32);
}

static void side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	int each = focused_recording(r);
	char text[160], when[48], map[64], length[16];
	char const *base;

	ae_pve_menus_ui_plate(flow, x, width, 176.0f, 470.0f);
	snprintf(text, sizeof(text), "%d RECORDING%s \xE2\x80\xA2 %llu.%llu MB", in_folder, in_folder == 1 ? "" : "S",
		folder_bytes / (1024ULL * 1024ULL), folder_bytes % (1024ULL * 1024ULL) * 10ULL / (1024ULL * 1024ULL));
	if (each < 0)
	{
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 28.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, "THEATER");
		ae_pve_menus_ui_paragraph(flow, 16.0f, 22.0f, x, top + 262.0f * u, width, 4, AE_COLOR_TEXT,
			"The network games you play are recorded from their first tick when RECORD is on, from the next one on. Watch them back "
			"through any player's eyes or a free camera.");
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 15.0f, x, top + 470.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
		return;
	}
	/* (the map past its folders, in capitals) */
	base = strrchr(headers[each].map, '\\');
	snprintf(map, sizeof(map), "%s", readable[each] ? (base ? base + 1 : headers[each].map) : "UNREADABLE");
	upper(map);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, map);
	when[0] = 0;
	if (readable[each] && headers[each].time)
	{
		time_t stamp = (time_t)headers[each].time;
		struct tm *local = localtime(&stamp);

		if (local)
			strftime(when, sizeof(when), "%Y-%m-%d %H:%M", local);
	}
	snprintf(text, sizeof(text), "%s%s%s", when, when[0] && readable[each] && headers[each].variant[0] ? " \xE2\x80\xA2 " : "",
		readable[each] ? headers[each].variant : "");
	upper(text);
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 15.0f, x, top + 250.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	ae_draw_rect(x, top + 272.0f * u, width, u, 0.0f, AE_COLOR_RULE);
	if (!readable[each])
	{
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x, top + 306.0f * u, AE_ALIGN_LEFT, AE_COLOR_DESTRUCTIVE_INK, "CANNOT PLAY");
		ae_pve_menus_ui_paragraph(flow, 16.0f, 22.0f, x, top + 340.0f * u, width, 3, AE_COLOR_TEXT, refusals[each]);
	}
	else
	{
		if (headers[each].length_ticks)
			ae_theater_time_text((long)(headers[each].length_ticks / AE_REPLAY_TICKS_PER_SECOND), length, sizeof(length));
		else
			snprintf(length, sizeof(length), "-");
		fact(flow, x, width, 300.0f, "LENGTH", length);
		snprintf(text, sizeof(text), "%llu.%llu MB", sizes[each] / (1024ULL * 1024ULL),
			sizes[each] % (1024ULL * 1024ULL) * 10ULL / (1024ULL * 1024ULL));
		fact(flow, x, width, 338.0f, "SIZE", text);
		snprintf(text, sizeof(text), "%s", headers[each].player[0] ? headers[each].player : headers[each].server[0] ? headers[each].server : "-");
		upper(text);
		fact(flow, x, width, 376.0f, "RECORDED BY", text);
		snprintf(text, sizeof(text), "%s", headers[each].flags & AE_REPLAY_FLAG_CLIENT ? "A CLIENT'S" : "THE HOST'S");
		fact(flow, x, width, 414.0f, "RECORDING", text);
	}
	ae_pve_menus_ui_paragraph(flow, 15.0f, 20.0f, x, top + 462.0f * u, width, 2, AE_COLOR_MUTED,
		recording_on() ? "RECORD is ON: the network games you play are recorded, from the next one on." :
		"RECORD is OFF. Turn it on with Y to record the network games you play, from the next one on.");
}

static int open_over(void)
{
	if (ae_ui_holds(&theater_screen))
		ae_pve_menus_rows_close(&theater_screen);
	memset(&theater_screen, 0, sizeof(theater_screen));
	theater_screen.name = "theater";
	theater_screen.title = "THEATER";
	/* (a testing feature: the crumb starts with the mark) */
	snprintf(theater_screen.crumb, sizeof(theater_screen.crumb), AE_TESTING_CRUMB "WATCH A RECORDED GAME");
	theater_screen.build = build;
	theater_screen.step = step;
	theater_screen.a_word = a_word;
	theater_screen.x_action = delete_asked;
	theater_screen.x_word = "Delete";
	theater_screen.y_action = record_toggle;
	theater_screen.y_word = record_word;
	theater_screen.side = side;
	theater_screen.dialog_open = delete_open;
	theater_screen.hit_base = THEATER_HIT;
	theater_screen.backing = 1;
	list_read();
	return ae_pve_menus_rows_open(&theater_screen);
}

int ae_screen_theater_open(void)
{
	staged_empty = 0;
	return open_over();
}

int ae_screen_theater_open_staged(int empty, int delete_dialog)
{
	staged_empty = empty != 0;
	if (!open_over())
		return 0;
	theater_screen.dialog_pending = delete_dialog && listed > 0;
	return 1;
}

void ae_screen_theater_close(void)
{
	if (ae_ui_holds(&theater_screen))
		ae_pve_menus_rows_close(&theater_screen);
}
