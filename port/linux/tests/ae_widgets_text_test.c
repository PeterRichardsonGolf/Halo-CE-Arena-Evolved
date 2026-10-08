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

static void middle_of(struct ae_stub_call const *call, float *x, float *y, float window_height)
{
	struct ae_rect pixels;

	ae_stub_pixels(call, &pixels);
	*x = (pixels.x + pixels.width * 0.5f) * 1080.0f / window_height;
	*y = (pixels.y + pixels.height * 0.5f) * 1080.0f / window_height;
}

/* the test's host (P13): typed keys queued here, a clipboard, typing mode counted */
static struct ae_text_key queued[16];
static int queued_count, typing_begun, typing_ended;
static char clipboard[1024];
static void test_begin(void) { typing_begun++; }
static void test_end(void) { typing_ended++; }
static int test_keys(struct ae_text_key *keys, int maximum)
{
	int count = queued_count < maximum ? queued_count : maximum;

	memcpy(keys, queued, sizeof(keys[0]) * (size_t)count);
	queued_count = 0;
	return count;
}
static int test_clipboard_get(char *text, int size)
{
	snprintf(text, (size_t)size, "%s", clipboard);
	return clipboard[0] != 0;
}
static void test_clipboard_set(const char *text) { snprintf(clipboard, sizeof(clipboard), "%s", text); }
static struct ae_host const test_host = { NULL, test_begin, test_end, test_keys, test_clipboard_get, test_clipboard_set };

static void key(short kind, char character)
{
	queued[queued_count].kind = kind;
	queued[queued_count].shift = 0;
	queued[queued_count].character = character;
	queued_count++;
}

