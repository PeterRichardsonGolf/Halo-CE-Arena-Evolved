#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_draw_stub.h"
#include "ae_font.h"
#include "ae_result.h"
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

/* the first call of a kind and colour, in window pixels */
static int find_rect(int kind, unsigned int rgba, struct ae_rect *pixels)
{
	int index;

	for (index = 0; index < ae_stub_count(); index++)
		if (ae_stub_get(index)->kind == kind && ae_stub_get(index)->rgba == rgba)
		{
			ae_stub_pixels(ae_stub_get(index), pixels);
			return 1;
		}
	return 0;
}

/* inside, with 2 px of horizontal ink allowed */
static int inside(struct ae_rect const *a, struct ae_rect const *b)
{
	return a->x >= b->x - 2.0f && a->y >= b->y - 0.05f && a->x + a->width <= b->x + b->width + 2.0f &&
		a->y + a->height <= b->y + b->height + 0.05f;
}

static void middle_of(struct ae_stub_call const *call, float *x, float *y, float window_height)
{
	struct ae_rect pixels;

	ae_stub_pixels(call, &pixels);
	*x = (pixels.x + pixels.width * 0.5f) * 1080.0f / window_height;
	*y = (pixels.y + pixels.height * 0.5f) * 1080.0f / window_height;
}

/* the test's host: the log kept */
static char logged[512];
static void test_log(const char *text) { snprintf(logged, sizeof(logged), "%s", text); }
static struct ae_host const test_host = { test_log, NULL, NULL, NULL, NULL, NULL };

static short picked_choice = -1;
static int picked_count;
static void picked(short choice, void *context)
{
	(void)context;
	picked_choice = choice;
	picked_count++;
}

static struct ae_screen_class const base = { .name = "base" };

/* (N1) a revert's callback that opens another player's dialog */
static struct ae_dialog_spec other_spec;
static int other_opened;
static void picked_then_open(short choice, void *context)
{
	(void)context;
	picked_choice = choice;
	picked_count++;
	other_opened = ae_dialog_open(&other_spec, 1);
}

static void dispatch(short player, int action)
{
	struct ae_event event;

	event.player = player;
	event.device = AE_DEVICE_XBOX;
	event.repeat = 0;
	event.action = (unsigned char)action;
	ae_ui_dispatch(&event);
}

static void draw(float window_height)
{
	ae_stub_reset(1920, window_height);
	ae_hits_clear();
	ae_ui_draw();
}

static void confirm_spec(struct ae_dialog_spec *spec, struct ae_density const *density)
{
	memset(spec, 0, sizeof(*spec));
	spec->kind = AE_DIALOG_CONFIRM;
	spec->title = "Quit Arena Evolved?";
	spec->body = "You are hosting a LAN game with 3 players. Quitting ends it for everyone.";
	spec->choices[0] = "QUIT";
	spec->choices[1] = "CANCEL";
	spec->choice_count = 2;
	spec->safe_choice = 1;
	spec->cancel_choice = 1;
	spec->density = *density;
	spec->bounds.width = 1920;
	spec->bounds.height = 1080;
	spec->picked = picked;
}

