/* ae_widgets_dialog.c: Arena Evolved menus, the dialog (a popover screen) and the roster cards (see ae_widgets.h). No
engine includes.

Spec 4.11 (dialog), 4.12 (roster card). A dialog's spec is in layout units of the whole frame, with the view it draws
in (preflight P12: a VIEW dialog draws in the player's own view, inside its panel, at most the panel's width less a
pad each side: preflight P14). Its open motion is its own (ae_motion_dialog over 150 ms, finished by any input); its
close is instant (preflight P7: the owner decides at the design gate). Text is placed by its capitals' middle; 2 px of
horizontal ink are allowed. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../src/ae_draw.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_widgets.h"

/* spec units */
#define DIALOG_WIDTH_U 720.0f
#define DIALOG_MAXIMUM_U 840.0f
#define DIALOG_STRIPE_U 4.0f
#define DIALOG_TITLE_U 32.0f
#define DIALOG_BODY_U 22.0f
#define DIALOG_LINE_U 32.0f
#define DIALOG_BAR_U 3.0f
#define DIALOG_LINES 8
#define CARD_STRIPE_U 5.0f
#define CARD_OUTLINE_U 2.0f
#define CARD_NAME_GAP_U 18.0f
#define CARD_BADGE_PAD_U 6.0f
#define CARD_RULE_U 1.0f
#define CARD_DASH_U 6.0f
#define CARD_AWAY 0.43f
#define TEXT_FLOOR_PIXELS 16.0f
#define MINOR_FLOOR_PIXELS 14.0f
#define LINE 1.4f                            /* a line of text: its size x this, at least */
/* the dialogs' hits: DIALOG_HIT_ID + the slot (one per local player, one full-screen): ids 0x7E21..0x7E25 are
reserved for them (the pickers' are 0x7E01..0x7E05, AE's on-screen keyboard's 0x7E11..0x7E15) */
enum { DIALOG_HIT_ID = 0x7E21 };
/* " • " between the sub-line's parts */
static const char separator[] = " \xE2\x80\xA2 ";

static float units(struct ae_density const *density, float spec_units)
{
	return spec_units * density->unit;
}

static float floored(struct ae_density const *density, float spec_units, float floor_pixels)
{
	return ae_size(density, spec_units, density->kind == AE_DENSITY_VIEW ? floor_pixels : 0.0f);
}

static float text_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

/* a colour's alpha times a factor */
static unsigned int faded(unsigned int rgba, float factor)
{
	return (rgba & 0xFFFFFF00u) | (unsigned int)floorf((float)(rgba & 0xFFu) * factor + 0.5f);
}

/* ---------- the dialog */

/* the dialog's text sizes keep the spec's FULL sizes in VIEW's proportion (its row text over FULL's) */
static float dialog_ratio(struct ae_density const *density)
{
	return density->metrics->text / ae_metrics_full.text;
}

/* the dialog's parts, drawing units of its view, from the box's top left */
struct dialog_layout
{
	float width, height, pad, gap, title_size, title_center, body_size, line, body_top, minor, revert_center, bar_y,
		bar, row, rows_y, glyph, prompts_center;
	char title[160];
	char lines[DIALOG_LINES][160];
	int line_count;
};

static void dialog_layout(struct ae_dialog_spec const *spec, float maximum_width, float maximum_height,
	struct dialog_layout *l)
{
	struct ae_density const *density = &spec->density;
	float ratio = dialog_ratio(density), content, wrap, title_width;
	int index, maximum_lines = DIALOG_LINES;

