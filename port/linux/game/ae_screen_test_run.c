/*
AE_SCREEN_TEST_RUN.C

The TEST RUN screen (ae_screen_test_run.h). It is laid out as SETTINGS' group screens are (the title and the crumb, the
rule, one list, the help, the prompts: the same positions and sizes, as ae_screen_keys.c's), without the sections' rail.
Neither the draw nor the screen resets the view at its start: the screen's slide and fade (ae_hooks' before_draw) stay
theirs.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "interface/player_ui.h"
#include "../src/ae_draw.h"
#include "../src/build_identity.h"
#include "ae_glue_player_settings.h"
#include "ae_dev_mode.h"
#include "ae_hooks.h"
#include "ae_screen_test_run.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_test_run.h"
#include "ae_test_run_list.h"
#include "ae_test_run_results.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include <xtl.h>

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);

enum
{
	RUN_HIT = 0x7E80,
	HIT_LIST = RUN_HIT + 1, HIT_PROMPTS,
	MAX_LISTS = 32,
	MAX_ITEMS = MAX_LISTS + 2,
	PROMPTS = 3,
	PATH_BYTES = 1024
};

/* SETTINGS' group screens' frame (fractions of 1920 x 1080 at 100 %) and sizes (u): ae_screen_settings.c's */
#define TITLE_Y (70.0f / 1080.0f)
#define RULE_Y (156.0f / 1080.0f)
#define CONTENT_Y (186.0f / 1080.0f)
#define TITLE_U 60.0f
#define CRUMB_U 20.0f
#define CRUMB_TRACKING 0.09f
#define CRUMB_GAP_U 14.0f
#define CRUMB_DOT_U 5.0f
#define RULE_U 1.0f
#define HELP_LINES 2
#define HELP_GAP_U 28.0f
#define FOOTER_ABOVE_U 62.0f
#define FOOTER_ROOM_U 40.0f
#define HEADER_INSET_U 10.0f
#define COLOR_PASS 0x7BD88FFFu
#define COLOR_PASS_ON_BAR 0x1E7B3AFFu /* (the green on the selection bar) */

/* one list as the screen shows it */
struct entry
{
	char id[32], title[64], last[24], reason[96];
	short tests, pending;
	/* from the results: tests failed, skipped, never played (NEW), played before a retest (RETEST); records at all;
	the last run left a test it should have played (RESUME) */
	short failed, skipped, fresh, stale;
	int played, resume;
	int error; /* it cannot be read: greyed, with the reason */
};

/* the list's items: a header (TO TEST, DONE) or an entry's index; -1 and -2 are the headers */
static struct
{
	struct ae_density density;
	struct ae_frame frame;
	float logged_width, logged_scale;
	struct entry entries[MAX_LISTS];
	short count;
	short items[MAX_ITEMS], item_count;
	struct ae_list_view list;
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	char note[120]; /* a line over the help after a refusal ("" none) */
	short more;     /* lists past MAX_LISTS, not shown */
} screen;

/* the list a scan reads one file into (about 225 KB: static, not on the stack) */
static struct ae_test_run_list parsed;
static enum ae_test_run_state states[AE_TEST_RUN_TESTS];

static float pu(float spec_units)
{
	return spec_units * screen.frame.unit;
}

static float cap_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

static void upper(const char *text, char *out, int size)
{
	int at;

	for (at = 0; at < size - 1 && text[at]; at++)
		out[at] = text[at] >= 'a' && text[at] <= 'z' ? (char)(text[at] - 'a' + 'A') : text[at];
	out[at] = 0;
}

/* ---------- reading the lists */

/* "2026-10-25T14:03:11" as "OCT 25" ("" when there is none) */
static void date_shown(const char *stamp, char *out, size_t size)
{
	static const char *const months[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV",
		"DEC" };
	int month = 0, day = 0;

	if (stamp && sscanf(stamp, "%*d-%d-%d", &month, &day) == 2 && month >= 1 && month <= 12)
		snprintf(out, size, "%s %d", months[month - 1], day);
	else
		out[0] = 0;
}