static void confirm(void)
{
	struct ae_density d;
	struct ae_dialog_spec spec;
	struct ae_rect box, pixels, frame = { 0, 0, 1920, 1080 };
	struct ae_pointer pointer;

	ae_host_set(&test_host);
	ae_motion_set_reduced(1);
	ae_motion_set_now(1000);
	ae_ui_reset();
	ae_ui_set_before_draw(NULL);
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_ui_top()->focus = 2;
	ae_density_full(1080, 1.0f, &d);
	confirm_spec(&spec, &d);
	ae_sound_reset();
	/* it opens (forward) with the focus on the safe choice: CANCEL on the white bar */
	CHECK(ae_dialog_open(&spec, AE_OWNER_ANY) && ae_ui_depth() == 2 && ae_sound_take(0) == AE_SOUND_FORWARD);
	CHECK(!ae_dialog_open(&spec, AE_OWNER_ANY));
	draw(1080);
	CHECK(text_call("CANCEL") && text_call("CANCEL")->rgba == AE_COLOR_SELECTION_TEXT && text_call("QUIT") &&
		text_call("QUIT")->rgba == AE_COLOR_TEXT);
	/* the scrim, the panel with its accent stripe (4 u), the title (32 u) and the body; its prompts inside it */
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_SCRIM) == 1 && find_rect(AE_STUB_RECT, AE_COLOR_POPOVER, &box));
	CHECK(find_rect(AE_STUB_RECT, AE_COLOR_ACCENT, &pixels) && near(pixels.height, 4.0f, 0.01f) &&
		near(pixels.width, box.width, 0.01f) && near(pixels.y, box.y, 0.01f));
	CHECK(text_call("Quit Arena Evolved?") && text_call("Quit Arena Evolved?")->size == 32 &&
		text_call("Quit Arena Evolved?")->rgba == AE_COLOR_TITLE);
	CHECK(text_call("Select") && text_call("Cancel"));
	ae_stub_pixels(text_call("Cancel"), &pixels);
	CHECK(inside(&pixels, &box));
	/* the box: 720 u (I2: the floor at FULL), inside the frame, centred; ae_dialog_place says the same */
	CHECK(near(box.width, 720.0f, 0.01f) && inside(&box, &frame));
	CHECK(near(box.x + box.width * 0.5f, 960.0f, 0.5f) && near(box.y + box.height * 0.5f, 540.0f, 0.5f));
	{
		struct ae_rect placed;

		ae_dialog_place(&spec, &placed);
		CHECK(near(placed.x, box.x, 0.01f) && near(placed.width, box.width, 0.01f) &&
			near(placed.height, box.height, 0.01f));
	}
	/* (every body line inside the box) */
	{
		int index;

		for (index = 0; index < ae_stub_count(); index++)
			if (ae_stub_get(index)->kind == AE_STUB_TEXT)
			{
				ae_stub_pixels(ae_stub_get(index), &pixels);
				CHECK(inside(&pixels, &box));
			}
	}
	/* a click outside: nothing (still open, nothing picked) */
	memset(&pointer, 0, sizeof(pointer));
	pointer.x = 40;
	pointer.y = 40;
	pointer.left_clicks = 1;
	ae_ui_dispatch_pointer(&pointer);
	CHECK(ae_ui_depth() == 2 && picked_count == 0);
	/* down / up move the focus; B picks the cancel choice (back) and the opener's focus is as it was */
	dispatch(0, AE_ACTION_UP);
	draw(1080);
	CHECK(text_call("QUIT")->rgba == AE_COLOR_SELECTION_TEXT);
	ae_sound_reset();
	dispatch(0, AE_ACTION_BACK);
	CHECK(picked_choice == 1 && picked_count == 1 && ae_ui_depth() == 1 && ae_ui_top()->focus == 2 &&
		ae_sound_take(0) == AE_SOUND_BACK);
	/* A picks the focused one (forward) */
	CHECK(ae_dialog_open(&spec, AE_OWNER_ANY));
	dispatch(0, AE_ACTION_DOWN);
	dispatch(0, AE_ACTION_UP);
	ae_sound_reset();
	dispatch(0, AE_ACTION_ACCEPT);
	CHECK(picked_choice == 0 && picked_count == 2 && ae_ui_depth() == 1 && ae_sound_take(0) == AE_SOUND_FORWARD);
	/* a click on a choice picks it */
	CHECK(ae_dialog_open(&spec, AE_OWNER_ANY));
	draw(1080);
	middle_of(text_call("QUIT"), &pointer.x, &pointer.y, 1080);
	pointer.left_clicks = 1;
	ae_ui_dispatch_pointer(&pointer);
	CHECK(picked_choice == 0 && picked_count == 3 && ae_ui_depth() == 1);
	/* a click on the B prompt: the cancel choice */
	CHECK(ae_dialog_open(&spec, AE_OWNER_ANY));
	ae_motion_set_now(1000);
	draw(1080);
	{
		struct ae_rect prompt;

		ae_stub_pixels(text_call("Cancel"), &prompt);
		pointer.x = prompt.x + prompt.width * 0.5f;
		pointer.y = prompt.y + prompt.height * 0.5f;
	}
	ae_ui_dispatch_pointer(&pointer);
	CHECK(picked_choice == 1 && picked_count == 4 && ae_ui_depth() == 1);
	CHECK(!ae_stub_overflowed());
}