static void fields(void)
{
	struct ae_density d;
	struct ae_text text;
	struct ae_field field;
	struct ae_field_edit edit;
	float height;

	ae_host_set(&test_host);
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_density_full(1080, 1.0f, &d);
	ae_text_init(&text, "SLAYER PRO", 11, 1);
	memset(&field, 0, sizeof(field));
	field.label = "GAME TYPE NAME";
	field.text = &text;
	/* normal: the well and its rule, the label tracked and muted, the text in the title colour, no ring */
	height = ae_widget_field(&d, 100, 100, 800, &field, 4);
	CHECK(height > 50 && count_calls(AE_STUB_RECT, AE_COLOR_WELL) == 1 && count_calls(AE_STUB_OUTLINE, AE_COLOR_RULE) == 1);
	CHECK(text_call("GAME TYPE NAME") && text_call("GAME TYPE NAME")->rgba == AE_COLOR_MUTED &&
		near(text_call("GAME TYPE NAME")->thickness, 0.09f, 1e-6f));
	CHECK(text_call("SLAYER PRO") && text_call("SLAYER PRO")->rgba == AE_COLOR_TITLE && text_call("SLAYER PRO")->size == 24);
	CHECK(count_calls(AE_STUB_OUTLINE, AE_COLOR_ACCENT) == 0 && !text_call("10 / 11"));
	/* focus: a 2 u accent ring and the count; editing: the caret, 2 u, 60 % of 50 u, accent */
	field.flags = AE_FIELD_FOCUSED;
	ae_stub_reset(1920, 1080);
	ae_widget_field(&d, 100, 100, 800, &field, 4);
	CHECK(count_calls(AE_STUB_OUTLINE, AE_COLOR_ACCENT) == 1 && text_call("10 / 11") &&
		text_call("10 / 11")->rgba == AE_COLOR_MUTED);
	field.flags = AE_FIELD_EDITING;
	ae_motion_set_reduced(1);
	ae_stub_reset(1920, 1080);
	ae_widget_field(&d, 100, 100, 800, &field, 4);
	{
		int index, carets = 0;

		for (index = 0; index < ae_stub_count(); index++)
		{
			struct ae_stub_call const *call = ae_stub_get(index);

			if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_ACCENT && near(call->width, 2, 0.01f) &&
				near(call->height, 30, 0.01f))
				carets++;
		}
		CHECK(carets == 1);
	}
	/* the count turns warning at the limit */
	ae_text_insert(&text, "X");
	ae_stub_reset(1920, 1080);
	ae_widget_field(&d, 100, 100, 800, &field, 4);
	CHECK(text_call("11 / 11") && text_call("11 / 11")->rgba == AE_COLOR_WARNING);
	/* a selection: accent 43 % behind the text */
	ae_text_select_all(&text);
	ae_stub_reset(1920, 1080);
	ae_widget_field(&d, 100, 100, 800, &field, 4);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_TEXT_SELECT) == 1);
	/* an error: a 2 u warning ring and its reason under the field */
	field.flags = AE_FIELD_FOCUSED | AE_FIELD_ERROR;
	field.error = "A game type with this name exists";
	ae_stub_reset(1920, 1080);
	height = ae_widget_field(&d, 100, 100, 800, &field, 4);
	CHECK(count_calls(AE_STUB_OUTLINE, AE_COLOR_WARNING) == 1 && count_calls(AE_STUB_OUTLINE, AE_COLOR_ACCENT) == 0);
	CHECK(text_call(field.error) && text_call(field.error)->rgba == AE_COLOR_WARNING &&
		text_call(field.error)->y > 100 + 50);
	/* hover: a wash; empty: the placeholder, muted, Overpass 750 */
	ae_text_init(&text, "", 40, 0);
	field.flags = AE_FIELD_HOVER;
	field.error = NULL;
	field.placeholder = "Optional: one line";
	ae_stub_reset(1920, 1080);
	ae_widget_field(&d, 100, 100, 800, &field, 4);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_HOVER) == 1 && text_call("Optional: one line") &&
		text_call("Optional: one line")->font == AE_FONT_BODY && text_call("Optional: one line")->rgba == AE_COLOR_MUTED);

	/* typing (P17): editing begins typing mode; a, e, Backspace, b into an uppercase field: exactly "AB"; Esc
	(cancel) restores the text from before; Enter (done) keeps it */
	ae_text_init(&text, "", 11, 1);
	typing_begun = typing_ended = 0;
	ae_field_edit_begin(&edit, &text);
	CHECK(typing_begun == 1 && edit.active);
	key(AE_TEXT_KEY_CHAR, 'a');
	key(AE_TEXT_KEY_CHAR, 'e');
	key(AE_TEXT_KEY_BACKSPACE, 0);
	key(AE_TEXT_KEY_CHAR, 'b');
	CHECK(ae_field_edit_keys(&edit) == 1 && !strcmp(text.text, "AB"));
	ae_field_edit_end(&edit, 1);
	CHECK(typing_ended == 1 && !edit.active && !strcmp(text.text, "AB"));
	ae_field_edit_begin(&edit, &text);
	key(AE_TEXT_KEY_CHAR, 'z');
	ae_field_edit_keys(&edit);
	CHECK(!strcmp(text.text, "ABZ"));
	ae_field_edit_end(&edit, 0);
	CHECK(!strcmp(text.text, "AB"));
	/* Home / End with Shift, Ctrl+A, Ctrl+C, Ctrl+V; a paste past the limit refused (0) */
	ae_text_init(&text, "SLAYER", 11, 1);
	queued_count = 0;
	key(AE_TEXT_KEY_SELECT_ALL, 0);
	key(AE_TEXT_KEY_COPY, 0);
	ae_field_type(&text, queued, queued_count);
	CHECK(!strcmp(clipboard, "SLAYER"));
	queued_count = 0;
	key(AE_TEXT_KEY_END, 0);
	key(AE_TEXT_KEY_PASTE, 0);
	CHECK(ae_field_type(&text, queued, queued_count) == 0 && !strcmp(text.text, "SLAYERSLAYE"));
	queued_count = 0;
	key(AE_TEXT_KEY_HOME, 0);
	queued[queued_count - 1].shift = 0;
	key(AE_TEXT_KEY_END, 0);
	queued[queued_count - 1].shift = 1;
	key(AE_TEXT_KEY_DELETE, 0);
	ae_field_type(&text, queued, queued_count);
	CHECK(text.length == 0);
	/* a paste longer than the clipboard buffer (600 characters): cut at the limit, refused (0), nothing overrun */
	ae_text_init(&text, "", AE_TEXT_MAXIMUM - 1, 0);
	memset(clipboard, 'a', 600);
	clipboard[600] = 0;
	queued_count = 0;
	key(AE_TEXT_KEY_PASTE, 0);
	CHECK(ae_field_type(&text, queued, queued_count) == 0 && text.length == AE_TEXT_MAXIMUM - 1 &&
		(short)strlen(text.text) == text.length && text.text[0] == 'a' && text.text[text.length - 1] == 'a');
	/* a non-ASCII paste ("e", e acute, "z"): the ASCII kept (upper case here), the rest dropped, refused (0) */
	ae_text_init(&text, "", 11, 1);
	snprintf(clipboard, sizeof(clipboard), "e\xC3\xA9z");
	queued_count = 0;
	key(AE_TEXT_KEY_PASTE, 0);
	CHECK(ae_field_type(&text, queued, queued_count) == 0 && !strcmp(text.text, "EZ"));
	queued_count = 0;
	ae_host_set(NULL);
	/* (with no host: nothing read, nothing breaks) */
	CHECK(ae_host_text_keys(queued, 4) == 0 && ae_host_clipboard_get(clipboard, sizeof(clipboard)) == 0);
	ae_host_set(&test_host);
	CHECK(!ae_stub_overflowed());
}

