/* MY PARTY drawn from the party record (see ae_screen_party_panel.h). */
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_glue_party.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_party_panel.h"
#include "ae_setup_flow.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

#define INK_DARK 0x0C1218FFu
#define OK_FILL 0x1F6B3AFFu
#define OK_TEXT 0xD7F5E1FFu
#define LEADER_FILL 0xC0392BFFu
#define TAG_SIZE_U 13.0f
#define TAG_HEIGHT_U 20.0f
#define TAG_PAD_U 8.0f
#define ROW_HEIGHT_U 64.0f
#define ROW_PITCH_U 72.0f

static void tag_style(int tag, int on_light, unsigned int *fill, unsigned int *ink, unsigned int *edge)
{
	*fill = 0;
	*edge = AE_COLOR_RULE;
	*ink = AE_COLOR_TEXT;
	switch (tag)
	{
	case AE_TAG_LEADER:
		*fill = AE_COLOR_SELECTION;
		*ink = INK_DARK;
		break;
	case AE_TAG_YOU:
	case AE_TAG_PARTY:
	case AE_TAG_FAVOURITE:
		*fill = AE_COLOR_ACCENT;
		*ink = INK_DARK;
		break;
	case AE_TAG_READY:
		*fill = OK_FILL;
		*ink = OK_TEXT;
		break;
	default:
		*edge = on_light ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED;
		*ink = on_light ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT;
		break;
	}
}

float ae_party_tag_width(struct ae_flow_screen *flow, int tag)
{
	return ae_draw_text_width(AE_FONT_ROW, ae_flow_u(flow, TAG_SIZE_U), ae_party_tag_word(tag)) + 2.0f * ae_flow_u(flow, TAG_PAD_U);
}

float ae_party_tag_draw_left(struct ae_flow_screen *flow, float x, float mid_y, int tag, int on_light)
{
	unsigned int fill, ink, edge;
	float width = ae_party_tag_width(flow, tag), height = ae_flow_u(flow, TAG_HEIGHT_U), size = ae_flow_u(flow, TAG_SIZE_U);

	if (tag == AE_TAG_NONE)
		return 0.0f;
	tag_style(tag, on_light, &fill, &ink, &edge);
	if (fill)
		ae_draw_rect(x, mid_y - height * 0.5f, width, height, 0.0f, fill);
	else
		ae_draw_outline(x, mid_y - height * 0.5f, width, height, 0.0f, ae_flow_u(flow, 1.0f), edge);
	ae_draw_text(AE_FONT_ROW, size, x + width * 0.5f, ae_flow_cap_top(AE_FONT_ROW, size, mid_y), AE_ALIGN_CENTER, ink,
		ae_party_tag_word(tag));
	return width;
}

float ae_party_tag_draw_right(struct ae_flow_screen *flow, float right_x, float mid_y, int tag, int on_light)
{
	float width = ae_party_tag_width(flow, tag);

	if (tag == AE_TAG_NONE)
		return 0.0f;
	ae_party_tag_draw_left(flow, right_x - width, mid_y, tag, on_light);
	return width;
}

static int panel_limit = 4;

void ae_party_panel_set_limit(int limit)
{
	panel_limit = limit > 0 ? limit : 4;
}

int ae_party_panel_limit(void)
{
	return panel_limit;
}

int ae_party_panel_wanted(void)
{
	int source;
	struct ae_party const *party = ae_glue_party(&source);

	return ae_glue_party_staged() || party->count > ae_social_party_size();
}

static void text(struct ae_flow_screen *flow, int font, float size_u, float x, float mid_y, int align, unsigned int color, char const *words)
{
	float size = ae_flow_u(flow, size_u);

	ae_draw_text(font, size, x, ae_flow_cap_top(font, size, mid_y), align, color, words);
}

static void tracked(struct ae_flow_screen *flow, float size_u, float x, float mid_y, int align, unsigned int color, char const *words)
{
	float size = ae_flow_u(flow, size_u);

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x, ae_flow_cap_top(AE_FONT_BODY, size, mid_y), align, color, words);
}

/* a member's detail: "T7 / with you 6 games, 4-2" for a bot, "ONLINE / 38 ms / ready" for a person */
static void member_detail(struct ae_party_member const *m, int source, char *out, int size)
{
	out[0] = 0;
	if (m->kind == AE_MEMBER_BOT)
	{
		snprintf(out, (size_t)size, "T%d", m->tier);
		if (source == AE_PARTY_SOURCE_ONLINE)
			;   /* (its tag, OFFLINE ONLY, says the rest) */
		else if (m->together_games > 0)
			snprintf(out + strlen(out), (size_t)size - strlen(out), "  /  %d-%d", m->together_wins, m->together_games - m->together_wins);
		return;
	}
	if (m->state == AE_PARTY_INVITED)
		snprintf(out, (size_t)size, "INVITED  /  expires in %d:%02d", m->expires_s / 60, m->expires_s % 60);
	else
	{
		snprintf(out, (size_t)size, "ONLINE");
		if (m->ping_ms >= 0)
			snprintf(out + strlen(out), (size_t)size - strlen(out), "  /  %d ms", m->ping_ms);
		snprintf(out + strlen(out), (size_t)size - strlen(out), "  /  %s", m->state == AE_PARTY_READY ? "ready in the lobby" : ae_party_state_word(m->state));
	}
}

