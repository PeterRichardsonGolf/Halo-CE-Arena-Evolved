/*
AE_GLUE_TEXT.C

The typing glue for AE's text fields (ae_widgets.h): the platform's typing
mode, the keys typed, the clipboard, and the host table the pure widgets
reach the engine through (preflight P13).

Typing mode is upstream's, as its PC text field uses it
(port/linux/game/menu_functions.c text_field_begin :2673-2688 drains
input_get_key and calls platform_text_field(TRUE); text_field_end :2698
calls platform_text_field(FALSE); text_field_show :2726-2758 reads
input_get_key: Backspace, Ctrl+V pastes, printable ASCII). While it is on
(port/linux/src/xinput_sdl.c :250-300, typing_gamepad), the arrows drive the
d-pad (the caret: AE's LEFT / RIGHT), Enter the START button (done), Escape B
(cancel), and the other keys reach input_get_key instead of driving the
first controller (XInputDebugGetKeystroke :1213-1236 passes them only then).

input_get_key's queue is input_globals.buffered_keys (source/input/
input_xbox.c :613), refilled at each input_update (:1062), not the event
manager's queue ae_input_poll flushes: the field reads it in its screen's
update (ae_ui_update, before ae_input_poll), and nothing else takes it while
AE's screens are up (upstream's text_field_show runs only for its menus).
AE's own keys (Q, E, Tab, Page Up / Down) stand aside while typing
(ae_input.c, ae_glue_text_typing).
*/

#include "cseries.h"
#include "input/input.h"
#include "ae_widgets.h"

/* port/linux/src (declared here as the game's other port units do) */
void platform_text_field(int typing, int password);
int platform_clipboard_get(char *text, int size);
void platform_clipboard_set(char const *text);
void platform_log(char const *format, ...);

/* (a key stroke's modifiers, as input_xbox.c :171 has them: shift bit 0, control bit 1) */
#define KEY_MODIFIER_SHIFT 0x01
#define KEY_MODIFIER_CONTROL 0x02

static int typing;

void ae_glue_text_begin(
	void)
{
	struct key_stroke key;

	/* (as text_field_begin: what was typed before is not this field's) */
	while (input_get_key(&key))
		;
	platform_text_field(TRUE, FALSE);
	typing = 1;
}

void ae_glue_text_end(
	void)
{
	platform_text_field(FALSE, FALSE);
	typing = 0;
}

int ae_glue_text_typing(
	void)
{
	return typing;
}

int ae_glue_text_keys(
	struct ae_text_key *keys,
	int maximum)
{
	struct key_stroke key;
	int count = 0;

	while (count < maximum && input_get_key(&key))
	{
		struct ae_text_key *out = &keys[count];
		int control = (key.modifier_flags & KEY_MODIFIER_CONTROL) != 0;

		out->shift = (short)((key.modifier_flags & KEY_MODIFIER_SHIFT) != 0);
		out->character = 0;
		if (key.key_code == _key_backspace)
			out->kind = AE_TEXT_KEY_BACKSPACE;
		else if (key.key_code == _key_delete)
			out->kind = AE_TEXT_KEY_DELETE;
		else if (key.key_code == _key_home)
			out->kind = AE_TEXT_KEY_HOME;
		else if (key.key_code == _key_end)
			out->kind = AE_TEXT_KEY_END;
		else if (control && key.key_code == _key_a)
			out->kind = AE_TEXT_KEY_SELECT_ALL;
		else if (control && key.key_code == _key_c)
			out->kind = AE_TEXT_KEY_COPY;
		else if (control && key.key_code == _key_v)
			out->kind = AE_TEXT_KEY_PASTE;
		else if (!control && (unsigned char)key.ascii_code >= ' ' && (unsigned char)key.ascii_code <= '~')
		{
			out->kind = AE_TEXT_KEY_CHAR;
			out->character = key.ascii_code;
		}
		else
			continue;
		count++;
	}
	return count;
}

int ae_glue_clipboard_get(
	char *text,
	int size)
{
	return platform_clipboard_get(text, size);
}

void ae_glue_clipboard_set(
	char const *text)
{
	platform_clipboard_set(text);
}

static void glue_log(
	char const *text)
{
	platform_log("%s", text);
}

void ae_glue_text_install(
	void)
{
	static struct ae_host const host =
	{
		glue_log, ae_glue_text_begin, ae_glue_text_end, ae_glue_text_keys, ae_glue_clipboard_get, ae_glue_clipboard_set,
	};

	ae_host_set(&host);
}
