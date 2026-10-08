#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_draw_stub.h"
#include "ae_font.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_widgets.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int near(float a, float b, float tolerance)
{
	return fabsf(a - b) <= tolerance;
}

static struct ae_stub_call const *text_call(const char *text)
{
	int index = ae_stub_find_text(text, 0);

	return index >= 0 ? ae_stub_get(index) : NULL;
}

static int count_calls(int kind, unsigned int rgba)
{
	int index, count = 0;

	for (index = 0; index < ae_stub_count(); index++)
		if (ae_stub_get(index)->kind == kind && ae_stub_get(index)->rgba == rgba)
			count++;
	return count;
}

static int inside(struct ae_rect const *a, struct ae_rect const *b, float slack)
{
	return a->x >= b->x - slack && a->y >= b->y - slack && a->x + a->width <= b->x + b->width + slack &&
		a->y + a->height <= b->y + b->height + slack;
}

/* a call's middle in layout units (the window window_height pixels tall) */
static void middle_of(struct ae_stub_call const *call, float *x, float *y, float window_height)
{
	struct ae_rect pixels;

	ae_stub_pixels(call, &pixels);
	*x = (pixels.x + pixels.width * 0.5f) * 1080.0f / window_height;
	*y = (pixels.y + pixels.height * 0.5f) * 1080.0f / window_height;
}

static struct ae_screen_class const base = { .name = "base" };
static short picked_index = -1;
static int picked_count;
static void picked(short index, void *context)
{
	(void)context;
	picked_index = index;
	picked_count++;
}

static void placement(void)
{
	static const char *const short_values[3] = { "LOCAL", "LAN", "ONLINE" };
	static const char *const long_values[2] = { "OFF", "A MUCH LONGER VALUE NAME THAN USUAL" };
	struct ae_density d;
	struct ae_rect frame = { 0, 0, 1920, 1080 }, row, pop;
	short first;

	ae_stub_reset(1920, 1080);
	ae_density_full(1080, 1.0f, &d);
	/* width: at least 240 u, else the longest + 76 u */
	CHECK(near(ae_picker_width(&d, short_values, 3), 240, 0.01f));
	CHECK(near(ae_picker_width(&d, long_values, 2),
		ae_draw_text_width(AE_FONT_ROW, 22, "A MUCH LONGER VALUE NAME THAN USUAL") + 76, 0.01f));
	ae_density_full(1080, 1.3f, &d);
	CHECK(near(ae_picker_width(&d, short_values, 3), 240 * 1.3f, 0.01f));
	/* a row in the middle: the current item over it */
	row.x = 100; row.y = 500; row.width = 800; row.height = 50;
	ae_picker_place(&row, &frame, 3, 1, 42, 300, &pop, &first);
	CHECK(first == 0 && near(pop.height, 3 * 42, 0.01f));
	CHECK(near(pop.y + 1.5f * 42, row.y + row.height * 0.5f, 0.01f));
	CHECK(near(pop.x + pop.width, row.x + row.width, 0.01f) && near(pop.width, 300, 0.01f));
	/* (Review Focus 2) a row 60 u above the frame bottom with 12 values opens upward, shows 8, inside the bounds */
	row.y = 1080 - 60 - 50;
	ae_picker_place(&row, &frame, 12, 10, 42, 300, &pop, &first);
	CHECK(pop.y >= frame.y && pop.y + pop.height <= frame.y + frame.height && pop.height == 8 * 42);
	CHECK(first <= 10 && 10 < first + 8);
	/* (the current item still over the row: the window moved up rather than the list off the frame) */
	CHECK(near(pop.y + ((float)(10 - first) + 0.5f) * 42, row.y + row.height * 0.5f, 0.01f));
	/* a row on the frame's last line, the first value current: the list pushed up into the frame, ending at it */
	row.y = 1080 - 50;
	ae_picker_place(&row, &frame, 12, 0, 42, 300, &pop, &first);
	CHECK(near(pop.y + pop.height, 1080, 0.01f) && first == 0 && pop.height == 8 * 42);
	/* a row at the top: down, inside */
	row.y = 10;
	ae_picker_place(&row, &frame, 12, 0, 42, 300, &pop, &first);
	CHECK(pop.y >= frame.y && first == 0 && pop.height == 8 * 42);
	/* never wider than the bounds, never left of them */
	row.x = 0; row.width = 200;
	ae_picker_place(&row, &frame, 3, 0, 42, 3000, &pop, &first);
	CHECK(pop.x >= frame.x && pop.x + pop.width <= frame.x + frame.width);
	/* inside a 640x360 VIEW panel (layout units of a 1280x720 window's quarter): fewer than 8 when it can't hold 8 */
	{
		struct ae_rect panel, bounds;
		float item;

		ae_density_view(640, 360, 1.0f, &d);
		ae_view_panel_rect(640, 360, 1.0f, &panel);
		/* (pixels of a 720-tall window to layout units: x 1.5) */
		bounds.x = panel.x * 1.5f; bounds.y = panel.y * 1.5f; bounds.width = panel.width * 1.5f;
		bounds.height = panel.height * 1.5f;
		item = 42.0f * 0.5f * d.unit;   /* (the picker's items at VIEW: 42 view u, in layout units of the quarter) */
		row.x = bounds.x; row.width = bounds.width; row.height = 40; row.y = bounds.y + bounds.height - 60;
		ae_picker_place(&row, &bounds, 12, 10, item, bounds.width * 0.8f, &pop, &first);
		CHECK(inside(&pop, &bounds, 0.01f));
		CHECK(pop.height >= item && first <= 10 && 10 < first + (short)(pop.height / item + 0.5f));
	}
}

