/* ae_test_run_note.c: Arena Evolved TEST RUN's note box (see ae_test_run_note.h). The keys are read as the chat line
reads them (chat.c typing_keys): input_get_key, which xinput_sdl.c fills while ae_test_run_typing says 1. The pad's
presses come from ae_test_run_input.c. The editing is ae_test_run_edit.c's. */

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "input/input.h"
#include "ae_test_run_edit.h"
#include "ae_test_run_input.h"
#include "ae_test_run_keyboard.h"
#include "ae_test_run_note.h"
#include "ae_ui.h"

/* port/linux/src/ae_platform.c */
void ae_platform_note_keyboard_used(void);
/* port/linux/src/sdl_platform.c */
int platform_clipboard_get(char *text, int size);

/* (a key stroke's modifiers, as input_xbox.c :171 has them: control bit 1) */
#define KEY_MODIFIER_CONTROL 0x02

enum { TAG_TYPE = 4, TAG_NO_NOTE = 5 };
static const char *const tag_names[] = { "LOOKS WRONG", "WRONG NUMBER", "CRASH / FREEZE", "SOUND", "TYPE\xE2\x80\xA6", "NO NOTE" };

static int open_box;
static int cursor;
static int tag_at;
static int chooser_on; /* the verdict chooser has the box */
static int choice_at;
static int picker_on; /* TYPE…: the pad keyboard has the pad */
static struct ae_test_run_keyboard picker;

void ae_test_run_note_open(void)
{
	struct key_stroke key;

	/* (what was typed before is not this box's) */
	while (input_get_key(&key))
		;
	ae_test_run_input_note_edges();
	cursor = 0;
	tag_at = 0;
	picker_on = 0;
	chooser_on = 0;
	open_box = 1;
}

void ae_test_run_note_close(void)
{
	open_box = 0;
	picker_on = 0;
	chooser_on = 0;
}

int ae_test_run_typing(void)
{
	return open_box;
}

int ae_test_run_note_tag_count(void)
{
	return (int)(sizeof(tag_names) / sizeof(tag_names[0]));
}

const char *ae_test_run_note_tag_label(int index)
{
	return index >= 0 && index < ae_test_run_note_tag_count() ? tag_names[index] : "";
}

int ae_test_run_note_tag_index(void)
{
	return tag_at;
}

int ae_test_run_note_cursor(void)
{
	return cursor;
}

void ae_test_run_note_type_open(void)
{
	if (!open_box)
		return;
	ae_test_run_keyboard_open(&picker);
	picker_on = 1;
}

int ae_test_run_note_picker_open(void)
{
	return open_box && picker_on;
}

const struct ae_test_run_keyboard *ae_test_run_note_picker(void)
{
	return &picker;
}

/* the tag's text as the note: the label in capitals as shown (an ASCII label), NO NOTE empty */
static void save_tag(char *note, int size)
{
	snprintf(note, (size_t)size, "%s", tag_at == TAG_NO_NOTE ? "" : tag_names[tag_at]);
}

