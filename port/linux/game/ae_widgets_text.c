/* ae_widgets_text.c: Arena Evolved menus, text (see ae_widgets.h): the engine contact the pure widgets use (the host
table), the text field, typing into it, and AE's own on-screen keyboard (a popover). No engine includes.

Spec 4.8 (text field), 4.9 (AE's keyboard, owner decision 1: not interface/virtual_keyboard.c). The keyboard is a
popover like the value picker: pooled per owner, so every view can have one open; its spec in layout units with the
view it draws in (preflight P12). Its hits' ids are KEYBOARD_HIT_ID + the slot (0x7E11..0x7E15 reserved). */

#include <stdio.h>
#include <string.h>

#include "../src/ae_draw.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_widgets.h"

/* spec units */
#define FIELD_RING_U 2.0f
#define FIELD_PAD_U 20.0f
#define FIELD_LABEL_GAP_U 8.0f
#define FIELD_CARET_U 2.0f
#define FIELD_CARET_HEIGHT 0.6f
#define FIELD_TRACKING 0.09f
#define FIELD_LABEL_U 17.0f
#define KEY_WIDTH_U 60.0f
#define KEY_HEIGHT_U 52.0f
#define KEY_GAP_U 6.0f
#define KEY_VIEW_WIDTH_U 48.0f
#define KEY_VIEW_GAP_U 5.0f
#define KEY_BASE_U 4.0f
#define KEYBOARD_PAD_U 16.0f
#define KEYBOARD_GAP_U 12.0f           /* the field to the keyboard */
#define TEXT_FLOOR_PIXELS 16.0f
#define MINOR_FLOOR_PIXELS 14.0f
#define LINE 1.4f
enum { KEYBOARD_HIT_ID = 0x7E11, KEYBOARD_KEYS = 46, KEY_SHIFT = 40, KEY_PAGE, KEY_SPACE, KEY_LEFT, KEY_RIGHT, KEY_DONE };

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

/* ---------- the host (preflight P13) */

static struct ae_host const *host;

void ae_host_set(struct ae_host const *table)
{
	host = table;
}

void ae_host_log(const char *text)
{
	if (host && host->write_log)
		host->write_log(text);
}

void ae_host_text_begin(void)
{
	if (host && host->text_begin)
		host->text_begin();
}

void ae_host_text_end(void)
{
	if (host && host->text_end)
		host->text_end();
}

int ae_host_text_keys(struct ae_text_key *keys, int maximum)
{
	return host && host->text_keys ? host->text_keys(keys, maximum) : 0;
}

int ae_host_clipboard_get(char *text, int size)
{
	if (size > 0)
		text[0] = 0;
	return host && host->clipboard_get ? host->clipboard_get(text, size) : 0;
}

void ae_host_clipboard_set(const char *text)
{
	if (host && host->clipboard_set)
		host->clipboard_set(text);
}

/* ---------- typing into a field */

int ae_field_type(struct ae_text *text, struct ae_text_key const *keys, int count)
{
	char clipboard[AE_TEXT_MAXIMUM * 2];
	int index, whole = 1;

	for (index = 0; index < count; index++)
	{
		struct ae_text_key const *key = &keys[index];

		switch (key->kind)
		{
		case AE_TEXT_KEY_CHAR:
		{
			char character[2];

			character[0] = key->character;
			character[1] = 0;
			whole &= ae_text_insert(text, character);
			break;
		}
		case AE_TEXT_KEY_BACKSPACE: ae_text_backspace(text); break;
		case AE_TEXT_KEY_DELETE: ae_text_delete(text); break;
		case AE_TEXT_KEY_HOME: ae_text_home(text, key->shift); break;
		case AE_TEXT_KEY_END: ae_text_end(text, key->shift); break;
		case AE_TEXT_KEY_SELECT_ALL: ae_text_select_all(text); break;
		case AE_TEXT_KEY_COPY:
		{
			short from, to;

			/* (the selection, or all of it) */
			if (!ae_text_selection(text, &from, &to))
			{
				from = 0;
				to = text->length;
			}
			memcpy(clipboard, text->text + from, (size_t)(to - from));
			clipboard[to - from] = 0;
			ae_host_clipboard_set(clipboard);
			break;
		}
		case AE_TEXT_KEY_PASTE:
			if (ae_host_clipboard_get(clipboard, sizeof(clipboard)))
				whole &= ae_text_insert(text, clipboard);
			break;
		}
	}
	return whole;
}