static void picker(void)
{
	static const char *const values[12] = { "60", "72", "90", "120", "144", "165", "180", "200", "240", "300", "360",
		"UNLIMITED" };
	struct ae_picker_spec spec;
	struct ae_pointer pointer;
	struct ae_event event;
	struct ae_stub_call const *call;
	int index;

	ae_motion_set_reduced(1);
	ae_ui_reset();
	ae_ui_set_before_draw(NULL);
	ae_ui_push(&base, 0, NULL);
	memset(&spec, 0, sizeof(spec));
	spec.values = values;
	spec.count = 12;
	spec.current = 5;
	spec.row.x = 200; spec.row.y = 400; spec.row.width = 900; spec.row.height = 50;
	spec.bounds.x = 96; spec.bounds.y = 0; spec.bounds.width = 1728; spec.bounds.height = 1080;
	ae_density_full(1080, 1.0f, &spec.density);
	spec.picked = picked;
	/* open: a popover, the forward sound */
	ae_sound_reset();
	CHECK(ae_picker_open(&spec, 0) && ae_ui_depth() == 2 && ae_ui_top()->screen_class->popover);
	CHECK(ae_sound_take(0) == AE_SOUND_FORWARD);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_ui_draw();
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_POPOVER) == 1);
	for (index = 0; index < ae_stub_count(); index++)
	{
		call = ae_stub_get(index);
		if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_POPOVER)
		{
			struct ae_rect pop = { call->x, call->y, call->width, call->height };

			CHECK(inside(&pop, &spec.bounds, 0.01f) && near(pop.width, 240, 0.01f));
		}
	}
	/* 8 shown, the current one (165) focused: the white bar, the check as two strokes */
	CHECK(text_call("165") && text_call("165")->rgba == AE_COLOR_SELECTION_TEXT);
	CHECK(count_calls(AE_STUB_LINE, AE_COLOR_SELECTION_TEXT) == 2 && count_calls(AE_STUB_RECT, AE_COLOR_SELECTION) == 1);
	CHECK(text_call("UNLIMITED") == NULL || !text_call("UNLIMITED")->clipped || text_call("UNLIMITED")->y >= 0);
	/* down: the focus moves (cursor), the check stays on the current value (accent, off the bar) */
	event.player = 0; event.action = AE_ACTION_DOWN; event.device = AE_DEVICE_XBOX; event.repeat = 0;
	ae_ui_dispatch(&event);
	CHECK(ae_sound_take(0) == AE_SOUND_CURSOR);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_ui_draw();
	CHECK(text_call("165") && text_call("165")->rgba == AE_COLOR_ACCENT && count_calls(AE_STUB_LINE, AE_COLOR_ACCENT) == 2);
	CHECK(text_call("180") && text_call("180")->rgba == AE_COLOR_SELECTION_TEXT);
	/* A: picked, closed, forward */
	picked_count = 0;
	event.action = AE_ACTION_ACCEPT;
	ae_ui_dispatch(&event);
	CHECK(picked_count == 1 && picked_index == 6 && ae_ui_depth() == 1 && ae_sound_take(0) == AE_SOUND_FORWARD);
	/* B: closed unchanged (the back sound) */
	ae_picker_open(&spec, 0);
	ae_sound_take(0);
	event.action = AE_ACTION_BACK;
	ae_ui_dispatch(&event);
	CHECK(picked_count == 1 && ae_ui_depth() == 1 && ae_sound_take(0) == AE_SOUND_BACK);
	/* a click outside: closed unchanged */
	ae_picker_open(&spec, 0);
	ae_sound_take(0);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_ui_draw();
	memset(&pointer, 0, sizeof(pointer));
	pointer.x = 5;
	pointer.y = 5;
	pointer.left_clicks = 1;
	ae_ui_dispatch_pointer(&pointer);
	CHECK(picked_count == 1 && ae_ui_depth() == 1 && ae_sound_take(0) == AE_SOUND_BACK);
	/* a click on an item picks it */
	ae_picker_open(&spec, 0);
	ae_sound_take(0);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_ui_draw();
	call = text_call("200");
	CHECK(call != NULL);
	if (call)
	{
		middle_of(call, &pointer.x, &pointer.y, 1080);
		ae_ui_dispatch_pointer(&pointer);
		CHECK(picked_count == 2 && picked_index == 7 && ae_ui_depth() == 1 && ae_sound_take(0) == AE_SOUND_FORWARD);
	}
	/* hover moves the focus; the wheel scrolls */
	ae_picker_open(&spec, 0);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_ui_draw();
	memset(&pointer, 0, sizeof(pointer));
	middle_of(text_call("144"), &pointer.x, &pointer.y, 1080);
	pointer.moved = 1;
	ae_ui_dispatch_pointer(&pointer);
	event.action = AE_ACTION_ACCEPT;
	ae_ui_dispatch(&event);
	CHECK(picked_index == 4 && ae_ui_depth() == 1);

	/* VIEW (P12): drawn in the player's own view, inside the panel */
	{
		struct ae_rect panel, panel_pixels, pop_pixels = { 0, 0, 0, 0 };
		int found_view = 0;

		ae_view_panel_rect(640, 360, 1.0f, &panel);
		ae_density_view(640, 360, 1.0f, &spec.density);
		/* (the top-right quarter of a 1280x720 window: layout x 960..1920, y 0..540) */
		spec.view.x = 960; spec.view.y = 0; spec.view.width = 960; spec.view.height = 540;
		spec.bounds.x = 960 + panel.x * 1.5f; spec.bounds.y = panel.y * 1.5f;
		spec.bounds.width = panel.width * 1.5f; spec.bounds.height = panel.height * 1.5f;
		spec.row.x = spec.bounds.x; spec.row.width = spec.bounds.width; spec.row.height = 40;
		spec.row.y = spec.bounds.y + spec.bounds.height - 70;
		panel_pixels.x = 640 + panel.x; panel_pixels.y = panel.y; panel_pixels.width = panel.width;
		panel_pixels.height = panel.height;
		ae_picker_open(&spec, 1);
		ae_stub_reset(1920, 720);
		ae_hits_clear();
		ae_ui_draw();
		for (index = 0; index < ae_stub_count(); index++)
		{
			call = ae_stub_get(index);
			if (call->kind == AE_STUB_VIEW && near(call->x, 960, 0.01f) && near(call->width, 960, 0.01f))
				found_view = 1;
			if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_POPOVER)
				ae_stub_pixels(call, &pop_pixels);
		}
		CHECK(found_view && pop_pixels.width > 0 && inside(&pop_pixels, &panel_pixels, 0.05f));
		/* (its items at VIEW's floors: 16 px) */
		CHECK(text_call("165") && ae_stub_text_em_pixels(text_call("165")) >= 16.0f - 0.01f);
		event.player = 1; event.action = AE_ACTION_BACK;
		ae_ui_dispatch(&event);
		CHECK(ae_ui_depth() == 1);
	}
	CHECK(!ae_stub_overflowed());
}

