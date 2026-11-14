/*
AE_SCREEN_ATTENTION.C

The attention dialog (ae_screen_attention.h). Its numbers are the mockups' 1080p ones (final/6-join-helper-workshop,
final/7-vpn-confirm): 880 u wide, 40 u in, the title 40 u, the reason 19 u on 28 u lines, the item block 96 u, the
buttons 48 u with 16 u between, the note 16.5 u.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_motion.h"
#include "ae_screen_attention.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum { ATT_HIT = 0x7E30, HIT_BUTTONS = ATT_HIT + 1, HIT_PROMPTS, HIT_OUTSIDE };

#define BOX_WIDTH_U 880.0f
#define PAD_U 40.0f
#define STRIPE_U 4.0f
#define TITLE_U 40.0f
#define BODY_U 19.0f
#define LEAD_U 28.0f
#define LIST_U 27.0f
#define LIST_MAX 24
#define ITEM_U 96.0f
#define BUTTON_U 48.0f
#define BUTTON_GAP_U 16.0f
#define NOTE_U 16.5f
#define PANEL_FILL 0x181D25EBu       /* rgba(24,29,37,.92): an unfocused button's fill */
#define DANGER_EDGE 0xC0392BFFu      /* the team red's frame (192,57,43) */
#define DANGER_INK 0xEC8074FFu       /* its light tone (236,128,116) */
#define DANGER_FOCUS_INK 0x96281EFFu /* on the white bar */

static struct
{
	struct ae_attention_spec spec;
	char list[LIST_MAX][96];
	short list_count;
	char title[96], body[400], strong[160], item_title[96], item_line1[96], item_line2[96], item_tile[24];
	char button0[48], button1[48], note[160], back_label[32];
	short focus;
	int open;
	/* the buttons as drawn (drawing units) */
	struct ae_rect rects[2];
	float footer_y;
} dialog;

static struct ae_screen_class const attention_class;

static float un(float spec_units)
{
	return spec_units * dialog.spec.density.unit;
}

static float cap_top(int font, float size, float baseline_y)
{
	return baseline_y - ae_draw_cap_height(font, size);
}

static void pick(short button)
{
	void (*picked)(short, void *) = dialog.spec.picked;
	void *context = dialog.spec.context;

	ae_sound_request(button >= 0 ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
	dialog.open = 0;
	if (ae_ui_top() && ae_ui_top()->data == &dialog)
		ae_ui_pop();
	else
		ae_ui_remove(&dialog);
	if (picked)
		picked(button, context);
}

static void focus_step(short direction)
{
	if (dialog.spec.button_count < 2)
		return;
	dialog.focus = (short)((dialog.focus + direction + 2) % 2);
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static int attention_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_ACCEPT:
		pick(dialog.focus);
		return 1;
	case AE_ACTION_BACK:
		pick(dialog.spec.back);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_UP:
		focus_step(-1);
		return 1;
	case AE_ACTION_RIGHT:
	case AE_ACTION_DOWN:
	case AE_ACTION_TAB_NEXT:
	case AE_ACTION_TAB_PREVIOUS:
		focus_step(1);
		return 1;
	default:
		return 1;
	}
}

static void prompts_fill(struct ae_prompt *prompts)
{
	prompts[0] = (struct ae_prompt){ AE_BUTTON_A, ae_string(AE_STR_SELECT), NULL, AE_ACTION_ACCEPT, 0 };
	prompts[1] = (struct ae_prompt){ AE_BUTTON_B, dialog.back_label[0] ? dialog.back_label : ae_string(AE_STR_BACK),
		NULL, AE_ACTION_BACK, 0 };
}

static void attention_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	struct ae_prompt prompts[2];

	prompts_fill(prompts);
	if (ae_prompts_pointer(prompts, 2, pointer, HIT_PROMPTS, &event))
	{
		attention_handle(instance, &event);
		return;
	}
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit) || hit.id != HIT_BUTTONS || hit.index < 0 || hit.index >= 2)
		return;
	if (pointer->moved && dialog.focus != hit.index)
		dialog.focus = hit.index;
	if (pointer->left_clicks)
		pick(hit.index);
}