	memset(l, 0, sizeof(*l));
	l->pad = units(density, density->metrics->pad);
	l->gap = units(density, density->metrics->gap);
	l->title_size = floored(density, DIALOG_TITLE_U * ratio, TEXT_FLOOR_PIXELS);
	l->body_size = floored(density, DIALOG_BODY_U * ratio, TEXT_FLOOR_PIXELS);
	l->line = units(density, DIALOG_LINE_U * ratio);
	if (l->line < l->body_size * LINE)
		l->line = l->body_size * LINE;
	l->minor = ae_size_minor(density);
	l->bar = units(density, DIALOG_BAR_U);
	l->row = ae_size_row(density);
	l->glyph = ae_size_glyph(density);
	for (;;)
	{
		float y;

		/* the width: 720 u, wider (to 840 u) for a wide title or choice; under 720 u only when the room forces it
		(VIEW, P14) */
		wrap = units(density, DIALOG_WIDTH_U) - 2.0f * l->pad;
		if (wrap > maximum_width - 2.0f * l->pad)
			wrap = maximum_width - 2.0f * l->pad;
		if (wrap < density->pixel)
			wrap = density->pixel;
		l->line_count = spec->body && *spec->body ? ae_wrap_text(AE_FONT_BODY, l->body_size, spec->body, wrap, l->lines,
			maximum_lines) : 0;
		title_width = spec->title ? ae_draw_text_width(AE_FONT_ROW, l->title_size, spec->title) : 0.0f;
		content = title_width;
		for (index = 0; index < l->line_count; index++)
		{
			float width = ae_draw_text_width(AE_FONT_BODY, l->body_size, l->lines[index]);

			content = width > content ? width : content;
		}
		for (index = 0; index < spec->choice_count && index < AE_DIALOG_CHOICES; index++)
		{
			float width = spec->choices[index] ? ae_draw_text_width(AE_FONT_ROW, ae_size_text(density),
				spec->choices[index]) + 2.0f * l->pad : 0.0f;

			content = width > content ? width : content;
		}
		l->width = content + 2.0f * l->pad;
		if (l->width < units(density, DIALOG_WIDTH_U))
			l->width = units(density, DIALOG_WIDTH_U);
		if (l->width > units(density, DIALOG_MAXIMUM_U))
			l->width = units(density, DIALOG_MAXIMUM_U);
		if (l->width > maximum_width)
			l->width = maximum_width;
		/* (the body wrapped again at the width it got: never wider than the box) */
		if (spec->body && *spec->body)
			l->line_count = ae_wrap_text(AE_FONT_BODY, l->body_size, spec->body, l->width - 2.0f * l->pad, l->lines,
				maximum_lines);
		ae_fit_text(AE_FONT_ROW, l->title_size, spec->title ? spec->title : "", l->width - 2.0f * l->pad, l->title,
			sizeof(l->title));
		/* top to bottom: the stripe, the title, the body, (the countdown and its bar), the choices, the prompts */
		y = units(density, DIALOG_STRIPE_U) + l->pad;
		l->title_center = y + l->title_size * LINE * 0.5f;
		y += l->title_size * LINE + l->gap;
		l->body_top = y;
		y += (float)l->line_count * l->line;
		if (spec->kind == AE_DIALOG_TIMED_REVERT)
		{
			l->revert_center = y + l->minor * LINE * 0.5f;
			y += l->minor * LINE;
			l->bar_y = y;
			y += l->bar;
		}
		y += l->pad * 0.75f;
		l->rows_y = y;
		y += (float)spec->choice_count * (l->row + l->gap);
		l->prompts_center = y + l->glyph * 0.8f;
		y += l->glyph * 1.6f + l->pad * 0.75f;
		l->height = y;
		/* (taller than the room: the body loses lines, the last cut with "…"; never under one) */
		if (l->height <= maximum_height || l->line_count <= 1)
			break;
		maximum_lines = l->line_count - (int)ceilf((l->height - maximum_height) / l->line);
		if (maximum_lines < 1)
			maximum_lines = 1;
		if (maximum_lines >= l->line_count)
			break;
	}
}

static float dialog_scale(struct ae_dialog_spec const *spec)
{
	return spec->view.width > 0.0f && spec->view.height > 0.0f ? spec->view.height / (float)AE_LAYOUT_HEIGHT : 1.0f;
}

/* the room in the bounds (drawing units of its view): VIEW keeps a pad each side (P14) */
static void dialog_room(struct ae_dialog_spec const *spec, float *width, float *height)
{
	float scale = dialog_scale(spec), pad = units(&spec->density, spec->density.metrics->pad);

	*width = spec->bounds.width / scale;
	*height = spec->bounds.height / scale;
	if (spec->density.kind == AE_DENSITY_VIEW)
	{
		*width -= 2.0f * pad;
		*height -= 2.0f * pad;
	}
}

void ae_dialog_place(struct ae_dialog_spec const *spec, struct ae_rect *box)
{
	struct dialog_layout l;
	float scale = dialog_scale(spec), room_width, room_height;

	dialog_room(spec, &room_width, &room_height);
	dialog_layout(spec, room_width, room_height, &l);
	box->width = l.width * scale;
	box->height = l.height * scale;
	box->x = spec->bounds.x + (spec->bounds.width - box->width) * 0.5f;
	box->y = spec->bounds.y + (spec->bounds.height - box->height) * 0.5f;
	if (box->y < spec->bounds.y)
		box->y = spec->bounds.y;
}