static void chips(void)
{
	static const struct ae_chip ten[10] =
	{
		{ "SLAYER", -1, 0 }, { "CTF", -1, AE_CHIP_ON }, { "ODDBALL", -1, AE_CHIP_ON | AE_CHIP_FOCUSED },
		{ "KING", -1, AE_CHIP_UNSUPPORTED }, { "RACE", -1, AE_CHIP_DISABLED }, { "JUGGERNAUT", -1, AE_CHIP_HOVER },
		{ "ALL", 41, 0 }, { "HALO CE (XBOX)", 13, AE_CHIP_ON }, { "MODS", 8, AE_CHIP_FOCUSED }, { "TEAMS", -1, 0 },
	};
	struct ae_density d;
	struct ae_rect rects[10];
	float height;
	int index, rows = 1;

	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_density_full(1080, 1.0f, &d);
	height = ae_chips_layout(&d, 400, ten, 10, rects);
	for (index = 0; index < 10; index++)
	{
		CHECK(rects[index].x >= 0 && rects[index].x + rects[index].width <= 400 + 0.01f && near(rects[index].height, 34, 0.01f));
		if (index && rects[index].y > rects[index - 1].y)
		{
			/* (a new row: 8 u under the last, from the left) */
			CHECK(near(rects[index].y, rects[index - 1].y + 34 + 8, 0.01f) && rects[index].x == 0);
			rows++;
		}
		else if (index)
			CHECK(near(rects[index].x, rects[index - 1].x + rects[index - 1].width + 8, 0.01f));
	}
	CHECK(rows >= 3 && near(height, rects[9].y + 34, 0.01f));
	/* (the text 14 u in from each side) */
	CHECK(near(rects[1].width, ae_draw_text_width(AE_FONT_ROW, 18, "CTF") + 28, 0.01f));
	CHECK(near(ae_widget_chips(&d, 100, 200, 400, ten, 10, 5), height, 0.01f));
	/* on + focus: accent fill and a 2 u white ring 3 u outside */
	{
		int ring = -1;

		for (index = 0; index < ae_stub_count(); index++)
			if (ae_stub_get(index)->kind == AE_STUB_OUTLINE && ae_stub_get(index)->rgba == AE_COLOR_RING)
				ring = index;
		CHECK(ring >= 0);
		if (ring >= 0)
		{
			struct ae_stub_call const *call = ae_stub_get(ring);

			CHECK(near(call->x, 100 + rects[2].x - 5, 0.01f) && near(call->width, rects[2].width + 10, 0.01f) &&
				near(call->thickness, 2, 0.01f));
		}
	}
	/* states: off muted with a rule outline, on dark on accent, focus white, unsupported dashed and struck, disabled
	at 16 % / 59 %, hover a wash, counts muted */
	CHECK(text_call("SLAYER") && text_call("SLAYER")->rgba == AE_COLOR_MUTED);
	CHECK(text_call("CTF") && text_call("CTF")->rgba == AE_COLOR_SELECTION_TEXT);
	CHECK(text_call("MODS") && text_call("MODS")->rgba == AE_COLOR_SELECTION_TEXT &&
		count_calls(AE_STUB_RECT, AE_COLOR_SELECTION) == 1);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_ACCENT) == 3);
	CHECK(text_call("KING") && text_call("KING")->rgba == AE_COLOR_MUTED && count_calls(AE_STUB_RECT, AE_COLOR_RULE) >= 8);
	CHECK(text_call("RACE") && text_call("RACE")->rgba == AE_COLOR_CHIP_DISABLED_TEXT &&
		count_calls(AE_STUB_OUTLINE, AE_COLOR_CHIP_DISABLED) == 1);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_HOVER) == 1);
	CHECK(text_call("41") && text_call("41")->rgba == AE_COLOR_MUTED && text_call("13") &&
		text_call("13")->rgba == AE_COLOR_SELECTION_TEXT);
	/* (the strike through KING: a muted 2 u rect across its text) */
	{
		struct ae_stub_call const *king = text_call("KING");
		int struck = 0;

		for (index = 0; index < ae_stub_count(); index++)
		{
			struct ae_stub_call const *call = ae_stub_get(index);

			if (king && call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_MUTED && near(call->height, 2, 0.01f) &&
				near(call->x, king->x, 0.01f) && near(call->width, king->width, 0.01f))
				struck = 1;
		}
		CHECK(struck);
	}
	/* hits: each chip */
	{
		struct ae_hit hit;
		float x, y;

		middle_of(text_call("ODDBALL"), &x, &y, 1080);
		CHECK(ae_hit_at(x, y, 0, &hit) && hit.id == 5 && hit.part == AE_PART_CHIP && hit.index == 2);
	}
	/* VIEW: 28 view u tall, the text at the 14 px floor */
	ae_stub_reset(1920, 720);
	ae_draw_view(0, 0, 960, 540);
	ae_density_view(640, 360, 1.0f, &d);
	ae_chips_layout(&d, 600, ten, 3, rects);
	CHECK(near(rects[0].height, 28 * d.unit, 0.01f));
	ae_widget_chips(&d, 0, 0, 600, ten, 3, 5);
	CHECK(text_call("CTF") && ae_stub_text_em_pixels(text_call("CTF")) >= 14.0f - 0.01f);
	CHECK(!ae_stub_overflowed());
}

