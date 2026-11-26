/*
AE_THEATER_STRIP.C

See ae_theater_strip.h. 1080 tall layout units. The panels are AE's view-panel fill with a 4 u ice-blue edge, the text AE's body and title
faces, the prompts the footer's own glyphs; the margins to the frame's edges are 48 u at the sides and 40 u at the bottom, and no two
things are nearer than 8 u. The seek line is 6 u thick with a 20 u playhead; the bar is 112 u tall.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_style.h"
#include "ae_theater_rules.h"
#include "ae_theater_strip.h"
#include "network_replay.h"

#define SIDE_U 48.0f
#define BOTTOM_U 40.0f
#define TOP_U 40.0f
#define PANEL_PAD_U 28.0f
#define BAR_HEIGHT_U 112.0f
#define CHIP_HEIGHT_U 64.0f
#define EDGE_U 4.0f
#define LINE_U 6.0f
#define HEAD_U 20.0f
#define PROMPT_U 28.0f
#define LABEL_U 22.0f
#define PROMPT_GAP_U 32.0f
#define CARD_WIDTH_U 560.0f
#define CARD_HEIGHT_U 176.0f
/* the colour a seek clears the view to: the scrim's, whole */
#define AE_COLOR_SEEK_COVER 0x04070AFFu

/* text of a font whose capitals are centred on middle (ae_draw_text takes the capitals' top) */
static float centred_text(int font, float size, float x, float middle, int align, unsigned int rgba, char const *text)
{
	return ae_draw_text(font, size, x, middle - ae_draw_cap_height(font, size) * 0.5f, align, rgba, text);
}

/* a prompt's width: its glyphs (one or two), the gap and the label */
static float prompt_width(int first, int second, char const *label)
{
	float width = ae_draw_button_width(AE_FONT_LAST_DEVICE, first, PROMPT_U);

	if (second >= 0)
		width += 6.0f + ae_draw_button_width(AE_FONT_LAST_DEVICE, second, PROMPT_U);
	return width + 10.0f + ae_draw_text_width(AE_FONT_BODY, LABEL_U, label);
}

/* a prompt from x, its glyphs' ink centred on middle; returns its right edge */
static float prompt_draw(int first, int second, char const *label, float x, float middle)
{
	float top = 0.0f, bottom = PROMPT_U;

	ae_draw_button_ink(AE_FONT_LAST_DEVICE, first, PROMPT_U, &top, &bottom);
	x += ae_draw_button(AE_FONT_LAST_DEVICE, first, PROMPT_U, x, middle - (top + bottom) * 0.5f, AE_COLOR_PROMPT_WHITE);
	if (second >= 0)
	{
		x += 6.0f;
		ae_draw_button_ink(AE_FONT_LAST_DEVICE, second, PROMPT_U, &top, &bottom);
		x += ae_draw_button(AE_FONT_LAST_DEVICE, second, PROMPT_U, x, middle - (top + bottom) * 0.5f, AE_COLOR_PROMPT_WHITE);
	}
	x += 10.0f;
	centred_text(AE_FONT_BODY, LABEL_U, x, middle, AE_ALIGN_LEFT, AE_COLOR_TEXT, label);
	return x + ae_draw_text_width(AE_FONT_BODY, LABEL_U, label);
}

struct prompt_item
{
	int first, second;
	char const *label;
};

/* the prompts that fit in width, from the first, drawn from x on middle */
static void prompts_draw(struct prompt_item const *items, int count, float x, float middle, float width)
{
	float used = 0.0f;
	int each;

	for (each = 0; each < count; each++)
	{
		float next = prompt_width(items[each].first, items[each].second, items[each].label);

		if (each && used + PROMPT_GAP_U + next > width)
			break;
		if (each)
			used += PROMPT_GAP_U;
		prompt_draw(items[each].first, items[each].second, items[each].label, x + used, middle);
		used += next;
	}
}

