/*
AE_SCREEN_MOTD.C

See ae_screen_motd.h. The panel: x 96 to 1824, y 190 to 978 of the frame; the well 456 u wide with 36 u padding; the
message 56 u right of it, its paragraphs 22 u on a 34 u line (1000 u at most), bullets an 8 u accent square and 46 u
apart; a 4 u scroll track only when the text is taller than the panel. Files: CHANGELOG.md beside the game (read once),
ae_motd.txt and ae_motd_cache.txt in the save root's private folder.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_motd_fetch.h"
#include "../src/ae_private_files.h"
#include "../src/ae_shipped_files.h"
#include "ae_changelog_rules.h"
#include "ae_motd_rules.h"
#include "ae_motion.h"
#include "ae_screen_motd.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
const char *build_identity_version(void);

enum
{
	MOTD_HIT = 0x76C0,
	HIT_ROWS = MOTD_HIT + 1, HIT_PROMPTS, HIT_RELEASES
};
enum { KIND_MOTD, KIND_WHATS_NEW };
enum { BLOCK_PARAGRAPH, BLOCK_BULLET, BLOCK_SECTION };

#define STATE_FILE "ae_motd.txt"
#define CACHE_FILE "ae_motd_cache.txt"
#define LAUNCH_WAIT_MS 4000ul
#define BLOCKS_MAXIMUM 96
#define RELEASES_MAXIMUM 64

#define PANEL_LEFT (96.0f / 1920.0f)
#define PANEL_RIGHT (1824.0f / 1920.0f)
#define PANEL_TOP (190.0f / 1080.0f)
#define PANEL_BOTTOM (978.0f / 1080.0f)
#define WELL_U 456.0f
#define WELL_PAD_U 36.0f
#define TEXT_GAP_U 56.0f
#define TEXT_WIDTH_U 1000.0f
#define LINE_U 34.0f
#define BULLET_GAP_U 46.0f

struct block { short kind; char text[1200]; };

static struct
{
	struct ae_flow_screen flow;
	int kind, by_hand, reason, source;
	struct ae_motd motd;
	char dateline[96], headline[96];
	char day[8], month[32], weekday[48], number[16], version_fact[48];
	struct block blocks[BLOCKS_MAXIMUM];
	int block_count;
	float scroll, content_height, view_height;
	/* WHAT'S NEW: the releases, the one shown */
	struct ae_release_entry releases[RELEASES_MAXIMUM];
	int release_count, release;
} screen;

static struct ae_screen_class const motd_class;

/* ---------- files */

static char changelog[327680];
static int changelog_read_done;

static const char *changelog_text(void)
{
	if (changelog_read_done)
		return changelog[0] ? changelog : NULL;
	changelog_read_done = 1;
	if (ae_shipped_file_read("CHANGELOG.md", changelog, (long)sizeof(changelog)) <= 0)
		changelog[0] = 0;
	return changelog[0] ? changelog : NULL;
}

static struct ae_motd_state state;
static int state_read_done;

static void state_read(void)
{
	static char text[1024];

	if (state_read_done)
		return;
	state_read_done = 1;
	if (ae_private_file_read(STATE_FILE, text, (long)sizeof(text)) < 0)
		text[0] = 0;
	ae_motd_state_read(text, &state);
}

static void state_write(void)
{
	char text[1024];
	int length = ae_motd_state_write(&state, text, (int)sizeof(text));

	if (length > 0)
		ae_private_file_write(STATE_FILE, text, length);
}

static void today(char *text, int size)
{
	time_t now = time(NULL);
	struct tm *local = localtime(&now);

	if (local)
		strftime(text, (size_t)size, "%Y-%m-%d", local);
	else
		snprintf(text, (size_t)size, "-");
}

/* the saved message: its signature line, then its body */
static int cache_read(struct ae_motd *out)
{
	static char text[4096];
	char *newline;

	if (ae_private_file_read(CACHE_FILE, text, (long)sizeof(text)) <= 0)
		return 0;
	newline = strchr(text, '\n');
	if (!newline)
		return 0;
	*newline = 0;
	return ae_motd_fetch_check_saved(newline + 1, text, out) == AE_MOTD_OK;
}