/* the open motion: the scrim and the dialog fade in over 150 ms */
static void motion(void)
{
	struct ae_density d;
	struct ae_dialog_spec spec;
	int index, scrims = 0;

	ae_motion_set_reduced(0);
	ae_motion_set_now(5000);
	ae_ui_reset();
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_density_full(1080, 1.0f, &d);
	confirm_spec(&spec, &d);
	CHECK(ae_dialog_open(&spec, AE_OWNER_ANY));
	draw(1080);
	for (index = 0; index < ae_stub_count(); index++)
		if (ae_stub_get(index)->kind == AE_STUB_RECT && (ae_stub_get(index)->rgba & 0xFFFFFF00u) == (AE_COLOR_SCRIM & 0xFFFFFF00u))
		{
			CHECK((ae_stub_get(index)->rgba & 0xFFu) < 8);
			scrims++;
		}
	CHECK(scrims == 1 && count_calls(AE_STUB_RECT, AE_COLOR_POPOVER) == 0);
	ae_motion_set_now(5150);
	draw(1080);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_SCRIM) == 1 && count_calls(AE_STUB_RECT, AE_COLOR_POPOVER) == 1);
	ae_motion_set_reduced(1);
	ae_ui_reset();
}

/* the bar's width (the accent rect 3 u tall) */
static float bar_width(void)
{
	int index;

	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *call = ae_stub_get(index);

		if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_ACCENT && near(call->height, 3.0f, 0.01f))
			return call->width;
	}
	return -1.0f;
}

static void timed_revert(void)
{
	struct ae_density d;
	struct ae_dialog_spec spec;
	float full, at_2, at_2_5, at_3;

	ae_motion_set_reduced(0);
	ae_motion_set_now(100000);
	ae_ui_reset();
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_density_full(1080, 1.0f, &d);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_TIMED_REVERT;
	spec.title = "Keep these display settings?";
	spec.body = "BORDERLESS at 2560 x 1440.";
	spec.choices[0] = "KEEP";
	spec.choices[1] = "REVERT";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 1;
	spec.timeout_choice = 1;
	spec.density = d;
	spec.bounds.width = 1920;
	spec.bounds.height = 1080;
	spec.picked = picked;
	picked_count = 0;
	picked_choice = -1;
	CHECK(ae_dialog_open(&spec, AE_OWNER_ANY));
	/* the focus on KEEP; "Reverting in 10 s"; the bar full and draining continuously */
	ae_motion_set_now(100000 + 150);
	draw(1080);
	CHECK(text_call("KEEP") && text_call("KEEP")->rgba == AE_COLOR_SELECTION_TEXT);
	CHECK(text_call("Reverting in 10 s") && text_call("Reverting in 10 s")->rgba == AE_COLOR_ACCENT);
	full = bar_width();
	ae_motion_set_now(100000 + 2000);
	draw(1080);
	at_2 = bar_width();
	ae_motion_set_now(100000 + 2500);
	draw(1080);
	at_2_5 = bar_width();
	CHECK(text_call("Reverting in 8 s") && full > at_2 && at_2 > at_2_5 && near(at_2_5 / at_2, 0.75f / 0.8f, 0.001f));
	/* REDUCE MOTION: whole seconds (the width at 2.5 s is the width at 2 s) */
	ae_motion_set_reduced(1);
	draw(1080);
	at_2_5 = bar_width();
	ae_motion_set_now(100000 + 2000);
	draw(1080);
	at_2 = bar_width();
	ae_motion_set_now(100000 + 3000);
	draw(1080);
	at_3 = bar_width();
	CHECK(near(at_2_5, at_2, 0.001f) && at_3 < at_2);
	/* 9.9 s: nothing picked; 10 s: the timeout choice (REVERT) */
	ae_motion_set_now(100000 + 9900);
	ae_dialog_tick();
	CHECK(picked_count == 0 && ae_ui_depth() == 2);
	ae_motion_set_now(100000 + 10000);
	ae_dialog_tick();
	CHECK(picked_count == 1 && picked_choice == 1 && ae_ui_depth() == 1);
	/* covered by another screen at 10 s: reverted all the same, the cover stays on top */
	{
		static int cover;

		ae_motion_set_now(200000);
		CHECK(ae_dialog_open(&spec, AE_OWNER_ANY));
		ae_ui_push(&base, AE_OWNER_ANY, &cover);
		ae_motion_set_now(200000 + 9999);
		ae_dialog_tick();
		CHECK(picked_count == 1 && ae_ui_depth() == 3);
		ae_motion_set_now(200000 + 10000);
		ae_dialog_tick();
		CHECK(picked_count == 2 && picked_choice == 1 && ae_ui_depth() == 2 && ae_ui_top()->data == &cover);
		ae_dialog_tick();
		CHECK(picked_count == 2);
		ae_ui_pop();
	}
	/* a reset mid-countdown: the timeout choice at once, once */
	ae_motion_set_now(300000);
	CHECK(ae_dialog_open(&spec, AE_OWNER_ANY));
	ae_motion_set_now(303000);
	ae_ui_reset();
	CHECK(picked_count == 3 && picked_choice == 1 && ae_ui_depth() == 0);
	ae_motion_set_now(320000);
	ae_dialog_tick();
	ae_ui_reset();
	CHECK(picked_count == 3);
	/* (N1) a reset whose revert callback opens player 2's dialog: that dialog stays open (its slot not cleared) */
	{
		struct ae_dialog_spec timed = spec;

		other_spec = spec;
		other_spec.kind = AE_DIALOG_CONFIRM;
		other_spec.picked = picked;
		timed.picked = picked_then_open;
		other_opened = 0;
		CHECK(ae_dialog_open(&timed, 0));
		ae_ui_reset();
		CHECK(picked_count == 4 && other_opened && ae_ui_depth() == 1 && !ae_dialog_open(&other_spec, 1));
		ae_ui_reset();
		picked_count = 3;
	}
	/* (N2) a timed revert open is known to the hooks, which tick it with AE's menus off too: reverted at 10 s with
	nothing drawn or updated, then none open */
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_motion_set_now(350000);
	CHECK(!ae_dialog_timed_open() && ae_dialog_open(&spec, AE_OWNER_ANY) && ae_dialog_timed_open());
	ae_motion_set_now(360000);
	ae_dialog_tick();
	CHECK(picked_count == 4 && picked_choice == 1 && !ae_dialog_timed_open() && ae_ui_depth() == 1);
	picked_count = 3;
	ae_ui_reset();
	/* the clock gone back before the opening (M4): the whole 10 s left, nothing picked */
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_motion_set_now(400000);
	CHECK(ae_dialog_open(&spec, AE_OWNER_ANY));
	ae_motion_set_now(1000);
	ae_dialog_tick();
	draw(1080);
	CHECK(picked_count == 3 && text_call("Reverting in 10 s") != NULL);
	ae_ui_reset();
	CHECK(picked_count == 4);
	CHECK(!ae_stub_overflowed());
}

