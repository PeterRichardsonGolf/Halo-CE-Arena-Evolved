/* ae_test_run_card.c: Arena Evolved TEST RUN's card and note box (see ae_test_run_card.h). Every number is test-run-
mockups-v2.html's (1920 x 1080 units): the panel 530 wide at (1356, 172), a 5 left bar, padding 16 22 16 20, the header
Overpass 17 tracked .14em, the title 30, the checks 21, the hints 18 with 14 key caps. The panel and text colours are
ae_style.h's tokens; the mockup's FAIL orange and PASS green are this file's (the menus' warning token is blue). The
card is measured first (a dry run of the same code), then drawn, so the panel fits what it holds. */

#include <stdio.h>
#include <string.h>

#include "ae_style.h"
#include "ae_test_run.h"
#include "ae_test_run_card.h"
#include "ae_test_run_edit.h"
#include "ae_test_run_input.h"
#include "ae_test_run_keyboard.h"
#include "ae_test_run_note.h"
#include "ae_widgets.h"
#include "../src/ae_draw.h"

/* the mockup's FAIL orange and PASS green are not in ae_style.h (its warning token is blue); the rest are the mockup's too */
#define COLOR_FAIL 0xFE9428FFu
#define COLOR_PASS 0x7BD88FFFu
#define COLOR_REC 0xFF5A4EFFu
#define COLOR_FOCUS 0xE6ECF0FFu
#define COLOR_FIELD_TEXT 0x12181EFFu
#define COLOR_CHIP 0x1E2832D1u
#define COLOR_KEYCAP 0xD9DEE2FFu
#define COLOR_KEYCAP_TEXT 0x1A2128FFu
#define COLOR_SCRIM 0x06090C8Cu
#define COLOR_BUTTON_A 0x3DAA4AFFu
#define COLOR_BUTTON_B 0xD9443AFFu
#define COLOR_BUTTON_X 0x2F7BD6FFu
#define COLOR_BUTTON_Y 0xD9B12EFFu
#define COLOR_BUTTON_LETTER 0xFFFFFFFFu
#define COLOR_BUTTON_LETTER_Y 0x222222FFu /* (dark on the yellow) */
#define COLOR_UNSAVED_PANEL 0x2A1A0AE6u

enum
{
	CARD_Y = 172, CARD_WIDTH = 530, BAR = 5,
	BOTTOM_WIDTH = 980, BOTTOM_EDGE = 1050,
	SUMMARY_WIDTH = 700, SUMMARY_Y = 330,
	GAP = 6, /* between the card and the box */
	LINE_TEXT = 128,
	MAX_ITEMS = 12,
	MAX_LINES = 2,
};

/* a dry run measures; a drawing run draws */
static int dry;

/* ---------- text */

static float text_width(int font, float size, const char *text)
{
	return ae_draw_text_width(font, size, text);
}

static float measure_callback(const char *text, void *context)
{
	float const *spec = (float const *)context; /* { font, size, tracking in em } */

	if (spec[2] > 0.0f)
		return ae_draw_text_tracked_width((int)spec[0], spec[1], spec[2], text);
	return text_width((int)spec[0], spec[1], text);
}

/* text in a line box: the capitals centred in its height */
static void put(int font, float size, float x, float top, float line_height, int align, unsigned int color,
	const char *text)
{
	if (dry)
		return;
	ae_draw_text(font, size, x, top + (line_height - ae_draw_cap_height(font, size)) * 0.5f, align, color, text);
}

static void put_tracked(int font, float size, float tracking, float x, float top, float line_height, int align,
	unsigned int color, const char *text)
{
	if (dry)
		return;
	ae_draw_text_tracked(font, size, tracking, x, top + (line_height - ae_draw_cap_height(font, size)) * 0.5f, align,
		color, text);
}

static void rect(float x, float y, float width, float height, float radius, unsigned int color)
{
	if (!dry)
		ae_draw_rect(x, y, width, height, radius, color);
}

static void outline(float x, float y, float width, float height, float thickness, unsigned int color)
{
	if (!dry)
		ae_draw_outline(x, y, width, height, 0.0f, thickness, color);
}

static void upper(char *to, int size, const char *from)
{
	int at = 0;

	for (; from[at] && at < size - 1; at++)
		to[at] = from[at] >= 'a' && from[at] <= 'z' ? (char)(from[at] - 32) : from[at];
	to[at] = 0;
}

/* text cut with "…" to one line */
static void fit_one(int font, float size, float tracking, float width, const char *text, char *out, int out_size)
{
	float spec[3];
	int truncated;

	spec[0] = (float)font;
	spec[1] = size;
	spec[2] = tracking;
	if (!ae_test_run_card_wrap(text, width, 1, measure_callback, spec, out, out_size, &truncated))
		out[0] = 0;
}

/* ---------- hints */