static void cache_write(const char *body, const char *signature)
{
	char text[4096];
	int length = snprintf(text, sizeof(text), "%s\n%s", signature, body);

	if (length > 0 && length < (int)sizeof(text))
		ae_private_file_write(CACHE_FILE, text, length);
}

/* ---------- the text's blocks */

static void blocks_from_message(const char *text)
{
	static struct ae_motd_block parsed[BLOCKS_MAXIMUM];
	int count = ae_motd_parse_text(text, parsed, BLOCKS_MAXIMUM), index;

	screen.block_count = 0;
	for (index = 0; index < count; index++)
	{
		screen.blocks[screen.block_count].kind = parsed[index].bullet ? BLOCK_BULLET : BLOCK_PARAGRAPH;
		snprintf(screen.blocks[screen.block_count].text, sizeof(screen.blocks[0].text), "%s", parsed[index].text);
		screen.block_count++;
	}
}

/* a release's notes: "### Added" a section's label, "- " a bullet (its wrapped lines joined), other lines paragraphs */
static void blocks_from_release(const char *text, struct ae_release_entry const *release)
{
	const char *at = text + release->start, *end = text + release->end;

	screen.block_count = 0;
	while (at < end && screen.block_count < BLOCKS_MAXIMUM)
	{
		const char *line_end = memchr(at, '\n', (size_t)(end - at));
		size_t length = line_end ? (size_t)(line_end - at) : (size_t)(end - at);
		const char *words = at;
		struct block *block = &screen.blocks[screen.block_count];

		while (length && (words[0] == ' ' || words[0] == '\t'))
		{
			words++;
			length--;
		}
		if (length && words[length - 1] == '\r')
			length--;
		if (length > 4 && !strncmp(words, "### ", 4))
		{
			block->kind = BLOCK_SECTION;
			snprintf(block->text, sizeof(block->text), "%.*s", (int)(length - 4), words + 4);
			screen.block_count++;
		}
		else if (length > 2 && !strncmp(words, "- ", 2))
		{
			block->kind = BLOCK_BULLET;
			snprintf(block->text, sizeof(block->text), "%.*s", (int)(length - 2), words + 2);
			screen.block_count++;
		}
		else if (length && screen.block_count && screen.blocks[screen.block_count - 1].kind != BLOCK_SECTION &&
			(at != words || screen.blocks[screen.block_count - 1].kind == BLOCK_PARAGRAPH) && at > text + release->start &&
			at[-2] != '\n')
		{
			/* (a wrapped line: a bullet's (indented in the file) or a paragraph's (the line before it not blank)) */
			size_t had = strlen(screen.blocks[screen.block_count - 1].text);

			snprintf(screen.blocks[screen.block_count - 1].text + had, sizeof(block->text) - had, " %.*s", (int)length,
				words);
		}
		else if (length)
		{
			block->kind = BLOCK_PARAGRAPH;
			snprintf(block->text, sizeof(block->text), "%.*s", (int)length, words);
			screen.block_count++;
		}
		at += (line_end ? (size_t)(line_end - at) + 1 : (size_t)(end - at));
	}
	/* (the file's markdown marks dropped: `code` reads as its words; a section's label in capitals) */
	{
		int index;

		for (index = 0; index < screen.block_count; index++)
		{
			char *from = screen.blocks[index].text, *to = from;

			for (; *from; from++)
				if (*from != '`')
					*to++ = screen.blocks[index].kind == BLOCK_SECTION && *from >= 'a' && *from <= 'z' ?
						(char)(*from - 32) : *from;
			*to = 0;
		}
	}
}

static void well_from_time(unsigned long when)
{
	time_t t = (time_t)when;
	struct tm *local = localtime(&t);

	if (!local)
		return;
	strftime(screen.day, sizeof(screen.day), "%d", local);
	strftime(screen.month, sizeof(screen.month), "%B %Y", local);
	strftime(screen.weekday, sizeof(screen.weekday), "%A \xE2\x80\xA2 %H:%M", local);
	{
		char *c;

		for (c = screen.month; *c; c++)
			if (*c >= 'a' && *c <= 'z')
				*c = (char)(*c - 32);
	}
}

