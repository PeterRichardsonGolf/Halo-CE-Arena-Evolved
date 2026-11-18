/*
AE_SCREEN_BOT_RECORDS.C

See ae_screen_bot_records.h. Layout (mockup C-2, u at 1080): CAREER's (ae_screen_career.c): the rows left on AE's flow
(the header BOTS YOU HAVE MET, the sort's word at its right), the focused bot's card from AE_FLOW_PANEL_X.
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_bot_record_rules.h"
#include "ae_bots_seat_rules.h"
#include "ae_bots_style_rules.h"
#include "ae_dev_events.h"
#include "ae_glue_bot_records.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"
#include "ae_pgcr_screen_rules.h"
#include "ae_screen_bot_record.h"
#include "ae_screen_bot_records.h"
#include "ae_screen_main_menu.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

#define DASH "\xE2\x80\x94"
/* a rival's ink (ae_pgcr_draw.h's: violet, never the team's red or the focus's ice), darker on the focused white row */
#define RIVAL_INK 0xB8A4F5FFu
#define RIVAL_INK_ON_LIGHT 0x5A44B8FFu

enum
{
	BOTS_HIT = 0x7790,
	HIT_PROMPTS = BOTS_HIT + 1,
	/* the rows shown: the header, then 10 bots */
	SHOWN = 10,
	ROW_BASE = 1
};

static struct
{
	struct ae_flow_screen flow;
	char crumb[64];
	char key[AE_BOT_RECORD_KEY];
	char name[48];
	short player;
	int have_key;
	int sort;
	int order[AE_BOT_RECORD_BOTS];
	int count;
	/* the focused bot (in order) and the first shown; the focused bot's key, kept over a new order */
	int index, first;
	char focus_key[AE_BOT_RECORD_KEY];
	/* the list as built: built again when one of these changes */
	int built;
	struct ae_bot_records const *built_records;
	long built_clock;
	int built_bots, built_versus, built_sort;
	short built_player;
	char values[SHOWN][16];
} bots;

static struct ae_screen_class const bots_class;

/* ---------- the list */

static void profile_read(void)
{
	char name[48];
	int at;

	bots.have_key = ae_social_screen_key(bots.player, bots.key, sizeof(bots.key)) && bots.key[0];
	if (ae_social_profile_name(bots.player, name, sizeof(name)))
		snprintf(bots.name, sizeof(bots.name), "%s", name);
	else
		snprintf(bots.name, sizeof(bots.name), "PLAYER %d", (int)bots.player + 1);
	for (at = 0; bots.name[at]; at++)
		bots.name[at] = (char)toupper((unsigned char)bots.name[at]);
	if (ae_social_profile_name(bots.player, name, sizeof(name)))
		snprintf(bots.crumb, sizeof(bots.crumb), "%s \xC2\xB7 PLAYER %d", bots.name, (int)bots.player + 1);
	else
		snprintf(bots.crumb, sizeof(bots.crumb), "PLAYER %d", (int)bots.player + 1);
}

static struct ae_bot_record const *bot_at(int index)
{
	struct ae_bot_records const *records = ae_bot_records_active();

	return index >= 0 && index < bots.count ? &records->bot[bots.order[index]] : NULL;
}

static struct ae_bot_versus const *versus_of(struct ae_bot_record const *bot)
{
	return bot && bots.have_key ? ae_bot_record_versus(ae_bot_records_active(), bots.key, bot->key) : NULL;
}

static void window_fit(void)
{
	if (bots.index >= bots.count)
		bots.index = bots.count - 1;
	if (bots.index < 0)
		bots.index = 0;
	if (bots.index < bots.first)
		bots.first = bots.index;
	if (bots.index >= bots.first + SHOWN)
		bots.first = bots.index - SHOWN + 1;
	if (bots.first > bots.count - SHOWN)
		bots.first = bots.count - SHOWN;
	if (bots.first < 0)
		bots.first = 0;
	if (bot_at(bots.index))
		snprintf(bots.focus_key, sizeof(bots.focus_key), "%s", bot_at(bots.index)->key);
}