static void help(void)
{
	static const char *const values[4] = { "CLASSIC", "REACH", "HALO 2", "HALO 3" };
	static const char body[] =
		"How players heal. HALO 2: shields and health recharge, no health packs needed. REACH: shields recharge, "
		"health recharges in thirds. CLASSIC: shields recharge, health only from packs. HALO 3: as HALO 2, a little "
		"faster. Everyone in the lobby plays with the same rule, set by the host for the whole match.";
	struct ae_density d;
	struct ae_help h;
	struct ae_rect rect, preview;
	struct ae_hit hit;
	int index, lines = 0, dots = 0;
	float x, y;

	CHECK(strlen(body) >= 300);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_density_full(1080, 1.0f, &d);
	memset(&h, 0, sizeof(h));
	h.title = "Health";
	h.body = body;
	h.values = values;
	h.value_count = 4;
	h.current = 2;
	h.default_value = "REACH";
	h.changed_from = "REACH";
	h.preview = 1;
	rect.x = 100; rect.y = 50; rect.width = 440; rect.height = 900;
	ae_widget_help(&d, &rect, &h, 8, &preview);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_PANEL) == 1);
	/* (the panel's rule: an outline as big as it; the off value chips have theirs) */
	{
		int panel_rule = 0;

		for (index = 0; index < ae_stub_count(); index++)
			if (ae_stub_get(index)->kind == AE_STUB_OUTLINE && ae_stub_get(index)->rgba == AE_COLOR_RULE &&
				near(ae_stub_get(index)->width, 440, 0.01f) && near(ae_stub_get(index)->height, 900, 0.01f))
				panel_rule++;
		CHECK(panel_rule == 1 && count_calls(AE_STUB_OUTLINE, AE_COLOR_RULE) == 1 + 3);
	}
	CHECK(text_call("Health") && text_call("Health")->size == 28 && near(text_call("Health")->x, 128, 0.01f));
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *call = ae_stub_get(index);

		if (call->kind == AE_STUB_TEXT)
			CHECK(!strstr(call->text, "\xC2\xB7"));
		if (call->kind == AE_STUB_TEXT && call->font == AE_FONT_BODY && call->size == 21 && call->rgba == AE_COLOR_TEXT)
		{
			/* body lines: none wider than 440 - 2 x 28 u (2 px of ink allowed) */
			CHECK(call->width <= 440 - 56 + 2);
			lines++;
		}
		if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_ACCENT && near(call->width, call->height, 0.01f) &&
			call->width < 10)
			dots++;
	}
	CHECK(lines >= 5);
	/* VALUES: tracked, muted; every value a chip (the current filled) and a hit */
	CHECK(text_call("VALUES") && near(text_call("VALUES")->thickness, 0.09f, 1e-6f) && text_call("VALUES")->size == 18);
	CHECK(text_call("HALO 2") && text_call("HALO 2")->rgba == AE_COLOR_SELECTION_TEXT);
	middle_of(text_call("HALO 3"), &x, &y, 1080);
	CHECK(ae_hit_at(x, y, 0, &hit) && hit.id == 8 && hit.part == AE_PART_CHIP && hit.index == 3);
	/* Default, and the changed line: "Changed from REACH" in accent, a drawn separator dot, the Y glyph and Reset */
	CHECK(text_call("Default: REACH") && text_call("Default: REACH")->rgba == AE_COLOR_MUTED);
	CHECK(text_call("Changed from REACH") && text_call("Changed from REACH")->rgba == AE_COLOR_ACCENT);
	CHECK(dots == 1 && text_call("Reset") && text_call("Reset")->rgba == AE_COLOR_ACCENT);   /* (the whole line accent) */
	middle_of(text_call("Reset"), &x, &y, 1080);
	CHECK(ae_hit_at(x, y, 0, &hit) && hit.id == 8 && hit.part == AE_PART_PROMPT && hit.index == -1);
	/* the preview: the rest of the panel, dashed */
	CHECK(preview.width > 0 && preview.height > 0 && preview.y > text_call("Reset")->y &&
		preview.y + preview.height <= rect.y + rect.height - 28 + 0.01f && near(preview.x, 128, 0.01f) &&
		near(preview.width, 440 - 56, 0.01f));
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_RULE) >= 8);
	/* no preview, unchanged, no values: none of those parts */
	ae_stub_reset(1920, 1080);
	h.preview = 0;
	h.changed_from = NULL;
	h.value_count = 0;
	h.default_value = NULL;
	ae_widget_help(&d, &rect, &h, 8, &preview);
	CHECK(preview.width == 0 && preview.height == 0 && !text_call("VALUES") && !text_call("Reset") &&
		!text_call("Default: REACH"));
	/* 34 % of the frame (653 u) wraps into fewer lines than 440 u */
	CHECK(!ae_stub_overflowed());
}

