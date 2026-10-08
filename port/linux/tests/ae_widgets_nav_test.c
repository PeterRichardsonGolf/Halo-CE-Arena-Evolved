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

/* the device prompts show: the last one that reached a screen (ae_ui) */
static struct ae_screen_class const dummy = { .name = "dummy" };
static void use_device(int device)
{
	struct ae_event event;

	ae_ui_reset();
	ae_ui_push(&dummy, AE_OWNER_ANY, NULL);
	event.player = 0;
	event.action = AE_ACTION_NONE;
	event.device = (unsigned char)device;
	event.repeat = 0;
	ae_ui_dispatch(&event);
}

/* a layout point from a call's text box middle (the stub's pixels are layout units at a 1080 window) */
static void middle_of(struct ae_stub_call const *call, float *x, float *y, float window_height)
{
	struct ae_rect pixels;

	ae_stub_pixels(call, &pixels);
	*x = (pixels.x + pixels.width * 0.5f) * 1080.0f / window_height;
	*y = (pixels.y + pixels.height * 0.5f) * 1080.0f / window_height;
}

static void tabs(void)
{
	static const struct ae_tab eight[8] =
	{
		{ "PLAYER", 0, 0, NULL }, { "WEAPONS", 0, 0, NULL }, { "ITEMS", 0, 0, NULL }, { "VEHICLES", 0, 0, NULL },
		{ "INDICATORS", 0, 0, NULL }, { "TEAMS", 0, 0, NULL }, { "ARENA", 1, 0, NULL },
		{ "TRAINING OPTIONS", 0, 0, NULL },
	};
	static const struct ae_tab three[3] =
	{
		{ "GAME", 1, 0, NULL }, { "NETWORK", 0, 1, "Not while a game is running" }, { "PROFILE", 0, 0, NULL },
	};
	struct ae_density d;
	struct ae_frame frame;
	struct ae_tabs strip;
	struct ae_stub_call const *call;
	struct ae_pointer pointer;
	struct ae_event event;
	float left, right, height;
	int index, left_fades = 0, right_fades = 0;

	/* 8 tabs at 1440 wide (4:3), 130 %: they don't fit; the strip scrolls so the active tab is whole */
	ae_motion_set_reduced(1);
	use_device(AE_DEVICE_XBOX);
	ae_stub_reset(1440, 1080);
	ae_hits_clear();
	ae_density_full(1080, 1.3f, &d);
	ae_frame_compute(1440, 1.3f, &frame);
	memset(&strip, 0, sizeof(strip));
	strip.tabs = eight;
	strip.count = 8;
	strip.active = 4;       /* (a middle tab: both edges cut) */
	strip.hover = -1;
	left = frame.rect.x + frame.margin;
	right = frame.rect.x + frame.rect.width - frame.margin;
	height = ae_widget_tabs(&d, &strip, left, 100, right - left, 2);
	CHECK(height > 0);
	call = text_call("INDICATORS");
	CHECK(call != NULL);
	if (call)
	{
		CHECK(call->x >= left && call->x + call->width <= right);
		CHECK(call->clipped && call->x >= call->clip[0] - 2 && call->x + call->width <= call->clip[2] + 2);
		CHECK(call->rgba == AE_COLOR_TITLE);
	}
	/* (fades at the cut edges: a 70 u run of rects in the fade colour, and the ‹ › marks) */
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *rect = ae_stub_get(index);

		if (rect->kind != AE_STUB_RECT || (rect->rgba & 0xFFFFFF00u) != (AE_COLOR_FADE & 0xFFFFFF00u))
			continue;
		if (rect->x < (left + right) * 0.5f)
			left_fades++;
		else
			right_fades++;
	}
	CHECK(left_fades > 0 && ae_stub_find_text("\xE2\x80\xB9", 0) >= 0);
	CHECK(right_fades > 0 && ae_stub_find_text("\xE2\x80\xBA", 0) >= 0);
	/* (the active underline, 4 u accent) */
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_ACCENT) >= 1);
	/* LB / RB glyphs at the ends (Xbox) */
	CHECK(count_calls(AE_STUB_BUTTON, AE_COLOR_KEY_CAP) == 2);
	/* the last tab active: the strip scrolls to its end, no fade on the right */
	strip.active = 7;
	ae_stub_reset(1440, 1080);
	ae_widget_tabs(&d, &strip, left, 100, right - left, 2);
	call = text_call("TRAINING OPTIONS");
	CHECK(call && call->x + call->width <= right && ae_stub_find_text("\xE2\x80\xBA", 0) < 0);
	/* stepping: skips a disabled tab, wraps */
	memset(&strip, 0, sizeof(strip));
	strip.tabs = three;
	strip.count = 3;
	strip.hover = -1;
	CHECK(ae_tabs_step(&strip, 1) == 2);
	strip.active = 2;
	CHECK(ae_tabs_step(&strip, 1) == 0 && ae_tabs_step(&strip, -1) == 0);
	strip.active = 0;
	CHECK(ae_tabs_step(&strip, -1) == 2);
	event.player = 0; event.action = AE_ACTION_TAB_NEXT; event.device = AE_DEVICE_XBOX; event.repeat = 0;
	ae_sound_reset();
	CHECK(ae_tabs_event(&strip, &event) == 1 && strip.active == 2 && ae_sound_take(0) == AE_SOUND_CURSOR);
	event.action = AE_ACTION_TAB_NEXT;
	CHECK(ae_tabs_event(&strip, &event) == 1 && strip.active == 0);
	event.action = AE_ACTION_DOWN;
	CHECK(ae_tabs_event(&strip, &event) == 0);
	/* the change dot, the disabled tab muted at 43 % */
	use_device(AE_DEVICE_KEYBOARD_MOUSE);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_density_full(1080, 1.0f, &d);
	ae_widget_tabs(&d, &strip, 96, 100, 1728, 3);
	call = text_call("NETWORK");
	CHECK(call && call->rgba == ((AE_COLOR_MUTED & 0xFFFFFF00u) | 110u));
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_ACCENT) >= 2);    /* the underline and GAME's dot */
	/* (keyboard: Q / E caps at the ends, no glyphs) */
	CHECK(ae_stub_find_text("Q", 0) >= 0 && ae_stub_find_text("E", 0) >= 0 && count_calls(AE_STUB_BUTTON, AE_COLOR_KEY_CAP) == 0);
	/* a click on the disabled tab returns 2 (failure sound); on an enabled one switches (cursor sound) */
	memset(&pointer, 0, sizeof(pointer));
	middle_of(text_call("NETWORK"), &pointer.x, &pointer.y, 1080);
	pointer.left_clicks = 1;
	ae_sound_reset();
	CHECK(ae_tabs_pointer(&strip, &pointer, 3) == 2 && strip.active == 0 && ae_sound_take(0) == AE_SOUND_FAILURE);
	middle_of(text_call("PROFILE"), &pointer.x, &pointer.y, 1080);
	CHECK(ae_tabs_pointer(&strip, &pointer, 3) == 1 && strip.active == 2 && ae_sound_take(0) == AE_SOUND_CURSOR);
	/* the end caps click too: E steps on (wrapping to GAME) */
	middle_of(text_call("E"), &pointer.x, &pointer.y, 1080);
	CHECK(ae_tabs_pointer(&strip, &pointer, 3) == 1 && strip.active == 0);
	/* hover: the text colour after a move */
	pointer.left_clicks = 0;
	pointer.moved = 1;
	middle_of(text_call("PROFILE"), &pointer.x, &pointer.y, 1080);
	ae_tabs_pointer(&strip, &pointer, 3);
	CHECK(strip.hover == 2);
	ae_stub_reset(1920, 1080);
	ae_widget_tabs(&d, &strip, 96, 100, 1728, 3);
	CHECK(text_call("PROFILE") && text_call("PROFILE")->rgba == AE_COLOR_TEXT);
	CHECK(text_call("GAME") && text_call("GAME")->rgba == AE_COLOR_TITLE);
	CHECK(!ae_stub_overflowed());
}