static void keyboard_layout(void)
{
	struct ae_density d;
	struct ae_rect keys[64];
	const char *labels[64];
	short count;
	int index;

	ae_stub_reset(1920, 1080);
	ae_density_full(1080, 1.0f, &d);
	/* letters: 4 x 10 + 6 keys; 60 x 52, 6 apart; SHIFT 2 x 60 + 6 wide, SPACE 3 x 60 + 2 x 6 */
	count = ae_keyboard_layout(&d, 1000, 0, keys, labels, 64);
	CHECK(count == 46);
	CHECK(near(keys[0].width, 60, 0.01f) && near(keys[0].height, 52, 0.01f) && near(keys[1].x - keys[0].x, 66, 0.01f));
	CHECK(near(keys[10].y - keys[0].y, 58, 0.01f));
	CHECK(!strcmp(labels[0], "1") && !strcmp(labels[10], "Q") && !strcmp(labels[29], "-") && !strcmp(labels[39], "'"));
	CHECK(!strcmp(labels[40], "SHIFT") && near(keys[40].width, 2 * 60 + 6, 0.01f));
	CHECK(!strcmp(labels[41], "#+=") && near(keys[41].width, 2 * 60 + 6, 0.01f));
	CHECK(!strcmp(labels[42], "SPACE") && near(keys[42].width, 3 * 60 + 2 * 6, 0.01f));
	CHECK(!strcmp(labels[45], "DONE") && near(keys[45].width, 60, 0.01f) &&
		near(keys[45].x + keys[45].width, keys[9].x + keys[9].width, 0.01f));
	/* the symbols page: printable ASCII; #+= reads ABC */
	count = ae_keyboard_layout(&d, 1000, 1, keys, labels, 64);
	CHECK(count == 46 && !strcmp(labels[41], "ABC") && !strcmp(labels[10], "!") && !strcmp(labels[28], "\\"));
	for (index = 0; index < 40; index++)
		CHECK(strlen(labels[index]) == 1 && labels[index][0] >= ' ' && labels[index][0] <= '~');
}