/* the member's place among the party's others: 0 first */
static int index_in_others(struct ae_party_member const *m)
{
	struct ae_party_member others[AE_PARTY_MAX];
	int count = ae_party_panel_others(others, AE_PARTY_MAX), at;

	for (at = 0; at < count; at++)
		if (!strcmp(others[at].id, m->id))
			return at;
	return 0;
}

float ae_party_panel_member_row(struct ae_flow_screen *flow, float x, float y, float width, struct ae_party_member const *m, int source,
	int sits, int focused, short hit_id, short index)
{
	struct ae_density const *d = &flow->density;
	struct ae_party_row row;
	char detail[96];
	float height, mid, right = x + width - ae_flow_u(flow, 14.0f);
	int tag = ae_party_tag(m, source), second = AE_TAG_NONE;

	memset(&row, 0, sizeof(row));
	row.kind = m->kind == AE_MEMBER_BOT ? AE_PARTY_BOT : AE_PARTY_PERSON;
	row.name = m->name;
	/* (it plays on your team: your team's red on its stripe and emblem; its number is its place in the party, after this PC's players) */
	row.slot = (short)(ae_screen_main_menu_party_count() + 1 + index_in_others(m));
	row.color = 0xC0392BFFu;
	member_detail(m, source, detail, (int)sizeof(detail));
	if (row.kind == AE_PARTY_PERSON)
		row.sub_line = detail;
	if (focused)
		row.flags |= AE_CARD_FOCUSED;
	height = ae_widget_party_row(d, x, y, width, &row, hit_id, index);
	mid = y + ae_party_row_height(d, row.kind) * 0.5f;
	if (m->kind == AE_MEMBER_BOT && source == AE_PARTY_SOURCE_ONLINE && m->saved)
		second = AE_TAG_FAVOURITE;
	if (sits)
	{
		tracked(flow, 13.0f, right, mid, AE_ALIGN_RIGHT, AE_COLOR_DESTRUCTIVE_INK, "SITS OUT");
		right -= ae_draw_text_tracked_width(AE_FONT_BODY, ae_flow_u(flow, 13.0f), 0.09f, "SITS OUT") + ae_flow_u(flow, 12.0f);
	}
	else if (second != AE_TAG_NONE && row.kind == AE_PARTY_PERSON)
	{
		ae_party_tag_draw_right(flow, right, mid - ae_flow_u(flow, 12.0f), second, focused);
		ae_party_tag_draw_right(flow, right, mid + ae_flow_u(flow, 12.0f), tag, focused);
		return height;
	}
	else if (tag != AE_TAG_NONE)
		right -= ae_party_tag_draw_right(flow, right, mid, tag, focused) + ae_flow_u(flow, 12.0f);
	if (second != AE_TAG_NONE)
		right -= ae_party_tag_draw_right(flow, right, mid, second, focused) + ae_flow_u(flow, 12.0f);
	/* (the detail only where the name leaves room for it: a narrow row drops it) */
	if (row.kind == AE_PARTY_BOT && ae_draw_text_width(AE_FONT_BODY, ae_flow_u(flow, 16.0f), detail) < right - (x + ae_flow_u(flow, 230.0f)))
		text(flow, AE_FONT_BODY, 16.0f, right, mid, AE_ALIGN_RIGHT, focused ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED, detail);
	return height;
}

float ae_party_panel_recent_row(struct ae_flow_screen *flow, float x, float y, float width, struct ae_party_record const *r, int focused,
	short hit_id, short index)
{
	struct ae_density const *d = &flow->density;
	struct ae_party_row row;
	char detail[96];
	float height, mid;

	/* (an action row of the PC menus: the name, PARTY UP in accent on the right; the detail muted before it) */
	memset(&row, 0, sizeof(row));
	row.kind = AE_PARTY_ACTION;
	row.name = r->name;
	row.value = "PARTY UP";
	if (focused)
		row.flags |= AE_CARD_FOCUSED;
	height = ae_widget_party_row(d, x, y, width, &row, hit_id, index);
	mid = y + ae_party_row_height(d, AE_PARTY_ACTION) * 0.5f;
	if (r->kind == AE_MEMBER_BOT)
		snprintf(detail, sizeof(detail), "T%d  /  against you %d", r->tier, r->against_games);
	else if (r->together_games)
		snprintf(detail, sizeof(detail), "played with you recently  /  %d games", r->together_games);
	else
		snprintf(detail, sizeof(detail), "played against you  /  %d", r->against_games);
	text(flow, AE_FONT_BODY, 15.0f, x + width - ae_flow_u(flow, 190.0f), mid, AE_ALIGN_RIGHT, focused ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED,
		detail);
	return height;
}