static void dots(void)
{
	struct ae_density d;
	float pill_x = -1, dot_right[6];
	int index, dots_seen = 0;

	ae_stub_reset(1920, 1080);
	use_device(AE_DEVICE_XBOX);
	ae_density_full(1080, 1.0f, &d);
	ae_widget_page_dots(&d, 960, 300, "YOUR SETTINGS", 6, 2, AE_COLOR_ACCENT, 4);
	CHECK(text_call("YOUR SETTINGS") && text_call("YOUR SETTINGS")->size == 24.0f);
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *call = ae_stub_get(index);

		if (call->kind != AE_STUB_RECT)
			continue;
		if (call->rgba == AE_COLOR_DOT)
		{
			CHECK(near(call->width, 8, .01f) && near(call->height, 8, .01f));
			if (dots_seen < 6)
				dot_right[dots_seen] = call->x + call->width;
			dots_seen++;
		}
		else if (call->rgba == AE_COLOR_ACCENT)
		{
			CHECK(near(call->width, 22, .01f));
			pill_x = call->x;
		}
	}
	/* a 22 u pill at the third place, 11 u after the second dot */
	CHECK(dots_seen == 5 && pill_x > 0 && near(pill_x - dot_right[1], 11, .01f));
	CHECK(near(dot_right[2] - 8 - (pill_x + 22), 11, .01f));
	/* centred: as much on each side */
	CHECK(near((dot_right[0] - 8 + dot_right[4]) * 0.5f, 960, .01f));
	CHECK(ae_page_step(6, 5, 1) == 0 && ae_page_step(6, 0, -1) == 5 && ae_page_step(6, 2, 1) == 3 &&
		ae_page_step(0, 0, 1) == 0 && ae_page_step(1, 0, 1) == 0);
	/* LB / RB glyphs at the ends */
	CHECK(count_calls(AE_STUB_BUTTON, AE_COLOR_KEY_CAP) == 2);
}