static short done_keep = -1;
static void done(int keep, void *context)
{
	(void)context;
	done_keep = (short)keep;
}

static struct ae_screen_class const base = { .name = "base" };

static void keyboards(void)
{
	struct ae_density d;
	struct ae_keyboard_spec spec;
	struct ae_text text;
	struct ae_event event;
	struct ae_pointer pointer;
	struct ae_rect frame = { 0, 0, 1920, 1080 }, pop = { 0, 0, 0, 0 };
	int index;

	/* at 1920x1080 130 %: beside a left-column field, inside the frame */
	ae_motion_set_reduced(1);
	ae_ui_reset();
	ae_ui_set_before_draw(NULL);
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_density_full(1080, 1.3f, &d);
	ae_text_init(&text, "SLAYER PRO", 11, 1);
	memset(&spec, 0, sizeof(spec));
	spec.text = &text;
	spec.field.x = 96; spec.field.y = 300; spec.field.width = 600; spec.field.height = 65;
	spec.bounds = frame;
	spec.density = d;
	spec.done = done;
	ae_sound_reset();
	CHECK(ae_keyboard_open(&spec, AE_OWNER_ANY) && ae_ui_depth() == 2 && ae_sound_take(0) == AE_SOUND_FORWARD);
	CHECK(!ae_keyboard_open(&spec, AE_OWNER_ANY));
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_ui_draw();
	for (index = 0; index < ae_stub_count(); index++)
		if (ae_stub_get(index)->kind == AE_STUB_RECT && ae_stub_get(index)->rgba == AE_COLOR_POPOVER)
			ae_stub_pixels(ae_stub_get(index), &pop);
	CHECK(pop.width > 0 && inside(&pop, &frame, 0.05f) && pop.x >= 96 + 600);
	/* the focus on Q (a white key, a 4 u accent base); upper case for an uppercase field */
	CHECK(text_call("Q") && text_call("Q")->rgba == AE_COLOR_SELECTION_TEXT && text_call("W") &&
		text_call("W")->rgba == AE_COLOR_TEXT);
	CHECK(count_calls(AE_STUB_RECT, AE_COLOR_SELECTION) == 1);
	/* (the pad's shortcuts strip: Backspace, Space, Move caret, Shift, Done) */
	CHECK(text_call("Backspace") && text_call("Space") && text_call("Move caret") && text_call("Shift") && text_call("Done"));
	/* A types the focused key (forward); at the limit: failure */
	event.player = 0; event.device = AE_DEVICE_XBOX; event.repeat = 0;
	event.action = AE_ACTION_ACCEPT;
	ae_ui_dispatch(&event);
	CHECK(!strcmp(text.text, "SLAYER PROQ") && ae_sound_take(0) == AE_SOUND_FORWARD);
	ae_ui_dispatch(&event);
	CHECK(!strcmp(text.text, "SLAYER PROQ") && ae_sound_take(0) == AE_SOUND_FAILURE);
	/* X backspace, Y space, LB caret left; moving the focus */
	event.action = AE_ACTION_X;
	ae_ui_dispatch(&event);
	ae_ui_dispatch(&event);
	CHECK(!strcmp(text.text, "SLAYER PR"));
	event.action = AE_ACTION_Y;
	ae_ui_dispatch(&event);
	CHECK(!strcmp(text.text, "SLAYER PR "));
	event.action = AE_ACTION_TAB_PREVIOUS;
	ae_ui_dispatch(&event);
	CHECK(text.caret == 9);
	event.action = AE_ACTION_RIGHT;
	ae_ui_dispatch(&event);
	event.action = AE_ACTION_ACCEPT;
	ae_ui_dispatch(&event);
	CHECK(!strcmp(text.text, "SLAYER PRW "));
	/* a click on a key types it (the mouse) */
	ae_stub_reset(1920, 1080);
	ae_hits_clear();
	ae_ui_draw();
	ae_text_end(&text, 0);
	ae_text_backspace(&text);
	memset(&pointer, 0, sizeof(pointer));
	middle_of(text_call("Z"), &pointer.x, &pointer.y, 1080);
	pointer.left_clicks = 1;
	ae_ui_dispatch_pointer(&pointer);
	CHECK(!strcmp(text.text, "SLAYER PRWZ"));
	/* B cancels: the text from before, done(0), back */
	done_keep = -1;
	event.action = AE_ACTION_BACK;
	ae_ui_dispatch(&event);
	CHECK(done_keep == 0 && !strcmp(text.text, "SLAYER PRO") && ae_ui_depth() == 1 && ae_sound_take(0) == AE_SOUND_BACK);
	/* START is done: kept */
	ae_text_init(&text, "", 11, 0);
	ae_keyboard_open(&spec, AE_OWNER_ANY);
	event.action = AE_ACTION_ACCEPT;
	ae_ui_dispatch(&event);
	CHECK(!strcmp(text.text, "q"));   /* (a lower-case field types lower case) */
	event.action = AE_ACTION_PAGE_UP;   /* LT: shift for the next key */
	ae_ui_dispatch(&event);
	event.action = AE_ACTION_ACCEPT;
	ae_ui_dispatch(&event);
	ae_ui_dispatch(&event);
	CHECK(!strcmp(text.text, "qQq"));
	event.action = AE_ACTION_START;
	ae_ui_dispatch(&event);
	CHECK(done_keep == 1 && !strcmp(text.text, "qQq") && ae_ui_depth() == 1);
	CHECK(!ae_stub_overflowed());
}