void ae_field_edit_begin(struct ae_field_edit *edit, struct ae_text *text)
{
	edit->text = text;
	edit->before = *text;
	edit->active = 1;
	ae_host_text_begin();
}

int ae_field_edit_keys(struct ae_field_edit *edit)
{
	struct ae_text_key keys[32];
	int count;

	if (!edit->active || !edit->text)
		return 1;
	count = ae_host_text_keys(keys, 32);
	return count > 0 ? ae_field_type(edit->text, keys, count) : 1;
}

void ae_field_edit_end(struct ae_field_edit *edit, int keep)
{
	if (!edit->active)
		return;
	/* (cancel: the text from before editing) */
	if (!keep && edit->text)
		*edit->text = edit->before;
	edit->active = 0;
	ae_host_text_end();
}

/* ---------- the field */

/* the width of a text's first count characters */
static float prefix_width(int font, float size, const char *text, short count)
{
	char buffer[AE_TEXT_MAXIMUM];

	if (count <= 0)
		return 0.0f;
	memcpy(buffer, text, (size_t)count);
	buffer[count] = 0;
	return ae_draw_text_width(font, size, buffer);
}

float ae_widget_field(struct ae_density const *density, float x, float y, float width, struct ae_field const *field,
	short hit_id)
{
	float label_size = floored(density, FIELD_LABEL_U, MINOR_FLOOR_PIXELS), text_size = ae_size_text(density);
	float minor = floored(density, density->metrics->minor, MINOR_FLOOR_PIXELS);
	float height = density->kind == AE_DENSITY_VIEW ? ae_size_row(density) : units(density, 50.0f);
	float pad = units(density, FIELD_PAD_U), ring = units(density, FIELD_RING_U), total = 0.0f;
	float allowance = 2.0f * density->pixel, center, room, offset = 0.0f, count_width = 0.0f, text_x;
	unsigned int flags = field->flags;
	struct ae_text const *text = field->text;
	int editing = (flags & AE_FIELD_EDITING) != 0, focused = editing || (flags & AE_FIELD_FOCUSED);
	int error = (flags & AE_FIELD_ERROR) && field->error;
	char count[24];

	/* the label above, tracked, muted */
	if (field->label && *field->label)
	{
		ae_draw_text_tracked(AE_FONT_BODY, label_size, FIELD_TRACKING, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, field->label);
		total = ae_draw_cap_height(AE_FONT_BODY, label_size) + units(density, FIELD_LABEL_GAP_U);
		y += total;
	}
	center = y + height * 0.5f;
	/* the well, its rule; hover a wash; focus an accent ring, an error a warning one */
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_WELL);
	ae_draw_outline(x, y, width, height, 0.0f, units(density, 1.0f), AE_COLOR_RULE);
	if ((flags & AE_FIELD_HOVER) && !(flags & AE_FIELD_DISABLED))
		ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_HOVER);
	if (error)
		ae_draw_outline(x, y, width, height, 0.0f, ring, AE_COLOR_WARNING);
	else if (focused && !(flags & AE_FIELD_DISABLED))
		ae_draw_outline(x, y, width, height, 0.0f, ring, AE_COLOR_ACCENT);
	/* the count, n / max, right (warning at the limit), while focused */
	if (focused && text && text->limit > 0)
	{
		snprintf(count, sizeof(count), ae_string(AE_STR_COUNT), text->length, text->limit);
		count_width = ae_draw_text(AE_FONT_BODY, minor, x + width - pad, text_top(AE_FONT_BODY, minor, center),
			AE_ALIGN_RIGHT, text->length >= text->limit ? AE_COLOR_WARNING : AE_COLOR_MUTED, count) + pad * 0.5f;
	}
	room = width - 2.0f * pad - count_width;
	text_x = x + pad;
	ae_draw_clip_push(text_x - allowance, y, room + 2.0f * allowance, height);
	if (text && text->length > 0)
	{
		float caret_x = prefix_width(AE_FONT_ROW, text_size, text->text, text->caret);
		short from, to;

		/* (a text longer than the room scrolls to keep the caret in sight) */
		if (editing && caret_x > room)
			offset = caret_x - room;
		if (editing && ae_text_selection(text, &from, &to))
		{
			float left = prefix_width(AE_FONT_ROW, text_size, text->text, from);
			float right = prefix_width(AE_FONT_ROW, text_size, text->text, to);

			ae_draw_rect(text_x - offset + left, center - height * 0.3f, right - left, height * 0.6f, 0.0f,
				AE_COLOR_TEXT_SELECT);
		}
		ae_draw_text(AE_FONT_ROW, text_size, text_x - offset, text_top(AE_FONT_ROW, text_size, center), AE_ALIGN_LEFT,
			flags & AE_FIELD_DISABLED ? AE_COLOR_MUTED : AE_COLOR_TITLE, text->text);
	}
	else if (field->placeholder && *field->placeholder)
		ae_draw_text(AE_FONT_BODY, text_size, text_x, text_top(AE_FONT_BODY, text_size, center), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, field->placeholder);
	/* the caret: 2 u wide, 60 % of the field, accent, blinking (steady with REDUCE MOTION) */
	if (editing && text && ae_caret_visible(field->caret_since))
	{
		float caret_x = prefix_width(AE_FONT_ROW, text_size, text->text, text->caret);

		ae_draw_rect(text_x - offset + caret_x, center - height * FIELD_CARET_HEIGHT * 0.5f, units(density, FIELD_CARET_U),
			height * FIELD_CARET_HEIGHT, 0.0f, AE_COLOR_ACCENT);
	}
	ae_draw_clip_pop();
	ae_hit_add(x, y, width, height, hit_id, AE_PART_FIELD, -1);
	total += height;
	/* the error's reason under it, one line, until the text changes */
	if (error)
	{
		float line = minor * LINE;
		char reason[160];

		ae_fit_text(AE_FONT_BODY, minor, field->error, width, reason, sizeof(reason));
		ae_draw_text(AE_FONT_BODY, minor, x, text_top(AE_FONT_BODY, minor, y + height + line * 0.5f + units(density, 4.0f)),
			AE_ALIGN_LEFT, AE_COLOR_WARNING, reason);
		total += line + units(density, 4.0f);
	}
	return total;
}