static void chip_draw(void)
{
	char who[48], text[96], speed[16];
	long number = network_replay_watched_number(who, sizeof(who));
	float width, text_width, x = SIDE_U;

	ae_theater_speed_text(network_replay_speed_percent(), speed, sizeof(speed));
	if (number)
		snprintf(text, sizeof(text), "PLAYER %ld  \xC2\xB7  %s  \xC2\xB7  %s  \xC2\xB7  %s", number, who,
			network_replay_view_name(0, 1), speed);
	else
		snprintf(text, sizeof(text), "%s  \xC2\xB7  %s", network_replay_view_is_free() ? "FREE CAMERA" : "WAITING FOR A PLAYER", speed);
	text_width = ae_draw_text_width(AE_FONT_ROW, 26.0f, text);
	width = PANEL_PAD_U + text_width + PANEL_PAD_U;
	ae_draw_rect(x, TOP_U, width, CHIP_HEIGHT_U, 0.0f, AE_COLOR_VIEW_PANEL);
	ae_draw_rect(x, TOP_U, EDGE_U, CHIP_HEIGHT_U, 0.0f, AE_COLOR_ACCENT);
	centred_text(AE_FONT_ROW, 26.0f, x + PANEL_PAD_U, TOP_U + CHIP_HEIGHT_U * 0.5f, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
}

static void bar_draw(struct ae_layout const *layout, int paused)
{
	float x = SIDE_U, width = layout->width - 2.0f * SIDE_U, y = layout->height - BOTTOM_U - BAR_HEIGHT_U;
	float inner = x + PANEL_PAD_U, inner_width = width - 2.0f * PANEL_PAD_U;
	long position = network_replay_position_seconds(), length = network_replay_length_seconds();
	char left[16], right[16];
	struct prompt_item items[6];
	int count = 0;
	float line_x, line_width, line_y, progress, time_width;

	ae_draw_rect(x, y, width, BAR_HEIGHT_U, 0.0f, AE_COLOR_VIEW_PANEL);
	ae_draw_rect(x, y, width, EDGE_U, 0.0f, AE_COLOR_ACCENT);
	/* (the prompts: the pause has two, a game playing six, those that fit) */
	if (paused)
	{
		/* (Resume and Leave are the card's: not said twice) */
		items[count++] = (struct prompt_item){ AE_BUTTON_DPAD_LEFT, AE_BUTTON_DPAD_RIGHT, "Skip 10 s" };
		items[count++] = (struct prompt_item){ AE_BUTTON_LEFT_SHOULDER, AE_BUTTON_RIGHT_SHOULDER, "Speed" };
	}
	else
	{
		items[count++] = (struct prompt_item){ AE_BUTTON_DPAD_LEFT, AE_BUTTON_DPAD_RIGHT, "Skip 10 s" };
		items[count++] = (struct prompt_item){ AE_BUTTON_LEFT_SHOULDER, AE_BUTTON_RIGHT_SHOULDER, "Speed" };
		items[count++] = (struct prompt_item){ AE_BUTTON_START, -1, "Pause" };
		items[count++] = (struct prompt_item){ AE_BUTTON_A, AE_BUTTON_X, "Player" };
		items[count++] = (struct prompt_item){ AE_BUTTON_Y, -1, network_replay_view_name(1, 0) };
		items[count++] = (struct prompt_item){ AE_BUTTON_B, -1, "Hide" };
	}
	prompts_draw(items, count, inner, y + 34.0f, inner_width);
	/* (the seek bar: the time, the line with the playhead, the length) */
	ae_theater_time_text(position, left, sizeof(left));
	ae_theater_time_text(length, right, sizeof(right));
	time_width = ae_draw_text_width(AE_FONT_BODY, LABEL_U, "0:00:00");
	line_y = y + 84.0f;
	centred_text(AE_FONT_BODY, LABEL_U, inner, line_y, AE_ALIGN_LEFT, AE_COLOR_TEXT, left);
	centred_text(AE_FONT_BODY, LABEL_U, inner + inner_width, line_y, AE_ALIGN_RIGHT, AE_COLOR_MUTED, right);
	line_x = inner + time_width + 16.0f;
	line_width = inner_width - 2.0f * (time_width + 16.0f);
	progress = ae_theater_progress(position, length);
	ae_draw_rect(line_x, line_y - LINE_U * 0.5f, line_width, LINE_U, LINE_U * 0.5f, AE_COLOR_TRACK);
	if (progress > 0.0f)
		ae_draw_rect(line_x, line_y - LINE_U * 0.5f, line_width * progress, LINE_U, LINE_U * 0.5f, AE_COLOR_ACCENT);
	ae_draw_rect(line_x + line_width * progress - HEAD_U * 0.5f, line_y - HEAD_U * 0.5f, HEAD_U, HEAD_U, HEAD_U * 0.5f,
		AE_COLOR_SELECTION);
}

static void pause_card_draw(struct ae_layout const *layout)
{
	float x = (layout->width - CARD_WIDTH_U) * 0.5f, y = layout->height * 0.36f;
	float inner = x + PANEL_PAD_U;
	struct prompt_item items[2] = { { AE_BUTTON_START, -1, "Resume" }, { AE_BUTTON_B, -1, "Leave" } };

	ae_draw_rect(x, y, CARD_WIDTH_U, CARD_HEIGHT_U, 0.0f, AE_COLOR_VIEW_PANEL);
	ae_draw_rect(x, y, CARD_WIDTH_U, EDGE_U, 0.0f, AE_COLOR_ACCENT);
	ae_draw_text_tracked(AE_FONT_BODY, 20.0f, 0.12f, inner, y + 44.0f - ae_draw_cap_height(AE_FONT_BODY, 20.0f) * 0.5f,
		AE_ALIGN_LEFT, AE_COLOR_MUTED, "REPLAY");
	centred_text(AE_FONT_TITLE, 44.0f, inner, y + 92.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, "PAUSED");
	prompts_draw(items, 2, inner, y + CARD_HEIGHT_U - 36.0f, CARD_WIDTH_U - 2.0f * PANEL_PAD_U);
}

/* a seek's card over a view cleared to black: the game is played to the second asked for at speed (or loaded again for a seek
back) and none of that is shown. The panel is the pause card's size; the line under the title is the seek bar's, the way played */
static void seek_card_draw(struct ae_layout const *layout, long target, float progress)
{
	float x = (layout->width - CARD_WIDTH_U) * 0.5f, y = layout->height * 0.36f;
	float inner = x + PANEL_PAD_U, inner_width = CARD_WIDTH_U - 2.0f * PANEL_PAD_U, line_y = y + CARD_HEIGHT_U - 44.0f;
	char time[16], text[32];

	ae_draw_rect(0.0f, 0.0f, layout->width, layout->height, 0.0f, AE_COLOR_SEEK_COVER);
	ae_draw_rect(x, y, CARD_WIDTH_U, CARD_HEIGHT_U, 0.0f, AE_COLOR_VIEW_PANEL);
	ae_draw_rect(x, y, CARD_WIDTH_U, EDGE_U, 0.0f, AE_COLOR_ACCENT);
	ae_theater_time_text(target, time, sizeof(time));
	snprintf(text, sizeof(text), "SEEKING %s", time);
	ae_draw_text_tracked(AE_FONT_BODY, 20.0f, 0.12f, inner, y + 44.0f - ae_draw_cap_height(AE_FONT_BODY, 20.0f) * 0.5f,
		AE_ALIGN_LEFT, AE_COLOR_MUTED, "REPLAY");
	centred_text(AE_FONT_TITLE, 44.0f, inner, y + 92.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	ae_draw_rect(inner, line_y - LINE_U * 0.5f, inner_width, LINE_U, LINE_U * 0.5f, AE_COLOR_TRACK);
	if (progress > 0.0f)
		ae_draw_rect(inner, line_y - LINE_U * 0.5f, inner_width * progress, LINE_U, LINE_U * 0.5f, AE_COLOR_ACCENT);
}

void ae_theater_strip_draw(void)
{
	static unsigned int drawn_frame;
	static int drawn;
	struct ae_layout layout;
	unsigned int frame;
	int paused;
	long target = 0;
	float progress = 0.0f;
	int seeking;

	if (!network_replay_playing() || !ae_draw_available())
		return;
	seeking = network_replay_seek_progress(&target, &progress);
	if (network_replay_playback_starting() && !seeking)
		return;
	frame = ae_draw_frame();
	if (drawn && drawn_frame == frame)
		return;
	drawn = 1;
	drawn_frame = frame;
	paused = network_replay_paused() && !seeking;
	if (!seeking && network_replay_lines_hidden() && !paused)
		return;
	ae_draw_view_full();
	ae_draw_current_layout(&layout);
	if (seeking)
	{
		seek_card_draw(&layout, target, progress);
		return;
	}
	if (!network_replay_lines_hidden())
		chip_draw();
	bar_draw(&layout, paused);
	if (paused)
		pause_card_draw(&layout);
}