/* an edit's own done: which edit heard which end */
static int edit_dones[2], edit_keeps[2];
static void edit_done(int keep, void *context)
{
	int which = *(int const *)context;

	edit_dones[which]++;
	edit_keeps[which] = keep;
}

/* typing never left on (I1) and one typing owner (I2) */
static void typing_owners(void)
{
	static int const zero = 0, one = 1;
	struct ae_text text, other;
	struct ae_field_edit edit, second;
	int screen_data;

	memset(edit_dones, 0, sizeof(edit_dones));
	memset(&edit, 0, sizeof(edit));
	memset(&second, 0, sizeof(second));
	edit.done = edit_done;
	edit.context = (void *)&zero;
	second.done = edit_done;
	second.context = (void *)&one;
	queued_count = 0;
	/* a reset stack mid-edit: typing mode ends, the text from before, done(0) */
	ae_ui_reset();
	ae_ui_push(&base, AE_OWNER_ANY, &screen_data);
	ae_text_init(&text, "BEFORE", 11, 1);
	typing_begun = typing_ended = 0;
	ae_field_edit_begin(&edit, &text);
	ae_text_insert(&text, "X");
	CHECK(typing_begun == 1 && ae_field_edit_live());
	ae_ui_reset();
	CHECK(typing_ended == 1 && !edit.active && !ae_field_edit_live() && !strcmp(text.text, "BEFORE") &&
		edit_dones[0] == 1 && edit_keeps[0] == 0);
	/* the edit's screen popped (its holder gone): the frame's guard ends it the same way */
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_ui_push(&base, AE_OWNER_ANY, &screen_data);
	edit.holder = &screen_data;
	ae_field_edit_begin(&edit, &text);
	ae_text_insert(&text, "Y");
	ae_field_edit_guard();
	CHECK(typing_ended == 1 && edit.active);   /* (still on the stack: nothing) */
	ae_ui_pop();
	ae_field_edit_guard();
	CHECK(typing_ended == 2 && !edit.active && !strcmp(text.text, "BEFORE") && edit_dones[0] == 2);
	/* the flag going off (or AE's menus gone) mid-edit: abort ends it */
	edit.holder = NULL;
	ae_field_edit_begin(&edit, &text);
	ae_field_edit_abort();
	CHECK(typing_begun == 3 && typing_ended == 3 && !edit.active && !ae_field_edit_live() && edit_dones[0] == 3);
	ae_field_edit_abort();
	CHECK(typing_ended == 3 && edit_dones[0] == 3);   /* (nothing left to end) */
	/* two views: P1 editing, then P2 begins: P1 committed (kept, done(1)), typing mode stays on; P2 types; P1's end
	does nothing to P2; P2's end turns typing off */
	memset(edit_dones, 0, sizeof(edit_dones));
	ae_text_init(&text, "ONE", 11, 1);
	ae_text_init(&other, "TWO", 11, 1);
	typing_begun = typing_ended = 0;
	ae_field_edit_begin(&edit, &text);
	key(AE_TEXT_KEY_CHAR, 'a');
	ae_field_edit_keys(&edit);
	CHECK(!strcmp(text.text, "ONEA"));
	ae_field_edit_begin(&second, &other);
	CHECK(typing_begun == 1 && typing_ended == 0 && !edit.active && second.active && edit_dones[0] == 1 &&
		edit_keeps[0] == 1 && !strcmp(text.text, "ONEA"));
	key(AE_TEXT_KEY_CHAR, 'b');
	CHECK(ae_field_edit_keys(&edit) == 1 && queued_count == 1);   /* (not the owner: reads nothing) */
	ae_field_edit_keys(&second);
	CHECK(!strcmp(other.text, "TWOB") && !strcmp(text.text, "ONEA"));
	ae_field_edit_end(&edit, 0);
	CHECK(typing_ended == 0 && second.active && ae_field_edit_live() && !strcmp(text.text, "ONEA") &&
		edit_dones[1] == 0);
	ae_field_edit_end(&second, 1);
	CHECK(typing_ended == 1 && !ae_field_edit_live() && !strcmp(other.text, "TWOB") && edit_dones[1] == 0);
	queued_count = 0;
	ae_ui_reset();
}

