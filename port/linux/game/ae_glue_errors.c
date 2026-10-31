/*
AE_GLUE_ERRORS.C

The game's error messages with AE's menus on (0.4.0): display_error (ui_widget.c) asks ae_errors_take first, and an
error AE takes is shown as AE's error dialog, never as the game's error widget. The game's widget drew over AE's
screens but took no input under them (process_ui_widgets returns while AE's are up): its START never reached it, so
it stayed up for good over AE's main menu (a join without the host's map, 0.4.0 box probe) while A and B went to the
screen under it.

AE takes an error when its menus are on and drawn, at the main menu (the errors queued for it:
display_error[_text]_when_main_menu_loaded, about 25 sites: a join that failed, a connection lost, a host that left,
a map missing, ...) or over any AE screen (the pause menu's). In a game with no AE screen up the game's widget shows
as before, and takes its input. The dialog: a title, the game's text (the port's own, or the game's string for the
error from ui\shell\strings\displayed_error_messages), one OK; A, B, START, Enter, Esc or a click on OK closes it,
and it takes all input while up (the top of AE's stack). An error that comes before AE's screens are there (back
from a game the game's main menu loads a moment before AE's) waits for them (ae_errors_tick); a newer one replaces a
waiting one.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "tag_files/tag_groups.h"
#include "text/text_group.h"
#include "interface/ui_widget.h"
#include "../src/ae_draw.h"
#include "ae_hooks.h"
#include "ae_setup_flow.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

#define ERROR_STRINGS "ui\\shell\\strings\\displayed_error_messages"

static struct
{
	int waiting;
	char title[48];
	char body[600];
} error;

/* the title for an error code: the game's header for it (its error widget's ATTENTION, ERROR or WARNING) in AE's
words */
static const char *error_title(short error_code)
{
	switch (error_code)
	{
	case _error_network_generic:
	case _error_network_join_game_closed:
	case _error_network_join_game_generic:
	case _error_network_failed_to_join_game:
		return "Couldn't join";
	case _error_network_server_shut_down:
	case _error_network_connection_lost:
	case _error_network_connected_play_no_network:
		return "Disconnected";
	case _error_network_out_of_sync_alert:
	case _error_network_trouble_is_brewing:
		return "Network problem";
	case _error_controller_unplugged_start_to_continue:
	case _error_controller_unplugged:
	case _error_controller_generic:
		return "Controller";
	default:
		return "Attention";
	}
}

/* UTF-8 of the game's text (UTF-16), its line breaks as spaces, at most size - 1 bytes */
static void utf8_of(wchar_t const *text, char *out, size_t size)
{
	size_t used = 0;

	for (; text && *text && used + 4 < size; text++)
	{
		unsigned int c = (unsigned int)(unsigned short)*text;

		if (c == '\r' || c == '\n' || c == '\t')
			c = ' ';
		if (c < 0x20)
			continue;
		if (c < 0x80)
			out[used++] = (char)c;
		else if (c < 0x800)
		{
			out[used++] = (char)(0xC0 | (c >> 6));
			out[used++] = (char)(0x80 | (c & 0x3F));
		}
		/* (surrogates dropped: the game's fonts have none) */
		else if (c < 0xD800 || c > 0xDFFF)
		{
			out[used++] = (char)(0xE0 | (c >> 12));
			out[used++] = (char)(0x80 | ((c >> 6) & 0x3F));
			out[used++] = (char)(0x80 | (c & 0x3F));
		}
	}
	/* (no space runs: "a  b" from a line break) */
	out[used] = 0;
	{
		char *read = out, *write = out;
		int space = 1;

		for (; *read; read++)
		{
			if (*read == ' ' && space)
				continue;
			space = *read == ' ';
			*write++ = *read;
		}
		if (write > out && write[-1] == ' ')
			write--;
		*write = 0;
	}
	/* (the game's "System Link" is AE's LAN: shorter, so in place) */
	{
		static char const from[] = "System Link", to[] = "LAN";
		char *at;

		while ((at = strstr(out, from)) != NULL)
		{
			memcpy(at, to, sizeof(to) - 1);
			memmove(at + sizeof(to) - 1, at + sizeof(from) - 1, strlen(at + sizeof(from) - 1) + 1);
		}
	}
}

/* AE's screens are there to show an error over: on, drawn */
static boolean shown_by_ae(void)
{
	return ae_menus_active() && ae_draw_available() && (main_menu_is_active() || ae_ui_depth() > 0);
}

static void closed(short choice, void *context)
{
	(void)choice;
	(void)context;
	platform_log("ae errors: closed");
}

static void show(void)
{
	static struct ae_flow_screen flow;
	struct ae_dialog_spec spec;

	error.waiting = 0;
	memset(&flow, 0, sizeof(flow));
	ae_flow_full_density(&flow);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_ERROR;
	spec.title = error.title;
	spec.body = error.body;
	spec.choices[0] = ae_string(AE_STR_OK);
	spec.choice_count = 1;
	spec.has_cancel = 1;
	spec.density = flow.density;
	spec.bounds = flow.frame.rect;
	spec.picked = closed;
	platform_log("ae errors: shown: %s", error.title);
	/* (an AE dialog already up: the error waits for it, ae_widgets_dialog.c's queue) */
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

boolean ae_errors_take(
	short error_code,
	wchar_t const *port_text)
{
	wchar_t const *text = port_text;

	if (!shown_by_ae())
		return FALSE;
	if (!text)
	{
		long strings = tag_loaded(UNICODE_STRING_LIST_TAG, ERROR_STRINGS);

		text = strings != NONE ? unicode_string_list_get_string(strings, error_code) : NULL;
	}
	snprintf(error.title, sizeof(error.title), "%s", error_title(error_code));
	utf8_of(text, error.body, sizeof(error.body));
	if (!error.body[0])
		snprintf(error.body, sizeof(error.body), "Something went wrong (error %d).", error_code);
	platform_log("ae errors: error %d taken (the game's dialog is not loaded): %s", error_code, error.body);
	error.waiting = 1;
	/* (AE's screens up: at once; else when they are: ae_errors_tick) */
	if (ae_ui_depth() > 0)
		show();
	return TRUE;
}

void ae_errors_tick(
	void)
{
	if (!error.waiting)
		return;
	/* (AE's menus left before it was shown (a game started): dropped, as the game's main menu error is) */
	if (!shown_by_ae())
	{
		error.waiting = 0;
		platform_log("ae errors: a waiting error dropped (AE's menus are gone)");
		return;
	}
	if (ae_ui_depth() > 0)
		show();
}