static void list_announce(void)
{
	platform_log("ae botrecord: list (player %d, %d bots, %s)%s", (int)bots.player + 1, bots.count,
		ae_bot_record_sort_name(bots.sort), ae_bot_records_demo_on() ? " made-up store" : "");
	ae_dev_event("bot_records_list", "\"count\":%d,\"sort\":\"%s\",\"player\":%d,\"demo\":%d", bots.count,
		ae_bot_record_sort_name(bots.sort), (int)bots.player + 1, ae_bot_records_demo_on());
}

/* the order again when the store, the sort or the player changed (a game added, DEV MODE's made-up store) */
static void list_build(void)
{
	struct ae_bot_records const *records = ae_bot_records_active();
	int index;

	profile_read();
	if (bots.built && bots.built_records == records && bots.built_clock == records->clock && bots.built_bots == records->count &&
		bots.built_versus == records->versus_count && bots.built_sort == bots.sort && bots.built_player == bots.player)
		return;
	bots.built = 1;
	bots.built_records = records;
	bots.built_clock = records->clock;
	bots.built_bots = records->count;
	bots.built_versus = records->versus_count;
	bots.built_sort = bots.sort;
	bots.built_player = bots.player;
	bots.count = bots.have_key ? ae_bot_records_list(records, bots.key, bots.sort, bots.order, AE_BOT_RECORD_BOTS) : 0;
	bots.index = 0;
	for (index = 0; bots.focus_key[0] && index < bots.count; index++)
		if (!strcmp(records->bot[bots.order[index]].key, bots.focus_key))
			bots.index = index;
	window_fit();
}

static void rows_build(void)
{
	struct ae_flow_screen *flow = &bots.flow;
	short count = 0;
	int index;

	flow->rows[count++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "BOTS YOU HAVE MET", NULL, NULL, NULL };
	for (index = bots.first; index < bots.count && index < bots.first + SHOWN; index++)
	{
		struct ae_bot_record const *bot = bot_at(index);
		struct ae_bot_versus const *versus = versus_of(bot);
		char *value = bots.values[index - bots.first];

		snprintf(value, sizeof(bots.values[0]), "%ld", versus ? versus->against + versus->with : 0L);
		flow->rows[count] = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, bot->name, value, NULL, NULL };
		flow->rows[count++].subscreen = 1;
	}
	flow->count = count;
	flow->first = 0;
	flow->focus = (short)(bots.count > 0 ? ROW_BASE + bots.index - bots.first : 0);
}

/* ---------- drawing */

static void ctext(int font, float size_u, float x, float center_y, int align, unsigned int color, char const *text)
{
	float size = ae_flow_u(&bots.flow, size_u);

	ae_draw_text(font, size, x, ae_flow_cap_top(font, size, center_y), align, color, text);
}

static float tracked(float size_u, float x, float center_y, int align, unsigned int color, char const *text)
{
	float size = ae_flow_u(&bots.flow, size_u);

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x, ae_flow_cap_top(AE_FONT_BODY, size, center_y), align, color, text);
	return ae_draw_text_tracked_width(AE_FONT_BODY, size, 0.09f, text);
}

/* an outlined tag of a word: its width */
static float outlined_tag(float x, float cy, char const *word, unsigned int ink)
{
	struct ae_flow_screen *flow = &bots.flow;
	float small = ae_flow_u(flow, 15.0f), h = ae_flow_u(flow, 22.0f);
	float w = ae_draw_text_tracked_width(AE_FONT_ROW, small, 0.06f, word) + ae_flow_u(flow, 16.0f);

	ae_draw_outline(x, cy - h * 0.5f, w, h, 0.0f, ae_flow_u(flow, 1.0f), ink);
	ae_draw_text_tracked(AE_FONT_ROW, small, 0.06f, x + ae_flow_u(flow, 8.0f), ae_flow_cap_top(AE_FONT_ROW, small, cy),
		AE_ALIGN_LEFT, ink, word);
	return w;
}

