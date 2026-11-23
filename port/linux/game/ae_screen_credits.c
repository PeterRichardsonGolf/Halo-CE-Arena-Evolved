/*
AE_SCREEN_CREDITS.C

See ae_screen_credits.h. The look is WHAT'S NEW's text page (ae_screen_motd.c): the panel x 96 to 1824, y 190 to 978 of
the frame, the text 22 u on a 34 u line, bullets an 8 u accent square, the section headings muted and tracked, a 4 u scroll
track when the text is taller than the panel; the text column at most 1000 u, centred in the panel.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_shipped_files.h"
#include "ae_credits_rules.h"
#include "ae_screen_credits.h"
#include "ae_screen_tab_menu.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	CREDITS_HIT = 0x76F0,
	HIT_ROWS = CREDITS_HIT + 1, HIT_PROMPTS
};

#define BLOCKS_MAXIMUM 96
#define PANEL_LEFT (96.0f / 1920.0f)
#define PANEL_RIGHT (1824.0f / 1920.0f)
#define PANEL_TOP (190.0f / 1080.0f)
#define PANEL_BOTTOM (978.0f / 1080.0f)
#define PAD_U 36.0f
#define TEXT_WIDTH_U 1000.0f
#define LINE_U 34.0f
#define BULLET_GAP_U 46.0f
#define PARAGRAPH_GAP_U 12.0f
#define HEADING_ABOVE_U 28.0f

struct block
{
	short kind, line_count;
	char lines[16][160];
	float height;
};

static struct
{
	struct ae_flow_screen flow;
	short player;
	int from_file;
	struct ae_credits_block parsed[BLOCKS_MAXIMUM];
	struct block blocks[BLOCKS_MAXIMUM];
	int count;
	int layout_valid;
	float layout_width, layout_size, layout_scale;
	float scroll, content_height, view_height;
} screen;

static char file_text[65536];

static void blocks_read(void)
{
	long length = ae_shipped_file_read("credits.txt", file_text, (long)sizeof(file_text));
	char const *text = file_text;

	screen.from_file = length > 0;
	if (!screen.from_file)
		text = ae_credits_fallback();
	screen.count = ae_credits_parse(text, screen.parsed, BLOCKS_MAXIMUM);
	screen.layout_valid = 0;
}

static float block_height(struct block const *block, int first)
{
	struct ae_flow_screen const *flow = &screen.flow;
	float line = ae_flow_u(flow, LINE_U);

	if (block->kind == AE_CREDITS_HEADING)
		return (first ? 0.0f : ae_flow_u(flow, HEADING_ABOVE_U)) + line + ae_flow_u(flow, 4.0f);
	return (float)block->line_count * line + (block->kind == AE_CREDITS_BULLET ?
		ae_flow_u(flow, BULLET_GAP_U) - line : ae_flow_u(flow, PARAGRAPH_GAP_U));
}

static void layout(float width)
{
	struct ae_flow_screen const *flow = &screen.flow;
	float size = ae_size(&flow->density, 22.0f, 12.0f), indent = ae_flow_u(flow, 24.0f);
	int index;

	if (screen.layout_valid && screen.layout_width == width && screen.layout_size == size &&
		screen.layout_scale == flow->density.unit)
		return;
	for (index = 0; index < screen.count; index++)
	{
		struct block *block = &screen.blocks[index];
		struct ae_credits_block const *parsed = &screen.parsed[index];

		block->kind = parsed->kind;
		block->line_count = 0;
		if (parsed->kind != AE_CREDITS_HEADING)
			block->line_count = (short)ae_wrap_text(AE_FONT_BODY, size, parsed->text,
				width - (parsed->kind == AE_CREDITS_BULLET ? indent : 0.0f), block->lines, 16);
		block->height = block_height(block, index == 0);
	}
	screen.layout_width = width;
	screen.layout_size = size;
	screen.layout_scale = flow->density.unit;
	screen.layout_valid = 1;
}

/* draws the blocks from y (those inside the clip); the height they take */
static float blocks_draw(float x, float y, float width, float clip_top, float clip_bottom)
{
	struct ae_flow_screen const *flow = &screen.flow;
	float size = ae_size(&flow->density, 22.0f, 12.0f), line = ae_flow_u(flow, LINE_U), at = y;
	float bullet = ae_flow_u(flow, 8.0f), indent = ae_flow_u(flow, 24.0f);
	int index, row;

	layout(width);
	for (index = 0; index < screen.count; index++)
	{
		struct block const *block = &screen.blocks[index];
		float top = at;

		if (at + block->height < clip_top || at > clip_bottom)
		{
			at += block->height;
			continue;
		}
		if (block->kind == AE_CREDITS_HEADING)
		{
			at += index == 0 ? 0.0f : ae_flow_u(flow, HEADING_ABOVE_U);
			ae_draw_text_tracked(AE_FONT_BODY, ae_size(&flow->density, 16.0f, 12.0f), 0.12f, x, at, AE_ALIGN_LEFT,
				AE_COLOR_MUTED, screen.parsed[index].text);
			at = top + block->height;
			continue;
		}
		for (row = 0; row < block->line_count; row++)
		{
			if (block->kind == AE_CREDITS_BULLET && row == 0)
				ae_draw_rect(x, ae_flow_cap_top(AE_FONT_BODY, size, at + size * 0.5f) + ae_draw_cap_height(AE_FONT_BODY,
					size) * 0.5f - bullet * 0.5f, bullet, bullet, 0.0f, AE_COLOR_ACCENT);
			ae_draw_text(AE_FONT_BODY, size, block->kind == AE_CREDITS_BULLET ? x + indent : x, at, AE_ALIGN_LEFT,
				AE_COLOR_TEXT, block->lines[row]);
			at += line;
		}
		at = top + block->height;
	}
	return at - y;
}