/* a dialog's own copy of its text (M1: the caller's strings need not outlive the call) */
struct dialog_text
{
	char title[128], body[512];
	char choices[AE_DIALOG_CHOICES][96];
};

static const char *copied(char *out, size_t size, const char *text)
{
	if (!text)
		return NULL;
	snprintf(out, size, "%s", text);
	return out;
}

/* the spec with its strings pointing into text */
static void dialog_copy(struct ae_dialog_spec *out, struct dialog_text *text, struct ae_dialog_spec const *spec)
{
	short index;

	*out = *spec;
	out->title = copied(text->title, sizeof(text->title), spec->title);
	out->body = copied(text->body, sizeof(text->body), spec->body);
	for (index = 0; index < AE_DIALOG_CHOICES; index++)
		out->choices[index] = index < spec->choice_count ? copied(text->choices[index], sizeof(text->choices[index]),
			spec->choices[index]) : NULL;
}

/* the open dialogs: one slot per local player and one for a full-screen dialog (as the pickers) */
enum { DIALOG_SLOTS = AE_MAXIMUM_PLAYERS + 1 };
struct dialog_state
{
	int open;
	short slot, focus, pressed;
	struct ae_dialog_spec spec;
	struct dialog_text text;
	struct ae_rect box;            /* layout units */
	struct ae_motion motion;       /* the open motion, 0 -> 1 */
	unsigned long opened;          /* the timed revert's start */
};
static struct dialog_state dialogs[DIALOG_SLOTS];
/* (M2 ruling) an error that came while its owner's slot was busy: logged and failed at once, shown when the slot frees
(the newest one; a reset drops it) */
struct dialog_pending
{
	int waiting;
	short owner;
	struct ae_dialog_spec spec;
	struct dialog_text text;
};
static struct dialog_pending pending[DIALOG_SLOTS];

static short dialog_hit_id(struct dialog_state const *state)
{
	return (short)(DIALOG_HIT_ID + state->slot);
}

static int dialog_choice_valid(struct dialog_state const *state, short choice)
{
	return choice >= 0 && choice < state->spec.choice_count;
}

/* the timed revert's milliseconds left (a clock gone back before the opening: all of them) */
static unsigned long dialog_remaining(struct dialog_state const *state)
{
	unsigned long now = ae_motion_now(), elapsed;

	if (now < state->opened)
		return AE_DIALOG_REVERT_MS;
	elapsed = now - state->opened;

	return elapsed >= AE_DIALOG_REVERT_MS ? 0 : AE_DIALOG_REVERT_MS - elapsed;
}

static void dialog_leave(struct ae_screen *screen)
{
	struct dialog_state *state = screen->data;

	state->open = 0;
}

/* picks a choice: its sound, closed (instant), then the callback (which may open another) */
static void dialog_pick(struct dialog_state *state, short choice, int sound)
{
	void (*picked)(short, void *) = state->spec.picked;
	void *context = state->spec.context;

	ae_sound_request(sound, 0);
	ae_ui_pop();
	if (picked)
		picked(choice, context);
}