static void style_word(struct ae_bot_record const *bot, char *out, size_t size)
{
	char const *name = bot->style >= 0 && bot->style < AE_STYLE_PRESETS ? ae_bots_style_preset_name(bot->style) : NULL;
	size_t at = 0;

	for (; name && *name && at + 1 < size; name++)
		out[at++] = (char)toupper((unsigned char)*name);
	out[at] = 0;
}

/* each shown row's badges after its name: the style's (muted), the RIVAL tag */
static void badges_draw(void)
{
	struct ae_flow_screen *flow = &bots.flow;
	float label_size = ae_size_text(&flow->density);
	int index;

	for (index = bots.first; index < bots.count && index < bots.first + SHOWN; index++)
	{
		struct ae_bot_record const *bot = bot_at(index);
		struct ae_bot_versus const *versus = versus_of(bot);
		struct ae_rect const *r = &flow->rects[ROW_BASE + index - bots.first];
		int on = ROW_BASE + index - bots.first == flow->focus;
		char style[24];
		float x, cy;

		if (r->width <= 0.0f || !bot)
			continue;
		x = r->x + ae_flow_u(flow, 20.0f) + ae_draw_text_width(AE_FONT_ROW, label_size, bot->name) + ae_flow_u(flow, 14.0f);
		cy = r->y + r->height * 0.5f;
		style_word(bot, style, sizeof(style));
		if (style[0])
			x += outlined_tag(x, cy, style, on ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED) + ae_flow_u(flow, 10.0f);
		if (versus && ae_bot_record_is_rival(versus))
			outlined_tag(x, cy, "RIVAL", on ? RIVAL_INK_ON_LIGHT : RIVAL_INK);
	}
}

/* a label (16 tracked muted) and its value (row 24 white, a dash muted) between x and right */
static void record_stat(float x, float right, float y, char const *label, char const *value)
{
	char fitted[64];
	float w = tracked(16.0f, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);

	ae_fit_text(AE_FONT_ROW, ae_flow_u(&bots.flow, 24.0f), value, right - x - w - ae_flow_u(&bots.flow, 16.0f), fitted,
		(int)sizeof(fitted));
	ctext(AE_FONT_ROW, 24.0f, right, y, AE_ALIGN_RIGHT, strcmp(value, DASH) ? AE_COLOR_TITLE : AE_COLOR_MUTED, fitted);
}