static void button_draw(struct ae_rect const *r, char const *label, int focused, int danger, short index)
{
	struct ae_density const *d = &dialog.spec.density;
	float size = un(22.0f), notch = un(6.0f), line = un(2.0f);
	unsigned int ink;

	if (focused)
	{
		ae_draw_rect(r->x, r->y, r->width, r->height, 0.0f, AE_COLOR_SELECTION);
		ae_draw_rect(r->x, r->y, notch, r->height, 0.0f, danger ? DANGER_EDGE : AE_COLOR_ACCENT);
		ink = danger ? DANGER_FOCUS_INK : AE_COLOR_SELECTION_TEXT;
	}
	else
	{
		ae_draw_rect(r->x, r->y, r->width, r->height, 0.0f, PANEL_FILL);
		ae_draw_outline(r->x, r->y, r->width, r->height, 0.0f, line, danger ? DANGER_EDGE : AE_COLOR_WARNING);
		ink = danger ? DANGER_INK : 0xFFFFFFFFu;
	}
	(void)d;
	ae_draw_text(AE_FONT_ROW, size, r->x + r->width * 0.5f, cap_top(AE_FONT_ROW, size, r->y + r->height * 0.5f +
		ae_draw_cap_height(AE_FONT_ROW, size) * 0.5f), AE_ALIGN_CENTER, ink, label);
	ae_hit_add(r->x, r->y, r->width, r->height, HIT_BUTTONS, AE_PART_ITEM, index);
}