static void prompts(void)
{
	static const struct ae_prompt pair[2] =
	{
		{ AE_BUTTON_A, "Select", NULL, AE_ACTION_ACCEPT }, { AE_BUTTON_B, "Back", NULL, AE_ACTION_BACK },
	};
	static const struct ae_prompt ps[3] =
	{
		{ AE_BUTTON_A, "Select", NULL, AE_ACTION_ACCEPT }, { AE_BUTTON_START, "Add player", NULL, AE_ACTION_START },
		{ AE_BUTTON_BACK, "Scores", NULL, AE_ACTION_SELECT },
	};
	struct ae_density d;
	struct ae_pointer pointer;
	struct ae_event event;
	int index;

	CHECK(!strcmp(ae_prompt_key(AE_BUTTON_A), "Enter") && !strcmp(ae_prompt_key(AE_BUTTON_B), "Esc") &&
		!strcmp(ae_prompt_key(AE_BUTTON_X), "Ctrl+F") && !strcmp(ae_prompt_key(AE_BUTTON_Y), "R") &&
		!strcmp(ae_prompt_key(AE_BUTTON_LEFT_SHOULDER), "Q") && !strcmp(ae_prompt_key(AE_BUTTON_RIGHT_SHOULDER), "E") &&
		!strcmp(ae_prompt_key(AE_BUTTON_LEFT_TRIGGER), "PgUp") && !strcmp(ae_prompt_key(AE_BUTTON_RIGHT_TRIGGER), "PgDn") &&
		!strcmp(ae_prompt_key(AE_BUTTON_START), "Enter"));
	CHECK(ae_prompt_tint(AE_DEVICE_XBOX, AE_BUTTON_A) == 0x66CC66FFu && ae_prompt_tint(AE_DEVICE_XBOX, AE_BUTTON_Y) == AE_TINT_XBOX_Y &&
		ae_prompt_tint(AE_DEVICE_PLAYSTATION, AE_BUTTON_B) == AE_TINT_PS_CIRCLE &&
		ae_prompt_tint(AE_DEVICE_NINTENDO, AE_BUTTON_A) == AE_COLOR_KEY_CAP &&
		ae_prompt_tint(AE_DEVICE_XBOX, AE_BUTTON_LEFT_SHOULDER) == AE_COLOR_KEY_CAP &&
		ae_prompt_tint(AE_DEVICE_XBOX, AE_BUTTON_START) == AE_COLOR_KEY_CAP);
	/* keyboard: drawn key caps with words, one button each, no Kenney pictures */
	use_device(AE_DEVICE_KEYBOARD_MOUSE);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_density_full(1080, 1.0f, &d);
	ae_widget_prompts(&d, 96, 1018, pair, 2, "Changes apply as you make them", 1824, -1, 6);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_KEY_CAP) == 2 && text_call("Enter") && text_call("Esc"));
	CHECK(text_call("Enter")->rgba == AE_COLOR_KEY_CAP_TEXT && text_call("Select") && text_call("Back"));
	CHECK(count_calls(AE_STUB_OUTLINE, AE_COLOR_RULE) == 2);
	for (index = 0; index < ae_stub_count(); index++)
		CHECK(ae_stub_get(index)->kind != AE_STUB_BUTTON);
	/* (the status, muted, right-aligned at right_x) */
	CHECK(text_call("Changes apply as you make them") && text_call("Changes apply as you make them")->rgba == AE_COLOR_MUTED &&
		text_call("Changes apply as you make them")->align == AE_ALIGN_RIGHT);
	/* (the cap: the glyph size tall, its words half that) */
	CHECK(near(text_call("Esc")->size, 30 * 0.5f, .01f));
	/* a click inside the "Esc" prompt: BACK, player 0, from the keyboard */
	memset(&pointer, 0, sizeof(pointer));
	middle_of(text_call("Back"), &pointer.x, &pointer.y, 1080);
	pointer.left_clicks = 1;
	CHECK(ae_prompts_pointer(pair, 2, &pointer, 6, &event) == 1 && event.action == AE_ACTION_BACK &&
		event.player == 0 && event.device == AE_DEVICE_KEYBOARD_MOUSE);
	middle_of(text_call("Esc"), &pointer.x, &pointer.y, 1080);
	CHECK(ae_prompts_pointer(pair, 2, &pointer, 6, &event) == 1 && event.action == AE_ACTION_BACK);
	pointer.x = 5;
	CHECK(ae_prompts_pointer(pair, 2, &pointer, 6, &event) == 0);
	/* hover (after a move) washes the prompt; pressed: accent 35 % and an accent outline */
	pointer.left_clicks = 0;
	pointer.moved = 1;
	middle_of(text_call("Select"), &pointer.x, &pointer.y, 1080);
	CHECK(ae_prompts_pointer(pair, 2, &pointer, 6, &event) == 0);
	ae_stub_reset(1920, 1080);
	ae_widget_prompts(&d, 96, 1018, pair, 2, NULL, 1824, 1, 6);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_HOVER) == 1 && count_calls(AE_STUB_RECT, AE_COLOR_PRESSED) == 1 &&
		count_calls(AE_STUB_OUTLINE, AE_COLOR_ACCENT) == 1);
	/* Xbox: the Kenney glyphs, face buttons tinted */
	use_device(AE_DEVICE_XBOX);
	ae_stub_reset(1920, 1080);
	ae_widget_prompts(&d, 96, 1018, pair, 2, NULL, 1824, -1, 6);
	CHECK(count_calls(AE_STUB_BUTTON, 0x66CC66FFu) == 1 && count_calls(AE_STUB_BUTTON, AE_TINT_XBOX_B) == 1);
	CHECK(text_call("Enter") == NULL && count_calls(AE_STUB_RECT, AE_COLOR_KEY_CAP) == 0);
	/* (glyphs the glyph size tall, centred on the row) */
	for (index = 0; index < ae_stub_count(); index++)
		if (ae_stub_get(index)->kind == AE_STUB_BUTTON)
			CHECK(near(ae_stub_get(index)->size, 30, .01f) && near(ae_stub_get(index)->y, 1018 - 15, .01f));
	/* PlayStation: START and BACK are drawn OPTIONS / CREATE caps */
	use_device(AE_DEVICE_PLAYSTATION);
	ae_stub_reset(1920, 1080);
	ae_widget_prompts(&d, 96, 1018, ps, 3, NULL, 1824, -1, 6);
	CHECK(text_call("OPTIONS") && text_call("CREATE") && count_calls(AE_STUB_BUTTON, AE_TINT_PS_CROSS) == 1);
	CHECK(count_calls(AE_STUB_BUTTON, AE_COLOR_KEY_CAP) == 0);
	/* the key cap alone: width at least its height */
	ae_stub_reset(1920, 1080);
	CHECK(ae_widget_key_cap(&d, 0, 0, 30, "R") >= 30.0f - 1e-3f && ae_widget_key_cap(&d, 0, 0, 30, "Ctrl+Shift+S") > 60.0f);
	CHECK(!ae_stub_overflowed());
}