/* fix round 1: a picker per view at once (4 players), refused re-opening, degenerate bounds, the help's sections
cut at the panel's bottom */
static short picks[5];
static void picked_view(short index, void *context)
{
	picks[*(short *)context] = (short)(index + 1);
}

static void fixes(void)
{
	static const char *const values[12] = { "60", "72", "90", "120", "144", "165", "180", "200", "240", "300", "360",
		"UNLIMITED" };
	static short contexts[4] = { 0, 1, 2, 3 };
	static const float origins[4][2] = { { 0, 0 }, { 960, 0 }, { 0, 540 }, { 960, 540 } };
	struct ae_picker_spec spec;
	struct ae_rect panel, pops[4], pixels;
	struct ae_event event;
	struct ae_pointer pointer;
	struct ae_hit hit;
	short ids[4];
	int index, n;

	ae_motion_set_reduced(1);
	ae_ui_reset();
	ae_ui_set_before_draw(NULL);
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_view_panel_rect(960, 540, 1.0f, &panel);
	for (n = 0; n < 4; n++)
	{
		memset(&spec, 0, sizeof(spec));
		spec.values = values;
		spec.count = 12;
		spec.current = (short)(2 + n);
		ae_density_view(960, 540, 1.0f, &spec.density);
		spec.view.x = origins[n][0]; spec.view.y = origins[n][1]; spec.view.width = 960; spec.view.height = 540;
		/* (a 1920 x 1080 window: layout units are its pixels, so the panel's pixels from the quarter's corner are
		layout units too) */
		spec.bounds.x = origins[n][0] + panel.x; spec.bounds.y = origins[n][1] + panel.y;
		spec.bounds.width = panel.width; spec.bounds.height = panel.height;
		spec.row.x = spec.bounds.x; spec.row.width = spec.bounds.width; spec.row.height = 40;
		spec.row.y = spec.bounds.y + 120;
		spec.picked = picked_view;
		spec.context = &contexts[n];
		CHECK(ae_picker_open(&spec, (short)n));
		/* (its owner's picker is open: a second is refused, the first untouched) */
		CHECK(!ae_picker_open(&spec, (short)n));
	}
	CHECK(ae_ui_depth() == 5);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_ui_draw();
	/* each drawn in its own view, inside its panel */
	n = 0;
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *call = ae_stub_get(index);

		if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_POPOVER && n < 4)
		{
			struct ae_rect bounds = { origins[n][0] + panel.x, origins[n][1] + panel.y, panel.width, panel.height };

			ae_stub_pixels(call, &pops[n]);
			CHECK(near(call->view.x, origins[n][0], 0.01f) && near(call->view.y, origins[n][1], 0.01f));
			CHECK(inside(&pops[n], &bounds, 0.05f));
			n++;
		}
	}
	CHECK(n == 4);
	/* each takes its own hits: the current value's item in each view, four ids */
	for (n = 0; n < 4; n++)
	{
		float x = pops[n].x + pops[n].width * 0.5f, y;

		y = pops[n].y + 2.0f;
		CHECK(ae_hit_at(x, y, 0, &hit) && hit.part == AE_PART_ITEM && hit.index >= 0);
		ids[n] = hit.id;
		/* (and its view's empty space is its outside) */
		CHECK(ae_hit_at(origins[n][0] + 900, origins[n][1] + 500, 0, &hit) && hit.id == ids[n] && hit.part == AE_PART_OUTSIDE);
	}
	CHECK(ids[0] != ids[1] && ids[0] != ids[2] && ids[0] != ids[3] && ids[1] != ids[2] && ids[1] != ids[3] &&
		ids[2] != ids[3]);
	/* closing, each its own way, each its own callback: player 4 picks with A; player 3 closes with B; player 2 clicks
	an item of its own; player 1 clicks outside, in its view */
	memset(picks, 0, sizeof(picks));
	event.device = AE_DEVICE_XBOX; event.repeat = 0;
	event.player = 3; event.action = AE_ACTION_ACCEPT;
	ae_ui_dispatch(&event);
	CHECK(picks[3] == 5 + 1 && ae_ui_depth() == 4 && picks[0] == 0 && picks[1] == 0 && picks[2] == 0);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_ui_draw();
	n = 0;
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *call = ae_stub_get(index);

		if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_POPOVER && n < 3)
		{
			/* (the others where they were) */
			ae_stub_pixels(call, &pixels);
			CHECK(near(pixels.x, pops[n].x, 0.01f) && near(pixels.y, pops[n].y, 0.01f));
			n++;
		}
	}
	CHECK(n == 3);
	event.player = 2; event.action = AE_ACTION_BACK;
	ae_ui_dispatch(&event);
	CHECK(ae_ui_depth() == 3 && picks[2] == 0);
	memset(&pointer, 0, sizeof(pointer));
	pointer.player = 1;
	pointer.x = pops[1].x + pops[1].width * 0.5f;
	pointer.y = pops[1].y + 2.0f;
	pointer.left_clicks = 1;
	ae_ui_dispatch_pointer(&pointer);
	CHECK(ae_ui_depth() == 2 && picks[1] > 0 && picks[0] == 0);
	pointer.player = 0;
	pointer.x = origins[0][0] + 900;
	pointer.y = origins[0][1] + 500;
	ae_ui_dispatch_pointer(&pointer);
	CHECK(ae_ui_depth() == 1 && picks[0] == 0);
	/* (closed: its slot opens again; a reset stack leaves no slot taken) */
	spec.current = 0;
	CHECK(ae_picker_open(&spec, 0));
	ae_ui_reset();
	CHECK(ae_picker_open(&spec, 0));
	ae_ui_reset();

	/* degenerate bounds, shorter than one item: the popover no taller than they are */
	{
		struct ae_rect row = { 100, 100, 400, 50 }, bounds = { 0, 100, 1920, 20 }, pop;
		short first;

		ae_picker_place(&row, &bounds, 5, 2, 42, 300, &pop, &first);
		CHECK(inside(&pop, &bounds, 0.01f) && pop.height <= 20.01f);
	}

	/* the help's sections stop at the panel's bottom: a short panel with a long body, values and a change */
	{
		static const char *const options[4] = { "CLASSIC", "REACH", "HALO 2", "HALO 3" };
		struct ae_density d;
		struct ae_help h;
		struct ae_rect rect = { 100, 50, 440, 260 }, preview;
		float bottom = 50 + 260 - 28;
		int texts = 0;

		ae_stub_reset(1920, 1080);
		ae_hits_clear();
		ae_density_full(1080, 1.0f, &d);
		memset(&h, 0, sizeof(h));
		h.title = "Health";
		h.body = "How players heal. HALO 2: shields and health recharge, no health packs needed. REACH: shields recharge, "
			"health recharges in thirds. CLASSIC: shields recharge, health only from packs.";
		h.values = options;
		h.value_count = 4;
		h.current = 1;
		h.default_value = "REACH";
		h.changed_from = "REACH";
		h.preview = 1;
		ae_widget_help(&d, &rect, &h, 8, &preview);
		for (index = 0; index < ae_stub_count(); index++)
		{
			struct ae_stub_call const *call = ae_stub_get(index);

			if (call->kind == AE_STUB_TEXT)
			{
				CHECK(call->y + call->bottom <= bottom + 2.01f);
				CHECK(call->clipped && call->clip[3] <= bottom + 0.01f);
				texts++;
			}
		}
		/* (the body cut short with "…", nothing after it: no room) */
		CHECK(texts >= 3 && preview.height == 0);
		for (index = 0; index < ae_stub_count(); index++)
			if (ae_stub_get(index)->kind == AE_STUB_TEXT && strstr(ae_stub_get(index)->text, "\xE2\x80\xA6"))
				texts = -1;
		CHECK(texts == -1 && !text_call("VALUES") && !text_call("Reset"));
		CHECK(!ae_hit_at(300, bottom + 5, 0, &hit));
	}
	CHECK(!ae_stub_overflowed());
}

int main(void)
{
	placement();
	picker();
	chips();
	help();
	fixes();
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