/* what the results file says about the list: the time of its last record that parses, whether any test was played,
and whether the last run left a test unplayed (RESUME: ae_test_run_resume_index, the point a new run starts at) */
static void analyse(const char *text, char *stamp, size_t size, struct entry *entry)
{
	static short ever[AE_TEST_RUN_TESTS];
	struct ae_test_run_record record;
	char line[1024];
	const char *at;
	short test;

	stamp[0] = 0;
	memset(ever, 0, sizeof(ever));
	for (at = text; *at;)
	{
		size_t length = strcspn(at, "\n");

		if (length < sizeof(line))
		{
			memcpy(line, at, length);
			line[length] = 0;
			if (ae_test_run_record_parse(line, &record))
			{
				if (record.time[0])
					snprintf(stamp, size, "%s", record.time);
				entry->played = 1;
			}
		}
		at += length;
		if (*at)
			at++;
	}
	for (at = text; *at;)
	{
		size_t length = strcspn(at, "\n");

		if (length < sizeof(line))
		{
			memcpy(line, at, length);
			line[length] = 0;
			if (ae_test_run_record_parse(line, &record))
			{
				for (test = 0; test < parsed.tests; test++)
				{
					if (!strcmp(parsed.test[test].id, record.test))
						ever[test] = 1;
				}
			}
		}
		at += length;
		if (*at)
			at++;
	}
	entry->resume = entry->played && ae_test_run_resume_index(&parsed, text) >= 0;
	for (test = 0; test < parsed.tests; test++)
	{
		if (states[test] == AE_TEST_RUN_FAILED)
			entry->failed++;
		else if (states[test] == AE_TEST_RUN_SKIPPED)
			entry->skipped++;
		else if (states[test] == AE_TEST_RUN_TODO)
		{
			if (ever[test])
				entry->stale++;
			else
				entry->fresh++;
		}
	}
}

static void read_entry(struct entry *entry, const char *id)
{
	char path[PATH_BYTES], *text;

	memset(entry, 0, sizeof(*entry));
	snprintf(entry->id, sizeof(entry->id), "%s", id);
	snprintf(entry->title, sizeof(entry->title), "%s", id);
	if (!ae_test_run_list_path(id, path, sizeof(path)) || !ae_test_run_list_load(path, &parsed))
	{
		entry->error = 1;
		snprintf(entry->reason, sizeof(entry->reason), "CANNOT READ THE FILE");
		return;
	}
	if (parsed.title[0])
		snprintf(entry->title, sizeof(entry->title), "%s", parsed.title);
	entry->tests = parsed.tests;
	if (parsed.error[0] || !parsed.tests)
	{
		entry->error = 1;
		upper(parsed.error[0] ? parsed.error : "NO TESTS", entry->reason, sizeof(entry->reason));
	}
	memset(states, 0, sizeof(states));
	if (ae_test_run_results_path(parsed.id[0] ? parsed.id : id, path, sizeof(path)) &&
		(text = ae_test_run_read_results(path)) != NULL)
	{
		char stamp[24];

		ae_test_run_states(&parsed, text, states);
		analyse(text, stamp, sizeof(stamp), entry);
		date_shown(stamp, entry->last, sizeof(entry->last));
		free(text);
	}
	else
		entry->fresh = parsed.tests;
	entry->pending = ae_test_run_pending(&parsed, states);
}

static int by_name(const void *a, const void *b)
{
	return strcmp((const char *)a, (const char *)b);
}

enum { MAX_NAMES = 256, NAME_BYTES = 32 };