static void dialog_draw(struct ae_screen *screen)
{
	struct dialog_state *state = screen->data;
	struct ae_dialog_spec const *spec = &state->spec;
	struct ae_density const *density = &spec->density;
	struct dialog_layout l;
	struct ae_view view;
	struct ae_rect box, frame;
	struct ae_prompt prompts[2];
	float scrim, scale, alpha, room_width, room_height, x, y, inner;
	short index, id = dialog_hit_id(state);
	int error = spec->kind == AE_DIALOG_ERROR;

	ae_motion_dialog(ae_motion_progress(&state->motion), 1, &scrim, &scale, &alpha);
	/* the scrim over its view (P12: the player's own for VIEW; else the whole frame), clicks outside taken there */
	if (spec->view.width > 0.0f && spec->view.height > 0.0f)
	{
		frame = spec->view;
		ae_draw_view(frame.x, frame.y, frame.width, frame.height);
	}
	else
	{
		struct ae_layout layout;

		ae_draw_view_full();
		ae_draw_current_layout(&layout);
		frame.x = frame.y = 0.0f;
		frame.width = layout.width;
		frame.height = layout.height;
	}
	ae_draw_set_alpha(scrim);
	ae_draw_rect(0.0f, 0.0f, ae_draw_view_width(), (float)AE_LAYOUT_HEIGHT, 0.0f, AE_COLOR_SCRIM);
	ae_hit_add(0.0f, 0.0f, ae_draw_view_width(), (float)AE_LAYOUT_HEIGHT, id, AE_PART_OUTSIDE, -1);
	/* the dialog: scaled about its view's middle as it opens, faded in */
	if (scale != 1.0f)
		ae_draw_view(frame.x + frame.width * (1.0f - scale) * 0.5f, frame.y + frame.height * (1.0f - scale) * 0.5f,
			frame.width * scale, frame.height * scale);
	ae_draw_set_alpha(alpha);
	ae_draw_current_view(&view);
	/* (placed again each frame: its text measured as it draws) */
	ae_dialog_place(spec, &state->box);
	dialog_room(spec, &room_width, &room_height);
	dialog_layout(spec, room_width, room_height, &l);
	box.x = (state->box.x - view.x) / view.scale;
	box.y = (state->box.y - view.y) / view.scale;
	box.width = l.width;
	box.height = l.height;
	/* (VIEW: what is under it in its bounds, the panel's page, hidden by the view panel's fill: nothing peeks out
	around a dialog smaller than the page, mockup 25) */
	if (density->kind == AE_DENSITY_VIEW)
		ae_draw_rect((spec->bounds.x - view.x) / view.scale, (spec->bounds.y - view.y) / view.scale,
			spec->bounds.width / view.scale, spec->bounds.height / view.scale, 0.0f, AE_COLOR_VIEW_PANEL);
	ae_draw_rect(box.x, box.y, box.width, box.height, units(density, density->metrics->corner), AE_COLOR_POPOVER);
	ae_hit_add(box.x, box.y, box.width, box.height, id, AE_PART_CARD, -1);
	ae_draw_rect(box.x, box.y, box.width, units(density, DIALOG_STRIPE_U), 0.0f, error ? AE_COLOR_WARNING : AE_COLOR_ACCENT);
	x = box.x + l.pad;
	inner = box.width - 2.0f * l.pad;
	ae_draw_text(AE_FONT_ROW, l.title_size, x, text_top(AE_FONT_ROW, l.title_size, box.y + l.title_center), AE_ALIGN_LEFT,
		error ? AE_COLOR_WARNING : AE_COLOR_TITLE, l.title);
	for (index = 0; index < l.line_count; index++)
		ae_draw_text(AE_FONT_BODY, l.body_size, x, text_top(AE_FONT_BODY, l.body_size,
			box.y + l.body_top + ((float)index + 0.5f) * l.line), AE_ALIGN_LEFT, AE_COLOR_TEXT, l.lines[index]);
	/* the timed revert: "Reverting in N s" and the bar draining (REDUCE MOTION: whole seconds) */
	if (spec->kind == AE_DIALOG_TIMED_REVERT)
	{
		unsigned long remaining = dialog_remaining(state);
		int seconds = (int)((remaining + 999) / 1000);
		float fraction = ae_motion_reduced() ? (float)seconds * 1000.0f / (float)AE_DIALOG_REVERT_MS :
			(float)remaining / (float)AE_DIALOG_REVERT_MS;
		char text[64];

		snprintf(text, sizeof(text), ae_string(AE_STR_REVERTING), seconds);
		ae_draw_text(AE_FONT_ROW, l.minor, x, text_top(AE_FONT_ROW, l.minor, box.y + l.revert_center), AE_ALIGN_LEFT,
			AE_COLOR_ACCENT, text);
		ae_draw_rect(x, box.y + l.bar_y, inner, l.bar, 0.0f, AE_COLOR_TRACK);
		if (fraction > 0.0f)
			ae_draw_rect(x, box.y + l.bar_y, inner * fraction, l.bar, 0.0f, AE_COLOR_ACCENT);
	}
	/* the choices: ordinary rows */
	y = box.y + l.rows_y;
	for (index = 0; index < spec->choice_count; index++)
	{
		struct ae_row row;

		memset(&row, 0, sizeof(row));
		row.label = spec->choices[index] ? spec->choices[index] : "";
		row.flags = index == state->focus ? AE_ROW_FOCUSED : 0;
		y += ae_widget_row(density, x, y, inner, &row, id, index) + l.gap;
	}
	/* its prompts inside it */
	prompts[0].button = AE_BUTTON_A;
	prompts[0].label = ae_string(AE_STR_SELECT);
	prompts[0].key = NULL;
	prompts[0].action = AE_ACTION_ACCEPT;
	prompts[1].button = AE_BUTTON_B;
	prompts[1].label = ae_string(AE_STR_CANCEL);
	prompts[1].key = NULL;
	prompts[1].action = AE_ACTION_BACK;
	ae_widget_prompts(density, x, box.y + l.prompts_center, prompts, 2, NULL, x + inner, state->pressed, id);
}