static void attention_draw(struct ae_screen *instance)
{
	struct ae_attention_spec const *spec = &dialog.spec;
	struct ae_density const *d = &spec->density;
	char lines[6][160];
	struct ae_prompt prompts[2];
	float view_width = ae_draw_view_width(), width = un(BOX_WIDTH_U), pad = un(PAD_U), body = un(BODY_U), lead = un(LEAD_U);
	float x0, y0, ix, iw, y, height, buttons_top, buttons_bottom, list_u = un(LIST_U);
	int count, index, strong_lines = 0;
	char const *labels[2];

	(void)instance;
	labels[0] = dialog.button0;
	labels[1] = dialog.button1;
	iw = width - 2.0f * pad;
	count = ae_wrap_text(AE_FONT_BODY, body, dialog.body, iw - un(24.0f), lines, 6);
	if (dialog.strong[0])
		strong_lines = 1;
	/* (the height: title, the lines, an item block, the buttons, a note) */
	height = un(66.0f + 40.0f) + lead * (float)(count + strong_lines) + list_u * (float)dialog.list_count + (dialog.list_count ? un(20.0f) : 0.0f) + (dialog.item_title[0] ? un(8.0f + ITEM_U + 20.0f) :
		un(24.0f)) + un(BUTTON_U) + (dialog.note[0] ? un(50.0f) : un(26.0f));
	x0 = (view_width - width) * 0.5f;
	y0 = ((float)AE_LAYOUT_HEIGHT - height) * 0.5f;
	ix = x0 + pad;
	/* the scrim over the whole view, and the clicks outside it taken */
	ae_draw_rect(-(float)AE_LAYOUT_HEIGHT, -(float)AE_LAYOUT_HEIGHT, view_width + 2.0f * AE_LAYOUT_HEIGHT,
		3.0f * AE_LAYOUT_HEIGHT, 0.0f, AE_COLOR_SCRIM);
	ae_hit_add(0.0f, 0.0f, view_width, (float)AE_LAYOUT_HEIGHT, HIT_OUTSIDE, AE_PART_OUTSIDE, -1);
	ae_draw_rect(x0, y0, width, height, un(2.0f), AE_COLOR_POPOVER);
	ae_hit_add(x0, y0, width, height, HIT_OUTSIDE, AE_PART_CARD, -1);
	ae_draw_rect(x0, y0, width, un(STRIPE_U), 0.0f, AE_COLOR_ACCENT);
	ae_draw_text(AE_FONT_TITLE, un(TITLE_U), ix, cap_top(AE_FONT_TITLE, un(TITLE_U), y0 + un(66.0f)), AE_ALIGN_LEFT,
		0xFFFFFFFFu, dialog.title);
	y = y0 + un(106.0f);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, ix, cap_top(AE_FONT_BODY, body, y + lead * (float)index), AE_ALIGN_LEFT,
			AE_COLOR_TEXT, lines[index]);
	/* (the list: a small ice square and the line, the lines apart a little less than the reason's) */
	y += lead * (float)count + un(6.0f);
	for (index = 0; index < dialog.list_count; index++)
	{
		float row_y = y + list_u * (float)index;

		ae_draw_rect(ix + un(8.0f), row_y - un(11.0f), un(8.0f), un(8.0f), 0.0f, AE_COLOR_ACCENT);
		ae_draw_text(AE_FONT_BODY, body, ix + un(30.0f), cap_top(AE_FONT_BODY, body, row_y), AE_ALIGN_LEFT,
			AE_COLOR_TEXT, dialog.list[index]);
	}
	y += list_u * (float)dialog.list_count;
	if (strong_lines)
		ae_draw_text(AE_FONT_ROW, body, ix, cap_top(AE_FONT_ROW, body, y + (dialog.list_count ? un(14.0f) : 0.0f)),
			AE_ALIGN_LEFT, AE_COLOR_TITLE, dialog.strong);
	y = y0 + un(106.0f) + lead * (float)(count + strong_lines) + list_u * (float)dialog.list_count +
		(strong_lines && dialog.list_count ? un(14.0f) : 0.0f);
	if (dialog.item_title[0])
	{
		float top = y + un(8.0f), tile = un(128.0f);

		ae_draw_rect(ix, top, iw, un(ITEM_U), 0.0f, AE_COLOR_ROW);
		ae_draw_outline(ix + un(12.0f), top + un(12.0f), tile, un(72.0f), 0.0f, un(1.0f), 0xC8D6E05Au);
		ae_draw_text_tracked(AE_FONT_ROW, un(11.0f), 0.1f, ix + un(76.0f), cap_top(AE_FONT_ROW, un(11.0f), top + un(53.0f)),
			AE_ALIGN_CENTER, AE_COLOR_MUTED, dialog.item_tile);
		ae_draw_text(AE_FONT_ROW, un(22.0f), ix + un(160.0f), cap_top(AE_FONT_ROW, un(22.0f), top + un(36.0f)),
			AE_ALIGN_LEFT, AE_COLOR_TITLE, dialog.item_title);
		ae_draw_text(AE_FONT_BODY, un(18.0f), ix + un(160.0f), cap_top(AE_FONT_BODY, un(18.0f), top + un(62.0f)),
			AE_ALIGN_LEFT, AE_COLOR_TEXT, dialog.item_line1);
		ae_draw_text(AE_FONT_BODY, un(15.5f), ix + un(160.0f), cap_top(AE_FONT_BODY, un(15.5f), top + un(84.0f)),
			AE_ALIGN_LEFT, AE_COLOR_MUTED, dialog.item_line2);
		buttons_top = top + un(ITEM_U) + un(20.0f);
	}
	else
		buttons_top = y + un(24.0f);
	buttons_bottom = buttons_top + un(BUTTON_U);
	if (spec->button_count >= 2)
	{
		float bw = (iw - un(BUTTON_GAP_U)) * 0.5f;

		dialog.rects[0] = (struct ae_rect){ ix, buttons_top, bw, un(BUTTON_U) };
		dialog.rects[1] = (struct ae_rect){ ix + bw + un(BUTTON_GAP_U), buttons_top, bw, un(BUTTON_U) };
	}
	else
	{
		dialog.rects[0] = (struct ae_rect){ ix, buttons_top, iw, un(BUTTON_U) };
		dialog.rects[1] = (struct ae_rect){ 0.0f, 0.0f, 0.0f, 0.0f };
	}
	for (index = 0; index < spec->button_count && index < 2; index++)
		button_draw(&dialog.rects[index], labels[index], index == dialog.focus, index == spec->danger, (short)index);
	if (dialog.note[0])
		ae_draw_text(AE_FONT_BODY, un(NOTE_U), ix, cap_top(AE_FONT_BODY, un(NOTE_U), buttons_bottom + un(30.0f)),
			AE_ALIGN_LEFT, AE_COLOR_MUTED, dialog.note);
	/* the footer, where the screens' are */
	prompts_fill(prompts);
	ae_widget_prompts(d, un(96.0f), un(1020.0f), prompts, 2, NULL, un(96.0f) + un(600.0f), -1, HIT_PROMPTS);
}