/* the lists in the data root's test-runs/, by id: the first MAX_LISTS read; how many more there are */
static void scan_into(struct entry *entries, short *count, short *more)
{
	static char names[MAX_NAMES][NAME_BYTES];
	WIN32_FIND_DATAA data;
	HANDLE find = FindFirstFileA("d:\\test-runs\\*.txt", &data);
	short found = 0, index;

	*count = *more = 0;
	if (find != INVALID_HANDLE_VALUE)
	{
		do
		{
			size_t length = strlen(data.cFileName);

			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || length <= 4 || length - 4 >= NAME_BYTES ||
				found >= MAX_NAMES)
				continue;
			snprintf(names[found], NAME_BYTES, "%.*s", (int)(length - 4), data.cFileName);
			if (ae_test_run_list_id_ok(names[found]))
				found++;
		}
		while (FindNextFileA(find, &data));
		CloseHandle(find);
	}
	/* (sorted before the cap: the same lists every time, not the directory's order) */
	qsort(names, (size_t)found, NAME_BYTES, by_name);
	for (index = 0; index < found && index < MAX_LISTS; index++)
		read_entry(&entries[(*count)++], names[index]);
	if (found > MAX_LISTS)
	{
		*more = (short)(found - MAX_LISTS);
		platform_log("ae test run screen: %d more lists past the first %d are not shown", (int)*more, MAX_LISTS);
	}
}

static void scan(void)
{
	scan_into(screen.entries, &screen.count, &screen.more);
}

/* SETTINGS' TEST RUN row's value ("3 LISTS TO TEST"): the lists read again at most every 2 s, never every frame */
const char *ae_screen_test_run_summary(void)
{
	static struct entry entries[MAX_LISTS];
	static char text[48];
	static unsigned long read_at;
	static int read_once;
	unsigned long now = system_milliseconds();

	if (!read_once || now - read_at > 2000)
	{
		short count, more, index, to_test = 0;

		scan_into(entries, &count, &more);
		for (index = 0; index < count; index++)
			to_test += !entries[index].error && entries[index].pending > 0;
		if (!count)
			snprintf(text, sizeof(text), "NO LISTS");
		else if (!to_test)
			snprintf(text, sizeof(text), "ALL PASSED");
		else
			snprintf(text, sizeof(text), "%d LIST%s TO TEST", (int)to_test, to_test == 1 ? "" : "S");
		read_once = 1;
		read_at = now;
	}
	return text;
}

/* the items: TO TEST's header and lists with tests left, then DONE's header and the rest */
static void build_items(void)
{
	short index, group;

	screen.item_count = 0;
	for (group = 0; group < 2; group++)
	{
		short before = screen.item_count;

		screen.items[screen.item_count++] = (short)(-1 - group);
		for (index = 0; index < screen.count; index++)
		{
			struct entry const *entry = &screen.entries[index];
			int to_test = !entry->error && entry->pending > 0;

			if (to_test == (group == 0))
				screen.items[screen.item_count++] = index;
		}
		if (screen.item_count == before + 1)
			screen.item_count = before; /* (no empty group) */
	}
}

static short first_row(void)
{
	short item;

	for (item = 0; item < screen.item_count; item++)
	{
		if (screen.items[item] >= 0)
			return item;
	}
	return -1;
}

/* a header holds no focus: past it in direction */
static void skip_headers(short direction)
{
	short focus = screen.list.list.focus;

	while (focus >= 0 && focus < screen.item_count && screen.items[focus] < 0)
		focus = (short)(focus + direction);
	if (focus < 0 || focus >= screen.item_count)
	{
		focus = (short)(screen.list.list.focus - direction);
		while (focus >= 0 && focus < screen.item_count && screen.items[focus] < 0)
			focus = (short)(focus - direction);
	}
	if (focus >= 0 && focus < screen.item_count)
		ae_list_set_focus(&screen.list.list, focus);
}

static struct entry const *focused_entry(void)
{
	short focus = screen.list.list.focus;

	return focus >= 0 && focus < screen.item_count && screen.items[focus] >= 0 ? &screen.entries[screen.items[focus]] :
		NULL;
}