static int dialog_handle(struct ae_screen *screen, struct ae_event const *event)
{
	struct dialog_state *state = screen->data;
	short before = state->focus;

	/* (a press finishes the open motion) */
	ae_motion_finish(&state->motion);
	switch (event->action)
	{
	case AE_ACTION_UP:
		state->focus = ae_value_step(state->spec.choice_count, state->focus, -1);
		break;
	case AE_ACTION_DOWN:
		state->focus = ae_value_step(state->spec.choice_count, state->focus, 1);
		break;
	case AE_ACTION_ACCEPT:
		if (dialog_choice_valid(state, state->focus))
			dialog_pick(state, state->focus, AE_SOUND_FORWARD);
		return 1;
	/* (B / Esc: the cancel choice, with the back sound; handled here, so the stack never pops it on its own) */
	case AE_ACTION_BACK:
		if (dialog_choice_valid(state, state->spec.cancel_choice))
			dialog_pick(state, state->spec.cancel_choice, AE_SOUND_BACK);
		return 1;
	default:
		return 1;
	}
	if (state->focus != before)
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
	return 1;
}

static void dialog_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct dialog_state *state = screen->data;
	struct ae_prompt prompts[2];
	struct ae_event event;
	struct ae_hit hit;
	short id = dialog_hit_id(state);
	int mine;

	memset(prompts, 0, sizeof(prompts));
	prompts[0].button = AE_BUTTON_A;
	prompts[0].action = AE_ACTION_ACCEPT;
	prompts[1].button = AE_BUTTON_B;
	prompts[1].action = AE_ACTION_BACK;
	mine = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == id;
	/* (hover focuses a choice) */
	if (pointer->moved && mine && hit.part == AE_PART_ROW && dialog_choice_valid(state, hit.index))
		state->focus = hit.index;
	if (ae_prompts_pointer(prompts, 2, pointer, id, &event))
	{
		event.player = screen->owner >= 0 ? screen->owner : 0;
		dialog_handle(screen, &event);
		return;
	}
	if (!pointer->left_clicks)
		return;
	ae_motion_finish(&state->motion);
	/* a click on a choice picks it; anywhere else (outside too) does nothing */
	if (mine && hit.part == AE_PART_ROW && dialog_choice_valid(state, hit.index))
		dialog_pick(state, hit.index, AE_SOUND_FORWARD);
}

static struct ae_screen_class const dialog_class =
{
	.name = "dialog", .leave = dialog_leave, .handle = dialog_handle, .draw = dialog_draw, .pointer = dialog_pointer,
	.popover = 1,
};

static int dialog_live(struct dialog_state const *state)
{
	return state->open && ae_ui_holds(state);
}

static int dialog_timed(struct dialog_state const *state)
{
	return state->spec.kind == AE_DIALOG_TIMED_REVERT && dialog_choice_valid(state, state->spec.timeout_choice);
}

/* a reset stack: every open timed revert picks its timeout choice (once: the slot is closed first); a pending error
is dropped (there is nothing left to show it over) */
static void dialog_reset(void)
{
	struct { void (*picked)(short, void *); void *context; short choice; } picks[DIALOG_SLOTS];
	short slot;

	/* (every slot cleared first, then the picks: a callback opening a dialog or an error for any slot keeps it) */
	for (slot = 0; slot < DIALOG_SLOTS; slot++)
	{
		struct dialog_state *state = &dialogs[slot];

		picks[slot].picked = state->open && dialog_timed(state) ? state->spec.picked : NULL;
		picks[slot].context = state->spec.context;
		picks[slot].choice = state->spec.timeout_choice;
		pending[slot].waiting = 0;
		state->open = 0;
	}
	for (slot = 0; slot < DIALOG_SLOTS; slot++)
		if (picks[slot].picked)
			picks[slot].picked(picks[slot].choice, picks[slot].context);
}