enum { ITEM_KEY, ITEM_BUTTON, ITEM_TEXT };

struct item
{
	int kind;
	char cap[12], label[28];
	unsigned int color;
	float width;
};

/* the device the hints are drawn for this frame: AE's device last used (ae_test_run_input_device) */
static int card_device;

/* the glyph font of a pad family (the footers': ae_widgets_nav.c) */
static int pad_font(void)
{
	return card_device == AE_DEVICE_PLAYSTATION ? AE_FONT_PLAYSTATION : card_device == AE_DEVICE_NINTENDO ?
		AE_FONT_NINTENDO : AE_FONT_XBOX;
}

/* the glyph's AE_BUTTON_* for a face button's letter on this card (the Xbox letters name the buttons by position;
ae_draw.c's Nintendo row draws the Nintendo pad's by position too: its B glyph is the south button, AE_BUTTON_A) */
static int face_button(const char *letter)
{
	return letter[0] == 'B' ? AE_BUTTON_B : letter[0] == 'X' ? AE_BUTTON_X : letter[0] == 'Y' ? AE_BUTTON_Y : AE_BUTTON_A;
}

/* a pad key cap's words in the device's family and wording ("LB" -> L1, L; "START" -> OPTIONS, +; "BACK" -> CREATE, -);
the same as the footers' (ae_help_button_word); other caps ("D-PAD", "D-UP") as they are */
static void pad_cap(char *out, size_t size, const char *cap)
{
	const char *words = NULL;
	int device = card_device == AE_DEVICE_KEYBOARD_MOUSE ? AE_DEVICE_XBOX : card_device;

	if (!strcmp(cap, "LB"))
		words = ae_help_button_word(device, AE_BUTTON_LEFT_SHOULDER);
	else if (!strcmp(cap, "RB"))
		words = ae_help_button_word(device, AE_BUTTON_RIGHT_SHOULDER);
	else if (!strcmp(cap, "START"))
		words = ae_help_button_word(device, AE_BUTTON_START);
	else if (!strcmp(cap, "BACK"))
		words = device == AE_DEVICE_PLAYSTATION ? ae_help_button_word(device, AE_BUTTON_BACK) :
			device == AE_DEVICE_NINTENDO ? "-" : "BACK";
	if (!words)
		words = cap;
	{
		size_t at;

		for (at = 0; words[at] && at + 1 < size; at++)
			out[at] = words[at] >= 'a' && words[at] <= 'z' ? (char)(words[at] - 'a' + 'A') : words[at];
		out[at] = 0;
	}
}

static int add_item(struct item *items, int count, int kind, const char *cap, const char *label, unsigned int color)
{
	struct item *item = &items[count];

	item->kind = kind;
	if (kind == ITEM_KEY && card_device != AE_DEVICE_KEYBOARD_MOUSE)
		pad_cap(item->cap, sizeof(item->cap), cap);
	else
		snprintf(item->cap, sizeof(item->cap), "%s", cap);
	snprintf(item->label, sizeof(item->label), "%s", label);
	item->color = color;
	item->width = kind == ITEM_KEY ? text_width(AE_FONT_ROW, 14, item->cap) + 12.0f :
		kind == ITEM_BUTTON ? ae_draw_button_width(pad_font(), face_button(cap), 22.0f) : 0.0f;
	if (label[0])
		item->width += (kind == ITEM_TEXT ? 0.0f : 5.0f) + text_width(AE_FONT_BODY, 18, label);
	return count + 1;
}

/* the hint items for what the owner last touched */
static int hints(struct item *items, int pad, int bottom_card)
{
	int count = 0;

	(void)bottom_card;
	if (!pad)
	{
		count = add_item(items, count, ITEM_KEY, "F7", "Pass", 0);
		count = add_item(items, count, ITEM_KEY, "F8", "Fail", 0);
		count = add_item(items, count, ITEM_KEY, "F4", "Note", 0);
		count = add_item(items, count, ITEM_KEY, "F5", "Skip", 0);
		count = add_item(items, count, ITEM_KEY, "F1", "Reload", 0);
		count = add_item(items, count, ITEM_KEY, "HOME", "Back", 0);
		count = add_item(items, count, ITEM_KEY, "END", "Hold to hide", 0);
		return count;
	}
	count = add_item(items, count, ITEM_TEXT, "", "hold", 0);
	count = add_item(items, count, ITEM_KEY, "BACK", "+", 0);
	count = add_item(items, count, ITEM_BUTTON, "A", "Pass", COLOR_BUTTON_A);
	count = add_item(items, count, ITEM_BUTTON, "B", "Fail", COLOR_BUTTON_B);
	count = add_item(items, count, ITEM_BUTTON, "X", "Note", COLOR_BUTTON_X);
	count = add_item(items, count, ITEM_BUTTON, "Y", "Skip", COLOR_BUTTON_Y);
	count = add_item(items, count, ITEM_KEY, "D-UP", "Reload", 0);
	count = add_item(items, count, ITEM_KEY, "LB", "Back", 0);
	count = add_item(items, count, ITEM_KEY, "RB", "Hold to hide", 0);
	return count;
}