static void credits_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &screen.flow;
	float left, right, top, bottom, text_x, text_width, pad, track, y, clip_top, clip_bottom;

	(void)instance;
	ae_flow_full_density(flow);
	ae_flow_title(flow, "CREDITS", "MENU");
	left = flow->frame.rect.x + flow->frame.rect.width * PANEL_LEFT;
	right = flow->frame.rect.x + flow->frame.rect.width * PANEL_RIGHT;
	top = ae_frame_y(&flow->frame, PANEL_TOP);
	bottom = ae_frame_y(&flow->frame, PANEL_BOTTOM);
	ae_draw_rect(left, top, right - left, bottom - top, 0.0f, AE_COLOR_PANEL);
	ae_draw_outline(left, top, right - left, bottom - top, 0.0f, ae_flow_u(flow, AE_FLOW_RULE_U), AE_COLOR_RULE);
	pad = ae_flow_u(flow, PAD_U);
	text_width = right - left - 2.0f * pad - ae_flow_u(flow, 24.0f);
	if (text_width > ae_flow_u(flow, TEXT_WIDTH_U))
		text_width = ae_flow_u(flow, TEXT_WIDTH_U);
	text_x = left + (right - left - text_width) * 0.5f;
	clip_top = top + pad;
	clip_bottom = bottom - pad;
	screen.view_height = clip_bottom - clip_top;
	y = clip_top - screen.scroll;
	ae_draw_clip_push(left, clip_top, right - left, clip_bottom - clip_top);
	y += blocks_draw(text_x, y, text_width, clip_top - ae_flow_u(flow, 100.0f), clip_bottom + ae_flow_u(flow, 100.0f));
	ae_draw_clip_pop();
	screen.content_height = y + screen.scroll - clip_top + ae_flow_u(flow, 24.0f);
	if (screen.scroll > screen.content_height - screen.view_height)
		screen.scroll = screen.content_height > screen.view_height ? screen.content_height - screen.view_height : 0.0f;
	/* (the scroll track: only when the text is taller than the panel) */
	track = ae_flow_u(flow, 4.0f);
	if (screen.content_height > screen.view_height && screen.view_height > 0.0f)
	{
		float x = right - ae_flow_u(flow, 16.0f), ty = top + ae_flow_u(flow, 24.0f), h = bottom - top - ae_flow_u(flow, 48.0f);
		float share = screen.view_height / screen.content_height;
		float at = screen.scroll / (screen.content_height - screen.view_height);

		ae_draw_rect(x, ty, track, h, 0.0f, AE_COLOR_TRACK);
		ae_draw_rect(x, ty + (h - h * share) * at, track, h * share, 0.0f, AE_COLOR_THUMB_MOUSE);
	}
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_LEFT_TRIGGER, "Page up", AE_ACTION_PAGE_UP);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_RIGHT_TRIGGER, "Page down", AE_ACTION_PAGE_DOWN);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static void scroll_by(float amount)
{
	float most = screen.content_height - screen.view_height;

	screen.scroll += amount;
	if (screen.scroll > most)
		screen.scroll = most;
	if (screen.scroll < 0.0f)
		screen.scroll = 0.0f;
}

/* back to the TAB menu it came from */
static void credits_back(void)
{
	short player = screen.player;

	platform_log("ae credits: closed");
	ae_sound_request(AE_SOUND_BACK, 0);
	ae_ui_remove(&screen);
	ae_screen_tab_menu_open(NULL, player);
}

static int credits_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		credits_back();
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		scroll_by((event->action == AE_ACTION_UP ? -0.15f : 0.15f) * screen.view_height);
		return 1;
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		scroll_by((event->action == AE_ACTION_PAGE_UP || event->action == AE_ACTION_TAB_PREVIOUS ? -1.0f : 1.0f) *
			screen.view_height * 0.9f);
		return 1;
	case AE_ACTION_HOME:
		screen.scroll = 0.0f;
		return 1;
	case AE_ACTION_END:
		scroll_by(screen.content_height);
		return 1;
	default:
		return 1;
	}
}

static void credits_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;

	if (pointer->right_clicks)
	{
		credits_back();
		return;
	}
	if (pointer->wheel_steps)
		scroll_by(-(float)pointer->wheel_steps * 0.15f * screen.view_height);
	if (ae_prompts_pointer(screen.flow.prompts, screen.flow.prompt_count, pointer, HIT_PROMPTS, &event))
		credits_handle(instance, &event);
}

static struct ae_screen_class const credits_class =
{
	.name = "credits", .handle = credits_handle, .draw = credits_draw, .pointer = credits_pointer,
};

int ae_screen_credits_open(short local_player)
{
	if (ae_ui_holds(&screen))
		return 0;
	memset(&screen, 0, sizeof(screen));
	screen.player = local_player;
	screen.flow.hit_rows = HIT_ROWS;
	screen.flow.hit_prompts = HIT_PROMPTS;
	blocks_read();
	if (!ae_ui_push(&credits_class, AE_OWNER_ANY, &screen))
		return 0;
	platform_log("ae credits: open (%s, %d blocks)", screen.from_file ? "credits.txt" : "built in", screen.count);
	return 1;
}