/* ---------- AE's keyboard */

static const char *const letter_keys[40] =
{
	"1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
	"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
	"A", "S", "D", "F", "G", "H", "J", "K", "L", "-",
	"Z", "X", "C", "V", "B", "N", "M", "_", ".", "'",
};
/* (the symbols page: a ruling, the spec names none; printable ASCII only) */
static const char *const symbol_keys[40] =
{
	"1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
	"!", "@", "#", "$", "%", "^", "&", "*", "(", ")",
	"~", "`", "+", "=", "[", "]", "{", "}", "\\", "|",
	";", ":", "\"", ",", "<", ">", "/", "?", ".", "'",
};

/* a key's size and gap (drawing units): FULL 60 x 52, 6 apart; VIEW (a ruling) min(48 view u, (width - 9 gaps of 5) /
10), 52/60 as tall */
static void key_size(struct ae_density const *density, float content_width, float *width, float *height, float *gap)
{
	if (density->kind == AE_DENSITY_VIEW)
	{
		*gap = units(density, KEY_VIEW_GAP_U);
		*width = (content_width - 9.0f * *gap) / 10.0f;
		if (*width > units(density, KEY_VIEW_WIDTH_U))
			*width = units(density, KEY_VIEW_WIDTH_U);
		*height = *width * KEY_HEIGHT_U / KEY_WIDTH_U;
	}
	else
	{
		*gap = units(density, KEY_GAP_U);
		*width = units(density, KEY_WIDTH_U);
		*height = units(density, KEY_HEIGHT_U);
	}
}

static float word_size(struct ae_density const *density)
{
	return floored(density, density->metrics->chip_text, MINOR_FLOOR_PIXELS);
}