static void well_from_date(const char *date)
{
	struct tm day;

	memset(&day, 0, sizeof(day));
	if (sscanf(date, "%d-%d-%d", &day.tm_year, &day.tm_mon, &day.tm_mday) == 3)
	{
		day.tm_year -= 1900;
		day.tm_mon -= 1;
		day.tm_hour = 12;
		well_from_time((unsigned long)mktime(&day));
		strftime(screen.weekday, sizeof(screen.weekday), "%A", &day);
	}
}

/* the installed release's entry (else the newest) */
static int installed_release(void)
{
	const char *version = build_identity_version();
	int index;

	for (index = 0; index < screen.release_count; index++)
		if (!ae_motd_version_compare(screen.releases[index].version, version))
			return index;
	return screen.release_count ? 0 : -1;
}

static void releases_read(void)
{
	const char *text = changelog_text();

	screen.release_count = text ? ae_changelog_releases(text, screen.releases, RELEASES_MAXIMUM) : 0;
}

/* the screen's message: the master's (fresh), the saved one, else the installed release's notes */
static void content_set(int source)
{
	char const *version = build_identity_version();

	screen.source = source;
	screen.scroll = 0.0f;
	snprintf(screen.version_fact, sizeof(screen.version_fact), "%s%s", version,
		screen.reason == AE_MOTD_UPDATE ? " \xE2\x80\xA2 NEW" : "");
	if (source != AE_MOTD_FROM_NOTES)
	{
		snprintf(screen.headline, sizeof(screen.headline), "%s", screen.motd.title);
		snprintf(screen.number, sizeof(screen.number), "#%ld", screen.motd.rev);
		if (source == AE_MOTD_FROM_CACHE)
			snprintf(screen.dateline, sizeof(screen.dateline), "SAVED BRIEFING \xE2\x80\xA2 OFFLINE");
		else
			snprintf(screen.dateline, sizeof(screen.dateline), "BRIEFING");
		well_from_time(screen.motd.posted ? screen.motd.posted : (unsigned long)time(NULL));
		blocks_from_message(screen.motd.text);
		return;
	}
	releases_read();
	screen.release = installed_release();
	snprintf(screen.number, sizeof(screen.number), "\xE2\x80\x94");
	if (screen.release < 0)
	{
		snprintf(screen.dateline, sizeof(screen.dateline), "WHAT'S NEW");
		snprintf(screen.headline, sizeof(screen.headline), "ARENA EVOLVED %s", version);
		screen.block_count = 1;
		screen.blocks[0].kind = BLOCK_PARAGRAPH;
		snprintf(screen.blocks[0].text, sizeof(screen.blocks[0].text), "Release notes are not in this install.");
		well_from_time((unsigned long)time(NULL));
		return;
	}
	snprintf(screen.dateline, sizeof(screen.dateline), "WHAT'S NEW \xE2\x80\xA2 RELEASED %s",
		screen.releases[screen.release].date);
	snprintf(screen.headline, sizeof(screen.headline), "ARENA EVOLVED %s", screen.releases[screen.release].version);
	well_from_date(screen.releases[screen.release].date);
	blocks_from_release(changelog_text(), &screen.releases[screen.release]);
}

/* ---------- drawing */

