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

/* a call's pixel box */
static struct ae_rect pixels_of(struct ae_stub_call const *call)
{
	struct ae_rect pixels;

	ae_stub_pixels(call, &pixels);
	return pixels;
}

/* the fix round's checks: footers side by side, VIEW footers and caps, right-hand views, more panel sizes, tiny views,
the first draw of a scrolled strip, steps from no current */
static void fixes(void)
{
	static const struct ae_prompt pair[2] =
	{
		{ AE_BUTTON_A, "Select", NULL, AE_ACTION_ACCEPT }, { AE_BUTTON_B, "Back", NULL, AE_ACTION_BACK },
	};
	static const struct ae_prompt three[3] =
	{
		{ AE_BUTTON_A, "Select", NULL, AE_ACTION_ACCEPT }, { AE_BUTTON_B, "Resume", NULL, AE_ACTION_BACK },
		{ AE_BUTTON_LEFT_SHOULDER, "Pages", NULL, AE_ACTION_TAB_PREVIOUS },
	};
	static const struct ae_tab eight[8] =
	{
		{ "PLAYER", 0, 0, NULL }, { "WEAPONS", 0, 0, NULL }, { "ITEMS", 0, 0, NULL }, { "VEHICLES", 0, 0, NULL },
		{ "INDICATORS", 0, 0, NULL }, { "TEAMS", 0, 0, NULL }, { "ARENA", 0, 0, NULL },
		{ "TRAINING OPTIONS", 0, 0, NULL },
	};
	static const struct ae_tab gated[4] =
	{
		{ "A", 0, 1, NULL }, { "B", 0, 0, NULL }, { "C", 0, 0, NULL }, { "D", 0, 1, NULL },
	};
	static const struct { float window_height, view_x, view_width_layout, view_height_layout, view_width, view_height;
		float panel_width, panel_height; } panels[4] =
	{
		{ 720, 960, 960, 540, 640, 360, 294.4f, 309.6f },     /* 720p quarter (right) */
		{ 1080, 960, 960, 540, 960, 540, 441.6f, 464.4f },    /* 1080p quarter */
		{ 1440, 0, 960, 540, 1280, 720, 588.8f, 619.2f },     /* a 1280x720 view (1440p quarter) */
		{ 1080, 0, 1920, 540, 1920, 540, 470.0f, 464.4f },    /* 1080p 2p: a 1920 x 540 half */
	};
	struct ae_density d;
	struct ae_pointer pointer;
	struct ae_event event;
	struct ae_tabs strip;
	struct ae_hit hit;
	struct ae_stub_call const *call;
	struct ae_rect content, box, cap;
	float help_y, prompts_y;
	int index, n;

	/* I1: two footers; a move over the first's prompt, then the second's pointer call (not over it): the first keeps
	its hover */
	use_device(AE_DEVICE_KEYBOARD_MOUSE);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_density_full(1080, 1.0f, &d);
	ae_widget_prompts(&d, 96, 500, pair, 2, NULL, 900, -1, 6);
	ae_widget_prompts(&d, 1000, 500, pair, 2, NULL, 1800, -1, 7);
	memset(&pointer, 0, sizeof(pointer));
	call = text_call("Select");
	CHECK(call != NULL);
	middle_of(call, &pointer.x, &pointer.y, 1080);
	pointer.moved = 1;
	ae_prompts_pointer(pair, 2, &pointer, 6, &event);
	ae_prompts_pointer(pair, 2, &pointer, 7, &event);
	ae_stub_reset(1920, 1080);
	ae_widget_prompts(&d, 96, 500, pair, 2, NULL, 900, -1, 6);
	ae_widget_prompts(&d, 1000, 500, pair, 2, NULL, 1800, -1, 7);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_HOVER) == 1);
	for (index = 0; index < ae_stub_count(); index++)
		if (ae_stub_get(index)->kind == AE_STUB_RECT && ae_stub_get(index)->rgba == AE_COLOR_HOVER)
			CHECK(ae_stub_get(index)->x < 900);
	/* (moved off the first footer: its own call clears it) */
	pointer.x = 5;
	ae_prompts_pointer(pair, 2, &pointer, 6, &event);
	ae_stub_reset(1920, 1080);
	ae_widget_prompts(&d, 96, 500, pair, 2, NULL, 900, -1, 6);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_HOVER) == 0);

	/* I2 / I3: a VIEW footer at a 720p quarter: key words at the 14 px floor, the cap grown to hold them (never
	clipped), labels 14 px; pads' glyphs 18 px */
	ae_stub_reset(1920, 720);
	ae_hits_clear();
	ae_draw_view(960, 0, 960, 540);
	ae_density_view(640, 360, 1.0f, &d);
	ae_widget_prompts(&d, 30, 900, pair, 2, NULL, 900, -1, 8);
	call = text_call("Esc");
	CHECK(call && ae_stub_text_em_pixels(call) >= 14.0f - 0.01f);
	CHECK(text_call("Back") && ae_stub_text_em_pixels(text_call("Back")) >= 14.0f - 0.01f);
	n = 0;
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *rect = ae_stub_get(index);

		if (rect->kind != AE_STUB_RECT || rect->rgba != AE_COLOR_KEY_CAP)
			continue;
		cap = pixels_of(rect);
		/* (each cap holds its words: the text box inside the cap, 2 px of ink allowed at the sides) */
		call = ae_stub_get(index + 1);
		CHECK(call->kind == AE_STUB_TEXT);
		box = pixels_of(call);
		CHECK(box.x >= cap.x - 2 && box.x + box.width <= cap.x + cap.width + 2 && box.y >= cap.y - 0.5f &&
			box.y + box.height <= cap.y + cap.height + 0.5f);
		CHECK(cap.height >= ae_stub_text_em_pixels(call) / 0.5f - 0.01f);
		n++;
	}
	CHECK(n == 2);
	/* (and a hit in this right-hand view: layout units, past the view's left) */
	middle_of(text_call("Back"), &pointer.x, &pointer.y, 720);
	CHECK(ae_hit_at(pointer.x, pointer.y, 0, &hit) && hit.id == 8 && hit.part == AE_PART_PROMPT && hit.index == 1 &&
		hit.rect.x >= 960);
	pointer.left_clicks = 1;
	CHECK(ae_prompts_pointer(pair, 2, &pointer, 8, &event) == 1 && event.action == AE_ACTION_BACK);
	pointer.left_clicks = 0;
	use_device(AE_DEVICE_XBOX);
	ae_stub_reset(1920, 720);
	ae_draw_view(960, 0, 960, 540);
	ae_widget_prompts(&d, 30, 900, pair, 2, NULL, 900, -1, 8);
	for (index = 0; index < ae_stub_count(); index++)
		if (ae_stub_get(index)->kind == AE_STUB_BUTTON)
			CHECK(ae_stub_text_em_pixels(ae_stub_get(index)) >= 18.0f - 0.01f);
	/* M2: three keyboard prompts in a 720p quarter's panel: the row stays left of right_x (labels shortened), the
	status dropped first */
	use_device(AE_DEVICE_KEYBOARD_MOUSE);
	ae_stub_reset(1920, 720);
	ae_hits_clear();
	ae_draw_view(960, 0, 960, 540);
	ae_widget_view_panel(&d, 640, 360, 0, 0xD94B4BFFu, "GAME", 0, 6, &content);
	ae_view_panel_footer(&d, 640, 360, &help_y, &prompts_y);
	ae_stub_reset(1920, 720);
	ae_draw_view(960, 0, 960, 540);
	ae_widget_prompts(&d, content.x, prompts_y, three, 3, "Changes apply as you make them", content.x + content.width,
		-1, 9);
	CHECK(text_call("Changes apply as you make them") == NULL);
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *any = ae_stub_get(index);

		if (any->kind == AE_STUB_OUTLINE)
			CHECK(any->x + any->width <= content.x + content.width + 0.01f);
	}

	/* I3: tabs and page dots in a right-hand view: hits in layout units */
	use_device(AE_DEVICE_XBOX);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_draw_view(960, 0, 960, 540);
	ae_density_view(960, 540, 1.0f, &d);
	memset(&strip, 0, sizeof(strip));
	strip.tabs = eight;
	strip.count = 3;
	strip.hover = -1;
	ae_widget_tabs(&d, &strip, 30, 30, 1800, 10);
	ae_widget_page_dots(&d, 960, 300, "MATCH", 6, 1, AE_COLOR_ACCENT, 11);
	middle_of(text_call("ITEMS"), &pointer.x, &pointer.y, 1080);
	CHECK(ae_hit_at(pointer.x, pointer.y, 0, &hit) && hit.id == 10 && hit.part == AE_PART_TAB && hit.index == 2 &&
		hit.rect.x >= 960);
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *pill = ae_stub_get(index);

		if (pill->kind == AE_STUB_RECT && pill->rgba == AE_COLOR_ACCENT && near(pill->width, 22 * d.unit, 0.01f))
		{
			middle_of(pill, &pointer.x, &pointer.y, 1080);
			CHECK(ae_hit_at(pointer.x, pointer.y, 0, &hit) && hit.id == 11 && hit.part == AE_PART_DOT && hit.index == 1 &&
				hit.rect.x >= 960);
		}
	}

	/* I3: the panel at more sizes (spec 6's table), its pill 22 u in the player colour; M1: its dots add no hits */
	for (n = 0; n < 4; n++)
	{
		int panel_found = 0, pill_found = 0;

		ae_stub_reset(1920, panels[n].window_height);
		ae_hits_clear();
		ae_draw_view(panels[n].view_x, 0, panels[n].view_width_layout, panels[n].view_height_layout);
		ae_density_view(panels[n].view_width, panels[n].view_height, 1.0f, &d);
		ae_widget_view_panel(&d, panels[n].view_width, panels[n].view_height, 3, 0xE0B020FFu, "TRAINING", 2, 6, &content);
		for (index = 0; index < ae_stub_count(); index++)
		{
			struct ae_stub_call const *rect = ae_stub_get(index);
			float view_left = panels[n].view_x * panels[n].window_height / 1080.0f;

			if (rect->kind != AE_STUB_RECT)
				continue;
			box = pixels_of(rect);
			if (rect->rgba == AE_COLOR_VIEW_PANEL)
			{
				panel_found = 1;
				CHECK(near(box.width, panels[n].panel_width, .6f) && near(box.height, panels[n].panel_height, .6f));
				CHECK(box.x >= view_left && box.x + box.width < view_left + panels[n].view_width * 0.5f);
				CHECK(box.y + box.height <= panels[n].view_height);
			}
			if (rect->rgba == 0xE0B020FFu && near(rect->width, 22 * d.unit, 0.01f) && near(rect->height, 8 * d.unit, 0.01f))
			{
				pill_found = 1;
				middle_of(rect, &pointer.x, &pointer.y, panels[n].window_height);
				CHECK(!ae_hit_at(pointer.x, pointer.y, 0, &hit));
			}
		}
		CHECK(panel_found && pill_found);
		CHECK(text_call("PLAYER 4") && ae_stub_text_em_pixels(text_call("PLAYER 4")) >= 16.0f - 0.01f);
		ae_view_panel_footer(&d, panels[n].view_width, panels[n].view_height, &help_y, &prompts_y);
		CHECK(content.height > 0 && content.y + content.height <= help_y + 0.01f && help_y < prompts_y);
	}
	/* M3: a tiny view: the footer starts under the dots, never over the header */
	ae_stub_reset(1920, 1080);
	ae_draw_view(0, 0, 320, 180);
	ae_density_view(320, 180, 1.0f, &d);
	ae_widget_view_panel(&d, 320, 180, 0, 0xD94B4BFFu, "GAME", 0, 6, &content);
	ae_view_panel_footer(&d, 320, 180, &help_y, &prompts_y);
	CHECK(content.height >= 0 && help_y >= content.y - 0.01f && prompts_y > help_y);

	/* M4: a strip never drawn, the last tab active: drawn already scrolled, no slide in */
	ae_motion_set_reduced(0);
	ae_motion_set_now(20000);
	ae_stub_reset(1440, 1080);
	ae_density_full(1080, 1.3f, &d);
	memset(&strip, 0, sizeof(strip));
	strip.tabs = eight;
	strip.count = 8;
	strip.active = 7;
	strip.hover = -1;
	ae_widget_tabs(&d, &strip, 72, 100, 1296, 2);
	call = text_call("TRAINING OPTIONS");
	CHECK(call && call->x + call->width <= 72 + 1296 && !ae_motion_running(&strip.strip));
	ae_motion_set_reduced(1);

	/* M6: a step with no valid current: the first that can be chosen from the direction's end */
	memset(&strip, 0, sizeof(strip));
	strip.tabs = gated;
	strip.count = 4;
	strip.active = -1;
	CHECK(ae_tabs_step(&strip, 1) == 1 && ae_tabs_step(&strip, -1) == 2);
	CHECK(ae_page_step(5, -1, 1) == 0 && ae_page_step(5, 9, -1) == 4);
	CHECK(!ae_stub_overflowed());
}

int main(void)
{
	tabs();
	dots();
	prompts();
	panel();
	fixes();
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