int ae_party_panel_state_height(struct ae_flow_screen *flow)
{
	(void)flow;
	return 96;
}

void ae_party_panel_state(struct ae_flow_screen *flow, float x, float y, float width, int limit)
{
	int source, seats;
	struct ae_party const *party = ae_glue_party(&source);
	char head[48], words[200], lines[3][160];
	int count, at;

	seats = ae_party_search_seats(party, source);
	if (source == AE_PARTY_SOURCE_ONLINE)
	{
		snprintf(head, sizeof(head), "SEARCH FOR: PEOPLE, BOTS FILL");
		snprintf(words, sizeof(words), "Your party of %d takes %d of %d seats on your team. If nobody is found in 2:00, bots fill the rest.", seats, seats, limit);
	}
	else if (seats > limit)
	{
		snprintf(head, sizeof(head), "A PARTY OF %d AT MOST", limit);
		snprintf(words, sizeof(words), "Find a game is refused until the party fits. The last to join are marked SITS OUT.");
	}
	else
	{
		snprintf(head, sizeof(head), "%s", seats > 1 ? "SEARCHING AS A PARTY" : "SEARCHING ALONE");
		snprintf(words, sizeof(words), "Your party takes %d of the %d seats on your team. Bots fill the rest. The party stays together across games.", seats,
			limit);
	}
	tracked(flow, 16.0f, x, y + ae_flow_u(flow, 10.0f), AE_ALIGN_LEFT, seats > limit ? AE_COLOR_DESTRUCTIVE_INK : AE_COLOR_ACCENT, head);
	count = ae_wrap_text(AE_FONT_BODY, ae_flow_u(flow, 18.0f), words, width, lines, 3);
	for (at = 0; at < count; at++)
		text(flow, AE_FONT_BODY, 18.0f, x, y + ae_flow_u(flow, 38.0f + 26.0f * (float)at), AE_ALIGN_LEFT, at ? AE_COLOR_MUTED : AE_COLOR_TEXT, lines[at]);
}

int ae_party_panel_others(struct ae_party_member *out, int max)
{
	int source, at, count = 0;
	struct ae_party const *party = ae_glue_party(&source);

	for (at = 0; at < party->count && count < max; at++)
		if (party->member[at].kind != AE_MEMBER_LOCAL)
			out[count++] = party->member[at];
	return count;
}

int ae_party_panel_source(void)
{
	int source;

	(void)ae_glue_party(&source);
	return source;
}

void ae_party_panel_draw_below(struct ae_flow_screen *flow, float y, int limit)
{
	struct ae_density const *d = &flow->density;
	float x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X + ae_flow_u(flow, AE_FLOW_INSET_U), right = ae_flow_right_x(flow) -
		ae_flow_u(flow, AE_FLOW_INSET_U), width = right - x, bottom = ae_flow_content_bottom(flow), gap = ae_flow_u(flow, 6.0f), indent = ae_flow_u(flow, AE_PARTY_INDENT_U);
	struct ae_party_member others[AE_PARTY_MAX];
	int source = ae_party_panel_source(), count = ae_party_panel_others(others, AE_PARTY_MAX), at, sit_out[AE_PARTY_MAX], sit_count, each;
	struct ae_party const *party = ae_glue_party(&source);
	float state_h = ae_flow_u(flow, (float)ae_party_panel_state_height(flow));

	if (y <= 0.0f)
		return;
	/* (a panel under it: the section's words stay readable over the backdrop, as the PARTY manager's own panel is) */
	{
		float plate = ae_flow_u(flow, 8.0f + 12.0f) + state_h;

		if (count)
			plate += ae_flow_u(flow, 8.0f + 36.0f);
		for (at = 0; at < count; at++)
			plate += ae_party_row_height(d, others[at].kind == AE_MEMBER_BOT ? AE_PARTY_BOT : AE_PARTY_PERSON) + ae_flow_u(flow, 6.0f);
		if (y + plate > bottom)
			plate = bottom - y;
		ae_draw_rect(x, y + gap, width, plate, 0.0f, AE_COLOR_PANEL);
	}
	sit_count = ae_party_sit_out(party, source, limit, sit_out, AE_PARTY_MAX);
	if (count)
	{
		y += gap + ae_flow_u(flow, 6.0f);
		y += ae_widget_party_section(d, x, y, width, "PARTY BOTS AND FRIENDS");
	}
	for (at = 0; at < count; at++)
	{
		int sits = 0, index = ae_party_find_id(party, others[at].id);

		if (y + ae_party_row_height(d, others[at].kind == AE_MEMBER_BOT ? AE_PARTY_BOT : AE_PARTY_PERSON) > bottom - state_h)
			break;
		for (each = 0; each < sit_count; each++)
			sits |= sit_out[each] == index;
		y += ae_party_panel_member_row(flow, x, y, width, &others[at], source, sits, 0, 0, (short)at);
	}
	y += gap;
	if (y + state_h <= bottom)
		ae_party_panel_state(flow, x + indent, y, width - indent - ae_flow_u(flow, 16.0f), limit);
}