static float blocks_draw(float x, float y, float width, float clip_top, float clip_bottom, int draw)
{
	struct ae_flow_screen const *flow = &screen.flow;
	float size = ae_size(&flow->density, 22.0f, 12.0f), line = ae_flow_u(flow, LINE_U), at = y;
	float bullet = ae_flow_u(flow, 8.0f), indent = ae_flow_u(flow, 24.0f);
	int index;

	for (index = 0; index < screen.block_count; index++)
	{
		struct block const *block = &screen.blocks[index];
		char lines[16][160];
		int count, row;
		float left = block->kind == BLOCK_BULLET ? x + indent : x;

		if (block->kind == BLOCK_SECTION)
		{
			at += ae_flow_u(flow, 12.0f);
			if (draw && at >= clip_top && at + line <= clip_bottom)
				ae_draw_text_tracked(AE_FONT_BODY, ae_size(&flow->density, 16.0f, 12.0f), 0.12f, x, at, AE_ALIGN_LEFT,
					AE_COLOR_MUTED, block->text);
			at += line;
			continue;
		}
		count = ae_wrap_text(AE_FONT_BODY, size, block->text, width - (left - x), lines, 16);
		for (row = 0; row < count; row++)
		{
			if (draw && at >= clip_top && at + line <= clip_bottom)
			{
				if (block->kind == BLOCK_BULLET && row == 0)
					ae_draw_rect(x, ae_flow_cap_top(AE_FONT_BODY, size, at + size * 0.5f) + ae_draw_cap_height(AE_FONT_BODY,
						size) * 0.5f - bullet * 0.5f, bullet, bullet, 0.0f, AE_COLOR_ACCENT);
				ae_draw_text(AE_FONT_BODY, size, left, at, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[row]);
			}
			at += line;
		}
		at += block->kind == BLOCK_BULLET ? ae_flow_u(flow, BULLET_GAP_U) - line : ae_flow_u(flow, 12.0f);
	}
	return at - y;
}