/* ---------- input */

static void note(const char *text)
{
	snprintf(screen.note, sizeof(screen.note), "%s", text);
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

static void start(int all)
{
	struct entry const *entry = focused_entry();
	char id[32];

	screen.note[0] = 0;
	if (!entry)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (entry->error)
	{
		note("This list cannot be played until its file is fixed.");
		return;
	}
	if (!all && entry->pending <= 0)
	{
		note("Every test has passed. X plays them all again.");
		return;
	}
	snprintf(id, sizeof(id), "%s", entry->id);
	platform_log("ae test run screen: start %s (%s)", id, all ? "all" : "pending");
	if (!(all ? ae_test_run_start_all(id) : ae_test_run_start(id)))
	{
		note("The run did not start. The log says why.");
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	/* (the run's setup may have closed the screens already) */
	ae_ui_remove(&screen);
}

static int run_handle(struct ae_screen *instance, struct ae_event const *event)
{
	short direction = event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP ? -1 : 1;

	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_ACCEPT:
		start(0);
		return 1;
	case AE_ACTION_X:
		start(1);
		return 1;
	case AE_ACTION_BACK:
		return 0;
	default:
		break;
	}
	if (ae_list_view_event(&screen.list, event))
	{
		skip_headers(direction);
		screen.note[0] = 0;
	}
	return 1;
}

static void run_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	short item;

	if (ae_prompts_pointer(screen.prompts, screen.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			run_handle(instance, &event);
		return;
	}
	item = ae_list_view_pointer(&screen.list, pointer, HIT_LIST);
	if (item >= 0 && item < screen.item_count && screen.items[item] >= 0 && pointer->left_clicks)
	{
		screen.list.list.focus = item;
		start(0);
		return;
	}
	if (screen.list.list.focus >= 0 && screen.list.list.focus < screen.item_count &&
		screen.items[screen.list.list.focus] < 0)
		skip_headers(1);
}

/* ---------- drawing */

static void density_update(void)
{
	struct ae_layout layout;
	float scale = ae_settings_ui_scale();

	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, scale, &screen.density);
	ae_frame_compute(layout.width, scale, &screen.frame);
	if (layout.width != screen.logged_width || layout.scale != screen.logged_scale)
	{
		screen.logged_width = layout.width;
		screen.logged_scale = layout.scale;
		platform_log("ae test run screen: density FULL s=%.2f frame=%.0fx%.0f", screen.density.s,
			screen.frame.rect.width * layout.scale, screen.frame.rect.height * layout.scale);
	}
}

static void header_draw(void)
{
	float size = pu(TITLE_U), top_y = ae_frame_y(&screen.frame, TITLE_Y), middle, crumb = pu(CRUMB_U);
	float left = screen.frame.rect.x + screen.frame.margin, right = screen.frame.rect.x + screen.frame.rect.width -
		screen.frame.margin, x = right;
	char build[48];

	middle = top_y + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;
	ae_draw_text(AE_FONT_TITLE, size, left, top_y, AE_ALIGN_LEFT, AE_COLOR_TITLE, "TEST RUN");
	snprintf(build, sizeof(build), "BUILD %s", build_identity_commit());
	{
		char shown[48];

		upper(build, shown, sizeof(shown));
		x -= ae_draw_text_tracked(AE_FONT_BODY, crumb, CRUMB_TRACKING, x, cap_top(AE_FONT_BODY, crumb, middle),
			AE_ALIGN_RIGHT, AE_COLOR_MUTED, shown);
	}
	x -= pu(CRUMB_GAP_U);
	ae_widget_separator(&screen.density, x, middle, CRUMB_DOT_U, AE_COLOR_MUTED);
	x -= pu(CRUMB_GAP_U);
	ae_draw_text_tracked(AE_FONT_BODY, crumb, CRUMB_TRACKING, x, cap_top(AE_FONT_BODY, crumb, middle), AE_ALIGN_RIGHT,
		AE_COLOR_TEXT, "THIS PC");
	ae_draw_rect(left, ae_frame_y(&screen.frame, RULE_Y), right - left, pu(RULE_U), 0.0f, AE_COLOR_RULE);
}