/* the on-screen keyboard and a physical keyboard: a typed printable key closes it (kept) and typing goes on into the
field through its edit, that key inserted first; a pop by anything else than its own keys cancels it (done(0)) */
static void keyboard_typing(void)
{
	static int const zero = 0;
	struct ae_density d;
	struct ae_keyboard_spec spec;
	struct ae_text text;
	struct ae_field_edit edit;

	ae_motion_set_reduced(1);
	ae_ui_reset();
	ae_ui_set_before_draw(NULL);
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_density_full(1080, 1.0f, &d);
	ae_text_init(&text, "SLAY", 11, 1);
	memset(&edit, 0, sizeof(edit));
	memset(edit_dones, 0, sizeof(edit_dones));
	edit.done = edit_done;
	edit.context = (void *)&zero;
	memset(&spec, 0, sizeof(spec));
	spec.text = &text;
	spec.field.x = 96; spec.field.y = 300; spec.field.width = 600; spec.field.height = 65;
	spec.bounds.width = 1920; spec.bounds.height = 1080;
	spec.density = d;
	spec.done = done;
	spec.edit = &edit;
	queued_count = 0;
	typing_begun = typing_ended = 0;
	done_keep = -1;
	/* open: the keyboard owns the keys (typing mode on) */
	CHECK(ae_keyboard_open(&spec, AE_OWNER_ANY) && typing_begun == 1 && ae_field_edit_live());
	/* a Backspace typed: edited in place, the keyboard stays */
	key(AE_TEXT_KEY_BACKSPACE, 0);
	ae_ui_update();
	CHECK(!strcmp(text.text, "SLA") && ae_ui_depth() == 2 && done_keep == -1);
	/* "y", "e" typed: closed kept (done(1)), the field's edit owns the keys, typing still on; both inserted */
	key(AE_TEXT_KEY_CHAR, 'y');
	key(AE_TEXT_KEY_CHAR, 'e');
	ae_ui_update();
	CHECK(done_keep == 1 && ae_ui_depth() == 1 && edit.active && ae_field_edit_live() && typing_ended == 0 &&
		typing_begun == 1 && !strcmp(text.text, "SLAYE") && edit_dones[0] == 0);
	/* typing goes on through the field's edit; its end turns typing off */
	key(AE_TEXT_KEY_CHAR, 'r');
	ae_field_edit_keys(&edit);
	CHECK(!strcmp(text.text, "SLAYER"));
	ae_field_edit_end(&edit, 1);
	CHECK(typing_ended == 1 && !ae_field_edit_live());
	/* no field edit given: the key inserted, the keyboard closed, typing ends */
	spec.edit = NULL;
	done_keep = -1;
	CHECK(ae_keyboard_open(&spec, AE_OWNER_ANY) && typing_begun == 2);
	key(AE_TEXT_KEY_CHAR, 's');
	ae_ui_update();
	CHECK(done_keep == 1 && ae_ui_depth() == 1 && !strcmp(text.text, "SLAYERS") && typing_ended == 2 &&
		!ae_field_edit_live());
	/* popped by something else (not B / START / DONE): the text from before, done(0), typing off */
	done_keep = -1;
	CHECK(ae_keyboard_open(&spec, AE_OWNER_ANY) && typing_begun == 3);
	key(AE_TEXT_KEY_BACKSPACE, 0);
	ae_ui_update();
	CHECK(!strcmp(text.text, "SLAYER"));
	ae_ui_pop();
	CHECK(done_keep == 0 && !strcmp(text.text, "SLAYERS") && typing_ended == 3 && !ae_field_edit_live() &&
		ae_ui_depth() == 1);
	/* a reset with the keyboard open: typing off, and the slot opens again */
	CHECK(ae_keyboard_open(&spec, AE_OWNER_ANY) && typing_begun == 4);
	ae_ui_reset();
	CHECK(typing_ended == 4 && !ae_field_edit_live() && !strcmp(text.text, "SLAYERS"));
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	CHECK(ae_keyboard_open(&spec, AE_OWNER_ANY));
	ae_ui_reset();
	queued_count = 0;
}