static void well_draw(float x, float y, float width, float bottom)
{
	struct ae_flow_screen const *flow = &screen.flow;
	struct ae_density const *d = &flow->density;
	float pad = ae_flow_u(flow, WELL_PAD_U), left = x + pad, inner = width - 2.0f * pad, at = y + pad;
	float minor = ae_size(d, 16.0f, 12.0f), fact = ae_size(d, 18.0f, 12.0f);
	int row;

	ae_draw_rect(x, y, width, bottom - y, 0.0f, AE_COLOR_WELL);
	if (screen.kind == KIND_MOTD && screen.source != AE_MOTD_FROM_NOTES && screen.motd.rev != state.closed_rev)
	{
		ae_draw_rect(left, at + ae_flow_u(flow, 3.0f), ae_flow_u(flow, 8.0f), ae_flow_u(flow, 8.0f), 0.0f, AE_COLOR_ACCENT);
		ae_draw_text_tracked(AE_FONT_BODY, minor, 0.12f, left + ae_flow_u(flow, 16.0f), at, AE_ALIGN_LEFT, AE_COLOR_ACCENT,
			"NEW MESSAGE");
	}
	at += ae_flow_u(flow, 30.0f);
	ae_draw_text(AE_FONT_TITLE, ae_flow_u(flow, 132.0f), left, at, AE_ALIGN_LEFT, AE_COLOR_TITLE, screen.day);
	at += ae_flow_u(flow, 140.0f);
	ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 30.0f), left, at, AE_ALIGN_LEFT, AE_COLOR_TITLE, screen.month);
	at += ae_flow_u(flow, 44.0f);
	ae_draw_text(AE_FONT_BODY, ae_size(d, 20.0f, 12.0f), left, at, AE_ALIGN_LEFT, AE_COLOR_MUTED, screen.weekday);
	at += ae_flow_u(flow, 48.0f);
	for (row = 0; row < 3; row++)
	{
		static const char *const names[3] = { "FROM", "MESSAGE", "YOUR VERSION" };
		const char *values[3];

		values[0] = "ARENA EVOLVED";
		values[1] = screen.number;
		values[2] = screen.version_fact;
		ae_draw_rect(left, at, inner, ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
		ae_draw_text_tracked(AE_FONT_BODY, minor, 0.12f, left, at + ae_flow_u(flow, 14.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			names[row]);
		ae_draw_text(AE_FONT_ROW, fact, left + inner, at + ae_flow_u(flow, 12.0f), AE_ALIGN_RIGHT,
			row == 2 && screen.reason == AE_MOTD_UPDATE ? AE_COLOR_ACCENT : AE_COLOR_TITLE, values[row]);
		at += ae_flow_u(flow, 46.0f);
	}
	ae_draw_rect(left, at, inner, ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
	if (screen.kind == KIND_MOTD && !screen.by_hand)
	{
		ae_draw_text(AE_FONT_BODY, minor, left, bottom - pad - ae_flow_u(flow, 50.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"Shown once a day.");
		ae_draw_text(AE_FONT_BODY, minor, left, bottom - pad - ae_flow_u(flow, 24.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"Main Menu > Message of the Day");
	}
}

/* WHAT'S NEW's left list: the releases, the shown one lit */
static void releases_draw(float x, float y, float width, float bottom)
{
	struct ae_flow_screen const *flow = &screen.flow;
	struct ae_density const *d = &flow->density;
	float pad = ae_flow_u(flow, 24.0f), row_height = ae_flow_u(flow, 50.0f), at = y + pad;
	int first = screen.release - (int)((bottom - y - 2.0f * pad) / row_height) + 1, index;

	ae_draw_rect(x, y, width, bottom - y, 0.0f, AE_COLOR_WELL);
	if (first < 0)
		first = 0;
	for (index = first; index < screen.release_count && at + row_height <= bottom - pad; index++)
	{
		int lit = index == screen.release;
		struct ae_release_entry const *r = &screen.releases[index];

		if (lit)
		{
			ae_draw_rect(x + pad * 0.5f, at, width - pad, row_height - ae_flow_u(flow, 6.0f), 0.0f, AE_COLOR_SELECTION);
			ae_draw_rect(x + pad * 0.5f, at, ae_flow_u(flow, 5.0f), row_height - ae_flow_u(flow, 6.0f), 0.0f, AE_COLOR_ACCENT);
		}
		ae_draw_text(AE_FONT_ROW, ae_size_text(d), x + pad, at + ae_flow_u(flow, 10.0f), AE_ALIGN_LEFT,
			lit ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT, r->version);
		ae_draw_text(AE_FONT_BODY, ae_size(d, 16.0f, 12.0f), x + width - pad, at + ae_flow_u(flow, 14.0f), AE_ALIGN_RIGHT,
			lit ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED, r->date);
		ae_hit_add(x, at, width, row_height, HIT_RELEASES, AE_PART_ITEM, (short)index);
		at += row_height;
	}
}

static void motd_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &screen.flow;
	float left, right, top, bottom, well, text_x, text_width, track;
	char prompt[64];

	(void)instance;
	ae_flow_full_density(flow);
	ae_flow_title(flow, screen.kind == KIND_MOTD ? "MESSAGE OF THE DAY" : "WHAT'S NEW", "MAIN MENU");
	left = flow->frame.rect.x + flow->frame.rect.width * PANEL_LEFT;
	right = flow->frame.rect.x + flow->frame.rect.width * PANEL_RIGHT;
	top = ae_frame_y(&flow->frame, PANEL_TOP);
	bottom = ae_frame_y(&flow->frame, PANEL_BOTTOM);
	ae_draw_rect(left, top, right - left, bottom - top, 0.0f, AE_COLOR_PANEL);
	ae_draw_outline(left, top, right - left, bottom - top, 0.0f, ae_flow_u(flow, AE_FLOW_RULE_U), AE_COLOR_RULE);
	well = ae_flow_u(flow, WELL_U);
	if (screen.kind == KIND_MOTD)
		well_draw(left, top, well, bottom);
	else
		releases_draw(left, top, well, bottom);
	text_x = left + well + ae_flow_u(flow, TEXT_GAP_U);
	text_width = right - text_x - ae_flow_u(flow, TEXT_GAP_U);
	if (text_width > ae_flow_u(flow, TEXT_WIDTH_U))
		text_width = ae_flow_u(flow, TEXT_WIDTH_U);
	{
		float pad = ae_flow_u(flow, WELL_PAD_U), y = top + pad - screen.scroll, head = ae_flow_u(flow, 48.0f);
		float clip_top = top + pad, clip_bottom = bottom - pad;

		screen.view_height = clip_bottom - clip_top;
		ae_draw_clip_push(text_x, clip_top, right - text_x, clip_bottom - clip_top);
		ae_draw_text_tracked(AE_FONT_BODY, ae_size(&flow->density, 16.0f, 12.0f), 0.12f, text_x, y, AE_ALIGN_LEFT,
			AE_COLOR_MUTED, screen.dateline);
		y += ae_flow_u(flow, 32.0f);
		ae_draw_text(AE_FONT_ROW, head, text_x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, screen.headline);
		y += head + ae_flow_u(flow, 24.0f);
		y += blocks_draw(text_x, y, text_width, clip_top - head * 4.0f, clip_bottom + head * 4.0f, 1);
		if (screen.kind == KIND_MOTD && screen.source != AE_MOTD_FROM_NOTES)
			ae_draw_text(AE_FONT_BODY, ae_size(&flow->density, 18.0f, 12.0f), text_x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED,
				"See What's New for the full list.");
		ae_draw_clip_pop();
		screen.content_height = y + ae_flow_u(flow, 40.0f) + screen.scroll - (top + pad);
	}
	/* (the scroll track: only when the text is taller than the panel) */
	track = ae_flow_u(flow, 4.0f);
	if (screen.content_height > screen.view_height && screen.view_height > 0.0f)
	{
		float x = right - ae_flow_u(flow, 16.0f), y = top + ae_flow_u(flow, 24.0f), h = bottom - top - ae_flow_u(flow, 48.0f);
		float share = screen.view_height / screen.content_height;
		float at = screen.scroll / (screen.content_height - screen.view_height);

		ae_draw_rect(x, y, track, h, 0.0f, AE_COLOR_TRACK);
		ae_draw_rect(x, y + (h - h * share) * at, track, h * share, 0.0f, AE_COLOR_THUMB_MOUSE);
	}
	flow->prompt_count = 0;
	if (screen.kind == KIND_MOTD)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Continue", AE_ACTION_ACCEPT);
		snprintf(prompt, sizeof(prompt), "What's New in %s", build_identity_version());
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, prompt, AE_ACTION_Y);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Close", AE_ACTION_BACK);
	}
	else
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

/* ---------- input */

static void scroll_by(float amount)
{
	float most = screen.content_height - screen.view_height;

	screen.scroll += amount;
	if (screen.scroll > most)
		screen.scroll = most;
	if (screen.scroll < 0.0f)
		screen.scroll = 0.0f;
}

static void release_show(int index)
{
	char const *text = changelog_text();

	if (index < 0 || index >= screen.release_count || !text)
		return;
	screen.release = index;
	screen.scroll = 0.0f;
	snprintf(screen.dateline, sizeof(screen.dateline), "RELEASED %s", screen.releases[index].date);
	snprintf(screen.headline, sizeof(screen.headline), "ARENA EVOLVED %s", screen.releases[index].version);
	blocks_from_release(text, &screen.releases[index]);
	state_read();
	if (!state.whats_new_seen[0])
		ae_whats_new_seed(screen.releases, screen.release_count, build_identity_version(), state.whats_new_seen,
			(int)sizeof(state.whats_new_seen));
	ae_whats_new_open_release(screen.releases, screen.release_count, build_identity_version(),
		screen.releases[index].version, state.whats_new_seen, (int)sizeof(state.whats_new_seen), state.whats_new_opened,
		(int)sizeof(state.whats_new_opened));
	state_write();
	platform_log("ae whats new: release %s", screen.releases[index].version);
}

/* closing records what was seen: the rev (and, at launch, the day and the version) */
static void motd_record(void)
{
	char day[16];

	state_read();
	if (screen.source != AE_MOTD_FROM_NOTES)
		state.closed_rev = screen.motd.rev;
	if (!screen.by_hand)
	{
		today(day, (int)sizeof(day));
		snprintf(state.shown_day, sizeof(state.shown_day), "%s", day);
		snprintf(state.last_version, sizeof(state.last_version), "%s", build_identity_version());
	}
	state_write();
}

static void motd_close_as(const char *how, int sound)
{
	if (screen.kind == KIND_MOTD)
		motd_record();
	platform_log("ae %s: closed (%s)", screen.kind == KIND_MOTD ? "motd" : "whats new", how);
	if (screen.kind == KIND_WHATS_NEW)
	{
		const char *badge = ae_screen_whats_new_badge();

		platform_log("ae whats new: badge %s", badge ? badge : "none");
	}
	ae_sound_request(sound, 0);
	ae_ui_remove(&screen);
}

static int motd_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_ACCEPT:
		if (screen.kind == KIND_MOTD)
			motd_close_as("continue", AE_SOUND_FORWARD);
		return 1;
	case AE_ACTION_BACK:
		motd_close_as("close", AE_SOUND_BACK);
		return 1;
	case AE_ACTION_Y:
		if (screen.kind == KIND_MOTD)
		{
			motd_close_as("whats new", AE_SOUND_FORWARD);
			ae_screen_whats_new_open(build_identity_version());
		}
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		if (screen.kind == KIND_WHATS_NEW)
		{
			int next = screen.release + (event->action == AE_ACTION_UP ? -1 : 1);

			if (next >= 0 && next < screen.release_count)
			{
				release_show(next);
				ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			}
		}
		else
			scroll_by((event->action == AE_ACTION_UP ? -0.15f : 0.15f) * screen.view_height);
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		scroll_by((event->action == AE_ACTION_TAB_PREVIOUS ? -1.0f : 1.0f) * screen.view_height);
		return 1;
	default:
		return 1;
	}
}