static void panel_draw(float x, float width)
{
	struct ae_flow_screen *flow = &bots.flow;
	struct ae_bot_record const *bot = bot_at(bots.index);
	struct ae_bot_versus const *versus = versus_of(bot);
	float top = ae_flow_u(flow, 190.0f), pad = ae_flow_u(flow, 22.0f), left = x + pad, right = x + width - pad;
	float middle = x + width * 0.5f, w;
	char text[160], head[64], sentence[160], lines[2][160], style[24];
	int line, count, index;

	ae_draw_rect(x, top, width, ae_flow_u(flow, 600.0f), ae_flow_u(flow, 3.0f), AE_COLOR_PANEL);
	ae_draw_outline(x, top, width, ae_flow_u(flow, 600.0f), ae_flow_u(flow, 3.0f), ae_flow_u(flow, 1.0f), AE_COLOR_RULE);
	if (!bot)
		return;
	/* the bot: its name and tags, its style right */
	style_word(bot, style, sizeof(style));
	if (style[0])
		tracked(16.0f, right, top + ae_flow_u(flow, 40.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, style);
	ae_fit_text(AE_FONT_ROW, ae_flow_u(flow, 32.0f), bot->name, width * 0.5f, text, (int)sizeof(text));
	ctext(AE_FONT_ROW, 32.0f, left, top + ae_flow_u(flow, 40.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	w = left + ae_draw_text_width(AE_FONT_ROW, ae_flow_u(flow, 32.0f), text) + ae_flow_u(flow, 14.0f);
	w += outlined_tag(w, top + ae_flow_u(flow, 40.0f), "BOT", AE_COLOR_MUTED) + ae_flow_u(flow, 8.0f);
	if (versus && ae_bot_record_is_rival(versus))
		outlined_tag(w, top + ae_flow_u(flow, 40.0f), "RIVAL", RIVAL_INK);
	/* the rival line: its head (row 28), its sentence (body 18, two lines at most) */
	head[0] = sentence[0] = 0;
	ae_bot_record_rival_line(versus, head, sizeof(head), sentence, sizeof(sentence), NULL, 0);
	ctext(AE_FONT_ROW, 28.0f, left, top + ae_flow_u(flow, 97.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, head[0] ? head : DASH);
	count = sentence[0] ? ae_wrap_text(AE_FONT_BODY, ae_flow_u(flow, 18.0f), sentence, right - left, lines, 2) : 0;
	for (line = 0; line < count; line++)
		ctext(AE_FONT_BODY, 18.0f, left, top + ae_flow_u(flow, 130.0f + line * 24.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT,
			lines[line]);
	/* VS YOU, two columns */
	tracked(16.0f, left, top + ae_flow_u(flow, 186.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "VS YOU");
	{
		static char const *const labels[6] = { "GAMES AGAINST", "YOU WON", "IT WON", "YOUR KILLS", "ITS KILLS", "TOGETHER" };
		char values[6][24];

		for (index = 0; index < 6; index++)
			snprintf(values[index], sizeof(values[index]), DASH);
		if (versus)
		{
			snprintf(values[0], sizeof(values[0]), "%ld", versus->against);
			snprintf(values[1], sizeof(values[1]), "%ld", versus->you_won);
			snprintf(values[2], sizeof(values[2]), "%ld", versus->it_won);
			snprintf(values[3], sizeof(values[3]), "%ld", versus->your_kills);
			snprintf(values[4], sizeof(values[4]), "%ld", versus->its_kills);
			snprintf(values[5], sizeof(values[5]), "%ld", versus->with);
		}
		for (index = 0; index < 6; index++)
			record_stat(index < 3 ? left : middle + ae_flow_u(flow, 14.0f), index < 3 ? middle - ae_flow_u(flow, 24.0f) : right,
				top + ae_flow_u(flow, 226.0f + (index % 3) * 40.0f), labels[index], values[index]);
	}
	ae_draw_rect(left, top + ae_flow_u(flow, 340.0f), right - left, ae_flow_u(flow, 1.0f), 0.0f, AE_COLOR_RULE);
	/* CAREER TOTALS in short */
	tracked(16.0f, left, top + ae_flow_u(flow, 370.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "CAREER TOTALS");
	snprintf(text, sizeof(text), "%ld", bot->games);
	record_stat(left, middle - ae_flow_u(flow, 24.0f), top + ae_flow_u(flow, 410.0f), "GAMES", text);
	ae_pgr_kd_text(bot->kills, bot->deaths, text, sizeof(text));
	record_stat(middle + ae_flow_u(flow, 14.0f), right, top + ae_flow_u(flow, 410.0f), "K/D", text);
	if (bot->games > 0)
		snprintf(text, sizeof(text), "%ld%%", (bot->wins * 100 + bot->games / 2) / bot->games);
	else
		snprintf(text, sizeof(text), DASH);
	record_stat(left, middle - ae_flow_u(flow, 24.0f), top + ae_flow_u(flow, 450.0f), "WIN RATE", text);
	snprintf(text, sizeof(text), "%ld", bot->best_spree);
	record_stat(middle + ae_flow_u(flow, 14.0f), right, top + ae_flow_u(flow, 450.0f), "BEST SPREE", text);
	ae_draw_rect(left, top + ae_flow_u(flow, 484.0f), right - left, ae_flow_u(flow, 1.0f), 0.0f, AE_COLOR_RULE);
	/* how it plays */
	if (bot->shooting > 0 && bot->movement > 0)
		snprintf(text, sizeof(text), "%s / %s", ae_seat_step_name(bot->shooting), ae_seat_step_name(bot->movement));
	else
		snprintf(text, sizeof(text), DASH);
	record_stat(left, right, top + ae_flow_u(flow, 514.0f), "SKILL", text);
	if (bot->style < 0)
		snprintf(text, sizeof(text), DASH);
	else if (bot->favourite > AE_STYLE_WEAPON_NONE)
	{
		char const *name = ae_bots_style_weapon_name(bot->favourite);
		size_t at = 0;

		for (; name && *name && at + 1 < sizeof(text); name++)
			text[at++] = *name == '_' ? ' ' : (char)toupper((unsigned char)*name);
		text[at] = 0;
	}
	else
		snprintf(text, sizeof(text), "ANY");
	record_stat(left, right, top + ae_flow_u(flow, 554.0f), "FAVOURITE WEAPON", text);
}

static void bots_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &bots.flow;
	float width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH, left, panel_x, help_y;

	(void)instance;
	ae_flow_full_density(flow);
	list_build();
	rows_build();
	ae_flow_title(flow, "BOTS", bots.crumb);
	left = ae_flow_left_x(flow);
	if (bots.count > 0)
	{
		char more[48];

		ae_flow_draw_rows(flow, width);
		badges_draw();
		/* the rows left out above and below the ten shown */
		more[0] = 0;
		if (bots.first > 0 && bots.count - bots.first - SHOWN > 0)
			snprintf(more, sizeof(more), "\xE2\x96\xB2 %d more   \xE2\x96\xBC %d more", bots.first, bots.count - bots.first - SHOWN);
		else if (bots.first > 0)
			snprintf(more, sizeof(more), "\xE2\x96\xB2 %d more", bots.first);
		else if (bots.count - SHOWN > 0)
			snprintf(more, sizeof(more), "\xE2\x96\xBC %d more", bots.count - SHOWN);
		if (more[0])
			ctext(AE_FONT_BODY, 18.0f, left + width * 0.5f, flow->rows_end + ae_flow_u(flow, 24.0f), AE_ALIGN_CENTER,
				AE_COLOR_MUTED, more);
	}
	else
	{
		tracked(18.0f, left, ae_flow_u(flow, 190.0f) + ae_flow_u(flow, 22.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "BOTS YOU HAVE MET");
		ctext(AE_FONT_BODY, 21.0f, left, ae_flow_u(flow, 190.0f) + ae_flow_u(flow, 84.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT,
			bots.have_key ? "No bots met yet. Play an AE TOURNAMENT game: every bot you meet keeps a record here." :
			"No profile: sign in to a profile to keep a record of the bots you meet.");
	}
	tracked(18.0f, left + width - ae_flow_u(flow, 10.0f), ae_flow_u(flow, 190.0f) + ae_flow_u(flow, 22.0f), AE_ALIGN_RIGHT,
		AE_COLOR_MUTED, ae_bot_record_sort_name(bots.sort));
	panel_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	if (bots.count > 0)
		panel_draw(panel_x, flow->frame.rect.x + flow->frame.rect.width - flow->frame.margin - panel_x);
	help_y = ae_flow_u(flow, 902.0f);
	ae_draw_rect(left, help_y, flow->frame.rect.width - 2.0f * flow->frame.margin, ae_flow_u(flow, 1.0f), 0.0f, AE_COLOR_RULE);
	ctext(AE_FONT_BODY, 21.0f, left, help_y + ae_flow_u(flow, 22.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT,
		"Every bot keeps a service record of its tournament games on this PC: its totals, its record against you, how it plays.");
	flow->prompt_count = 0;
	if (bots.count > 0)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Record", AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Sort", AE_ACTION_X);
	flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Player", AE_ACTION_TAB_PREVIOUS);
	flow->prompts[flow->prompt_count++].pair = 1;
	ae_flow_draw_prompts(flow);
}

/* ---------- input */

static void step(int delta)
{
	int before = bots.index;

	bots.index += delta;
	window_fit();
	if (bots.index != before)
		ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void open_page(void)
{
	struct ae_bot_page_source *source;
	struct ae_bot_records const *records = ae_bot_records_active();
	int index;

	if (bots.count <= 0 || !(source = ae_screen_bot_record_source()))
		return;
	source->from_career = 1;
	for (index = 0; index < bots.count && index < AE_BOT_RECORD_BOTS; index++)
		snprintf(source->name[index], sizeof(source->name[index]), "%s", records->bot[bots.order[index]].name);
	source->count = index;
	if (bots.have_key)
	{
		snprintf(source->profile_key[0], sizeof(source->profile_key[0]), "%s", bots.key);
		if (!ae_social_profile_name(bots.player, source->profile_name[0], sizeof(source->profile_name[0])))
			snprintf(source->profile_name[0], sizeof(source->profile_name[0]), "PLAYER %d", (int)bots.player + 1);
		source->profile_chip[0] = AE_COLOR_ACCENT;
		source->profile_player[0] = (int)bots.player + 1;
		source->profiles = 1;
	}
	if (ae_screen_bot_record_open(AE_OWNER_PARENT, bots.index))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void player_step(int direction)
{
	short each, candidate = bots.player;

	for (each = 0; each < AE_SOCIAL_LOCAL; each++)
	{
		candidate = (short)((candidate + direction + AE_SOCIAL_LOCAL) % AE_SOCIAL_LOCAL);
		if (candidate == 0 || ae_screen_main_menu_signed_in(candidate))
		{
			bots.player = candidate;
			bots.focus_key[0] = 0;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			list_build();
			list_announce();
			return;
		}
	}
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

static int bots_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		step(event->action == AE_ACTION_UP ? -1 : 1);
		return 1;
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
		step(event->action == AE_ACTION_PAGE_UP ? -(SHOWN - 1) : SHOWN - 1);
		return 1;
	case AE_ACTION_ACCEPT:
		open_page();
		return 1;
	case AE_ACTION_X:
		bots.sort = (bots.sort + 1) % AE_BOT_RECORD_SORTS;
		ae_sound_request(AE_SOUND_CURSOR, 0);
		list_build();
		list_announce();
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		player_step(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_BACK:
		return 0;
	default:
		return 1;
	}
}

static void bots_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_flow_screen *flow = &bots.flow;
	struct ae_event event;
	short row;

	if (ae_prompts_pointer(flow->prompts, flow->prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			bots_handle(instance, &event);
		return;
	}
	if (bots.count <= 0)
		return;
	if (pointer->wheel_steps)
	{
		/* (the wheel moves the ten shown; the focus stays among them) */
		bots.first -= pointer->wheel_steps;
		if (bots.first > bots.count - SHOWN)
			bots.first = bots.count - SHOWN;
		if (bots.first < 0)
			bots.first = 0;
		if (bots.index < bots.first)
			bots.index = bots.first;
		if (bots.index >= bots.first + SHOWN)
			bots.index = bots.first + SHOWN - 1;
		window_fit();
		rows_build();
		return;
	}
	row = ae_flow_rows_pointer(flow, pointer);
	if (flow->focus >= ROW_BASE && bots.first + flow->focus - ROW_BASE < bots.count)
	{
		bots.index = bots.first + flow->focus - ROW_BASE;
		window_fit();
	}
	if (row >= ROW_BASE && pointer->left_clicks)
		open_page();
}

static void bots_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae botrecord: list closed");
}

static struct ae_screen_class const bots_class =
{
	.name = "bot_records", .leave = bots_leave, .handle = bots_handle, .draw = bots_draw, .pointer = bots_pointer,
};

int ae_screen_bot_records_open(short local_player, char const *focus, int sort)
{
	if (ae_ui_holds(&bots))
		return 0;
	memset(&bots, 0, sizeof(bots));
	bots.player = local_player < 0 || local_player >= AE_SOCIAL_LOCAL ? 0 : local_player;
	bots.sort = sort >= 0 && sort < AE_BOT_RECORD_SORTS ? sort : AE_BOT_RECORD_SORT_GAMES;
	bots.flow.hit_rows = BOTS_HIT;
	bots.flow.hit_prompts = HIT_PROMPTS;
	if (focus)
		ae_bot_record_key(focus, bots.focus_key, sizeof(bots.focus_key));
	list_build();
	rows_build();
	if (!ae_ui_push(&bots_class, AE_OWNER_ANY, &bots))
		return 0;
	list_announce();
	return 1;
}

void ae_screen_bot_records_close(void)
{
	(void)ae_ui_remove(&bots);
}