static void error_dialog(void)
{
	struct ae_density d;
	struct ae_dialog_spec spec;
	struct ae_result result;
	struct ae_rect pixels;

	ae_ui_reset();
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_density_full(1080, 1.0f, &d);
	result.ok = 0;
	snprintf(result.reason, sizeof(result.reason), "Host is on network version 21, you are on 20.");
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_ERROR;
	spec.title = "Can't join";
	spec.body = result.reason;
	spec.choices[0] = "BACK TO SERVER BROWSER";
	spec.choices[1] = "DETAILS";
	spec.choice_count = 2;
	spec.density = d;
	spec.bounds.width = 1920;
	spec.bounds.height = 1080;
	spec.picked = picked;
	logged[0] = 0;
	ae_sound_reset();
	/* failure as it opens; the glue's text as it is in the log; a warning stripe and title */
	CHECK(ae_dialog_open(&spec, AE_OWNER_ANY) && ae_sound_take(0) == AE_SOUND_FAILURE);
	CHECK(!strcmp(logged, "ae menus: error: Can't join: Host is on network version 21, you are on 20."));
	draw(1080);
	CHECK(find_rect(AE_STUB_RECT, AE_COLOR_WARNING, &pixels) && near(pixels.height, 4.0f, 0.01f));
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_ACCENT) == 1);   /* (only the focused row's notch) */
	CHECK(text_call("Can't join") && text_call("Can't join")->rgba == AE_COLOR_WARNING);
	CHECK(text_call(result.reason) && text_call(result.reason)->rgba == AE_COLOR_TEXT);
	CHECK(text_call("BACK TO SERVER BROWSER")->rgba == AE_COLOR_SELECTION_TEXT);
	dispatch(0, AE_ACTION_BACK);
	CHECK(picked_choice == 0 && ae_ui_depth() == 1);
	/* a long body: 720 u, its lines wrapped inside; a short one: 720 u too; a title wider than 720 u: wider, to 840 */
	spec.kind = AE_DIALOG_CONFIRM;
	spec.body = "A much longer reason than usual, which keeps going for a while so it has to wrap onto several "
		"lines inside the dialog, and not one of them may leave the box at any width.";
	ae_dialog_place(&spec, &pixels);
	CHECK(near(pixels.width, 720.0f, 0.01f));
	spec.body = "OK.";
	ae_dialog_place(&spec, &pixels);
	CHECK(near(pixels.width, 720.0f, 0.01f));
	spec.title = "A title much too long for seven hundred and twenty units";
	ae_dialog_place(&spec, &pixels);
	CHECK(pixels.width > 720.0f && pixels.width <= 840.0f + 0.01f);
	spec.title = "A title far, far too long for even eight hundred and forty units of dialog width";
	ae_dialog_place(&spec, &pixels);
	CHECK(near(pixels.width, 840.0f, 0.01f));
	CHECK(!ae_stub_overflowed());
}