/* inside, with 2 px of horizontal ink allowed (the clip / box tests' rule) */
static int inside_ink(struct ae_rect const *a, struct ae_rect const *b)
{
	return inside(a, b, 0.05f) || (a->y >= b->y - 0.05f && a->y + a->height <= b->y + b->height + 0.05f &&
		a->x >= b->x - 2.0f && a->x + a->width <= b->x + b->width + 2.0f);
}

/* the keyboard in a player's quarter (640x360 in a 720p window, 960x540 in 1080p): the whole popover inside the
panel's content, every key inside, key text at least 16 px, key words 14 px and each inside its own key */
static void keyboard_view(float window_height)
{
	struct ae_density d;
	struct ae_keyboard_spec spec;
	struct ae_text text;
	struct ae_rect content, content_pixels, field, pop = { 0, 0, 0, 0 };
	struct ae_rect key_rects[64];
	struct ae_event event;
	float pixels = window_height / 1080.0f;
	int index, keys = 0, words = 0;

	ae_ui_reset();
	ae_ui_set_before_draw(NULL);
	ae_ui_push(&base, AE_OWNER_ANY, NULL);
	ae_stub_reset(1920, (int)window_height);
	ae_draw_view(960, 0, 960, 540);
	ae_density_view(960.0f * pixels, 540.0f * pixels, 1.0f, &d);
	ae_widget_view_panel(&d, 960.0f * pixels, 540.0f * pixels, 1, 0x3B8ED8FFu, "PROFILE", 1, 4, &content);
	/* (the content in layout units: the view's drawing units x 0.5, from its corner) */
	content_pixels.x = (960 + content.x * 0.5f) * pixels;
	content_pixels.y = content.y * 0.5f * pixels;
	content_pixels.width = content.width * 0.5f * pixels;
	content_pixels.height = content.height * 0.5f * pixels;
	ae_text_init(&text, "PLAYER", 11, 1);
	memset(&spec, 0, sizeof(spec));
	spec.text = &text;
	spec.density = d;
	spec.done = done;
	spec.view.x = 960; spec.view.y = 0; spec.view.width = 960; spec.view.height = 540;
	spec.bounds.x = 960 + content.x * 0.5f; spec.bounds.y = content.y * 0.5f;
	spec.bounds.width = content.width * 0.5f; spec.bounds.height = content.height * 0.5f;
	field = spec.bounds;
	field.height = 40;
	spec.field = field;
	CHECK(ae_keyboard_open(&spec, 1));
	ae_stub_reset(1920, (int)window_height);
	ae_hits_clear();
	ae_ui_draw();
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *call = ae_stub_get(index);

		if (call->kind == AE_STUB_RECT && call->rgba == AE_COLOR_POPOVER)
			ae_stub_pixels(call, &pop);
		if (call->kind == AE_STUB_RECT && (call->rgba == AE_COLOR_ROW || call->rgba == AE_COLOR_SELECTION) &&
			keys < 64)
		{
			ae_stub_pixels(call, &key_rects[keys]);
			CHECK(inside(&key_rects[keys], &content_pixels, 0.05f));
			keys++;
		}
		if (call->kind == AE_STUB_TEXT && strlen(call->text) == 1)
			CHECK(ae_stub_text_em_pixels(call) >= 16.0f - 0.01f);
	}
	CHECK(keys == 46);
	CHECK(pop.width > 0 && inside(&pop, &content_pixels, 0.05f));
	if (!inside(&pop, &content_pixels, 0.05f))
		printf("  popover %.2f %.2f %.2f %.2f content %.2f %.2f %.2f %.2f; window %.0f\n", pop.x, pop.y, pop.width,
			pop.height, content_pixels.x, content_pixels.y, content_pixels.width, content_pixels.height, window_height);
	/* (each key word inside the key under its middle: the narrow keyboard's DONE takes two) */
	for (index = 0; index < ae_stub_count(); index++)
	{
		struct ae_stub_call const *call = ae_stub_get(index);
		struct ae_rect word;
		float middle_x, middle_y;
		int which, found = -1;

		if (call->kind != AE_STUB_TEXT || (strcmp(call->text, "SHIFT") && strcmp(call->text, "DONE") &&
			strcmp(call->text, "SPACE") && strcmp(call->text, "#+=")))
			continue;
		CHECK(ae_stub_text_em_pixels(call) >= 14.0f - 0.01f);
		ae_stub_pixels(call, &word);
		middle_x = word.x + word.width * 0.5f;
		middle_y = word.y + word.height * 0.5f;
		for (which = 0; which < keys; which++)
			if (middle_x >= key_rects[which].x && middle_x <= key_rects[which].x + key_rects[which].width &&
				middle_y >= key_rects[which].y && middle_y <= key_rects[which].y + key_rects[which].height)
				found = which;
		CHECK(found >= 0 && inside_ink(&word, &key_rects[found]));
		if (found < 0 || !inside_ink(&word, &key_rects[found]))
			printf("  %s at %.0f wide %.1f; window %.0f\n", call->text, word.x, word.width, window_height);
		words++;
	}
	CHECK(words == 4);
	event.player = 1; event.device = AE_DEVICE_XBOX; event.repeat = 0; event.action = AE_ACTION_BACK;
	ae_ui_dispatch(&event);
	CHECK(ae_ui_depth() == 1);
	CHECK(!ae_stub_overflowed());
}

int main(void)
{
	fields();
	keyboard_layout();
	keyboards();
	keyboard_view(720.0f);
	keyboard_view(1080.0f);
	typing_owners();
	keyboard_typing();
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