short ae_keyboard_layout(struct ae_density const *density, float content_width, int page, struct ae_rect *keys,
	const char **labels, short maximum)
{
	/* the bottom row's keys and their widths in keys: SHIFT 2, #+= 2, SPACE 3, ◀, ▶, DONE */
	static const short spans[6] = { 2, 2, 3, 1, 1, 1 };
	short span[6], index, count = 0, column = 0;
	float width, height, gap;
	const char *const *grid = page ? symbol_keys : letter_keys;

	key_size(density, content_width, &width, &height, &gap);
	memcpy(span, spans, sizeof(span));
	/* (a narrow VIEW keyboard: DONE's word doesn't fit one key; SPACE gives it one of its three: a ruling) */
	if (ae_draw_text_width(AE_FONT_ROW, word_size(density), ae_string(AE_STR_KEY_DONE)) > width * 0.92f)
	{
		span[2] = 2;
		span[5] = 2;
	}
	for (index = 0; index < 40 && count < maximum; index++, count++)
	{
		keys[count].x = (float)(index % 10) * (width + gap);
		keys[count].y = (float)(index / 10) * (height + gap);
		keys[count].width = width;
		keys[count].height = height;
		if (labels)
			labels[count] = grid[index];
	}
	for (index = 0; index < 6 && count < maximum; index++, count++)
	{
		static const int words[6] = { AE_STR_KEY_SHIFT, AE_STR_KEY_SYMBOLS, AE_STR_KEY_SPACE, -1, -1, AE_STR_KEY_DONE };

		keys[count].x = (float)column * (width + gap);
		keys[count].y = 4.0f * (height + gap);
		keys[count].width = (float)span[index] * width + (float)(span[index] - 1) * gap;
		keys[count].height = height;
		column = (short)(column + span[index]);
		if (labels)
		{
			if (index == 1 && page)
				labels[count] = ae_string(AE_STR_KEY_LETTERS);
			else if (words[index] >= 0)
				labels[count] = ae_string(words[index]);
			else
				labels[count] = index == 3 ? "\xE2\x97\x80" : "\xE2\x96\xB6";
		}
	}
	return count;
}

/* the open keyboards: one per local player and one full-screen, as the pickers */
enum { KEYBOARD_SLOTS = AE_MAXIMUM_PLAYERS + 1 };
struct keyboard_state
{
	int open;
	short slot;
	struct ae_keyboard_spec spec;
	struct ae_text before;
	int page, shift;
	short focus, hover;
	struct ae_rect popover;        /* layout units */
	float content;                 /* the keys' width to fit (drawing units of its view) */
};
static struct keyboard_state keyboards[KEYBOARD_SLOTS];

static short keyboard_hit_id(struct keyboard_state const *state)
{
	return (short)(KEYBOARD_HIT_ID + state->slot);
}

/* the popover's parts in drawing units: the padding, the keys' width (content), the strip's height */
static void keyboard_parts(struct ae_density const *density, float content_width, float *pad, float *keys_width,
	float *keys_height, float *strip)
{
	float width, height, gap;

	*pad = units(density, KEYBOARD_PAD_U);
	key_size(density, content_width, &width, &height, &gap);
	*keys_width = 10.0f * width + 9.0f * gap;
	*keys_height = 5.0f * height + 4.0f * gap;
	*strip = ae_size_glyph(density) * 0.75f * 1.5f;
}

/* the shortcuts strip's density: the panel's, its glyphs 3/4 and its words the sub-line's size (a hint, smaller than
a footer: a ruling) */
static void strip_density(struct ae_density const *density, struct ae_metrics *metrics, struct ae_density *strip)
{
	*metrics = *density->metrics;
	metrics->glyph *= 0.75f;
	metrics->footer = metrics->sub_line;
	*strip = *density;
	strip->metrics = metrics;
}

static int keyboard_case_upper(struct keyboard_state const *state)
{
	int upper = state->spec.text && state->spec.text->uppercase;

	return state->shift ? !upper : upper;
}

/* a key's label as shown: letters in the case they type */
static const char *key_shown(struct keyboard_state const *state, short key, const char *label, char *buffer)
{
	if (key < 40 && label[0] >= 'A' && label[0] <= 'Z' && !label[1] && !keyboard_case_upper(state))
	{
		buffer[0] = (char)(label[0] - 'A' + 'a');
		buffer[1] = 0;
		return buffer;
	}
	return label;
}