static struct ae_screen_class const attention_class =
{
	.name = "attention", .handle = attention_handle, .draw = attention_draw, .pointer = attention_pointer, .popover = 1,
};

static void copy(char *to, size_t size, char const *from)
{
	snprintf(to, size, "%s", from ? from : "");
}

int ae_attention_is_open(void)
{
	return dialog.open;
}

int ae_attention_open(struct ae_attention_spec const *spec)
{
	if (dialog.open && ae_ui_holds(&dialog))
		ae_ui_remove(&dialog);
	memset(&dialog, 0, sizeof(dialog));
	dialog.spec = *spec;
	copy(dialog.title, sizeof(dialog.title), spec->title);
	copy(dialog.body, sizeof(dialog.body), spec->body);
	copy(dialog.strong, sizeof(dialog.strong), spec->strong);
	copy(dialog.item_title, sizeof(dialog.item_title), spec->item_title);
	copy(dialog.item_line1, sizeof(dialog.item_line1), spec->item_line1);
	copy(dialog.item_line2, sizeof(dialog.item_line2), spec->item_line2);
	copy(dialog.item_tile, sizeof(dialog.item_tile), spec->item_tile);
	copy(dialog.button0, sizeof(dialog.button0), spec->buttons[0]);
	copy(dialog.button1, sizeof(dialog.button1), spec->buttons[1]);
	copy(dialog.note, sizeof(dialog.note), spec->note);
	copy(dialog.back_label, sizeof(dialog.back_label), spec->back_label);
	for (dialog.list_count = 0; spec->list && dialog.list_count < spec->list_count && dialog.list_count < LIST_MAX;
		dialog.list_count++)
		copy(dialog.list[dialog.list_count], sizeof(dialog.list[0]), spec->list[dialog.list_count]);
	dialog.spec.button_count = spec->button_count < 1 ? 1 : spec->button_count > 2 ? 2 : spec->button_count;
	dialog.focus = spec->focus >= 0 && spec->focus < dialog.spec.button_count ? spec->focus : 0;
	if (!ae_ui_push(&attention_class, AE_OWNER_ANY, &dialog))
		return 0;
	dialog.open = 1;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("attention: %s", dialog.title);
	return 1;
}

void *ae_attention_context(void)
{
	return dialog.open ? dialog.spec.context : NULL;
}

void ae_attention_set_item(char const *line1, char const *tile)
{
	if (!dialog.open)
		return;
	copy(dialog.item_line1, sizeof(dialog.item_line1), line1);
	copy(dialog.item_tile, sizeof(dialog.item_tile), tile);
}

void ae_attention_close(void *context)
{
	if (!dialog.open || dialog.spec.context != context)
		return;
	dialog.open = 0;
	if (ae_ui_holds(&dialog))
		ae_ui_remove(&dialog);
}

int ae_attention_pick(short button)
{
	if (!dialog.open || button < 0 || button >= dialog.spec.button_count)
		return 0;
	pick(button);
	return 1;
}