void ae_dialog_menus_off(unsigned long now_ms)
{
	/* (the clock moved only for an open timed revert: nothing else of AE's runs with its menus off) */
	if (!ae_dialog_timed_open())
		return;
	ae_motion_set_now(now_ms);
	ae_dialog_tick();
}

int ae_dialog_timed_open(void)
{
	short slot;

	for (slot = 0; slot < DIALOG_SLOTS; slot++)
		if (dialog_live(&dialogs[slot]) && dialog_timed(&dialogs[slot]))
			return 1;
	return 0;
}

/* pushes a slot's dialog; announce: an error's log line and failure (not again for a pending one) */
static int dialog_push(struct dialog_state *state, short owner, int announce)
{
	state->pressed = -1;
	/* (the first focus: the safe choice) */
	state->focus = state->spec.safe_choice >= 0 && state->spec.safe_choice < state->spec.choice_count ?
		state->spec.safe_choice : 0;
	ae_dialog_place(&state->spec, &state->box);
	state->opened = ae_motion_now();
	ae_motion_start(&state->motion, 0.0f, 1.0f, AE_MOTION_DIALOG_OPEN_MS);
	ae_ui_add_reset_hook(dialog_reset);
	if (!ae_ui_push(&dialog_class, owner, state))
		return 0;
	state->open = 1;
	if (announce)
		ae_sound_request(state->spec.kind == AE_DIALOG_ERROR ? AE_SOUND_FAILURE : AE_SOUND_FORWARD, 0);
	return 1;
}

/* an error's line in the log, its text as it is */
static void dialog_log_error(struct ae_dialog_spec const *spec)
{
	char text[700];

	snprintf(text, sizeof(text), "ae menus: error: %s: %s", spec->title ? spec->title : "", spec->body ? spec->body : "");
	ae_host_log(text);
}

void ae_dialog_tick(void)
{
	short slot;

	for (slot = 0; slot < DIALOG_SLOTS; slot++)
	{
		struct dialog_state *state = &dialogs[slot];

		/* a timed revert at 0 picks its timeout choice wherever it is on the stack (covered or not) */
		if (dialog_live(state) && dialog_timed(state) && !dialog_remaining(state))
		{
			void (*picked)(short, void *) = state->spec.picked;
			void *context = state->spec.context;
			short choice = state->spec.timeout_choice;

			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_remove(state);
			state->open = 0;
			if (picked)
				picked(choice, context);
		}
		/* a pending error once its slot is free (and AE's screens are there to show it over) */
		if (pending[slot].waiting && !dialog_live(state) && ae_ui_depth() > 0)
		{
			pending[slot].waiting = 0;
			memset(state, 0, sizeof(*state));
			state->slot = slot;
			dialog_copy(&state->spec, &state->text, &pending[slot].spec);
			dialog_push(state, pending[slot].owner, 0);
		}
	}
}

int ae_dialog_open(struct ae_dialog_spec const *spec, short owner)
{
	struct dialog_state *state;
	short slot = (short)(owner >= 0 && owner < AE_MAXIMUM_PLAYERS ? owner : AE_MAXIMUM_PLAYERS);

	if (!spec || spec->choice_count <= 0 || spec->choice_count > AE_DIALOG_CHOICES)
		return 0;
	state = &dialogs[slot];
	/* an error: its text in the log as it is and failure, at once (shown now, or when the slot frees) */
	if (spec->kind == AE_DIALOG_ERROR)
		dialog_log_error(spec);
	/* (the owner's dialog is open: refused, never replaced; an error waits for the slot, the newest one kept; a slot
	left marked by a reset stack is free) */
	if (dialog_live(state))
	{
		if (spec->kind != AE_DIALOG_ERROR)
			return 0;
		ae_sound_request(AE_SOUND_FAILURE, 0);
		pending[slot].waiting = 1;
		pending[slot].owner = owner;
		dialog_copy(&pending[slot].spec, &pending[slot].text, spec);
		return 0;
	}
	memset(state, 0, sizeof(*state));
	state->slot = slot;
	dialog_copy(&state->spec, &state->text, spec);
	return dialog_push(state, owner, 1);
}

/* ---------- roster cards */

/* WCAG relative luminance of an 0xRRGGBBAA colour */
static float luminance(unsigned int rgba)
{
	float channel[3];
	int index;

	for (index = 0; index < 3; index++)
	{
		float value = (float)((rgba >> (24 - 8 * index)) & 0xFFu) / 255.0f;

		channel[index] = value <= 0.03928f ? value / 12.92f : powf((value + 0.055f) / 1.055f, 2.4f);
	}
	return 0.2126f * channel[0] + 0.7152f * channel[1] + 0.0722f * channel[2];
}

