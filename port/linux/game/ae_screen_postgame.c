/*
AE_SCREEN_POSTGAME.C

AE's post-game (ae_screen_postgame.h; the approved design). Its facts and its two ways out: ae_glue_postgame.h.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_glue_pause.h"
#include "ae_glue_postgame.h"
#include "ae_hooks.h"
#include "ae_screen_postgame.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_test_hooks.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	POSTGAME_HIT = 0x7500,
	HIT_PROMPTS = POSTGAME_HIT + 1,
	PROMPTS = 2,
	ROWS = 4,
	STEP_RESULT = 1,
	STEP_CARNAGE = 2,
	/* the rows' timings (the first row at 600 ms, then one every 1100 ms; a number
	counts up in 700 ms), and a row's own fade and rise in */
	ROW_FIRST_MS = 600,
	ROW_EVERY_MS = 1100,
	COUNT_MS = 700,
	ROW_IN_MS = 220,
	/* CARNAGE's fade in */
	CARNAGE_IN_MS = 160
};

/* the backdrop: the game draws no world in its post-game (the scoreboard's scrim colour, opaque) */
#define BACKDROP 0x04070AFFu
#define RESULT_ROW AE_COLOR_ROW
#define CARD_PANEL AE_COLOR_VIEW_PANEL

/* GAME RESULT at 1080 (the mockup's layout units of the 16:9 frame, times UI SCALE for sizes): the column's left and
width, the title's top; the card's left, right and top */
#define COLUMN_X 154.0f
#define COLUMN_W 760.0f
#define COLUMN_TOP 96.0f
/* the title's least size, of its own, before a long one is cut (FFA's spelled places) */
#define TITLE_FLOOR 0.45f
#define CARD_X 1100.0f
#define CARD_RIGHT 1740.0f
#define CARD_TOP 302.0f
#define CARD_GAP 40.0f
#define PROMPTS_FROM_FOOT 96.0f
/* a split view's: the mockup's 0.62 of the full size (a half view's floors: row words 16 px and up), its column at
most 900 wide, the title 24 from the view's top, the prompts 32 from its foot */
#define SPLIT_SCALE 0.62f
#define SPLIT_COLUMN_W 900.0f
#define SPLIT_TOP 24.0f
#define SPLIT_PROMPTS_FROM_FOOT 32.0f
#define SPLIT_PROMPT_SCALE 0.73f
/* CARNAGE at 1080: the banner's top and height, the line under it, the lists' top, the prompts under the lists */
#define BANNER_TOP 90.0f
#define BANNER_H 96.0f
#define META_Y 212.0f
#define LISTS_TOP 248.0f
#define CARNAGE_PROMPTS_GAP 40.0f

static struct
{
	int on_stack;
	int step;
	unsigned long step_ms;
	/* the counting done (all rows in) or skipped */
	int skipped;
	/* a host's CONTINUE before the scores were up: taken when they are */
	int continue_asked;
	int leave_open;
	/* the leave dialog's screen data (ae_dialog_open's, the top as it opened), to close with this screen */
	void *dialog;
	struct ae_postgame_facts facts;
	struct ae_density density;
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	int logged_open;
} pg;

static struct ae_screen_class const postgame_class;

/* ---------- small things */