static void panel(void)
{
	struct ae_density d;
	struct ae_rect content, pixels;
	struct ae_stub_call const *call;
	float help_y, prompts_y;
	int index, found = 0;
	char lines_text[200];

	/* a 640 x 360 quarter of a 1280 x 720 window */
	use_device(AE_DEVICE_PLAYSTATION);
	ae_stub_reset(1920, 720);
	ae_draw_view(960, 0, 960, 540);
	ae_density_view(640, 360, 1.0f, &d);
	ae_widget_view_panel(&d, 640, 360, 1, 0x3B8ED8FFu, "GAME", 2, 6, &content);
	for (index = 0; index < ae_stub_count(); index++)
	{
		call = ae_stub_get(index);
		if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_VIEW_PANEL)
		{
			ae_stub_pixels(call, &pixels);
			/* (the view: pixels 640..1280 x 0..360 of the window) */
			CHECK(pixels.x >= 640 && pixels.y >= 0 && pixels.x + pixels.width <= 1280 && pixels.y + pixels.height <= 360);
			CHECK(pixels.x + pixels.width < 640 + 320);                /* left of the view's centre */
			CHECK(near(pixels.width, 294.4f, .6f) && near(pixels.height, 309.6f, .6f));
			CHECK(near(pixels.x, 640 + 0.03f * 640, .1f) && near(pixels.y, 0.07f * 360, .1f));
			found = 1;
		}
	}
	CHECK(found);
	/* the player colour: the 3 u stripe and the current page's pill; the header text at least 16 px */
	CHECK(count_calls(AE_STUB_RECT, 0x3B8ED8FFu) >= 3);   /* stripe, emblem, pill */
	call = text_call("PLAYER 2");
	CHECK(call && ae_stub_text_em_pixels(call) >= 16.0f - 0.01f);
	call = text_call("GAME");
	CHECK(call && ae_stub_text_em_pixels(call) >= 14.0f - 0.01f);
	CHECK(text_call("2") != NULL);
	/* the content inside the panel, under the dots, above the footer */
	CHECK(content.x > 0 && content.width > 0 && content.height > 0);
	ae_view_panel_footer(&d, 640, 360, &help_y, &prompts_y);
	CHECK(content.y + content.height <= help_y + 0.01f && help_y < prompts_y);
	CHECK(prompts_y * 360.0f / 1080.0f < 0.93f * 360.0f);
	/* the help strip: two lines at most, the second cut with "…", at the 14 px floor */
	ae_stub_reset(1920, 720);
	ae_draw_view(960, 0, 960, 540);
	ae_widget_help_strip(&d, content.x, help_y, content.width,
		"Your own spawn heat view: MINE, ENEMY or OFF. The host turned it on, and this sentence runs on far past two "
		"lines of the panel's width so the second line has to end with an ellipsis.");
	found = 0;
	lines_text[0] = 0;
	for (index = 0; index < ae_stub_count(); index++)
	{
		call = ae_stub_get(index);
		if (call->kind == AE_STUB_TEXT)
		{
			found++;
			CHECK(ae_stub_text_em_pixels(call) >= 14.0f - 0.01f && call->x + call->width <= content.x + content.width + 0.01f);
			snprintf(lines_text, sizeof(lines_text), "%s", call->text);
		}
	}
	CHECK(found == 2 && strstr(lines_text, "\xE2\x80\xA6"));
	CHECK(!ae_stub_overflowed());
}

int main(void)
{
	tabs();
	dots();
	prompts();
	panel();
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