/* a row's value (mockup 8): "ALL PASSED . OCT 24", "8 TO TEST" for a list never played, else what is left of it
("1 FAIL . 1 NEW . RESUME"), or why a list cannot be read */
static void value_text(struct entry const *entry, char *out, size_t size)
{
	static const char dot[] = " \xC2\xB7 ";
	char *at = out;
	size_t left = size;
	int parts = 0;

	if (entry->error)
	{
		snprintf(out, size, "LIST ERROR%s%s", dot, entry->reason);
		return;
	}
	if (entry->pending <= 0)
	{
		snprintf(out, size, "ALL PASSED%s%s", entry->last[0] ? dot : "", entry->last);
		return;
	}
	if (!entry->played)
	{
		snprintf(out, size, "%d TO TEST", (int)entry->pending);
		return;
	}
	{
		struct { short count; const char *name; } items[4] = { { entry->failed, "FAIL" }, { entry->skipped, "SKIP" },
			{ entry->fresh, "NEW" }, { entry->stale, "RETEST" } };
		int index, wrote;

		out[0] = 0;
		for (index = 0; index < 4; index++)
		{
			if (!items[index].count)
				continue;
			wrote = snprintf(at, left, "%s%d %s", parts++ ? dot : "", (int)items[index].count, items[index].name);
			if (wrote < 0 || (size_t)wrote >= left)
				return;
			at += wrote;
			left -= (size_t)wrote;
		}
	}
	if (entry->resume)
		snprintf(at, left, "%sRESUME", parts ? dot : "");
}

static void run_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_density const *density = context;
	short which = screen.items[item];
	struct ae_row row;
	char value[120];

	if (which < 0)
	{
		float head = density->metrics->group_height * density->unit;

		ae_widget_group(density, x, y + height - head, width, which == -1 ? "TO TEST" : "DONE");
		return;
	}
	{
		struct entry const *entry = &screen.entries[which];
		struct ae_row_layout layout;
		unsigned int color;
		char title[80];
		float size = ae_size_text(density), right;

		memset(&row, 0, sizeof(row));
		upper(entry->title, title, sizeof(title));
		row.label = title;
		row.flags = flags;
		value_text(entry, value, sizeof(value));
		if (entry->error)
		{
			row.flags |= AE_ROW_DISABLED;
			color = AE_COLOR_DISABLED_TEXT;
		}
		else if (entry->pending <= 0)
			color = (flags & AE_ROW_ON_BAR) ? COLOR_PASS_ON_BAR : COLOR_PASS;
		else
			color = (flags & AE_ROW_ON_BAR) ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT;
		/* (the row drawn alone; its value is placed as a status row's, and drawn in the list's own colours) */
		{
			struct ae_row probe = row;

			probe.value = value;
			probe.flags |= AE_ROW_STATUS;
			ae_row_layout(density, width, &probe, &layout);
		}
		ae_widget_row(density, x, y, width, &row, HIT_LIST, item);
		right = x + layout.value_x + layout.value_width;
		ae_draw_text(AE_FONT_ROW, size, right, y + layout.height * 0.5f - ae_draw_cap_height(AE_FONT_ROW, size) * 0.5f,
			AE_ALIGN_RIGHT, color, value);
	}
}

