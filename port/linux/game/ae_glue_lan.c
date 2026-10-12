/*
AE_GLUE_LAN.C

Join Game > LAN's firewall hint (item 73; ae_glue_lan.h): on Linux, when the list has found nothing for
AE_LAN_HINT_DELAY_MS and a firewall is on (ae_lan_hint.c reads which), its two lines under the list say that a
firewall can hide LAN games and where README says which ports to open, and debug.txt says it once. Elsewhere, or with
games found, or with no firewall found, the list is as it was.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "interface/ui_widget_instance.h"
#include "../src/ae_lan_hint.h"
#include "ae_glue_lan.h"

void platform_log(char const *format, ...);

/* the list's labels (strings.xml's join_game_ticker_labels: Players, Rules, Message) */
enum { LABEL_RULES = 1, LABEL_MESSAGE = 2 };

static struct
{
	unsigned long begin;
	/* looked for a firewall in this search, what was found */
	int looked;
	int firewall;
	char what[32];
	/* the hint shown this frame; said in debug.txt this run */
	int shown;
	int logged;
} lan;

void ae_lan_hint_begin(void)
{
	lan.begin = system_milliseconds();
	lan.looked = FALSE;
	lan.shown = FALSE;
}

int ae_lan_hint_line(int line, int empty, wchar_t *text, short size)
{
	char const *hint = line ? AE_LAN_HINT_LINE_2 : AE_LAN_HINT_LINE_1;
	short index;

	if (line == 0)
	{
		unsigned long elapsed = system_milliseconds() - lan.begin;

		/* (the files read once a search, when the delay is over with nothing found) */
		if (empty && elapsed >= AE_LAN_HINT_DELAY_MS && !lan.looked)
		{
			lan.looked = TRUE;
			lan.firewall = ae_lan_firewall(lan.what, sizeof(lan.what));
		}
		lan.shown = ae_lan_hint_due(elapsed, empty, lan.looked && lan.firewall);
		if (lan.shown && !lan.logged)
		{
			lan.logged = TRUE;
			platform_log("lan: no games found in %lu s and a firewall is on (%s): %s %s",
				AE_LAN_HINT_DELAY_MS / 1000, lan.what, AE_LAN_HINT_LINE_1, AE_LAN_HINT_LINE_2);
		}
	}
	if (!lan.shown || !text || size <= 0)
		return FALSE;
	for (index = 0; index < size - 1 && hint[index]; index++)
		text[index] = (wchar_t)hint[index];
	text[index] = 0;
	return TRUE;
}

void ae_lan_hint_label(struct widget_instance *label)
{
	if (label)
		label->parameters.text_box.string_list_index = lan.shown ? LABEL_MESSAGE : LABEL_RULES;
}