static unsigned int muted_color(void)
{
	return AE_COLOR_MUTED;
}

/* the items flowed from x over width, 24 high rows 6 apart, 16 between; returns the height */
static float flow_items(struct item const *items, int count, float x, float top, float width)
{
	float cx = x, cy = top;
	int index;

	for (index = 0; index < count; index++)
	{
		struct item const *item = &items[index];
		float ix;

		if (cx > x && cx + item->width > x + width)
		{
			cx = x;
			cy += 30.0f;
		}
		ix = cx;
		if (item->kind == ITEM_KEY)
		{
			float cap_width = text_width(AE_FONT_ROW, 14, item->cap) + 12.0f;

			rect(ix, cy + 2.0f, cap_width, 20.0f, 3.0f, COLOR_KEYCAP);
			put(AE_FONT_ROW, 14, ix + 6.0f, cy + 2.0f, 20.0f, AE_ALIGN_LEFT, COLOR_KEYCAP_TEXT, item->cap);
			ix += cap_width + 5.0f;
		}
		else if (item->kind == ITEM_BUTTON)
		{
			/* (the family's own glyph, as the footers draw it) */
			ix += ae_draw_button(pad_font(), face_button(item->cap), 22.0f, ix, cy + 1.0f,
				ae_prompt_tint(card_device, face_button(item->cap))) + 5.0f;
		}
		if (item->label[0])
			put(AE_FONT_BODY, 18, ix, cy, 24.0f, AE_ALIGN_LEFT,
				item->kind == ITEM_TEXT ? muted_color() : AE_COLOR_TEXT, item->label);
		cx += item->width + 16.0f;
	}
	return cy + 24.0f - top;
}

/* ---------- the card */

/* the header's right side: the REC dot or why there is no video; returns its width (0 for none) */
static float status(struct ae_test_run_view const *view, float right, float top)
{
	char const *text = NULL;
	unsigned int color = muted_color();
	float width;

	switch (view->video_state)
	{
		case AE_TEST_RUN_VIDEO_RECORDING: text = "REC"; color = COLOR_REC; break;
		case AE_TEST_RUN_VIDEO_SAVING: text = "SAVING CLIP"; break;
		case AE_TEST_RUN_VIDEO_WAITING: text = "WAITING FOR THE RECORDER"; break;
		case AE_TEST_RUN_VIDEO_NO_FFMPEG: text = "NO VIDEO (FFMPEG)"; color = COLOR_FAIL; break;
		case AE_TEST_RUN_VIDEO_NO_CLIP: text = "NO CLIP (RECORDER BUSY)"; color = COLOR_FAIL; break;
		default: return 0.0f;
	}
	width = ae_draw_text_tracked_width(AE_FONT_ROW, 17, 0.06f, text);
	if (view->video_state == AE_TEST_RUN_VIDEO_RECORDING)
	{
		rect(right - width - 22.0f, top + 6.0f, 12.0f, 12.0f, 6.0f, color);
		width += 22.0f;
	}
	put_tracked(AE_FONT_ROW, 17, 0.06f, right, top, 21.0f, AE_ALIGN_RIGHT, color, text);
	return width + 16.0f;
}

static void header_text(struct ae_test_run_view const *view, char *out, int size)
{
	char raw[160];

	snprintf(raw, sizeof(raw), "TEST %d / %d \xC2\xB7 %s", (int)view->number, (int)view->count, view->list);
	upper(out, size, raw);
}