int ae_test_run_note_poll(char *note, int size)
{
	struct key_stroke key;
	struct ae_test_run_edit edit;
	unsigned edges;
	int count = ae_test_run_note_tag_count();

	if (!open_box)
		return -1;
	ae_test_run_edit_init(&edit, note, size);
	if (cursor >= 0 && cursor <= edit.cursor)
	{
		edit.cursor = cursor;
		/* (a cursor off a character boundary is put back on one) */
		while (edit.cursor > 0 && ((unsigned char)note[edit.cursor] & 0xC0) == 0x80)
			edit.cursor--;
	}
	while (input_get_key(&key))
	{
		int control = (key.modifier_flags & KEY_MODIFIER_CONTROL) != 0;

		/* (a key typed is the keyboard used: AE's record, ae_platform_keyboard_last_used) */
		ae_platform_note_keyboard_used();
		if (key.key_code == _key_escape)
			return -1;
		if (key.key_code == _key_return)
		{
			cursor = edit.cursor;
			return 1;
		}
		if (key.key_code == _key_backspace)
			ae_test_run_edit_backspace(&edit);
		else if (key.key_code == _key_delete)
			ae_test_run_edit_delete(&edit);
		else if (key.key_code == _key_home)
			ae_test_run_edit_home(&edit);
		else if (key.key_code == _key_end)
			ae_test_run_edit_end(&edit);
		else if (key.key_code == _key_left_arrow)
			ae_test_run_edit_left(&edit);
		else if (key.key_code == _key_right_arrow)
			ae_test_run_edit_right(&edit);
		else if (control && key.key_code == _key_v)
		{
			char clipboard[512];

			if (platform_clipboard_get(clipboard, sizeof(clipboard)))
				ae_test_run_edit_paste(&edit, clipboard);
		}
		else if (!control && (unsigned char)key.ascii_code >= ' ' && (unsigned char)key.ascii_code <= '~')
			ae_test_run_edit_insert(&edit, &key.ascii_code, 1);
	}
	edges = ae_test_run_input_note_edges();
	/* (0.7.0, the owner's TEST RUN: Backspace moved like Left and Right and Delete did nothing. The keyboard also
	drives the pad (xinput_sdl.c: arrows the d-pad, Backspace B, Delete X), so while the keys type here the pad
	keyboard took the same press as a move or a key of its own. Typing on the keyboard: its keys alone count) */
	if (picker_on && ae_test_run_input_device() == AE_DEVICE_KEYBOARD_MOUSE)
		edges = 0;
	if (picker_on)
	{
		/* (the pad keyboard types into the same buffer as the keys do: B leaves it and keeps nothing) */
		int result = ae_test_run_keyboard_poll(&picker, edges, &edit);

		cursor = edit.cursor;
		if (result < 0)
		{
			note[0] = 0;
			cursor = 0;
			picker_on = 0;
			return 0;
		}
		return result;
	}
	cursor = edit.cursor;
	if (edges & (AE_TEST_RUN_PAD_LEFT | AE_TEST_RUN_PAD_UP))
		tag_at = (tag_at + count - 1) % count;
	if (edges & (AE_TEST_RUN_PAD_RIGHT | AE_TEST_RUN_PAD_DOWN))
		tag_at = (tag_at + 1) % count;
	if (edges & AE_TEST_RUN_PAD_B)
		return -1;
	if (edges & AE_TEST_RUN_PAD_A)
	{
		if (tag_at == TAG_TYPE)
			ae_test_run_note_type_open();
		else
		{
			save_tag(note, size);
			return 1;
		}
	}
	return 0;
}

void ae_test_run_note_choose_open(void)
{
	struct key_stroke key;

	/* (the Enter or A that saved the note is not a pick) */
	while (input_get_key(&key))
		;
	ae_test_run_input_note_edges();
	picker_on = 0;
	chooser_on = 1;
	choice_at = 0;
	open_box = 1;
}

int ae_test_run_note_choosing(void)
{
	return open_box && chooser_on;
}

int ae_test_run_note_choice_index(void)
{
	return choice_at;
}

int ae_test_run_note_choose_poll(void)
{
	struct key_stroke key;
	unsigned edges;

	if (!open_box || !chooser_on)
		return AE_TEST_RUN_CHOICE_STAY;
	while (input_get_key(&key))
	{
		ae_platform_note_keyboard_used();
		if (key.key_code == _key_f7)
			return AE_TEST_RUN_CHOICE_PASS;
		if (key.key_code == _key_f8)
			return AE_TEST_RUN_CHOICE_FAIL;
		if (key.key_code == _key_f5)
			return AE_TEST_RUN_CHOICE_CONTINUE;
		if (key.key_code == _key_escape)
			return AE_TEST_RUN_CHOICE_STAY;
		if (key.key_code == _key_return)
			return AE_TEST_RUN_CHOICE_PASS + choice_at;
		if (key.key_code == _key_left_arrow || key.key_code == _key_up_arrow)
			choice_at = (choice_at + 2) % 3;
		else if (key.key_code == _key_right_arrow || key.key_code == _key_down_arrow)
			choice_at = (choice_at + 1) % 3;
	}
	edges = ae_test_run_input_note_edges();
	if (edges & (AE_TEST_RUN_PAD_LEFT | AE_TEST_RUN_PAD_UP))
		choice_at = (choice_at + 2) % 3;
	if (edges & (AE_TEST_RUN_PAD_RIGHT | AE_TEST_RUN_PAD_DOWN))
		choice_at = (choice_at + 1) % 3;
	if (edges & AE_TEST_RUN_PAD_B)
		return AE_TEST_RUN_CHOICE_STAY;
	if (edges & AE_TEST_RUN_PAD_A)
		return AE_TEST_RUN_CHOICE_PASS + choice_at;
	return AE_TEST_RUN_CHOICE_NONE;
}