/* M2 ruling: an error while its owner's slot is busy: logged and failed at once, shown when the slot frees (the
newest); M1: the dialog keeps its own copy of the strings */
static void errors_waiting(void)
{
	struct ae_density d;
	struct ae_dialog_spec spec, error;
	char title[64], reason[128];

	ae_ui_reset();
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_density_full(1080, 1.0f, &d);
	confirm_spec(&spec, &d);
	snprintf(title, sizeof(title), "Quit now?");
	spec.title = title;
	CHECK(ae_dialog_open(&spec, 0));
	snprintf(title, sizeof(title), "Changed after the call");
	draw(1080);
	CHECK(text_call("Quit now?") != NULL && text_call("Changed after the call") == NULL);
	memset(&error, 0, sizeof(error));
	error.kind = AE_DIALOG_ERROR;
	error.title = "Can't join";
	error.body = reason;
	error.choices[0] = "BACK";
	error.choice_count = 1;
	error.density = d;
	error.bounds.width = 1920;
	error.bounds.height = 1080;
	error.picked = picked;
	snprintf(reason, sizeof(reason), "The first reason.");
	logged[0] = 0;
	ae_sound_reset();
	CHECK(!ae_dialog_open(&error, 0) && ae_ui_depth() == 2);
	CHECK(!strcmp(logged, "ae menus: error: Can't join: The first reason.") && ae_sound_take(0) == AE_SOUND_FAILURE);
	snprintf(reason, sizeof(reason), "The second reason.");
	CHECK(!ae_dialog_open(&error, 0));
	CHECK(!strcmp(logged, "ae menus: error: Can't join: The second reason."));
	snprintf(reason, sizeof(reason), "Overwritten by the caller.");
	/* (a confirm while busy: refused, nothing kept) */
	CHECK(!ae_dialog_open(&spec, 0));
	ae_dialog_tick();
	CHECK(ae_ui_depth() == 2);
	/* the slot frees: the newest error shows (no second log line, no second failure) */
	dispatch(0, AE_ACTION_BACK);
	logged[0] = 0;
	ae_sound_reset();
	ae_dialog_tick();
	CHECK(ae_ui_depth() == 2 && logged[0] == 0 && ae_sound_take(0) == AE_SOUND_NONE);
	draw(1080);
	CHECK(text_call("The second reason.") && text_call("Can't join") && !text_call("The first reason."));
	dispatch(0, AE_ACTION_ACCEPT);
	ae_dialog_tick();
	CHECK(ae_ui_depth() == 1);
	/* a reset drops a waiting error */
	CHECK(ae_dialog_open(&spec, 0));
	ae_dialog_open(&error, 0);
	ae_ui_reset();
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_dialog_tick();
	CHECK(ae_ui_depth() == 1);
	ae_ui_reset();
	CHECK(!ae_stub_overflowed());
}