static float contrast(unsigned int a, unsigned int b)
{
	float la = luminance(a), lb = luminance(b);

	return la > lb ? (la + 0.05f) / (lb + 0.05f) : (lb + 0.05f) / (la + 0.05f);
}

unsigned int ae_emblem_number_color(unsigned int emblem_rgba)
{
	return contrast(AE_COLOR_SELECTION_TEXT, emblem_rgba) > contrast(AE_COLOR_TITLE, emblem_rgba) ?
		AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE;
}

float ae_widget_roster_card(struct ae_density const *density, float x, float y, float width,
	struct ae_roster_card const *card, short hit_id, short index)
{
	struct ae_metrics const *m = density->metrics;
	float height = units(density, m->card), emblem = units(density, m->emblem), corner = units(density, m->corner);
	float pad = units(density, m->pad), inset = (height - emblem) * 0.5f, left = x + inset, right = x + width - pad;
	float name_size = ae_size_text(density), sub_size = floored(density, m->sub_line, MINOR_FLOOR_PIXELS);
	float side_size = floored(density, m->minor, MINOR_FLOOR_PIXELS), number_size = emblem * 0.55f, text_x;
	float factor = card->flags & AE_CARD_AWAY ? CARD_AWAY : 1.0f;
	int focused = (card->flags & AE_CARD_FOCUSED) != 0;
	unsigned int text_color = faded(focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE, factor);
	unsigned int sub_color = faded(focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, factor * (focused ? 0.75f : 1.0f));
	char text[256], fitted[256];

	/* (VIEW: the number at the minor floor at least) */
	if (density->kind == AE_DENSITY_VIEW && number_size < MINOR_FLOOR_PIXELS * density->pixel)
		number_size = MINOR_FLOOR_PIXELS * density->pixel;
	/* the card: a row's fill, the focus a white bar, the hover a wash */
	ae_draw_rect(x, y, width, height, corner, faded(focused ? AE_COLOR_SELECTION : AE_COLOR_ROW, factor));
	if (!focused && (card->flags & AE_CARD_HOVER))
		ae_draw_rect(x, y, width, height, corner, AE_COLOR_HOVER);
	/* team games: a 5 u stripe on the left in the team's colour */
	if ((card->flags & AE_CARD_TEAM) && card->team_color)
	{
		ae_draw_rect(x, y, units(density, CARD_STRIPE_U), height, 0.0f, faded(card->team_color, factor));
		left += units(density, CARD_STRIPE_U);
	}
	/* the emblem: the player's colour WITH the slot number (colour is never alone) */
	ae_draw_rect(left, y + inset, emblem, emblem, corner, faded(card->color, factor));
	snprintf(text, sizeof(text), "%d", card->slot);
	ae_draw_text(AE_FONT_ROW, number_size, left + emblem * 0.5f, text_top(AE_FONT_ROW, number_size, y + height * 0.5f),
		AE_ALIGN_CENTER, faded(ae_emblem_number_color(card->color), factor), text);
	text_x = left + emblem + units(density, CARD_NAME_GAP_U);
	/* the right side: the team's name, then the HOST badge left of it */
	if ((card->flags & AE_CARD_TEAM) && card->team_name && *card->team_name)
		right -= ae_draw_text(AE_FONT_ROW, side_size, right, text_top(AE_FONT_ROW, side_size, y + height * 0.5f),
			AE_ALIGN_RIGHT, text_color, card->team_name) + pad;
	if (card->flags & AE_CARD_HOST)
	{
		const char *host = ae_string(AE_STR_BADGE_HOST);
		float badge = floored(density, m->badge, MINOR_FLOOR_PIXELS), badge_pad = units(density, CARD_BADGE_PAD_U);
		float badge_width = ae_draw_text_width(AE_FONT_ROW, badge, host) + 2.0f * badge_pad;
		float badge_height = badge * 1.6f;

		ae_draw_outline(right - badge_width, y + (height - badge_height) * 0.5f, badge_width, badge_height, 0.0f,
			units(density, CARD_RULE_U), faded(AE_COLOR_ACCENT, factor));
		ae_draw_text(AE_FONT_ROW, badge, right - badge_width * 0.5f, text_top(AE_FONT_ROW, badge, y + height * 0.5f),
			AE_ALIGN_CENTER, faded(focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT, factor), host);
		right -= badge_width + pad;
	}
	/* the name and its one sub-line (input, where, state; a guest's "guest (not saved)"; away: the reason) */
	ae_fit_text(AE_FONT_ROW, name_size, card->name ? card->name : "", right - text_x, fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_ROW, name_size, text_x, text_top(AE_FONT_ROW, name_size, y + height * 0.34f), AE_ALIGN_LEFT,
		text_color, fitted);
	snprintf(text, sizeof(text), "%s", card->sub_line ? card->sub_line : "");
	if (card->flags & AE_CARD_GUEST)
		snprintf(text + strlen(text), sizeof(text) - strlen(text), "%s%s", text[0] ? separator : "",
			ae_string(AE_STR_GUEST));
	if ((card->flags & AE_CARD_AWAY) && card->reason && *card->reason)
		snprintf(text + strlen(text), sizeof(text) - strlen(text), "%s%s", text[0] ? separator : "", card->reason);
	if (text[0])
	{
		ae_fit_text(AE_FONT_BODY, sub_size, text, right - text_x, fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_BODY, sub_size, text_x, text_top(AE_FONT_BODY, sub_size, y + height * 0.71f), AE_ALIGN_LEFT,
			sub_color, fitted);
	}
	/* editing (its own Your Settings open): a 2 u outline in the player's colour */
	if (card->flags & AE_CARD_EDITING)
		ae_draw_outline(x, y, width, height, corner, units(density, CARD_OUTLINE_U), faded(card->color, factor));
	ae_hit_add(x, y, width, height, hit_id, AE_PART_CARD, index);
	return height;
}