static float ease_out(float t)
{
	t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

static float cap_top(int font, float size, float center)
{
	return center - ae_draw_cap_height(font, size) * 0.5f;
}

static unsigned int with_alpha(unsigned int rgba, float alpha)
{
	unsigned int a = (unsigned int)((float)(rgba & 0xFF) * (alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha) + 0.5f);

	return (rgba & 0xFFFFFF00u) | a;
}

static struct ae_prompt prompt_of(int button, const char *label, int action)
{
	struct ae_prompt prompt;

	memset(&prompt, 0, sizeof(prompt));
	prompt.button = button;
	prompt.label = label;
	prompt.action = action;
	return prompt;
}

/* the rows of a seat's GAME RESULT: RESULT, KILLS, ASSISTS, and BEST SPREE when it is known */
static short row_count(struct ae_postgame_seat const *seat)
{
	return seat->spree >= 0 ? 4 : 3;
}

static unsigned long since(void)
{
	return ae_motion_now() - pg.step_ms;
}

/* every row in and counted */
static unsigned long counting_ms(void)
{
	short most = 0, local;

	for (local = 0; local < AE_POSTGAME_SEATS; local++)
	{
		if (pg.facts.seats[local].present && row_count(&pg.facts.seats[local]) > most)
			most = row_count(&pg.facts.seats[local]);
	}
	return most ? ROW_FIRST_MS + (unsigned long)(most - 1) * ROW_EVERY_MS + COUNT_MS : 0;
}

static int counting(void)
{
	return pg.step == STEP_RESULT && !pg.skipped && since() < counting_ms();
}

/* ---------- the ways on and out */

static void to_carnage(void)
{
	pg.step = STEP_CARNAGE;
	pg.step_ms = ae_motion_now();
	pg.skipped = 1;
	platform_log("ae postgame: carnage");
	/* (debug.capture_on_event's "pg_carnage": the tests' frame of it) */
	ae_test_capture("pg_carnage", 900);
}

static void leave_picked(short choice, void *context)
{
	(void)context;
	pg.leave_open = 0;
	pg.dialog = NULL;
	if (choice != 1)
	{
		platform_log("ae postgame: stayed (leave not picked)");
		return;
	}
	ae_glue_postgame_leave();
}

static void ask_leave(void)
{
	struct ae_dialog_spec spec;
	struct ae_layout layout;
	struct ae_frame frame;

	ae_draw_current_layout(&layout);
	ae_frame_compute(layout.width, ae_settings_ui_scale(), &frame);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Leave the game?";
	spec.body = pg.facts.host ? "The game closes for everyone in it." : "You go back to the main menu.";
	spec.choices[0] = "STAY";
	spec.choices[1] = "LEAVE";
	spec.choice_count = 2;
	spec.has_cancel = 1;
	spec.density = pg.density;
	spec.bounds = frame.rect;
	spec.picked = leave_picked;
	platform_log("ae postgame: leave asked");
	if (ae_dialog_open(&spec, 0))
	{
		pg.leave_open = 1;
		pg.dialog = ae_ui_top() ? ae_ui_top()->data : NULL;
	}
	else
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static void ask_continue(void)
{
	if (!pg.facts.host)
	{
		/* (a guest's machine goes back with its host's) */
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae postgame: continue asked");
	if (!ae_glue_postgame_continue())
		pg.continue_asked = 1;
}

/* ---------- input */

static int postgame_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	if (pg.step == STEP_RESULT)
	{
		/* (anyone's A or START: the counting skipped, then on to CARNAGE) */
		if (event->action != AE_ACTION_ACCEPT && event->action != AE_ACTION_START)
			return 1;
		if (counting())
		{
			pg.skipped = 1;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			platform_log("ae postgame: counting skipped");
			return 1;
		}
		ae_sound_request(AE_SOUND_FORWARD, 0);
		to_carnage();
		return 1;
	}
	/* CARNAGE: player 1's (the owner's "p1/host only", as the game's report had it) */
	if (event->player != 0)
		return 1;
	switch (event->action)
	{
	case AE_ACTION_ACCEPT:
	case AE_ACTION_START:
		ask_continue();
		return 1;
	case AE_ACTION_BACK:
		ae_sound_request(AE_SOUND_FORWARD, 0);
		ask_leave();
		return 1;
	}
	return 1;
}

static void postgame_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;

	(void)screen;
	if (pointer->player != 0)
		return;
	if (ae_prompts_pointer(pg.prompts, pg.prompt_count, pointer, HIT_PROMPTS, &event))
		postgame_handle(NULL, &event);
}

/* ---------- GAME RESULT */

/* a view's rectangle in layout units, as compute_window_bounds splits the screen in play */
static void view_rect(short view, short views, short spans, struct ae_rect *rect)
{
	struct ae_layout layout;
	short columns = views <= 2 ? 1 : 2, rows = views <= 1 ? 1 : 2, slot = view;

	ae_draw_current_layout(&layout);
	if (views == 3 && view > 0)
		slot = (short)(view + 1);
	rect->width = layout.width / columns * (spans ? 2 : 1);
	rect->height = layout.height / rows;
	rect->x = (slot % columns) * (layout.width / columns);
	rect->y = (slot / columns) * (layout.height / rows);
}

/* the column's height at scale s (title to the last row), for a split view's fit */
static float column_height(short rows, float s)
{
	return (84.0f + 48.0f + 36.0f + 20.0f + 18.0f + rows * 100.0f - 8.0f) * s;
}

/* a seat's column: the title, MODE ON MAP, the player, the rows (as far as the clock has them in); returns its foot */
static float draw_column(struct ae_postgame_seat const *seat, float x, float y, float w, float s, int full)
{
	char line[112], number[24];
	float e = 36.0f * s, row_h = 92.0f * s, cy;
	unsigned long t = pg.skipped ? 0xFFFFFFFFul : since();
	short rows = row_count(seat), index;

	/* the title (OpenCE, as SETTINGS' titles) in the result's colour; the gametype on the map under it */
	{
		/* (a long one, "TWENTY-SECOND PLACE": smaller to fit the column, down to TITLE_FLOOR of its size, then cut) */
		float size = 76.0f * s, width = ae_draw_text_width(AE_FONT_TITLE, size, seat->title);
		char fitted[64];

		if (width > w)
			size = size * w / width > TITLE_FLOOR * size ? size * w / width : TITLE_FLOOR * size;
		ae_fit_text(AE_FONT_TITLE, size, seat->title, w, fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_TITLE, size, x, cap_top(AE_FONT_TITLE, size, y + 34.0f * s), AE_ALIGN_LEFT,
			seat->title_color, fitted);
	}
	y += 84.0f * s;
	snprintf(line, sizeof(line), "%s ON %s", pg.facts.gametype, pg.facts.map);
	{
		float size = full ? 22.0f * s : (22.0f * s > 14.0f ? 22.0f * s : 14.0f);
		char fitted[112];

		ae_fit_text(AE_FONT_BODY, size, line, w, fitted, sizeof(fitted));
		ae_draw_text_tracked(AE_FONT_BODY, size, 0.08f, x, cap_top(AE_FONT_BODY, size, y + 12.0f * s), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, fitted);
	}
	y += 48.0f * s;
	/* the player: the emblem with the slot number, the name, the team */
	ae_draw_rect(x, y, e, e, 2.0f * s, seat->chip);
	snprintf(number, sizeof(number), "%d", seat->slot);
	ae_draw_text(AE_FONT_ROW, 22.0f * s, x + e * 0.5f, cap_top(AE_FONT_ROW, 22.0f * s, y + e * 0.5f), AE_ALIGN_CENTER,
		ae_emblem_number_color(seat->chip), number);
	{
		char fitted[48];
		float team_w = seat->team[0] ? ae_draw_text_tracked_width(AE_FONT_BODY, 18.0f * s, 0.08f, seat->team) +
			24.0f * s : 0.0f;

		ae_fit_text(AE_FONT_ROW, 26.0f * s, seat->name, w - e - 14.0f * s - team_w, fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_ROW, 26.0f * s, x + e + 14.0f * s, cap_top(AE_FONT_ROW, 26.0f * s, y + e * 0.5f),
			AE_ALIGN_LEFT, AE_COLOR_TITLE, fitted);
	}
	if (seat->team[0])
		ae_draw_text_tracked(AE_FONT_BODY, 18.0f * s, 0.08f, x + w, cap_top(AE_FONT_BODY, 18.0f * s, y + e * 0.5f),
			AE_ALIGN_RIGHT, AE_COLOR_MUTED, seat->team);
	y += e + 20.0f * s;
	ae_draw_rect(x, y, w, s > 0.5f ? s : 1.0f, 0.0f, AE_COLOR_RULE);
	y += 18.0f * s;
	/* the rows, one by one */
	for (index = 0; index < rows; index++)
	{
		static const char *const labels[ROWS] = { "RESULT", "KILLS", "ASSISTS", "BEST SPREE" };
		unsigned long at = ROW_FIRST_MS + (unsigned long)index * ROW_EVERY_MS;
		float in, alpha, rise, count;
		const char *sub = NULL;
		char deaths[32];
		long value = 0;
		unsigned int value_color = AE_COLOR_TITLE;

		if (t < at)
			break;
		in = ae_settings_reduce_motion() || pg.skipped ? 1.0f : ease_out((float)(t - at) / ROW_IN_MS);
		alpha = in;
		rise = (1.0f - in) * 12.0f * s;
		count = pg.skipped ? 1.0f : ease_out((float)(t - at) / COUNT_MS);
		switch (index)
		{
		case 0:
			sub = seat->result_sub;
			snprintf(number, sizeof(number), "%s", seat->result_text);
			/* (the accent for a win or a place, white for a loss or a tie) */
			if (seat->result == AE_POSTGAME_WIN || seat->result == AE_POSTGAME_PLACE)
				value_color = AE_COLOR_ACCENT;
			break;
		case 1:
			value = seat->kills;
			snprintf(deaths, sizeof(deaths), "%ld death%s", seat->deaths, seat->deaths == 1 ? "" : "s");
			sub = deaths;
			break;
		case 2:
			value = seat->assists;
			break;
		default:
			value = seat->spree;
			sub = "kills in one life";
			break;
		}
		if (index > 0)
			snprintf(number, sizeof(number), "%ld", (long)((float)value * count + 0.5f));
		ae_draw_rect(x, y + rise, w, row_h, 2.0f * s, with_alpha(RESULT_ROW, alpha * (float)(RESULT_ROW & 0xFF) / 255.0f));
		if (index == 0)
			ae_draw_rect(x, y + rise, 6.0f * s, row_h, 0.0f, with_alpha(seat->stripe, alpha));
		cy = y + rise + row_h * 0.5f;
		if (sub && sub[0])
		{
			float sub_size = full ? 20.0f * s : (20.0f * s > 14.0f ? 20.0f * s : 14.0f);

			ae_draw_text(AE_FONT_ROW, 30.0f * s, x + 28.0f * s, cap_top(AE_FONT_ROW, 30.0f * s, cy - 13.0f * s),
				AE_ALIGN_LEFT, with_alpha(AE_COLOR_TITLE, alpha), labels[index]);
			ae_draw_text(AE_FONT_BODY, sub_size, x + 28.0f * s, cap_top(AE_FONT_BODY, sub_size, cy + 20.0f * s),
				AE_ALIGN_LEFT, with_alpha(AE_COLOR_MUTED, alpha), sub);
		}
		else
			ae_draw_text(AE_FONT_ROW, 30.0f * s, x + 28.0f * s, cap_top(AE_FONT_ROW, 30.0f * s, cy), AE_ALIGN_LEFT,
				with_alpha(AE_COLOR_TITLE, alpha), labels[index]);
		ae_draw_text(AE_FONT_ROW, 52.0f * s, x + w - 28.0f * s, cap_top(AE_FONT_ROW, 52.0f * s, cy), AE_ALIGN_RIGHT,
			with_alpha(value_color, alpha), number);
		y += row_h + 8.0f * s;
	}
	return y;
}

/* the FINAL SCORE card (one view): the team scores, FFA the leaders; TIME PLAYED */
static void draw_card(short local, float x, float right, float y, float u)
{
	struct ae_pause_card card;
	float w = right - x, pad = 29.0f * u, yy;
	short row;

	ae_glue_postgame_card(local, &card);
	ae_draw_rect(x, y, w, (86.0f + card.rows * 74.0f + 66.0f) * u, 3.0f * u, CARD_PANEL);
	ae_draw_rect(x, y, w, 5.0f * u, 0.0f, AE_COLOR_ACCENT);
	ae_draw_text_tracked(AE_FONT_BODY, 22.0f * u, 0.09f, x + pad, cap_top(AE_FONT_BODY, 22.0f * u, y + 46.0f * u),
		AE_ALIGN_LEFT, AE_COLOR_MUTED, "FINAL SCORE");
	yy = y + 86.0f * u;
	for (row = 0; row < card.rows; row++)
	{
		float mid = yy + 27.0f * u, score_w = ae_draw_text_width(AE_FONT_ROW, 48.0f * u, card.scores[row]);
		char fitted[48];

		ae_draw_rect(x + pad, yy, 10.0f * u, 54.0f * u, 0.0f, card.colors[row]);
		ae_fit_text(AE_FONT_ROW, 35.0f * u, card.names[row], w - 2.0f * pad - 29.0f * u - score_w - 16.0f * u, fitted,
			sizeof(fitted));
		ae_draw_text(AE_FONT_ROW, 35.0f * u, x + pad + 29.0f * u, cap_top(AE_FONT_ROW, 35.0f * u, mid), AE_ALIGN_LEFT,
			card.mine[row] ? AE_COLOR_TITLE : AE_COLOR_TEXT, fitted);
		ae_draw_text(AE_FONT_ROW, 48.0f * u, x + w - pad, cap_top(AE_FONT_ROW, 48.0f * u, mid), AE_ALIGN_RIGHT,
			AE_COLOR_TITLE, card.scores[row]);
		yy += 74.0f * u;
	}
	ae_draw_text(AE_FONT_BODY, 22.0f * u, x + pad, cap_top(AE_FONT_BODY, 22.0f * u, yy + 40.0f * u), AE_ALIGN_LEFT,
		AE_COLOR_MUTED, "TIME PLAYED");
	ae_draw_text(AE_FONT_ROW, 35.0f * u, x + w - pad, cap_top(AE_FONT_ROW, 35.0f * u, yy + 40.0f * u), AE_ALIGN_RIGHT,
		AE_COLOR_TEXT, pg.facts.played);
}

static void result_prompts(float x, float center_y)
{
	pg.prompts[0] = prompt_of(AE_BUTTON_A, counting() ? "Skip" : "Continue", AE_ACTION_ACCEPT);
	pg.prompt_count = 1;
	ae_widget_prompts(&pg.density, x, center_y, pg.prompts, pg.prompt_count, NULL, x + 1600.0f, -1, HIT_PROMPTS);
}

static void draw_result(void)
{
	struct ae_layout layout;
	struct ae_frame frame;
	short local, views = 1;
	float scale = ae_settings_ui_scale();

	ae_draw_current_layout(&layout);
	ae_frame_compute(layout.width, scale, &frame);
	ae_draw_rect(0.0f, 0.0f, layout.width, layout.height, 0.0f, BACKDROP);
	for (local = 0; local < AE_POSTGAME_SEATS; local++)
	{
		short view, spans;

		if (pg.facts.seats[local].present && ae_glue_postgame_view(local, &view, &views, &spans))
			break;
	}
	if (views <= 1)
	{
		/* one player: the column on the left, the card on the right (R-team; R-ffa with the leaders) */
		float k = frame.rect.width / 1920.0f, u = frame.unit;
		float x = frame.rect.x + COLUMN_X * k, w = COLUMN_W * u, card_x = frame.rect.x + CARD_X * k;
		float card_right = frame.rect.x + CARD_RIGHT * k;

		for (local = 0; local < AE_POSTGAME_SEATS && !pg.facts.seats[local].present; local++)
			;
		if (local >= AE_POSTGAME_SEATS)
			return;
		/* (at a bigger UI SCALE the card keeps its right edge and its gap from the column: it narrows) */
		if (card_x < x + w + CARD_GAP * k)
			card_x = x + w + CARD_GAP * k;
		ae_density_full(layout.height * layout.scale, scale, &pg.density);
		draw_column(&pg.facts.seats[local], x, COLUMN_TOP, w, u, 1);
		draw_card(local, card_x, card_right, CARD_TOP, u);
		result_prompts(x, layout.height - PROMPTS_FROM_FOOT);
		return;
	}
	/* split screen: each player's own result in their own view (R-2p), no card; one prompt line per view */
	ae_density_full(layout.height * layout.scale, SPLIT_PROMPT_SCALE * (scale > 1.0f ? scale : 1.0f), &pg.density);
	for (local = 0; local < AE_POSTGAME_SEATS; local++)
	{
		struct ae_postgame_seat const *seat = &pg.facts.seats[local];
		struct ae_rect view_r;
		short view, spans;
		float s, x, w, room;

		if (!seat->present || !ae_glue_postgame_view(local, &view, &views, &spans))
			continue;
		view_rect(view, views, spans, &view_r);
		x = view_r.x + COLUMN_X * view_r.width / 1920.0f;
		if (x < view_r.x + 32.0f)
			x = view_r.x + 32.0f;
		w = view_r.width - 2.0f * (x - view_r.x);
		if (w > SPLIT_COLUMN_W)
			w = SPLIT_COLUMN_W;
		/* (its size: the mockup's 0.62, larger with UI SCALE, as fits above the prompts) */
		room = view_r.height - SPLIT_TOP - SPLIT_PROMPTS_FROM_FOOT - 40.0f;
		s = SPLIT_SCALE * (scale > 1.0f ? scale : 1.0f);
		if (column_height(row_count(seat), s) > room)
			s = room / column_height(row_count(seat), 1.0f);
		if (local > 0)
			ae_draw_rect(view_r.x, view_r.y, view_r.width, 2.0f, 0.0f, 0x000000FFu);
		draw_column(seat, x, view_r.y + SPLIT_TOP, w, s, 0);
		/* (each view's prompt: anyone's A; the pointer's are player 1's, the first drawn) */
		pg.prompts[0] = prompt_of(AE_BUTTON_A, counting() ? "Skip" : "Continue", AE_ACTION_ACCEPT);
		pg.prompt_count = 1;
		ae_widget_prompts(&pg.density, x, view_r.y + view_r.height - SPLIT_PROMPTS_FROM_FOOT, pg.prompts, 1, NULL,
			x + w, -1, (short)(HIT_PROMPTS + 4 * local));
	}
}

/* ---------- CARNAGE */

static void draw_separator_line(float x, float center_y, float size, const char *const *parts, short count)
{
	short index;

	for (index = 0; index < count; index++)
	{
		if (!parts[index] || !parts[index][0])
			continue;
		if (index > 0)
		{
			ae_widget_separator(&pg.density, x + 11.0f, center_y, 4.0f, AE_COLOR_MUTED);
			x += 22.0f;
		}
		x += ae_draw_text_tracked(AE_FONT_BODY, size, 0.08f, x, cap_top(AE_FONT_BODY, size, center_y), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, parts[index]);
	}
}

static void draw_carnage(void)
{
	struct ae_layout layout;
	float bottom = 0.0f, x = 0.0f, w = 0.0f, in, prompts_y;
	const char *parts[3];

	ae_draw_current_layout(&layout);
	in = ae_settings_reduce_motion() ? 1.0f : ease_out((float)since() / CARNAGE_IN_MS);
	ae_draw_rect(0.0f, 0.0f, layout.width, layout.height, 0.0f, BACKDROP);
	ae_draw_set_alpha(ae_ui_drawing_alpha() * in);
	/* the board (its own scrim, opaque: the backdrop), then the banner on it */
	ae_glue_postgame_board(layout.width, layout.height, LISTS_TOP, &bottom, &x, &w);
	ae_draw_rect(x, BANNER_TOP, w, BANNER_H, 3.0f, with_alpha(pg.facts.banner_color, 235.0f / 255.0f));
	if (pg.facts.banner_stripe)
		ae_draw_rect(x, BANNER_TOP, 8.0f, BANNER_H, 0.0f, pg.facts.banner_stripe);
	{
		float score_w = ae_draw_text_width(AE_FONT_ROW, 46.0f, pg.facts.banner_score);
		char fitted[64];

		ae_fit_text(AE_FONT_ROW, 46.0f, pg.facts.banner, w - 56.0f - score_w - 32.0f, fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_ROW, 46.0f, x + 28.0f, cap_top(AE_FONT_ROW, 46.0f, BANNER_TOP + BANNER_H * 0.5f),
			AE_ALIGN_LEFT, pg.facts.banner_text_color, fitted);
		ae_draw_text(AE_FONT_ROW, 46.0f, x + w - 28.0f, cap_top(AE_FONT_ROW, 46.0f, BANNER_TOP + BANNER_H * 0.5f),
			AE_ALIGN_RIGHT, pg.facts.banner_text_color, pg.facts.banner_score);
	}
	parts[0] = pg.facts.gametype;
	parts[1] = pg.facts.map;
	parts[2] = pg.facts.played;
	ae_density_full(layout.height * layout.scale, ae_settings_ui_scale(), &pg.density);
	draw_separator_line(x, META_Y, 18.0f, parts, 3);
	/* the prompts under the lists: the host's A, everyone's B; a guest waits for its host */
	pg.prompt_count = 0;
	if (pg.facts.host)
		pg.prompts[pg.prompt_count++] = prompt_of(AE_BUTTON_A, "Continue", AE_ACTION_ACCEPT);
	pg.prompts[pg.prompt_count++] = prompt_of(AE_BUTTON_B, "Leave game", AE_ACTION_BACK);
	prompts_y = bottom + CARNAGE_PROMPTS_GAP;
	if (prompts_y > layout.height - 40.0f)
		prompts_y = layout.height - 40.0f;
	ae_widget_prompts(&pg.density, x, prompts_y, pg.prompts, pg.prompt_count,
		pg.facts.host ? (pg.continue_asked ? "Back to the lobby..." : NULL) : "Waiting for the host", x + w, -1,
		HIT_PROMPTS);
	ae_draw_set_alpha(ae_ui_drawing_alpha());
}

static void postgame_draw(struct ae_screen *screen)
{
	(void)screen;
	ae_draw_view_full();
	if (pg.step == STEP_CARNAGE)
		draw_carnage();
	else
		draw_result();
}

/* ---------- each frame */

static void postgame_close(void)
{
	if (!pg.on_stack)
		return;
	pg.on_stack = 0;
	/* (its own and its leave dialog only, wherever they are: the lobby the game's back to pushes over them) */
	if (pg.leave_open && pg.dialog && pg.dialog != (void *)&pg && ae_ui_holds(pg.dialog))
		ae_ui_remove(pg.dialog);
	pg.leave_open = 0;
	pg.dialog = NULL;
	if (ae_ui_holds(&pg))
		ae_ui_remove(&pg);
	platform_log("ae postgame: closed");
}

void ae_screen_postgame_tick(void)
{
	ae_glue_postgame_track();
	ae_glue_pause_restart_tick();
	if (!ae_glue_postgame_due())
	{
		postgame_close();
		return;
	}
	ae_glue_postgame_facts(&pg.facts);
	if (!pg.on_stack)
	{
		/* (over the game only: another AE screen up first, a dialog or the pause, closes as the game ends) */
		if (ae_ui_depth() > 0)
			return;
		memset(&pg, 0, sizeof(pg));
		ae_glue_postgame_facts(&pg.facts);
		if (!ae_ui_push(&postgame_class, AE_OWNER_ANY, &pg))
			return;
		pg.on_stack = 1;
		pg.step = STEP_RESULT;
		pg.step_ms = ae_motion_now();
		pg.skipped = ae_settings_reduce_motion() ? 1 : 0;
		platform_log("ae postgame: open (%d players, %s)", (int)pg.facts.players, pg.facts.teams ? "teams" : "ffa");
		/* (the tests' frames: GAME RESULT counting, and done) */
		ae_test_capture("pg_counting", ROW_FIRST_MS + ROW_EVERY_MS + COUNT_MS / 2);
		ae_test_capture("pg_result", (int)counting_ms() + 600);
		return;
	}
	if (pg.step == STEP_RESULT && !pg.skipped && since() >= counting_ms())
		pg.skipped = 1;
	/* (the host's CONTINUE asked before the scores were up) */
	if (pg.continue_asked && pg.facts.can_continue && ae_glue_postgame_continue())
		pg.continue_asked = 0;
}

int ae_screen_postgame_up(void)
{
	return pg.on_stack && ae_ui_holds(&pg);
}

static struct ae_screen_class const postgame_class =
{
	.name = "postgame", .handle = postgame_handle, .draw = postgame_draw, .pointer = postgame_pointer,
	.start_from_anyone = 1,
};