/* one card body at (x, y) of width; returns its height. bottom: the strip variant */
static float card_body(struct ae_test_run_view const *view, float x, float y, float width, int bottom, int pad)
{
	float inner_x = x + BAR + (bottom ? 15.0f : 15.0f);
	float inner_w = width - BAR - 15.0f - (bottom ? 20.0f : 22.0f);
	float pad_top = bottom ? 12.0f : 16.0f;
	float cy = y + pad_top, spec[3];
	char head[LINE_TEXT], line[LINE_TEXT], lines[MAX_LINES][LINE_TEXT];
	struct item items[MAX_ITEMS];
	int count, truncated, index, rows;
	float status_width;

	/* the header, and the REC dot or the video's trouble at its right */
	status_width = status(view, inner_x + inner_w, cy);
	header_text(view, head, sizeof(head));
	{
		char cut[LINE_TEXT];

		fit_one(AE_FONT_ROW, 17, 0.14f, inner_w - status_width, head, cut, sizeof(cut));
		put_tracked(AE_FONT_ROW, 17, 0.14f, inner_x, cy, 21.0f, AE_ALIGN_LEFT, AE_COLOR_ACCENT, cut);
	}
	cy += 21.0f;
	if (bottom)
	{
		/* the title with the checks after it, on one line */
		char checks[LINE_TEXT * 3] = "";
		float title_w, rest;

		cy += 2.0f;
		fit_one(AE_FONT_ROW, 26, 0.0f, inner_w, view->title, line, sizeof(line));
		title_w = text_width(AE_FONT_ROW, 26, line);
		put(AE_FONT_ROW, 26, inner_x, cy, 29.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, line);
		if (view->loading)
			snprintf(checks, sizeof(checks), "LOADING\xE2\x80\xA6");
		else if (view->error[0])
			snprintf(checks, sizeof(checks), "%s%s", strcmp(view->error, "RESULT NOT SAVED") ? "LIST ERROR: " : "", view->error);
		else
		{
			for (index = 0; index < 3 && view->check[index][0]; index++)
				snprintf(checks + strlen(checks), sizeof(checks) - strlen(checks), "%s%s", index ? " \xC2\xB7 " : "",
					view->check[index]);
		}
		rest = inner_w - title_w - 24.0f;
		if (rest > 80.0f && checks[0])
		{
			fit_one(AE_FONT_BODY, 19, 0.0f, rest, checks, line, sizeof(line));
			put(AE_FONT_BODY, 19, inner_x + title_w + 24.0f, cy, 29.0f, AE_ALIGN_LEFT,
				view->error[0] && !view->loading ? COLOR_FAIL : AE_COLOR_TEXT, line);
		}
		cy += 29.0f + 4.0f;
		if (!view->note_open)
		{
			count = hints(items, pad, 1);
			cy += flow_items(items, count, inner_x, cy, inner_w);
		}
		return cy + 12.0f - y;
	}
	/* the title: one line at 30, else two at 24 */
	cy += 4.0f;
	spec[0] = (float)AE_FONT_ROW;
	spec[1] = 30.0f;
	spec[2] = 0.0f;
	rows = ae_test_run_card_wrap(view->title, inner_w, 1, measure_callback, spec, &lines[0][0], LINE_TEXT, &truncated);
	if (truncated)
	{
		spec[1] = 24.0f;
		rows = ae_test_run_card_wrap(view->title, inner_w, MAX_LINES, measure_callback, spec, &lines[0][0], LINE_TEXT,
			&truncated);
	}
	for (index = 0; index < rows; index++)
	{
		float size = spec[1];

		put(AE_FONT_ROW, size, inner_x, cy, size * 1.1f, AE_ALIGN_LEFT, AE_COLOR_TITLE, lines[index]);
		cy += size * 1.1f;
	}
	if (!rows)
		cy += 33.0f;
	cy += 8.0f;
	/* what to check (or why there is nothing to check) */
	spec[0] = (float)AE_FONT_BODY;
	spec[1] = 21.0f;
	spec[2] = 0.0f;
	if (view->loading)
	{
		put(AE_FONT_BODY, 21, inner_x, cy, 28.0f, AE_ALIGN_LEFT, AE_COLOR_ACCENT, "LOADING\xE2\x80\xA6");
		cy += 28.0f;
	}
	else if (view->error[0] && strcmp(view->error, "RESULT NOT SAVED"))
	{
		char error[LINE_TEXT];

		snprintf(error, sizeof(error), "LIST ERROR: %s", view->error);
		rows = ae_test_run_card_wrap(error, inner_w, 3, measure_callback, spec, &lines[0][0], LINE_TEXT, &truncated);
		for (index = 0; index < rows; index++)
		{
			put(AE_FONT_BODY, 21, inner_x, cy, 28.0f, AE_ALIGN_LEFT, COLOR_FAIL, lines[index]);
			cy += 28.0f;
		}
	}
	else
	{
		int check;

		for (check = 0; check < 3 && view->check[check][0]; check++)
		{
			rows = ae_test_run_card_wrap(view->check[check], inner_w - 26.0f, MAX_LINES, measure_callback, spec,
				&lines[0][0], LINE_TEXT, &truncated);
			outline(inner_x + 2.0f, cy + 6.0f, 16.0f, 16.0f, 2.0f, AE_COLOR_ACCENT);
			for (index = 0; index < rows; index++)
			{
				put(AE_FONT_BODY, 21, inner_x + 26.0f, cy, 28.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
				cy += 28.0f;
			}
		}
	}
	/* RESULT NOT SAVED, where it cannot be missed */
	if (!strcmp(view->error, "RESULT NOT SAVED"))
	{
		cy += 8.0f;
		rect(inner_x, cy, inner_w, 34.0f, 0.0f, COLOR_UNSAVED_PANEL);
		rect(inner_x, cy, 4.0f, 34.0f, 0.0f, COLOR_FAIL);
		put(AE_FONT_ROW, 20, inner_x + 14.0f, cy, 34.0f, AE_ALIGN_LEFT, COLOR_FAIL, view->error);
		cy += 34.0f;
	}
	/* (while the note box is open its own hints are the ones to read: the card has none, as the mockups' 2 and 3) */
	if (!view->note_open)
	{
		count = hints(items, pad, 0);
		cy += 12.0f;
		cy += flow_items(items, count, inner_x, cy, inner_w);
	}
	return cy + 16.0f - y;
}

/* ---------- the note box */

/* the keyboard's text, scrolled so that the cursor shows */
static void field_text(struct ae_test_run_view const *view, float x, float y, float width, float height)
{
	char const *text = view->note;
	int cursor = ae_test_run_note_cursor(), start = 0, length = (int)strlen(text);
	float caret_x, text_x = x + 14.0f + 5.0f, room = width - 5.0f - 28.0f;
	char before[256];

	if (cursor > length)
		cursor = length;
	for (;;)
	{
		int copy = cursor - start;

		memcpy(before, text + start, (size_t)copy);
		before[copy] = 0;
		if (text_width(AE_FONT_ROW, 22, before) <= room - 4.0f || start >= cursor)
			break;
		start++;
		while (start < cursor && ((unsigned char)text[start] & 0xC0) == 0x80)
			start++;
	}
	if (dry)
		return;
	ae_draw_clip_push(x + 5.0f, y, width - 5.0f, height);
	put(AE_FONT_ROW, 22, text_x, y, height, AE_ALIGN_LEFT, COLOR_FIELD_TEXT, text + start);
	caret_x = text_x + text_width(AE_FONT_ROW, 22, before);
	/* (the caret blinks once a second; a frame count stands in for a clock) */
	if ((ae_draw_frame() / 30u) % 2u == 0)
		ae_draw_rect(caret_x, y + (height - 22.0f) * 0.5f - 1.0f, 2.0f, 24.0f, 0.0f, COLOR_FIELD_TEXT);
	ae_draw_clip_pop();
}

/* the pad keyboard's keys (ae_test_run_keyboard.h) at (x, y): chips like the tags', the cursor's key lit; returns the height */
static float picker_keys(float x, float y, float width)
{
	struct ae_test_run_keyboard const *keyboard = ae_test_run_note_picker();
	float gap = 4.0f, key_w = (width - gap * (AE_TEST_RUN_KEYBOARD_COLUMNS - 1)) / AE_TEST_RUN_KEYBOARD_COLUMNS, key_h = 34.0f;
	int row, column;

	if (key_w > 56.0f)
		key_w = 56.0f;
	for (row = 0; row < AE_TEST_RUN_KEYBOARD_ROWS; row++)
	{
		int last = row == AE_TEST_RUN_KEYBOARD_ROWS - 1, step = last ? AE_TEST_RUN_KEYBOARD_WIDE : 1;
		float top = y + (float)row * (key_h + gap);

		for (column = 0; column < AE_TEST_RUN_KEYBOARD_COLUMNS; column += step)
		{
			char label[8];
			float left = x + (float)column * (key_w + gap), w = key_w * (float)step + gap * (float)(step - 1);
			int focus = row == keyboard->row && column == keyboard->column;

			if (last)
				snprintf(label, sizeof(label), "%s",
					ae_test_run_keyboard_special_label(keyboard, ae_test_run_keyboard_special(row, column)));
			else
			{
				label[0] = ae_test_run_keyboard_character(keyboard, row, column);
				label[1] = 0;
			}
			if (focus)
			{
				rect(left, top, w, key_h, 0.0f, COLOR_FOCUS);
				outline(left, top, w, key_h, 2.0f, AE_COLOR_ACCENT);
			}
			else
				rect(left, top, w, key_h, 0.0f, COLOR_CHIP);
			put(AE_FONT_ROW, last ? 16 : 20, left + w * 0.5f, top, key_h, AE_ALIGN_CENTER,
				focus ? COLOR_FIELD_TEXT : AE_COLOR_TITLE, label);
		}
	}
	return (float)AE_TEST_RUN_KEYBOARD_ROWS * (key_h + gap) - gap;
}

/* after a NOTE is saved: the verdict chips (PASS, FAIL, CONTINUE) in the tag chips' style, each with its verdict's colour */
static float chooser_body(struct ae_test_run_view const *view, float x, float y, float width, int pad)
{
	static const char *const label[3] = { "PASS", "FAIL", "CONTINUE" };
	const unsigned int color[3] = { COLOR_PASS, COLOR_FAIL, AE_COLOR_ACCENT };
	float inner_x = x + BAR + 15.0f, inner_w = width - BAR - 15.0f - 22.0f, cy = y + 14.0f, cx = inner_x;
	struct item items[MAX_ITEMS];
	char line[LINE_TEXT];
	int count = 0, index;

	put_tracked(AE_FONT_ROW, 17, 0.14f, inner_x, cy, 21.0f, AE_ALIGN_LEFT, AE_COLOR_ACCENT, "NOTE SAVED \xC2\xB7 WHAT NOW?");
	if (view->shot_saved)
		put_tracked(AE_FONT_ROW, 15, 0.12f, inner_x + inner_w, cy, 21.0f, AE_ALIGN_RIGHT, AE_COLOR_ACCENT,
			"SCREENSHOT SAVED");
	cy += 21.0f + 8.0f;
	if (view->note[0])
	{
		fit_one(AE_FONT_BODY, 19, 0.0f, inner_w, view->note, line, sizeof(line));
		put(AE_FONT_BODY, 19, inner_x, cy, 26.0f, AE_ALIGN_LEFT, muted_color(), line);
		cy += 26.0f + 8.0f;
	}
	cy += 2.0f;
	for (index = 0; index < 3; index++)
	{
		float chip_w = text_width(AE_FONT_ROW, 20, label[index]) + 36.0f + 4.0f, chip_h = 42.0f;

		if (index == view->choice)
		{
			rect(cx, cy, chip_w, chip_h, 0.0f, COLOR_FOCUS);
			rect(cx, cy, 5.0f, chip_h, 0.0f, color[index]);
			outline(cx, cy, chip_w, chip_h, 2.0f, color[index]);
			put(AE_FONT_ROW, 20, cx + 20.0f, cy, chip_h, AE_ALIGN_LEFT, COLOR_FIELD_TEXT, label[index]);
		}
		else
		{
			rect(cx, cy, chip_w, chip_h, 0.0f, COLOR_CHIP);
			rect(cx, cy, 5.0f, chip_h, 0.0f, color[index]);
			put(AE_FONT_ROW, 20, cx + 20.0f, cy, chip_h, AE_ALIGN_LEFT, color[index], label[index]);
		}
		cx += chip_w + 8.0f;
	}
	cy += 42.0f + 12.0f;
	if (!pad)
	{
		count = add_item(items, count, ITEM_KEY, "F7", "Pass", 0);
		count = add_item(items, count, ITEM_KEY, "F8", "Fail", 0);
		count = add_item(items, count, ITEM_KEY, "F5", "Continue", 0);
		count = add_item(items, count, ITEM_KEY, "ARROWS", "Move", 0);
		count = add_item(items, count, ITEM_KEY, "ENTER", "Pick", 0);
		count = add_item(items, count, ITEM_KEY, "ESC", "Stay on this test", 0);
	}
	else
	{
		count = add_item(items, count, ITEM_KEY, "D-PAD", "Move", 0);
		count = add_item(items, count, ITEM_BUTTON, "A", "Pick", COLOR_BUTTON_A);
		count = add_item(items, count, ITEM_BUTTON, "B", "Stay on this test", COLOR_BUTTON_B);
	}
	cy += flow_items(items, count, inner_x, cy, inner_w) + 16.0f;
	return cy - y;
}

static float note_body(struct ae_test_run_view const *view, float x, float y, float width, int pad)
{
	float inner_x = x + BAR + 15.0f, inner_w = width - BAR - 15.0f - 22.0f, cy = y + 14.0f;
	unsigned int accent = view->note_fail ? COLOR_FAIL : AE_COLOR_ACCENT;
	char head[LINE_TEXT];
	struct item items[MAX_ITEMS];
	int count, picker = ae_test_run_note_picker_open();

	if (view->verdict_open)
		return chooser_body(view, x, y, width, pad);
	snprintf(head, sizeof(head), "%s", view->note_fail ? "FAIL \xC2\xB7 WHAT WENT WRONG?" : "NOTE \xC2\xB7 ANYTHING TO ADD?");
	put_tracked(AE_FONT_ROW, 17, 0.14f, inner_x, cy, 21.0f, AE_ALIGN_LEFT, accent, head);
	if (view->shot_saved)
		put_tracked(AE_FONT_ROW, 15, 0.12f, inner_x + inner_w, cy, 21.0f, AE_ALIGN_RIGHT, AE_COLOR_ACCENT,
			"SCREENSHOT SAVED");
	cy += 21.0f + 8.0f;
	if (!pad || picker)
	{
		float field_h = 44.0f;

		rect(inner_x, cy, inner_w, field_h, 0.0f, COLOR_FOCUS);
		rect(inner_x, cy, 5.0f, field_h, 0.0f, AE_COLOR_ACCENT);
		field_text(view, inner_x, cy, inner_w, field_h);
		cy += field_h + (picker ? 10.0f : 12.0f);
	}
	if (!pad && !picker)
	{
		count = 0;
		count = add_item(items, count, ITEM_KEY, "ENTER", "Save and next", 0);
		count = add_item(items, count, ITEM_KEY, "ESC", "Back to the test", 0);
	}
	else if (picker)
	{
		cy += picker_keys(inner_x, cy, inner_w) + 12.0f;
		count = 0;
		count = add_item(items, count, ITEM_BUTTON, "A", "Type", COLOR_BUTTON_A);
		count = add_item(items, count, ITEM_BUTTON, "X", "Delete", COLOR_BUTTON_X);
		count = add_item(items, count, ITEM_BUTTON, "Y", "Space", COLOR_BUTTON_Y);
		count = add_item(items, count, ITEM_KEY, "LB", "Case", 0);
		count = add_item(items, count, ITEM_KEY, "RB", "Symbols", 0);
		count = add_item(items, count, ITEM_KEY, "START", "Save and next", 0);
		count = add_item(items, count, ITEM_BUTTON, "B", "Back to the tags", COLOR_BUTTON_B);
	}
	else
	{
		float cx = inner_x;
		int index, tags = ae_test_run_note_tag_count();

		cy += 2.0f;
		for (index = 0; index < tags; index++)
		{
			char label[40];
			float chip_w, chip_h = 38.0f;

			snprintf(label, sizeof(label), "%s", ae_test_run_note_tag_label(index));
			chip_w = text_width(AE_FONT_ROW, 18, label) + 24.0f + 4.0f;
			if (cx > inner_x && cx + chip_w > inner_x + inner_w)
			{
				cx = inner_x;
				cy += chip_h + 8.0f;
			}
			if (index == ae_test_run_note_tag_index())
			{
				rect(cx, cy, chip_w, chip_h, 0.0f, COLOR_FOCUS);
				outline(cx, cy, chip_w, chip_h, 2.0f, AE_COLOR_ACCENT);
				put(AE_FONT_ROW, 18, cx + 14.0f, cy, chip_h, AE_ALIGN_LEFT, COLOR_FIELD_TEXT, label);
			}
			else
			{
				rect(cx, cy, chip_w, chip_h, 0.0f, COLOR_CHIP);
				put(AE_FONT_ROW, 18, cx + 14.0f, cy, chip_h, AE_ALIGN_LEFT, AE_COLOR_TITLE, label);
			}
			cx += chip_w + 8.0f;
		}
		cy += 38.0f + 12.0f;
		count = 0;
		count = add_item(items, count, ITEM_KEY, "D-PAD", "Move", 0);
		count = add_item(items, count, ITEM_BUTTON, "A", "Save and next", COLOR_BUTTON_A);
		count = add_item(items, count, ITEM_BUTTON, "B", "Back to the test", COLOR_BUTTON_B);
	}
	cy += flow_items(items, count, inner_x, cy, inner_w) + 16.0f;
	return cy - y;
}

/* a panel at (x, y): measured with a dry run, then the bar and the text drawn */
typedef float (*body_function)(struct ae_test_run_view const *view, float x, float y, float width, int bottom, int pad);

static float note_adapter(struct ae_test_run_view const *view, float x, float y, float width, int bottom, int pad)
{
	(void)bottom;
	return note_body(view, x, y, width, pad);
}

static float panel(struct ae_test_run_view const *view, body_function body, float x, float y, float width, int bottom,
	int pad, unsigned int bar)
{
	float height;

	dry = 1;
	height = body(view, x, y, width, bottom, pad);
	dry = 0;
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(x, y, (float)BAR, height, 0.0f, bar);
	body(view, x, y, width, bottom, pad);
	return height;
}

/* ---------- the summary */

static void count_cell(float x, float y, int number, const char *label, unsigned int color)
{
	char text[16];

	snprintf(text, sizeof(text), "%d", number);
	put(AE_FONT_ROW, 44, x, y, 52.0f, AE_ALIGN_LEFT, color, text);
	put_tracked(AE_FONT_ROW, 16, 0.12f, x, y + 52.0f, 20.0f, AE_ALIGN_LEFT, color, label);
}

static void summary(struct ae_test_run_view const *view, float layout_width, int pad)
{
	float x = (float)(int)((layout_width - SUMMARY_WIDTH) * 0.5f), y = SUMMARY_Y, cy, cx;
	float inner_x = x + BAR + 27.0f, inner_w = SUMMARY_WIDTH - BAR - 27.0f - 32.0f;
	char head[LINE_TEXT], title[48];
	struct item items[MAX_ITEMS];
	int total = view->passed + view->failed + view->skipped, count; /* (a NOTE is no verdict: not a test checked) */
	float height;

	ae_draw_rect(0.0f, 0.0f, layout_width, 1080.0f, 0.0f, COLOR_SCRIM);
	snprintf(title, sizeof(title), "%d test%s checked", total, total == 1 ? "" : "s");
	{
		char raw[160];

		snprintf(raw, sizeof(raw), "%s \xC2\xB7 DONE", view->list);
		upper(head, sizeof(head), raw);
	}
	if (pad)
	{
		/* (a pad ends the run with the BACK chord, as the card's own row: hold BACK + A) */
		count = add_item(items, 0, ITEM_TEXT, "", "hold", 0);
		count = add_item(items, count, ITEM_KEY, "BACK", "+", 0);
		count = add_item(items, count, ITEM_BUTTON, "A", "Finish testing", COLOR_BUTTON_A);
	}
	else
		count = add_item(items, 0, ITEM_KEY, "F7", "Finish testing", 0);
	/* (height: 26 + header 21 + 8 + title 40 + 18 + counts 72 + 16 + line 28 + 12 + hints 24 + 26) */
	height = 26.0f + 21.0f + 8.0f + 44.0f + 18.0f + 72.0f + 16.0f + 28.0f + 12.0f + 24.0f + 26.0f;
	ae_draw_rect(x, y, SUMMARY_WIDTH, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(x, y, (float)BAR, height, 0.0f, AE_COLOR_ACCENT);
	cy = y + 26.0f;
	put_tracked(AE_FONT_ROW, 17, 0.14f, inner_x, cy, 21.0f, AE_ALIGN_LEFT, AE_COLOR_ACCENT, head);
	cy += 21.0f + 8.0f;
	put(AE_FONT_ROW, 40, inner_x, cy, 44.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, title);
	cy += 44.0f + 18.0f;
	cx = inner_x;
	count_cell(cx, cy, view->passed, "PASS", COLOR_PASS);
	cx += 40.0f + 100.0f;
	count_cell(cx, cy, view->failed, "FAIL", COLOR_FAIL);
	cx += 140.0f;
	count_cell(cx, cy, view->noted, "NOTE", AE_COLOR_ACCENT);
	cx += 140.0f;
	count_cell(cx, cy, view->skipped, "SKIP", muted_color());
	cy += 72.0f + 16.0f;
	put(AE_FONT_BODY, 21, inner_x, cy, 28.0f, AE_ALIGN_LEFT, muted_color(),
		"Results saved. The agents read them; no need to copy anything.");
	if (view->error[0])
		put(AE_FONT_ROW, 21, inner_x + inner_w, cy, 28.0f, AE_ALIGN_RIGHT, COLOR_FAIL, view->error);
	cy += 28.0f + 12.0f;
	flow_items(items, count, inner_x, cy, inner_w);
}

/* ---------- the entry */

void ae_test_run_card_draw(void)
{
	struct ae_test_run_view view;
	struct ae_layout layout;
	float right, x, y, height;
	int pad;

	ae_test_run_view(&view);
	if (!view.active || view.hidden || !ae_draw_available())
		return;
	/* (the verdict chooser sits where the note box does, and the card drops its hints for it too) */
	view.note_open |= view.verdict_open;
	ae_draw_view_full();
	ae_draw_current_layout(&layout);
	/* (the mockup's frame is 16:9: its right edge, 34 in, on a wider screen; the screen's own on a narrower one) */
	right = layout.width > 1920.0f ? (layout.width - 1920.0f) * 0.5f + 1886.0f : layout.width - 34.0f;
	card_device = ae_test_run_input_device();
	pad = card_device != AE_DEVICE_KEYBOARD_MOUSE;
	dry = 0;
	if (view.summary)
	{
		summary(&view, layout.width, pad);
		return;
	}
	if (view.card_bottom)
	{
		/* (measured first: the strip sits on the bottom edge) */
		dry = 1;
		height = card_body(&view, 0.0f, 0.0f, (float)BOTTOM_WIDTH, 1, pad);
		dry = 0;
		x = (float)(int)((layout.width - BOTTOM_WIDTH) * 0.5f);
		y = BOTTOM_EDGE - height;
		panel(&view, card_body, x, y, (float)BOTTOM_WIDTH, 1, pad, AE_COLOR_ACCENT);
		if (view.note_open)
		{
			float box_h;

			dry = 1;
			box_h = note_body(&view, 0.0f, 0.0f, (float)BOTTOM_WIDTH, pad);
			dry = 0;
			panel(&view, note_adapter, x, y - GAP - box_h, (float)BOTTOM_WIDTH, 1, pad,
				view.note_fail ? COLOR_FAIL : AE_COLOR_ACCENT);
		}
		return;
	}
	x = right - CARD_WIDTH;
	y = CARD_Y;
	height = panel(&view, card_body, x, y, (float)CARD_WIDTH, 0, pad, AE_COLOR_ACCENT);
	if (view.note_open)
		panel(&view, note_adapter, x, y + height + GAP, (float)CARD_WIDTH, 0, pad,
			view.note_fail ? COLOR_FAIL : AE_COLOR_ACCENT);
}