/* in a 640x360 quarter's panel (VIEW): inside the panel (P14: under 560 view u there), in the player's own view; two
players' dialogs open at once */
static void view_dialogs(void)
{
	struct ae_density d;
	struct ae_dialog_spec spec;
	struct ae_rect panel, panel_pixels, box, placed;
	float scale = 540.0f / 1080.0f, to_pixels = 720.0f / 1080.0f;
	int index;

	ae_ui_reset();
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_density_view(640, 360, 1.0f, &d);
	ae_view_panel_rect(640, 360, 1.0f, &panel);
	confirm_spec(&spec, &d);
	spec.view.x = 960; spec.view.y = 0; spec.view.width = 960; spec.view.height = 540;
	/* (the panel in layout units of the frame: window pixels of a 720p window x 1.5) */
	spec.bounds.x = 960 + panel.x / to_pixels;
	spec.bounds.y = panel.y / to_pixels;
	spec.bounds.width = panel.width / to_pixels;
	spec.bounds.height = panel.height / to_pixels;
	panel_pixels.x = spec.bounds.x * to_pixels;
	panel_pixels.y = spec.bounds.y * to_pixels;
	panel_pixels.width = spec.bounds.width * to_pixels;
	panel_pixels.height = spec.bounds.height * to_pixels;
	ae_dialog_place(&spec, &placed);
	CHECK(placed.width <= spec.bounds.width - 2.0f * 14.0f * d.unit * scale + 0.01f);
	CHECK(ae_dialog_open(&spec, 1));
	spec.view.x = 0;
	spec.bounds.x -= 960;
	CHECK(ae_dialog_open(&spec, 0) && ae_ui_depth() == 3);
	ae_ui_pop();
	ae_stub_reset(1920, 720);
	ae_hits_clear();
	ae_ui_draw();
	CHECK(find_rect(AE_STUB_RECT, AE_COLOR_POPOVER, &box));
	CHECK(inside(&box, &panel_pixels));
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *call = ae_stub_get(index);
		struct ae_rect pixels;

		if (call->kind != AE_STUB_TEXT)
			continue;
		ae_stub_pixels(call, &pixels);
		CHECK(inside(&pixels, &box));
		/* (VIEW floors: text 16 px, minor 14 px) */
		CHECK(ae_stub_text_em_pixels(call) >= 14.0f - 0.01f);
		if (!strcmp(call->text, "QUIT") || !strcmp(call->text, "Quit Arena Evolved?"))
			CHECK(ae_stub_text_em_pixels(call) >= 16.0f - 0.01f);
	}
	/* (the scrim covers the player's view only) */
	CHECK(find_rect(AE_STUB_RECT, AE_COLOR_SCRIM, &box) && near(box.x, 640.0f, 0.5f) && near(box.width, 640.0f, 0.5f));
	/* player 1's B picks player 1's cancel */
	picked_choice = -1;
	dispatch(1, AE_ACTION_BACK);
	CHECK(picked_choice == 1 && ae_ui_depth() == 1);
	CHECK(!ae_stub_overflowed());
}