static void motd_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (pointer->right_clicks)
	{
		event.action = AE_ACTION_BACK;
		motd_handle(instance, &event);
		return;
	}
	if (pointer->wheel_steps)
		scroll_by(-(float)pointer->wheel_steps * 0.15f * screen.view_height);
	if (ae_prompts_pointer(screen.flow.prompts, screen.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		motd_handle(instance, &event);
		return;
	}
	if (screen.kind == KIND_WHATS_NEW && pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) &&
		hit.id == HIT_RELEASES)
	{
		release_show(hit.index);
	}
}

static struct ae_screen_class const motd_class =
{
	.name = "motd", .handle = motd_handle, .draw = motd_draw, .pointer = motd_pointer,
};

/* ---------- opening */

static int motd_push(int by_hand, int reason)
{
	struct ae_motd fresh;
	static char body[4096], signature[160];
	int fetched = ae_motd_fetch_result(&fresh, body, (int)sizeof(body), signature, (int)sizeof(signature)) == 1;
	int cached = 0;

	memset(&screen, 0, sizeof(screen));
	screen.kind = KIND_MOTD;
	screen.by_hand = by_hand;
	screen.reason = reason;
	screen.flow.hit_rows = HIT_ROWS;
	screen.flow.hit_prompts = HIT_PROMPTS;
	if (fetched)
	{
		screen.motd = fresh;
		cache_write(body, signature);
	}
	else
		cached = cache_read(&screen.motd);
	/* (an update launch with no message: its release's notes; else the message) */
	content_set(ae_motd_pick_source(fetched, cached));
	if (!ae_ui_push(&motd_class, AE_OWNER_ANY, &screen))
		return 0;
	platform_log("ae motd: open (%s, %s)", by_hand ? "by hand" : reason == AE_MOTD_UPDATE ? "update" : "message",
		screen.source == AE_MOTD_FROM_FRESH ? "fresh" : screen.source == AE_MOTD_FROM_CACHE ? "saved" : "notes");
	return 1;
}