static void keyboard_close(struct keyboard_state *state, int keep)
{
	void (*done)(int, void *) = state->spec.done;
	void *context = state->spec.context;

	if (!keep && state->spec.text)
		*state->spec.text = state->before;
	ae_sound_request(keep ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
	ae_ui_pop();
	if (done)
		done(keep, context);
}

/* types a character (the case the keyboard shows); failure at the limit */
static void keyboard_type(struct keyboard_state *state, const char *label)
{
	char character[2];

	character[0] = label[0];
	character[1] = 0;
	if (character[0] >= 'A' && character[0] <= 'Z' && !keyboard_case_upper(state))
		character[0] = (char)(character[0] - 'A' + 'a');
	ae_sound_request(ae_text_insert(state->spec.text, character) ? AE_SOUND_FORWARD : AE_SOUND_FAILURE, 0);
	state->shift = 0;
}

static void keyboard_press(struct keyboard_state *state, short key)
{
	struct ae_rect keys[KEYBOARD_KEYS];
	const char *labels[KEYBOARD_KEYS];

	if (!state->spec.text)
		return;
	ae_keyboard_layout(&state->spec.density, state->content, state->page, keys, labels, KEYBOARD_KEYS);
	switch (key)
	{
	case KEY_SHIFT: state->shift = !state->shift; ae_sound_request(AE_SOUND_FORWARD, 0); break;
	case KEY_PAGE: state->page = !state->page; ae_sound_request(AE_SOUND_FORWARD, 0); break;
	case KEY_SPACE: keyboard_type(state, " "); break;
	case KEY_LEFT: ae_text_move(state->spec.text, -1, 0); ae_sound_request(AE_SOUND_CURSOR, 0); break;
	case KEY_RIGHT: ae_text_move(state->spec.text, 1, 0); ae_sound_request(AE_SOUND_CURSOR, 0); break;
	case KEY_DONE: keyboard_close(state, 1); break;
	default:
		if (key >= 0 && key < 40)
			keyboard_type(state, labels[key]);
	}
}

static void keyboard_frame(struct keyboard_state const *state, struct ae_rect *popover)
{
	struct ae_view view;

	ae_draw_current_view(&view);
	popover->x = (state->popover.x - view.x) / view.scale;
	popover->y = (state->popover.y - view.y) / view.scale;
	popover->width = state->popover.width / view.scale;
	popover->height = state->popover.height / view.scale;
}

static void keyboard_enter(struct ae_screen *screen)
{
	(void)screen;
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void keyboard_leave(struct ae_screen *screen)
{
	struct keyboard_state *state = screen->data;

	state->open = 0;
}

static void keyboard_draw(struct ae_screen *screen)
{
	static const struct ae_prompt shortcuts[5] =
	{
		{ AE_BUTTON_X, NULL, NULL, AE_ACTION_X }, { AE_BUTTON_Y, NULL, NULL, AE_ACTION_Y },
		{ AE_BUTTON_LEFT_SHOULDER, NULL, NULL, AE_ACTION_TAB_PREVIOUS },
		{ AE_BUTTON_LEFT_TRIGGER, NULL, NULL, AE_ACTION_PAGE_UP }, { AE_BUTTON_START, NULL, NULL, AE_ACTION_START },
	};
	struct keyboard_state *state = screen->data;
	struct ae_density const *density = &state->spec.density;
	struct ae_rect popover, keys[KEYBOARD_KEYS];
	struct ae_prompt prompts[5];
	const char *labels[KEYBOARD_KEYS];
	float pad, keys_width, keys_height, strip, char_size = ae_size_text(density), words = word_size(density);
	short count, index, id = keyboard_hit_id(state);
	char buffer[2];

	if (state->spec.view.width > 0.0f && state->spec.view.height > 0.0f)
		ae_draw_view(state->spec.view.x, state->spec.view.y, state->spec.view.width, state->spec.view.height);
	keyboard_frame(state, &popover);
	keyboard_parts(density, state->content, &pad, &keys_width, &keys_height, &strip);
	ae_draw_rect(popover.x, popover.y, popover.width, popover.height, units(density, density->metrics->corner),
		AE_COLOR_POPOVER);
	ae_hit_add(popover.x, popover.y, popover.width, popover.height, id, AE_PART_CARD, -1);
	count = ae_keyboard_layout(density, state->content, state->page, keys, labels, KEYBOARD_KEYS);
	for (index = 0; index < count; index++)
	{
		struct ae_rect key = keys[index];
		int focused = index == state->focus, word = index >= 40 && index != KEY_LEFT && index != KEY_RIGHT;
		float size = word ? words : char_size;
		const char *shown = key_shown(state, index, labels[index], buffer);
		unsigned int color = focused ? AE_COLOR_SELECTION_TEXT : (index == KEY_SHIFT && state->shift) ? AE_COLOR_ACCENT :
			AE_COLOR_TEXT;

		key.x += popover.x + pad;
		key.y += popover.y + pad;
		/* focus: a white key with a 4 u accent base; hover a wash */
		ae_draw_rect(key.x, key.y, key.width, key.height, units(density, density->metrics->corner),
			focused ? AE_COLOR_SELECTION : AE_COLOR_ROW);
		if (focused)
			ae_draw_rect(key.x, key.y + key.height - units(density, KEY_BASE_U), key.width, units(density, KEY_BASE_U), 0.0f,
				AE_COLOR_ACCENT);
		else if (index == state->hover)
			ae_draw_rect(key.x, key.y, key.width, key.height, units(density, density->metrics->corner), AE_COLOR_HOVER);
		ae_draw_text(AE_FONT_ROW, size, key.x + key.width * 0.5f, text_top(AE_FONT_ROW, size, key.y + key.height * 0.5f),
			AE_ALIGN_CENTER, color, shown);
		ae_hit_add(key.x, key.y, key.width, key.height, id, AE_PART_KEY, index);
	}
	/* the pad's shortcuts, a strip inside the panel: X backspace, Y space, LB / RB caret, LT shift, START done */
	memcpy(prompts, shortcuts, sizeof(prompts));
	prompts[0].label = ae_string(AE_STR_BACKSPACE);
	prompts[1].label = ae_string(AE_STR_SPACE);
	prompts[2].label = ae_string(AE_STR_MOVE_CARET);
	prompts[3].label = ae_string(AE_STR_SHIFT);
	prompts[4].label = ae_string(AE_STR_DONE);
	{
		struct ae_metrics metrics;
		struct ae_density small;

		strip_density(density, &metrics, &small);
		ae_widget_prompts(&small, popover.x + pad, popover.y + pad + keys_height + strip * 0.5f + pad * 0.5f, prompts, 5,
			NULL, popover.x + popover.width - pad, -1, id);
	}
}

/* the key in a direction from the focus: along its row (wrapping), or the nearest in the row above / below */
static short keyboard_step(struct keyboard_state const *state, int action)
{
	struct ae_rect keys[KEYBOARD_KEYS];
	short count = ae_keyboard_layout(&state->spec.density, state->content, state->page, keys, NULL, KEYBOARD_KEYS);
	short focus = state->focus, index, best = focus;
	float x = keys[focus].x + keys[focus].width * 0.5f, y = keys[focus].y, best_distance = 1e30f;

	if (action == AE_ACTION_LEFT || action == AE_ACTION_RIGHT)
	{
		/* (the row's keys, wrapping) */
		short first = focus < 40 ? (short)(focus / 10 * 10) : 40, last = focus < 40 ? (short)(first + 9) : (short)(count - 1);

		if (action == AE_ACTION_LEFT)
			return (short)(focus > first ? focus - 1 : last);
		return (short)(focus < last ? focus + 1 : first);
	}
	{
		float row_height = keys[10].y - keys[0].y, target = y + (action == AE_ACTION_DOWN ? row_height : -row_height);

		/* (wrapping: past the last row to the first, before the first to the last) */
		if (target > keys[count - 1].y + 0.5f)
			target = keys[0].y;
		if (target < keys[0].y - 0.5f)
			target = keys[count - 1].y;
		for (index = 0; index < count; index++)
		{
			float distance = keys[index].x + keys[index].width * 0.5f - x;

			if (keys[index].y < target - 0.5f || keys[index].y > target + 0.5f)
				continue;
			distance = distance < 0.0f ? -distance : distance;
			if (distance < best_distance)
			{
				best_distance = distance;
				best = index;
			}
		}
	}
	return best;
}

static int keyboard_handle(struct ae_screen *screen, struct ae_event const *event)
{
	struct keyboard_state *state = screen->data;
	short before = state->focus;

	switch (event->action)
	{
	case AE_ACTION_UP: case AE_ACTION_DOWN: case AE_ACTION_LEFT: case AE_ACTION_RIGHT:
		state->focus = keyboard_step(state, event->action);
		if (state->focus != before)
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return 1;
	case AE_ACTION_ACCEPT: keyboard_press(state, state->focus); return 1;
	/* the pad's shortcuts */
	case AE_ACTION_X:
		ae_text_backspace(state->spec.text);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	case AE_ACTION_Y: keyboard_press(state, KEY_SPACE); return 1;
	case AE_ACTION_TAB_PREVIOUS: keyboard_press(state, KEY_LEFT); return 1;
	case AE_ACTION_TAB_NEXT: keyboard_press(state, KEY_RIGHT); return 1;
	case AE_ACTION_PAGE_UP: keyboard_press(state, KEY_SHIFT); return 1;
	case AE_ACTION_START: keyboard_close(state, 1); return 1;
	case AE_ACTION_BACK: keyboard_close(state, 0); return 1;
	}
	return 1;
}

static void keyboard_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	static const struct ae_prompt shortcuts[5] =
	{
		{ AE_BUTTON_X, NULL, NULL, AE_ACTION_X }, { AE_BUTTON_Y, NULL, NULL, AE_ACTION_Y },
		{ AE_BUTTON_LEFT_SHOULDER, NULL, NULL, AE_ACTION_TAB_PREVIOUS },
		{ AE_BUTTON_LEFT_TRIGGER, NULL, NULL, AE_ACTION_PAGE_UP }, { AE_BUTTON_START, NULL, NULL, AE_ACTION_START },
	};
	struct keyboard_state *state = screen->data;
	struct ae_hit hit;
	struct ae_event event;
	int mine = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == keyboard_hit_id(state);

	if (pointer->moved)
		state->hover = mine && hit.part == AE_PART_KEY ? hit.index : -1;
	if (!pointer->left_clicks || !mine)
		return;
	if (hit.part == AE_PART_KEY && hit.index >= 0 && hit.index < KEYBOARD_KEYS)
	{
		state->focus = hit.index;
		keyboard_press(state, hit.index);
	}
	else if (hit.part == AE_PART_PROMPT && ae_prompts_pointer(shortcuts, 5, pointer, keyboard_hit_id(state), &event))
		keyboard_handle(screen, &event);
}

static struct ae_screen_class const keyboard_class =
{
	.name = "keyboard", .enter = keyboard_enter, .leave = keyboard_leave, .handle = keyboard_handle,
	.draw = keyboard_draw, .pointer = keyboard_pointer, .popover = 1,
};

int ae_keyboard_open(struct ae_keyboard_spec const *spec, short owner)
{
	struct keyboard_state *state;
	struct ae_rect const *field, *bounds;
	short slot = (short)(owner >= 0 && owner < AE_MAXIMUM_PLAYERS ? owner : AE_MAXIMUM_PLAYERS);
	float scale, pad, keys_width, keys_height, strip, width, height, gap, content;

	if (!spec || !spec->text)
		return 0;
	state = &keyboards[slot];
	if (state->open && ae_ui_holds(state))
		return 0;
	memset(state, 0, sizeof(*state));
	state->slot = slot;
	state->spec = *spec;
	state->before = *spec->text;
	state->hover = -1;
	/* (focus on the first letter row's first key, Q) */
	state->focus = 10;
	scale = spec->view.width > 0.0f && spec->view.height > 0.0f ? spec->view.height / (float)AE_LAYOUT_HEIGHT : 1.0f;
	field = &spec->field;
	bounds = &spec->bounds;
	/* the size: the keys (VIEW: shrunk to the bounds' width), the padding, the strip; layout units */
	content = bounds->width / scale - 2.0f * units(&spec->density, KEYBOARD_PAD_U);
	state->content = content;
	keyboard_parts(&spec->density, content, &pad, &keys_width, &keys_height, &strip);
	width = (keys_width + 2.0f * pad) * scale;
	height = (keys_height + 2.0f * pad + strip) * scale;
	gap = units(&spec->density, KEYBOARD_GAP_U) * scale;
	/* beside the field when the bounds have room to its right, else under it (above when not under), in the bounds */
	if (field->x + field->width + gap + width <= bounds->x + bounds->width)
	{
		state->popover.x = field->x + field->width + gap;
		state->popover.y = field->y;
	}
	else
	{
		state->popover.x = field->x;
		state->popover.y = field->y + field->height + gap;
		if (state->popover.y + height > bounds->y + bounds->height)
			state->popover.y = field->y - gap - height;
	}
	state->popover.width = width;
	state->popover.height = height;
	if (state->popover.x + width > bounds->x + bounds->width)
		state->popover.x = bounds->x + bounds->width - width;
	if (state->popover.x < bounds->x)
		state->popover.x = bounds->x;
	if (state->popover.y + height > bounds->y + bounds->height)
		state->popover.y = bounds->y + bounds->height - height;
	if (state->popover.y < bounds->y)
		state->popover.y = bounds->y;
	if (!ae_ui_push(&keyboard_class, owner, state))
		return 0;
	state->open = 1;
	return 1;
}