static void run_draw(struct ae_screen *instance)
{
	float left, right, top, bottom, help_y, list_height, body, line;
	struct entry const *entry;
	char help[200], expanded[320];

	(void)instance;
	density_update();
	header_draw();
	left = screen.frame.rect.x + screen.frame.margin;
	right = screen.frame.rect.x + screen.frame.rect.width - screen.frame.margin;
	top = ae_frame_y(&screen.frame, CONTENT_Y);
	bottom = screen.frame.rect.height - pu(FOOTER_ABOVE_U) - pu(FOOTER_ROOM_U);
	body = pu(screen.density.metrics->body);
	line = pu(screen.density.metrics->body_line);
	help_y = bottom - line * (float)HELP_LINES;
	list_height = help_y - pu(HELP_GAP_U) - top;
	ae_widget_list(&screen.density, &screen.list, left, top, right - left, list_height, run_item, &screen.density,
		HIT_LIST);
	/* the help: why a refusal, else the focused list's words */
	entry = focused_entry();
	ae_draw_rect(left, help_y - pu(HELP_GAP_U) * 0.5f, right - left, pu(RULE_U), 0.0f, AE_COLOR_RULE);
	if (!screen.count)
		snprintf(help, sizeof(help), "No test lists. Put a list in the test-runs folder of the data folder.");
	else if (screen.note[0])
		snprintf(help, sizeof(help), "%s", screen.note);
	else if (entry && entry->error)
		snprintf(help, sizeof(help), "%s: %s", entry->id, entry->reason);
	else if (entry)
		snprintf(help, sizeof(help), "%d test%s in %s. {A} plays only what still needs you: new tests, fails and "
			"notes. {X} plays every test again.", entry->tests, entry->tests == 1 ? "" : "s", entry->id);
	else
		help[0] = 0;
	if (screen.more && !screen.note[0] && strlen(help) + 40 < sizeof(help))
		snprintf(help + strlen(help), sizeof(help) - strlen(help), " (%d more lists not shown.)", (int)screen.more);
	if (help[0])
		ae_draw_text(AE_FONT_BODY, body, left, help_y, AE_ALIGN_LEFT, screen.note[0] ? AE_COLOR_ACCENT : AE_COLOR_TEXT,
			ae_help_expand(help, expanded, sizeof(expanded)));
	screen.prompt_count = 0;
	screen.prompts[screen.prompt_count++] = (struct ae_prompt){ AE_BUTTON_A, "Play what is left", NULL, AE_ACTION_ACCEPT, 0 };
	screen.prompts[screen.prompt_count++] = (struct ae_prompt){ AE_BUTTON_X, "Restart all", NULL, AE_ACTION_X, 0 };
	screen.prompts[screen.prompt_count++] = (struct ae_prompt){ AE_BUTTON_B, ae_string(AE_STR_BACK), NULL, AE_ACTION_BACK,
		0 };
	ae_widget_prompts(&screen.density, left, screen.frame.rect.height - pu(FOOTER_ABOVE_U), screen.prompts,
		screen.prompt_count, NULL, right, -1, HIT_PROMPTS);
}

static void run_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae test run screen: closed");
}

static struct ae_screen_class const run_class =
{
	.name = "test run", .leave = run_leave, .handle = run_handle, .draw = run_draw, .pointer = run_pointer,
};

int ae_screen_test_run_open(void)
{
	short first;

	if (!ae_dev_mode_on())
	{
		platform_log("ae test run screen: not opened, DEV MODE is off");
		return 0;
	}
	memset(&screen, 0, sizeof(screen));
	screen.logged_width = -1.0f;
	scan();
	build_items();
	ae_list_view_init(&screen.list, screen.item_count, 12);
	first = first_row();
	if (first >= 0)
		ae_list_set_focus(&screen.list.list, first);
	if (!ae_ui_push(&run_class, AE_OWNER_PARENT, &screen))
		return 0;
	platform_log("ae test run screen: open (%d lists)", (int)screen.count);
	{
		short index;
		char value[120];

		for (index = 0; index < screen.count; index++)
		{
			value_text(&screen.entries[index], value, sizeof(value));
			platform_log("ae test run screen: row %s: %s", screen.entries[index].id, value);
		}
	}
	return 1;
}