int ae_screen_motd_open(int by_hand)
{
	if (ae_ui_holds(&screen))
		return 0;
	state_read();
	return motd_push(by_hand, AE_MOTD_NONE);
}

static struct { int started, done; unsigned long since; } launch;

void ae_screen_motd_launch_poll(void)
{
	struct ae_motd fresh;
	int result, reason;
	char day[16];

	if (launch.done)
		return;
	/* (the menus' automated tests: no MESSAGE OF THE DAY over the main menu unless a case asks for it) */
	if (getenv("HALO_AE_TEST_MOTD_SKIP") && !strcmp(getenv("HALO_AE_TEST_MOTD_SKIP"), "1"))
	{
		launch.done = 1;
		return;
	}
	if (!launch.started)
	{
		launch.started = 1;
		launch.since = ae_motion_now();
		ae_motd_fetch_start();
	}
	result = ae_motd_fetch_result(&fresh, NULL, 0, NULL, 0);
	if (result < 0 && ae_motion_now() - launch.since < LAUNCH_WAIT_MS)
		return;
	launch.done = 1;
	state_read();
	today(day, (int)sizeof(day));
	{
		struct ae_motd saved;
		int have = result == 1 || cache_read(&saved);
		long rev = result == 1 ? fresh.rev : have ? saved.rev : 0;

		reason = ae_motd_should_show(&state, build_identity_version(), have, (int)rev, day);
	}
	if (result < 0)
		platform_log("ae motd: no answer in 4 s");
	if (reason == AE_MOTD_NONE)
		return;
	motd_push(0, reason);
}