float ae_widget_open_card(struct ae_density const *density, float x, float y, float width, int keyboard_only,
	short hit_id)
{
	struct ae_metrics const *m = density->metrics;
	float height = units(density, m->card), size = floored(density, m->minor, MINOR_FLOOR_PIXELS);
	float center_y = y + height * 0.5f, top = text_top(AE_FONT_ROW, size, center_y);

	ae_widget_dashed(x, y, width, height, units(density, CARD_RULE_U), units(density, CARD_DASH_U), AE_COLOR_RULE);
	if (keyboard_only)
	{
		char text[200];

		snprintf(text, sizeof(text), "%s %s", ae_string(AE_STR_CONNECT_CONTROLLER), ae_string(AE_STR_TO_ADD_PLAYER));
		ae_draw_text(AE_FONT_ROW, size, x + width * 0.5f, top, AE_ALIGN_CENTER, AE_COLOR_MUTED, text);
	}
	else
	{
		/* "Press", the START glyph (the last pad's family; Xbox's from a keyboard; PlayStation: its OPTIONS cap),
		"to add a player (split screen)", centred together */
		int device = ae_ui_last_device();
		float glyph = ae_size_glyph(density), space = size * 0.35f, mark, press, rest, pen;
		const char *before = ae_string(AE_STR_PRESS), *after = ae_string(AE_STR_TO_ADD_PLAYER);
		int font = device == AE_DEVICE_NINTENDO ? AE_FONT_NINTENDO : AE_FONT_XBOX;

		if (device == AE_DEVICE_PLAYSTATION)
			mark = ae_key_cap_width(density, glyph, ae_string(AE_STR_CAP_OPTIONS));
		else
			mark = ae_draw_button_width(font, AE_BUTTON_START, glyph);
		press = ae_draw_text_width(AE_FONT_ROW, size, before);
		rest = ae_draw_text_width(AE_FONT_ROW, size, after);
		pen = x + (width - (press + space + mark + space + rest)) * 0.5f;
		ae_draw_text(AE_FONT_ROW, size, pen, top, AE_ALIGN_LEFT, AE_COLOR_TEXT, before);
		pen += press + space;
		if (device == AE_DEVICE_PLAYSTATION)
			ae_widget_key_cap(density, pen, center_y - glyph * 0.5f, glyph, ae_string(AE_STR_CAP_OPTIONS));
		else
			ae_draw_button(font, AE_BUTTON_START, glyph, pen, center_y - glyph * 0.5f, AE_COLOR_TEXT);
		pen += mark + space;
		ae_draw_text(AE_FONT_ROW, size, pen, top, AE_ALIGN_LEFT, AE_COLOR_TEXT, after);
	}
	ae_hit_add(x, y, width, height, hit_id, AE_PART_CARD, -1);
	return height;
}