static void roster_cards(void)
{
	struct ae_density d;
	struct ae_roster_card card;
	struct ae_rect emblem = { 0, 0, 0, 0 }, number, sub;
	float height;
	int index, found = 0;

	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_density_full(1080, 1.0f, &d);
	memset(&card, 0, sizeof(card));
	card.name = "Guest";
	card.slot = 3;
	card.color = 0x8CB43CFFu;
	card.sub_line = "Controller 3";
	card.team_name = "RED";
	card.team_color = 0xC0392BFFu;
	card.flags = AE_CARD_GUEST | AE_CARD_TEAM | AE_CARD_HOST;
	height = ae_widget_roster_card(&d, 360, 100, 1170, &card, 0x0100, 2);
	CHECK(near(height, 66.0f, 0.01f));
	/* the emblem: 42 u in the player's colour, the slot number inside it (colour is never alone) */
	for (index = 0; index < ae_stub_count(); index++)
		if (ae_stub_get(index)->kind == AE_STUB_RECT && ae_stub_get(index)->rgba == card.color)
		{
			ae_stub_pixels(ae_stub_get(index), &emblem);
			found++;
		}
	CHECK(found == 1 && near(emblem.width, 42.0f, 0.01f) && near(emblem.height, 42.0f, 0.01f));
	CHECK(text_call("3") != NULL);
	ae_stub_pixels(text_call("3"), &number);
	CHECK(number.x >= emblem.x && number.x + number.width <= emblem.x + emblem.width && number.y >= emblem.y &&
		number.y + number.height <= emblem.y + emblem.height);
	CHECK(text_call("3")->rgba == ae_emblem_number_color(card.color));
	/* a guest: "guest (not saved)" on the sub-line; the team's stripe (5 u) and name; HOST */
	CHECK(text_call("Controller 3 \xE2\x80\xA2 guest (not saved)") != NULL);
	ae_stub_pixels(text_call("Controller 3 \xE2\x80\xA2 guest (not saved)"), &sub);
	CHECK(sub.y > number.y && sub.x + sub.width < 360 + 1170);
	CHECK(text_call("Guest") && text_call("Guest")->size == 24 && text_call("RED") && text_call("HOST"));
	found = 0;
	for (index = 0; index < ae_stub_count(); index++)
		if (ae_stub_get(index)->kind == AE_STUB_RECT && ae_stub_get(index)->rgba == card.team_color &&
			near(ae_stub_get(index)->width, 5.0f, 0.01f))
			found++;
	CHECK(found == 1);
	/* focus: the white bar, dark text; editing: a 2 u outline in the player's colour */
	ae_stub_reset(1920, 1080);
	card.flags = AE_CARD_FOCUSED | AE_CARD_EDITING;
	ae_widget_roster_card(&d, 360, 100, 1170, &card, 0x0100, 2);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_SELECTION) == 1 && text_call("Guest")->rgba == AE_COLOR_SELECTION_TEXT);
	CHECK(count_calls(AE_STUB_OUTLINE, card.color) == 1);
	/* away: 43 % and the reason */
	ae_stub_reset(1920, 1080);
	card.flags = AE_CARD_AWAY;
	card.reason = "disconnected";
	ae_widget_roster_card(&d, 360, 100, 1170, &card, 0x0100, 2);
	CHECK(text_call("Controller 3 \xE2\x80\xA2 disconnected") != NULL);
	CHECK((text_call("Guest")->rgba & 0xFFu) == (unsigned int)floorf(255.0f * 0.43f + 0.5f));
	/* VIEW 640x360: the number at the 14 px floor at least (M3) */
	ae_stub_reset(1920, 720);
	ae_density_view(640, 360, 1.0f, &d);
	ae_draw_view(0, 0, 960, 540);
	ae_widget_roster_card(&d, 0, 0, 800, &card, 0x0100, 0);
	CHECK(text_call("3") && ae_stub_text_em_pixels(text_call("3")) >= 14.0f - 0.01f);
	/* VIEW: 52 / 32 */
	ae_stub_reset(1920, 1080);
	ae_density_view(960, 540, 1.0f, &d);
	ae_draw_view(0, 0, 960, 540);
	CHECK(near(ae_widget_roster_card(&d, 0, 0, 800, &card, 0x0100, 0), 52.0f * d.unit, 0.01f));
	/* the number's colour: whichever contrasts more */
	CHECK(ae_emblem_number_color(0xF2D249FFu) == AE_COLOR_SELECTION_TEXT);
	CHECK(ae_emblem_number_color(0x2A5DB0FFu) == AE_COLOR_TITLE);
	CHECK(ae_emblem_number_color(0x8B3FB5FFu) == AE_COLOR_TITLE);
	/* the open card: "Press" [START] "to add a player (split screen)"; keyboard only: the controller sentence */
	ae_stub_reset(1920, 1080);
	ae_density_full(1080, 1.0f, &d);
	CHECK(near(ae_widget_open_card(&d, 360, 600, 1170, 0, 0x0101), 66.0f, 0.01f));
	CHECK(text_call("Press") && text_call("to add a player (split screen)"));
	found = 0;
	for (index = 0; index < ae_stub_count(); index++)
		if (ae_stub_get(index)->kind == AE_STUB_BUTTON && ae_stub_get(index)->align == AE_BUTTON_START)
		{
			struct ae_rect glyph, press, rest;

			ae_stub_pixels(ae_stub_get(index), &glyph);
			ae_stub_pixels(text_call("Press"), &press);
			ae_stub_pixels(text_call("to add a player (split screen)"), &rest);
			CHECK(glyph.x >= press.x + press.width && glyph.x + glyph.width <= rest.x);
			found++;
		}
	CHECK(found == 1);
	ae_stub_reset(1920, 1080);
	ae_widget_open_card(&d, 360, 600, 1170, 1, 0x0101);
	{
		int texts = 0;

		for (index = 0; index < ae_stub_count(); index++)
			if (ae_stub_get(index)->kind == AE_STUB_TEXT &&
				strstr(ae_stub_get(index)->text, "Connect a controller and press START"))
				texts++;
		CHECK(texts == 1 && !text_call("Press"));
	}
	CHECK(!ae_stub_overflowed());
}

int main(void)
{
	confirm();
	motion();
	timed_revert();
	error_dialog();
	errors_waiting();
	view_dialogs();
	roster_cards();
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