int ae_screen_whats_new_open(const char *version)
{
	int index;

	if (ae_ui_holds(&screen))
		return 0;
	memset(&screen, 0, sizeof(screen));
	screen.kind = KIND_WHATS_NEW;
	screen.by_hand = 1;
	screen.flow.hit_rows = HIT_ROWS;
	screen.flow.hit_prompts = HIT_PROMPTS;
	releases_read();
	if (!ae_ui_push(&motd_class, AE_OWNER_ANY, &screen))
		return 0;
	platform_log("ae whats new: open (%s)", version ? version : "");
	if (!screen.release_count)
	{
		screen.block_count = 1;
		screen.blocks[0].kind = BLOCK_PARAGRAPH;
		snprintf(screen.blocks[0].text, sizeof(screen.blocks[0].text), "Release notes are not in this install.");
		return 1;
	}
	for (index = 0; index < screen.release_count; index++)
		if (version && !ae_motd_version_compare(screen.releases[index].version, version))
			break;
	release_show(index < screen.release_count ? index : 0);
	return 1;
}

void ae_screen_motd_close(void)
{
	if (ae_ui_holds(&screen))
	{
		platform_log("ae %s: closed (game)", screen.kind == KIND_MOTD ? "motd" : "whats new");
		ae_ui_remove(&screen);
	}
}

const char *ae_screen_motd_badge(void)
{
	struct ae_motd fresh;

	state_read();
	if (ae_motd_fetch_result(&fresh, NULL, 0, NULL, 0) == 1 && fresh.rev != state.closed_rev)
		return "NEW";
	return NULL;
}

const char *ae_screen_whats_new_badge(void)
{
	static char text[16];
	static struct ae_release_entry releases[RELEASES_MAXIMUM];
	static int count = -1;
	const char *log = NULL;
	int unseen;

	if (count < 0)
	{
		log = changelog_text();
		count = log ? ae_changelog_releases(log, releases, RELEASES_MAXIMUM) : 0;
	}
	state_read();
	/* (the first run's seed: only the installed release counts) */
	if (!state.whats_new_seen[0] && count)
	{
		ae_whats_new_seed(releases, count, build_identity_version(), state.whats_new_seen,
			(int)sizeof(state.whats_new_seen));
		state_write();
	}
	unseen = ae_whats_new_count(releases, count, build_identity_version(), state.whats_new_seen, state.whats_new_opened);
	if (unseen <= 0)
		return NULL;
	snprintf(text, sizeof(text), "%d NEW", unseen);
	return text;
}

void ae_screen_motd_reset(void)
{
	memset(&state, 0, sizeof(state));
	state.closed_rev = -1;
	state_read_done = 1;
	state_write();
	launch.done = 0;
	launch.started = 0;
	platform_log("ae motd: reset (nothing seen)");
}

void ae_screen_motd_offline(void)
{
	ae_motd_fetch_offline();
}
